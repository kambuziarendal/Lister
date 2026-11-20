// SPDX-License-Identifier: MIT
#include "MainWindow.h"
#include "ViewVisibility.h"
#include "MainWindowMenu.h"
#include "ApplicationServices.h"
#include "ArchivePasswords.h"
#include "Common.h"
#include "ClipboardFlow.h"
#include "FileDropFlow.h"
#include "FileOperationFlow.h"
#include "FlatViewWindow.h"
#include "PaneContextMenu.h"
#include "FunctionButtonBank.h"
#include "JobSnapshot.h"
#include "ArchiveEditor.h"
#include "FolderCompare.h"
#include "PropertiesWindow.h"
#include "OpenWith.h"
#include "ExplicitSubmit.h"
#include "SavedNavigationController.h"
#include "SearchWindow.h"
#include "Shelf.h"
#include "ShelfWindow.h"
#include "ImagePreview.h"
#include "InspectorEditor.h"
#include "InlineAction.h"
#include "OperationQueue.h"
#include "Pane.h"
#include "PanelTools.h"
#include "TerminalDock.h"
#include "VirtualFile.h"
#include "WindowKeyFilter.h"
#include "WindowScripting.h"
#include "WorkspaceEditor.h"
#include "WorkspaceSplit.h"
#include <Application.h>
#include <exception>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <SplitView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>
#include <cstring>
#include <map>
#include <memory>
using Listi::PanelPage;

void PostMainWindowNotice(const char *text) {
  BMessage error(kServiceError);
  error.AddString("error", text);
  be_app->PostMessage(&error);
}

namespace {
constexpr uint32 kPreviewPattern = 'sppv', kApplyPattern = 'spap';
class MainWindow : public BWindow {
  Preferences prefs;
  BMessenger searchWindow;
  std::map<std::string, BMessenger> flatViews;
  std::shared_ptr<Listi::Shelf> shelf = std::make_shared<Listi::Shelf>();
  BMessenger shelfWindow;
  void RefreshShelf() {
    for (auto pane : panes)
      pane->RefreshShelf();
    if (shelfWindow.IsValid()) {
      BMessage refresh(Listi::kRefreshShelf);
      shelfWindow.SendMessage(&refresh, (BHandler *)nullptr, 0);
    }
  }
  void AddToShelf(const std::map<std::string, Listi::NodeIdentity> &expected = {}) {
    try {
      const auto added = shelf->Add(panes[source]->Selected(), expected);
      BString status;
      if (added)
        status << int32(added)
               << " added to Shelf. Open it from the Listi menu.";
      else
        status = "Already on the Shelf.";
      panes[source]->ShowStatus(status);
      RefreshShelf();
    } catch (const std::exception &e) {
      PostMainWindowNotice(e.what());
    }
  }
  Listi::SavedNavigationController savedNavigation;
  void PublishPlaces() {
    for (auto pane : panes)
      pane->SetPlaces(savedNavigation.Places(), savedNavigation.CanEditPlaces());
  }
  Pane *panes[2];
  bool ToolAllowsCommand(uint32 action) const {
    switch (action) {
    case kCopy: case kMove: case kSame: case kCreateLink: case kDuplicate:
    case kCompareFolders: case kCreateArchive: case kExtractArchive:
      return split->Panel(0).Current() == PanelPage::Files &&
             split->Panel(1).Current() == PanelPage::Files;
    case kClipboardCopy: case kClipboardCut: case kClipboardPaste: case kCopyPaths:
    case kClipboardSaveText: case kClipboardSaveImage: case kUndoBulkRename:
    case kEdit: case kEditExternally: case kView: case kPreviewOtherPanel:
    case kOpenWith: case kOpenSystem: case kProperties:
    case kDelete: case kRestore: case kPermanentDelete: case kRename:
    case kBulkRename: case kMkdir: case kAddArchive: case kRemoveArchive:
    case kAll: case kNone: case kInvert: case kSelectType: case kSelectPattern:
    case kApplyPattern: case kGridMode: case Listi::kAddToShelf:
    case Listi::kFlatView:
      return split->Panel(source).Current() == PanelPage::Files;
    default:
      return true;
    }
  }
  void ActivatePanel(int pane) {
    source = pane;
    for (int i = 0; i < 2; ++i)
      panes[i]->SetSource(i == source);
  }
  void PublishSearchStore(
      const Listi::SavedNavigationController::SearchUpdate &reply) {
    auto send = [&](const BMessenger &recipient) {
      if (!recipient.IsValid())
        return;
      auto update = recipient == reply.author ? reply.message
                                              : savedNavigation.SearchStore();
      recipient.SendMessage(&update, static_cast<BHandler *>(nullptr), 0);
    };
    send(searchWindow);
    for (int pane = 0; pane < 2; ++pane)
      send(split->Panel(pane).SearchMessenger());
  }
  bool quickView = false;
  MainWindowMenu *menu;
  Listi::WorkspaceSplit *split;
  std::unique_ptr<Listi::TerminalDock> terminalDock;
  BGroupView *topTools, *bottomTools;
  void Save() {
    auto saved = prefs.Save();
    saved.what = kSavePreferences;
    service.SendMessage(&saved);
  }
  InlineAction *actionArea;
  void CloseAction() {
    actionArea->Close();
    clipboard->CancelPrompt();
    RefreshBank();
    split->Panel(source).FocusCurrent();
  }
  void AskInline(const char *text, const char *button, const BMessage &result,
                 const char *initial = nullptr) {
    if (!actionArea->Ask(text, button, result, initial))
      queue->ErrorInfo("Finish or cancel the open question first.");
  }
  void SelectByPattern(const BMessage &request) {
    auto *pane = panes[source];
    const int64 revision = pane->SelectionRevision();
    if (request.what == kSelectPattern) {
      if (revision < 0) {
        pane->ShowStatus("Load a folder before selecting by pattern.");
        return;
      }
      BMessage preview(kPreviewPattern);
      preview.AddInt32("pane", source);
      preview.AddInt64("revision", revision);
      AskInline("Match visible names, including folders. Use * or ? (case-sensitive).",
                "Preview", preview, "*");
      return;
    }
    if (revision < 0 || request.GetInt32("pane", -1) != source ||
        request.GetInt64("revision", -1) != revision) {
      pane->ShowStatus("Panel contents changed. Start Select by pattern again.");
      return;
    }
    const char *pattern = request.GetString("name", "");
    const size_t count = pane->MatchPattern(pattern, request.what == kApplyPattern);
    if (count == 0) {
      pane->ShowStatus("0 matches. Selection unchanged.");
    } else if (request.what == kPreviewPattern) {
      BMessage apply(request);
      apply.what = kApplyPattern;
      auto prompt = std::to_string(count) +
                    (count == 1 ? " visible entry matches " : " visible entries match ") +
                    pattern + ". Replace the selection?";
      AskInline(prompt.c_str(), "Select", apply);
    }
  }
  int source = 0;
  std::unique_ptr<Listi::ClipboardFlow> clipboard;
  std::unique_ptr<Listi::FileDropFlow> fileDrop;
  std::unique_ptr<Listi::FileOperationFlow> fileOperations;
  OperationQueue *queue;
  Listi::FunctionButtonBank *bank = nullptr;
  BTextControl *command;
  BMessenger service;
  void UpdateToolZones() {
    for (auto *zone : {topTools, bottomTools}) {
      const bool hasBank = bank->Parent() == zone && !bank->IsHidden(bank);
      const bool hasCommand = command->Parent() == zone && !command->IsHidden(command);
      Listi::SetViewVisible(zone, hasBank || hasCommand);
    }
  }
  void SetBankVisible(bool visible) {
    Listi::SetViewVisible(bank, visible);
    UpdateToolZones();
  }
  void ShowBank(bool visible) {
    const bool dual = !prefs.SinglePane() && !prefs.actionButtons.empty();
    visible = visible && dual;
    bool bankOverflow = false;
    SetBankVisible(visible);
    if (visible) {
      // Showing a layout item invalidates layout but does not run it inline.
      // Measure before the window's auto-size pass can impose a larger frame.
      auto minimum = GetLayout()->MinSize();
      if (minimum.width > Bounds().Width() && !bank->IsCompact()) {
        bank->SetActions(prefs.actionButtons, true);
        minimum = GetLayout()->MinSize();
      }
      bankOverflow = minimum.width > Bounds().Width() ||
                     minimum.height > Bounds().Height();
      if (bankOverflow)
        SetBankVisible(false);
    }
    menu->SetFunctionButtonState(dual, prefs.ActionButtonsVisible(), bankOverflow);
  }
  void RefreshBank() {
    ShowBank(prefs.ActionButtonsVisible() && !actionArea->Pending());
  }
  void UpdateBank(float width) {
    bank->SetActions(prefs.actionButtons, prefs.adaptive && width < 850);
  }
  void PlaceTools() {
    auto *bankZone = prefs.bankTop ? topTools : bottomTools;
    if (bank->Parent() != bankZone) {
      bank->RemoveSelf();
      bankZone->GroupLayout()->AddView(int32(0), bank);
    }
    auto *commandZone = prefs.commandTop ? topTools : bottomTools;
    if (command->Parent() != commandZone) {
      command->RemoveSelf();
      commandZone->AddChild(command);
    }
  }
  void RefreshPanelViews() {
    split->UpdateInspector(prefs, quickView, source);
    menu->SetPreview(prefs, quickView || split->HasPanelPreview());
  }
  void RefreshAndFocusPanel() {
    RefreshPanelViews();
    split->Panel(source).FocusCurrent();
  }
  void ClosePanelTool(int pane) {
    auto &panel = split->Panel(pane);
    const int focus = panel.Current() == PanelPage::Preview ? 1 - pane : pane;
    if (panel.CloseCurrent()) {
      ActivatePanel(focus);
      RefreshAndFocusPanel();
    }
  }
  ImagePreview *OpenPanelPreview(int pane) {
    auto *preview = split->OpenPanelPreview(pane);
    if (preview)
      RefreshAndFocusPanel();
    return preview;
  }
  void SetQuickView(bool enabled) {
    if (quickView != enabled) {
      if (!enabled && !split->Inspector().CloseEditor())
        return;
      quickView = enabled;
      ApplyWorkspace();
    }
    split->Panel(source).FocusCurrent();
  }
  void ApplyWorkspace() {
    const bool previewFirst = quickView ? source == 1 : prefs.inspectorLeft;
    split->PlaceInspector(previewFirst, prefs.splitWeights[2]);
    PlaceTools();
    UpdateBank(Bounds().Width());
    for (int i = 0; i < 2; ++i) {
      split->Panel(i).EnableTools(prefs.IsSmidja());
      split->SetPaneVisible(i,
                            i == source || (!quickView && !prefs.SinglePane()));
      panes[i]->SetWorkspace(prefs);
      panes[i]->SetSource(i == source);
    }
    RefreshPanelViews();
    split->ApplyGeometry(prefs, quickView, source);
    menu->SetWorkspace(prefs, quickView);
    queue->SetCompact(prefs.SinglePane());
    Listi::SetViewVisible(command, prefs.command && !prefs.SinglePane());
    UpdateToolZones();
    RefreshBank();
  }
  void SetWorkspace(Workspace value) {
    if (prefs.workspace == value || !split->CloseEditors())
      return;
    quickView = false;
    prefs.workspace = value;
    prefs.singlePane = source;
    ApplyWorkspace();
    if (!actionArea->Pending())
      split->Panel(source).FocusCurrent();
    Save();
  }
  void Apply() {
    for (auto p : panes) {
      p->Apply(prefs);
      p->Reload();
    }
    ApplyWorkspace();
  }
  void Open(uint32 action) {
    const auto path = panes[source]->SelectedPath();
    if (path.empty()) {
      PostMainWindowNotice("Select exactly one file.");
      return;
    }
    const bool editInPanel = action == kEdit && !Listi::IsVirtual(path);
    if (action == kPreviewOtherPanel || editInPanel) {
      ImagePreview *target;
      if (prefs.IsSmidja()) {
        target = OpenPanelPreview(1 - source);
        if (!target)
          return;
      } else {
        SetQuickView(true);
        target = &split->Inspector();
      }
      if (editInPanel)
        target->BeginEditing(path);
      return;
    }
    if (action == kView) {
      ImagePreview::OpenWindow(path);
      return;
    }
    const bool externalEdit = action != kOpenSystem;
    if (externalEdit && Listi::ParseLocation(path).Archive()) {
      OpenArchiveEditor(path, prefs.editor.String());
      return;
    }
    BMessage m(kOpenPaths);
    m.AddString("path", path.c_str());
    m.AddString("mode", externalEdit ? "edit" : "system");
    if (externalEdit)
      m.AddString("application", prefs.editor);
    service.SendMessage(&m);
  }

public:
  MainWindow(const Preferences &p, BMessenger svc,
             const Listi::SavedPlaces &saved, bool writable,
             const Listi::SavedSearches &queries, bool queriesWritable)
      : BWindow(BRect(70, 70, 1170, 710), "Listi 0.37.0", B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS),
        prefs(p), savedNavigation(svc, saved, writable, queries, queriesWritable),
        service(svc) {
    source = prefs.SinglePane() ? prefs.singlePane : 0;
    menu = new MainWindowMenu();
    panes[0] = new Pane(0, prefs, shelf);
    panes[1] = new Pane(1, prefs, shelf);
    split = new Listi::WorkspaceSplit(new Listi::PanelTools(0, *panes[0]),
                                      new Listi::PanelTools(1, *panes[1]));
    auto *terminalSplit = new BSplitView(B_VERTICAL, 6);
    terminalDock = std::make_unique<Listi::TerminalDock>(
        terminalSplit, split, BMessenger(this));
    bank = new Listi::FunctionButtonBank(*menu);
    command = new BTextControl("Command:", "", nullptr);
    command->TextView()->AddFilter(
        new Listi::ExplicitSubmit(BMessenger(this), BMessage(kCommand)));
    command->SetToolTip(
        "Runs a shell command in the active directory using Terminal.");
    queue = new OperationQueue(BMessenger(this));
    actionArea = new InlineAction(BMessenger(this), [this] { ShowBank(false); });
    clipboard = std::make_unique<Listi::ClipboardFlow>(*actionArea, *queue);
    fileDrop = std::make_unique<Listi::FileDropFlow>(
        *actionArea, *queue, BMessenger(this), service);
    fileOperations = std::make_unique<Listi::FileOperationFlow>(
        *actionArea, *queue, *clipboard, BMessenger(this));
    actionArea->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    actionArea->GroupLayout()->SetInsets(8, 6, 8, 6);
    actionArea->Hide();
    topTools = new BGroupView(B_VERTICAL, 5);
    topTools->SetName("top workspace tools");
    topTools->GroupLayout()->SetInsets(8, 4, 8, 4);
    bottomTools = new BGroupView(B_VERTICAL, 5);
    bottomTools->SetName("bottom workspace tools");
    bottomTools->GroupLayout()->SetInsets(8, 4, 8, 8);
    bottomTools->AddChild(bank);
    bottomTools->AddChild(command);
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu, 0)
        .Add(topTools, 0)
        .Add(terminalSplit)
        .Add(actionArea, 0)
        .Add(queue->View(), 0)
        .Add(bottomTools, 0);
    // Child visibility must not impose a transient maximum on the window.
    // Apply after attachment: Haiku replaces layout explicit-data proxies.
    GetLayout()->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
    queue->Start();
    AddCommonFilter(new Listi::WindowKeyFilter());

    Apply();
    PublishPlaces();
    split->Panel(source).FocusCurrent();
    ResizeTo(1140, 650);
  }
  void FrameResized(float width, float height) override {
    BWindow::FrameResized(width, height);
    if (bank) {
      UpdateBank(width);
      RefreshBank();
    }
  }

  status_t GetSupportedSuites(BMessage *m) override {
    Listi::AddWindowScriptingSuite(*m);
    return BWindow::GetSupportedSuites(m);
  }
  BHandler *ResolveSpecifier(BMessage *msg, int32 index, BMessage *spec,
                             int32 what, const char *property) override {
    if (Listi::IsWindowScriptProperty(property))
      return this;
    return BWindow::ResolveSpecifier(msg, index, spec, what, property);
  }
  ~MainWindow() {
    terminalDock.reset();
    delete queue;
  }
  void DispatchMessage(BMessage *message, BHandler *target) override {
    if (prefs.IsSmidja() &&
        (message->what == B_MOUSE_DOWN || message->what == B_KEY_DOWN)) {
      auto *receiver = message->what == B_MOUSE_DOWN
                           ? dynamic_cast<BView *>(target) : CurrentFocus();
      const int pane = split->PanelContaining(receiver);
      if (pane >= 0 && source != pane) {
        ActivatePanel(pane);
        split->UpdatePreviews(source);
      }
    }
    BWindow::DispatchMessage(message, target);
  }
  void MenusBeginning() override {
    BWindow::MenusBeginning();
    const char *path = panes[source]->Path();
    const bool canOpenFlatView = path[0] == '/' && !Listi::IsVirtual(path);
    const bool canSaveClipboard =
        !actionArea->Pending() && Listi::CanSaveClipboardIn(path);
    const bool canUndoBulkRename =
        !actionArea->Pending() && queue->CanUndoBulkRename();
    menu->SetFileActions(panes[source]->CurrentView(), canOpenFlatView,
                         canSaveClipboard, canUndoBulkRename);
  }

  bool QuitRequested() override {
    if (!InspectorEditorsAllowQuit() || !FilePropertiesAllowQuit() || !OpenWithAllowQuit() ||
        !ArchiveEditorsAllowQuit())
      return false;
    if (savedNavigation.Saving()) {
      PostMainWindowNotice(
          "Saved places or searches are still being written. Please try quitting again shortly.");
      return false;
    }
    if (queue->Busy() || clipboard->Busy()) {
      queue->Show();
      PostMainWindowNotice("File operations are still running. Let them finish, or use "
             "Cancel all before quitting.");
      return false;
    }
    if (clipboard->ArchivePromptPending())
      CloseAction();
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
  }
  void MessageReceived(BMessage *m) override {
    if (Listi::HandleWindowScript(*m, prefs.workspace, *panes[source],
                                  *panes[1 - source], *queue))
      return;
    if (prefs.IsSmidja() && !ToolAllowsCommand(m->what)) {
      queue->ErrorInfo("Switch to Files in the affected panel before using this command.");
      return;
    }
    if (fileOperations->Handle(*m, *panes[source], *panes[1 - source],
                               prefs.SinglePane()))
      return;
    switch (m->what) {
    case Listi::kShowShelf:
      Listi::OpenShelfWindow(BMessenger(this), shelf, shelfWindow);
      break;
    case Listi::kAddToShelf:
      AddToShelf();
      break;
    case Listi::kRefreshShelf:
      RefreshShelf();
      break;
    case Listi::kShelfChanged:
      for (auto pane : panes)
        pane->RefreshShelf();
      break;
    case Listi::kSubmitShelfTransfer: {
      auto job = Listi::DecodeSubmittedJob(
          *m, {Listi::Action::Copy, Listi::Action::Move});
      if (job && !job->sources.empty() &&
          job->expectedSources.size() == job->sources.size() &&
          job->expectedDestination.device >= 0)
        queue->Enqueue(*job);
      break;
    }
    case kPlaceAction: {
      if (!savedNavigation.CanEditPlaces()) {
        PostMainWindowNotice("Saved places are unavailable or still being saved.");
        break;
      }
      const char *path = m->GetString("path", "");
      int32 action = m->GetInt32("action", -1);
      auto place = savedNavigation.Places().Find(path);
      if (action < 0 || action > 2 || (action != 0 && !place))
        break;
      BMessage accepted(*m);
      accepted.what = kPlaceAccepted;
      accepted.AddBool("placeLabel", true);
      std::string initial = place ? place->name : Listi::LocationLeaf(path);
      if (initial.empty())
        initial = path;
      std::string question = action == 0   ? "Save this work location: "
                             : action == 1 ? "Rename saved place: "
                                           : "Forget saved place: ";
      question += path;
      AskInline(question.c_str(),
                action == 0   ? "Save place"
                : action == 1 ? "Rename place"
                              : "Forget place",
                accepted, action == 2 ? nullptr : initial.c_str());
      break;
    }
    case kPlaceAccepted:
      try {
        savedNavigation.EditPlace(*m);
        PublishPlaces();
      } catch (const std::exception &e) {
        PostMainWindowNotice(e.what());
      }
      break;
    case kSearchEdit: {
      BMessenger author(searchWindow);
      m->FindMessenger("searchEditor", &author);
      if (auto reply = savedNavigation.EditSearch(*m, author))
        PublishSearchStore(*reply);
      break;
    }
    case kSearchesSaved:
      if (auto reply = savedNavigation.FinishSearchSave(*m))
        PublishSearchStore(*reply);
      break;
    case kPlacesSaved:
      if (savedNavigation.FinishPlaceSave(*m)) {
        if (m->HasString("error"))
          PostMainWindowNotice(m->GetString("error", "Save failed."));
        PublishPlaces();
      }
      break;
    case kInlineNotice:
      if (actionArea->Pending()) {
        queue->ErrorInfo(m->GetString("error", "Operation failed"));
        break;
      }
      AskInline(m->GetString("error", "Operation failed"), "OK",
                BMessage(kInlineCancel));
      break;
    case kInlineClosed:
      if (!actionArea->Pending()) {
        if (clipboard->ArchivePromptPending())
          CloseAction();
        else
          clipboard->CancelPrompt();
      }
      RefreshBank();
      if (!actionArea->Pending() && !terminalDock->HasFocus())
        split->Panel(source).FocusCurrent();
      break;
    case kInlineCancel:
      if (panes[source]->CancelSelectionGesture())
        break;
      if (actionArea->Pending())
        CloseAction();
      else if (split->Panel(source).Current() == PanelPage::Preview)
        ClosePanelTool(source);
      else if (split->Panel(source).Current() != PanelPage::Files) {
        split->Panel(source).Select(PanelPage::Files);
        RefreshAndFocusPanel();
      } else if (split->HasPanelPreview()) {
        ClosePanelTool(1 - source);
      } else if (quickView) {
        SetQuickView(false);
      } else
        queue->Collapse();
      break;
    case Listi::kPanelPage:
    case Listi::kClosePanelPage: {
      const int32 pane = m->GetInt32("pane", -1);
      if (!prefs.IsSmidja() || pane < 0 || pane > 1)
        break;
      if (m->what == Listi::kClosePanelPage) {
        ClosePanelTool(pane);
        break;
      }
      const int32 page = m->GetInt32("page", -1);
      if (page >= int32(PanelPage::Files) &&
          page <= int32(PanelPage::Preview) &&
          split->Panel(pane).Select(PanelPage(page))) {
        ActivatePanel(pane);
        RefreshAndFocusPanel();
      }
      break;
    }
    case kQuickView:
      if (actionArea->Pending())
        break;
      if (prefs.IsSmidja()) {
        const int pane = split->Panel(source).Current() == PanelPage::Preview
                             ? source
                             : 1 - source;
        if (split->Panel(pane).Current() == PanelPage::Preview)
          ClosePanelTool(pane);
        else
          OpenPanelPreview(pane);
        break;
      }
      SetQuickView(!quickView);
      break;
    case kCloseQuickView:
      if (quickView && !actionArea->Pending())
        SetQuickView(false);
      break;
    case kPreviewToggle:
      if (prefs.SinglePane() || quickView || split->HasPanelPreview())
        break;
      if (!split->CloseVisibleInspectorEditor())
        break;
      prefs.SetPreview(!prefs.Preview());
      RefreshPanelViews();
      Save();
      break;
    case kPreviewSelection:
      if (m->GetInt32("pane", -1) == source || split->HasPanelPreview())
        split->UpdatePreviews(source);
      if (m->GetInt32("pane", -1) == source && m->HasString("operationError"))
        queue->ErrorInfo(m->GetString("operationError", ""));
      break;
    case kPaneDisplay:
      for (int i = 0; i < 2; ++i) {
        prefs.SetSidebar(i, panes[i]->Sidebar());
        prefs.SetPaneMode(i, panes[i]->PaneOverride());
      }
      if (m->HasString("folder")) {
        std::string folder = m->GetString("folder", "");
        if (m->GetBool("forget", false))
          prefs.folderModes.erase(folder);
        else
          prefs.folderModes[folder] = m->GetInt32("mode", 0);
      }
      panes[0]->FolderModes(prefs.folderModes);
      panes[1]->FolderModes(prefs.folderModes);
      Save();
      break;
    case kGridMode:
      panes[source]->MessageReceived(m);
      break;
    case kWorkspace: {
      int32 value =
          m->GetInt32("workspace", m->GetBool("simple", false) ? 1 : 0);
      if (value >= 0 && value <= 3)
        SetWorkspace(static_cast<Workspace>(value));
    } break;
    case kActivate: {
      const int32 pane = m->GetInt32("pane", source);
      if (pane < 0 || pane > 1 ||
          split->Panel(pane).Current() != PanelPage::Files ||
          ((quickView || prefs.SinglePane()) && pane != source))
        break;
      ActivatePanel(pane);
      split->UpdatePreviews(source);
      break;
    }
    case kSwitch:
      if (quickView)
        break;
      if (prefs.workspace == Workspace::askr) {
        panes[source]->ToggleTreeFocus();
        break;
      }
      if (prefs.SinglePane())
        break;
      ActivatePanel(1 - source);
      split->Panel(source).FocusCurrent();
      split->UpdatePreviews(source);
      break;
    case kSame:
      if (prefs.SinglePane())
        break;
      panes[1 - source]->Navigate(panes[source]->Path());
      break;
    case kRefresh:
      for (auto p : panes)
        p->Reload();
      break;
    case kSearch:
      if (prefs.IsSmidja()) {
        if (split->Panel(source).OpenSearch(savedNavigation.SearchStore()))
          RefreshAndFocusPanel();
        break;
      }
      if (searchWindow.IsValid())
        searchWindow.SendMessage(kSearch);
      else
        searchWindow = OpenFileSearch(BMessenger(this), source,
                                      panes[source]->Path(), savedNavigation.SearchStore());
      break;
    case Listi::kFlatView: {
      const char *path = panes[source]->Path();
      if (path[0] != '/' || Listi::IsVirtual(path))
        break;
      for (auto it = flatViews.begin(); it != flatViews.end();) {
        if (!it->second.IsValid())
          it = flatViews.erase(it);
        else
          ++it;
      }
      auto &window = flatViews[path];
      if (!window.IsValid() || window.SendMessage(Listi::kFlatView) != B_OK)
        window = Listi::OpenFlatView(BMessenger(this), shelf, path);
      break;
    }
    case kRevealSearch: {
      int32 which = m->GetInt32("pane", -1);
      if (which < 0 || which > 1)
        break;
      if (quickView && source != which) {
        if (!split->Inspector().CloseEditor())
          break;
        quickView = false;
        ApplyWorkspace();
      }
      if (!split->Panel(which).Select(PanelPage::Files))
        break;
      source = which;
      RefreshPanelViews();
      if (prefs.SinglePane()) {
        prefs.singlePane = source;
        ApplyWorkspace();
        Save();
      }
      for (int i = 0; i < 2; ++i)
        panes[i]->SetSource(i == source);
      panes[source]->MessageReceived(m);
      Activate();
      split->Panel(source).FocusCurrent();
      break;
    }
    case kFilter:
      if (!split->Panel(source).Select(PanelPage::Files))
        break;
      RefreshPanelViews();
      panes[source]->FocusFilter();
      break;
    case kContextMenu: {
      if (actionArea->Pending()) {
        queue->ErrorInfo("Finish or cancel the open question first.");
        break;
      }
      const int32 owner = m->GetInt32("pane", -1);
      if (owner < 0 || owner > 1 ||
          split->Panel(owner).Current() != PanelPage::Files ||
          ((prefs.SinglePane() || quickView) && owner != source))
        break;
      ActivatePanel(owner);
      try {
        auto result = Listi::RunPaneContextMenu(
            *m, *panes[owner], *panes[1 - owner], owner, source,
            prefs.SinglePane(),
            !prefs.SinglePane() &&
                split->Panel(1 - owner).Current() == PanelPage::Files,
            prefs.folderModes.count(panes[owner]->Path()),
            savedNavigation.Places(), savedNavigation.CanEditPlaces());
        // Keep dispatch synchronous so a queued activation cannot redirect it.
        if (result.command.what == Listi::kAddToShelf)
          AddToShelf(result.shelfSources);
        else if (result.command.what)
          MessageReceived(&result.command);
      } catch (const std::exception &e) {
        PostMainWindowNotice(e.what());
      }
      break;
    }
    case 'apfg':
      Listi::ForgetArchivePasswords();
      break;
    case kResumeArchiveEdits:
      ResumeArchiveEditors(prefs.editor.String());
      break;
    case kOpenWith: {
      const auto path = panes[source]->SelectedPath();
      if (path.empty())
        PostMainWindowNotice("Select one local file for Open with.");
      else
        OpenWithFile(path);
      break;
    }
    case kProperties: {
      const auto path = panes[source]->SelectedPath();
      if (path.empty())
        PostMainWindowNotice("Select one local file or folder for Properties.");
      else
        OpenFileProperties(path, BMessenger(panes[source]));
      break;
    }
    case kClipboardSaveText:
    case kClipboardSaveImage:
    case kClipboardCopy:
    case kClipboardCut:
    case kClipboardPaste:
    case kCopyPaths:
      clipboard->Begin(m->what, *panes[source]);
      break;
    case Listi::kClipboardFileAccepted: {
      const char *name = m->GetString("name", "");
      if (auto folder = clipboard->SaveFile(name)) {
        const auto status = std::string("Saved ") + name;
        for (auto *pane : panes) {
          if (pane->Path() == *folder) {
            pane->Reload();
            pane->ShowStatus(status.c_str());
          }
        }
      }
      break;
    }
    case kDrop:
    case Listi::kDropAccepted:
    case kImageDropOutputs:
    case Listi::kImageDropConvert:
      fileDrop->Handle(*m);
      break;
    case kFileProgress:
      for (auto p : panes) {
        p->MessageReceived(m);
        if (m->HasString("destination") &&
            Listi::ParentLocation(m->GetString("destination", "")) == p->Path())
          p->Reload();
      }
      break;
    case kQueueWake:
      for (auto &done : queue->TakeEvents()) {
        queue->PresentEvent(done);
        if (done.what != kJobDone)
          continue;
        clipboard->JobFinished(done);
        for (auto p : panes)
          p->MessageReceived(&done);
        RefreshShelf();
      }
      break;
    case kShowJobs:
      queue->Toggle();
      break;
    case kCompareFolders:
      OpenFolderCompare(panes[source]->Path(), panes[1 - source]->Path());
      break;
    case kSelectPattern:
    case kPreviewPattern:
    case kApplyPattern:
      SelectByPattern(*m);
      break;
    case kSelectType:
      if (!panes[source]->SelectSameFileType())
        PostMainWindowNotice(
            "Choose a file with a filename extension. Only visible files of that type are selected; folders and links are excluded.");
      break;
    case kAll:
      panes[source]->Select(1);
      break;
    case kNone:
      panes[source]->Select(0);
      break;
    case kInvert:
      panes[source]->Select(2);
      break;
    case kPreviewOtherPanel:
    case kView:
    case kEdit:
    case kEditExternally:
    case kOpenSystem:
      Open(m->what);
      break;
    case kOpenPaths:
      service.SendMessage(m);
      break;
    case Listi::kTerminalBelow:
    case Listi::kTerminalBeside:
    case Listi::kTerminalAbove:
    case Listi::kTerminalLeft:
    case Listi::kToggleTerminalSide:
    case Listi::kFloatTerminal:
    case Listi::kDockTerminal:
      terminalDock->ApplyPlacement(*m);
      break;
    case kTerminal:
    case Listi::kLeftCLI:
    case Listi::kRightCLI:
    case Listi::kShowFloatingTerminal: {
      const int lister = m->what == Listi::kLeftCLI    ? 0
                         : m->what == Listi::kRightCLI ? 1
                         : m->GetInt32("terminal lister", source);
      if (lister < 0 || lister > 1)
        break;
      if (auto prompt = terminalDock->Open(*m, lister, panes[lister]->Path()))
        AskInline(prompt->text.String(), "Start new CLI", prompt->accepted);
      break;
    }
    case Listi::kCloseTerminal:
      if (terminalDock->Close(m->GetInt64("terminal placement", -1))) {
        Activate();
        if (!terminalDock->HasFocus())
          split->Panel(source).FocusCurrent();
      }
      break;
    case Listi::kTerminalWindow:
    case kCommand: {
      if (!strcmp(panes[source]->Path(), "@disks") ||
          Listi::IsVirtual(panes[source]->Path())) {
        PostMainWindowNotice("Open a local folder first.");
        break;
      }
      BMessage launch(kOpenPaths);
      launch.AddString("path", panes[source]->Path());
      launch.AddString("mode", m->what == Listi::kTerminalWindow ? "terminal"
                                                               : "command");
      launch.AddString("command", command->Text());
      launch.AddString("application", prefs.terminal);
      service.SendMessage(&launch);
      break;
    }
    case kNetwork:
      if (!actionArea->Connect(BMessenger(panes[source])))
        queue->ErrorInfo("Finish or cancel the open question first.");
      break;
    case Listi::kSplitChanged:
      split->RememberWeights(prefs, quickView);
      Save();
      break;
    case kToggleBank:
      if (!prefs.SinglePane() && !prefs.actionButtons.empty()) {
        auto &choice = prefs.workspace == Workspace::smidja
                           ? prefs.smidjaButtons : prefs.tviskiptButtons;
        choice = !prefs.ActionButtonsVisible();
        RefreshBank();
        Save();
      }
      break;
    case kOpenWorkspaceEditor:
      if (actionArea->Pending())
        queue->ErrorInfo("Finish or cancel the open question first.");
      else
        OpenWorkspaceEditor(BMessenger(this), prefs, m->GetBool("modules", false));
      break;
    case kApplyWorkspaceEditor: {
      if (actionArea->Pending()) {
        queue->ErrorInfo("Finish or cancel the open question first.");
        break;
      }
      BMessage layoutMessage, profilesMessage;
      if (m->FindMessage("layout", &layoutMessage) != B_OK ||
          m->FindMessage("profiles", &profilesMessage) != B_OK)
        break;
      if (!split->CloseEditors())
        break;
      WorkspaceLayout layout;
      layout.Load(layoutMessage);
      prefs.profiles.Load(profilesMessage);
      if (layout.buttons != prefs.buttons)
        prefs.smidjaButtons = prefs.tviskiptButtons = -1;
      quickView = false;
      prefs.ApplyLayout(layout);
      if (prefs.SinglePane())
        source = prefs.singlePane;
      else
        prefs.singlePane = source;
      ApplyWorkspace();
      split->Panel(source).FocusCurrent();
      Save();
      break;
    }
    case kPreferences:
      OpenPreferences(BMessenger(this), prefs);
      break;
    case kApplyPreferences:
      prefs.ApplyDialog(*m);
      Apply();
      Save();
      break;
    default:
      BWindow::MessageReceived(m);
      break;
    }
  }
};
} // namespace

BWindow *CreateMainWindow(const Preferences &prefs, BMessenger service,
                          const Listi::SavedPlaces &places, bool placesWritable,
                          const Listi::SavedSearches &searches, bool searchesWritable) {
  return new MainWindow(prefs, service, places, placesWritable,
                        searches, searchesWritable);
}
