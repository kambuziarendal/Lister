// SPDX-License-Identifier: MIT
#include "MainWindow.h"
#include "MainWindowMenu.h"
#include "ApplicationServices.h"
#include "ArchivePasswords.h"
#include "Common.h"
#include "ClipboardFlow.h"
#include "FileDropFlow.h"
#include "FileOperationFlow.h"
#include "FlatViewWindow.h"
#include "PaneContextMenu.h"
#include "ArchiveEditor.h"
#include "FolderCompare.h"
#include "PropertiesWindow.h"
#include "OpenWith.h"
#include "SavedNavigationController.h"
#include "SearchWindow.h"
#include "ShelfController.h"
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
#include "WorkspaceToolbar.h"
#include <Application.h>
#include <array>
#include <exception>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <SplitView.h>
#include <Window.h>
#include <cstring>
#include <memory>
using Listi::PanelPage;

void PostMainWindowNotice(const char *text) {
  BMessage error(kServiceError);
  error.AddString("error", text);
  be_app->PostMessage(&error);
}

namespace {
class MainWindow : public BWindow {
  Preferences prefs;
  BMessenger searchWindow;
  Listi::SavedNavigationController savedNavigation;
  void PublishPlaces() {
    for (auto pane : panes)
      pane->SetPlaces(savedNavigation.Places(), savedNavigation.CanEditPlaces());
  }
  std::array<Pane *, 2> panes{};
  Listi::ShelfController shelf{BMessenger(this), panes};
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
    case Listi::kApplyPattern: case kGridMode: case Listi::kAddToShelf:
    case Listi::kFlatView:
      return split->Panel(split->Source()).Current() == PanelPage::Files;
    default:
      return true;
    }
  }
  MainWindowMenu *menu = new MainWindowMenu();
  Listi::WorkspaceToolbar toolbar{*this, *menu, prefs};
  Listi::WorkspaceSplit *split;
  std::unique_ptr<Listi::TerminalDock> terminalDock;
  void Save() {
    auto saved = prefs.Save();
    saved.what = kSavePreferences;
    service.SendMessage(&saved);
  }
  InlineAction *actionArea;
  void CloseAction() {
    actionArea->Close();
    clipboard->CancelPrompt();
    toolbar.Refresh(actionArea->Pending());
    split->Focus();
  }
  void AskInline(const char *text, const char *button, const BMessage &result,
                 const char *initial = nullptr) {
    if (!actionArea->Ask(text, button, result, initial))
      queue->ErrorInfo("Finish or cancel the open question first.");
  }
  std::unique_ptr<Listi::ClipboardFlow> clipboard;
  std::unique_ptr<Listi::FileDropFlow> fileDrop;
  std::unique_ptr<Listi::FileOperationFlow> fileOperations;
  OperationQueue *queue;
  BMessenger service;
  void SetQuickView(bool enabled) {
    if (split->QuickView() != enabled) {
      if (!split->SetQuickView(enabled))
        return;
      ApplyWorkspace();
    }
    split->Focus();
  }
  void ApplyWorkspace() {
    toolbar.Arrange();
    split->ApplyWorkspace();
    queue->SetCompact(prefs.SinglePane());
    // Fit the buttons after the rest of the workspace has its final geometry.
    toolbar.ApplyVisibility(actionArea->Pending());
  }
  void SetWorkspace(Workspace value) {
    if (prefs.workspace == value || !split->PrepareLayoutChange())
      return;
    prefs.workspace = value;
    prefs.singlePane = split->Source();
    ApplyWorkspace();
    if (!actionArea->Pending())
      split->Focus();
    Save();
  }
  void Apply() {
    for (auto p : panes) {
      p->Apply(prefs);
      p->Reload();
    }
    ApplyWorkspace();
  }
  ImagePreview *OpenSelectionPreview() {
    if (prefs.IsSmidja())
      return split->OpenPanelPreview(1 - split->Source());
    SetQuickView(true);
    return &split->Inspector();
  }

public:
  MainWindow(const Preferences &p, BMessenger svc,
             const Listi::SavedPlaces &saved, bool writable,
             const Listi::SavedSearches &queries, bool queriesWritable)
      : BWindow(BRect(70, 70, 1170, 710), "Listi 0.37.0", B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS),
        prefs(p), savedNavigation(svc, saved, writable, queries, queriesWritable),
        service(svc) {
    panes[0] = new Pane(0, prefs, shelf.Model());
    panes[1] = new Pane(1, prefs, shelf.Model());
    split = new Listi::WorkspaceSplit(*panes[0], *panes[1], prefs, *menu);
    auto *terminalSplit = new BSplitView(B_VERTICAL, 6);
    terminalDock = std::make_unique<Listi::TerminalDock>(
        terminalSplit, split, BMessenger(this));
    queue = new OperationQueue(BMessenger(this));
    actionArea = new InlineAction(BMessenger(this),
                                  [this] { toolbar.Refresh(true); });
    clipboard = std::make_unique<Listi::ClipboardFlow>(*actionArea, *queue);
    fileDrop = std::make_unique<Listi::FileDropFlow>(
        *actionArea, *queue, BMessenger(this), service);
    fileOperations = std::make_unique<Listi::FileOperationFlow>(
        *actionArea, *queue, *clipboard, BMessenger(this));
    actionArea->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    actionArea->GroupLayout()->SetInsets(8, 6, 8, 6);
    actionArea->Hide();
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu, 0)
        .Add(toolbar.Top(), 0)
        .Add(terminalSplit)
        .Add(actionArea, 0)
        .Add(queue->View(), 0)
        .Add(toolbar.Bottom(), 0);
    // Child visibility must not impose a transient maximum on the window.
    // Apply after attachment: Haiku replaces layout explicit-data proxies.
    GetLayout()->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
    queue->Start();
    AddCommonFilter(new Listi::WindowKeyFilter());

    Apply();
    PublishPlaces();
    split->Focus();
    ResizeTo(1140, 650);
  }
  void FrameResized(float width, float height) override {
    BWindow::FrameResized(width, height);
    toolbar.Resize(width, actionArea->Pending());
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
      split->FollowFocus(receiver);
    }
    BWindow::DispatchMessage(message, target);
  }
  void MenusBeginning() override {
    BWindow::MenusBeginning();
    const auto &pane = *panes[split->Source()];
    const char *path = pane.Path();
    const bool canOpenFlatView = Listi::ShelfController::CanOpenFlatView(path);
    const bool canSaveClipboard =
        !actionArea->Pending() && Listi::CanSaveClipboardIn(path);
    const bool canUndoBulkRename =
        !actionArea->Pending() && queue->CanUndoBulkRename();
    menu->SetFileActions(pane.CurrentView(), canOpenFlatView,
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
    const int source = split->Source();
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
    auto &pane = *panes[source];
    if (shelf.Handle(*m, pane, *queue) ||
        pane.HandleSelectionCommand(*m, *actionArea, *queue) ||
        pane.HandleOpenCommand(m->what, prefs.editor, service,
                               [this] { return OpenSelectionPreview(); }))
      return;
    switch (m->what) {
    case kPlaceAction:
      try {
        if (auto prompt = savedNavigation.PreparePlaceEdit(*m))
          AskInline(prompt->question.c_str(), prompt->button, prompt->accepted,
                    prompt->initial ? prompt->initial->c_str() : nullptr);
      } catch (const std::exception &e) {
        PostMainWindowNotice(e.what());
      }
      break;
    case kPlaceAccepted:
      try {
        savedNavigation.EditPlace(*m);
        PublishPlaces();
      } catch (const std::exception &e) {
        PostMainWindowNotice(e.what());
      }
      break;
    case kSearchEdit:
    case kSearchesSaved:
      savedNavigation.HandleSearchMessage(
          *m, searchWindow, {split->Panel(0).SearchMessenger(),
                            split->Panel(1).SearchMessenger()});
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
      toolbar.Refresh(actionArea->Pending());
      if (!actionArea->Pending() && !terminalDock->HasFocus())
        split->Focus();
      break;
    case kInlineCancel:
      if (panes[source]->CancelSelectionGesture())
        break;
      if (actionArea->Pending())
        CloseAction();
      else if (!split->DismissPanelTool()) {
        if (split->QuickView())
          SetQuickView(false);
        else
          queue->Collapse();
      }
      break;
    case Listi::kPanelPage:
    case Listi::kClosePanelPage:
      split->HandlePageCommand(*m);
      break;
    case kQuickView:
      if (!actionArea->Pending()) {
        if (prefs.IsSmidja())
          split->TogglePanelPreview();
        else
          SetQuickView(!split->QuickView());
      }
      break;
    case kCloseQuickView:
      if (split->QuickView() && !actionArea->Pending())
        SetQuickView(false);
      break;
    case kPreviewToggle:
      if (split->ToggleInspector())
        Save();
      break;
    case kPreviewSelection:
      if (m->GetInt32("pane", -1) == source || split->HasPanelPreview())
        split->UpdatePreviews();
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
    case kActivate:
      if (split->ActivateFiles(m->GetInt32("pane", source)))
        split->UpdatePreviews();
      break;
    case kSwitch:
      split->SwitchPane();
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
        split->OpenSearch(savedNavigation.SearchStore());
        break;
      }
      if (searchWindow.IsValid())
        searchWindow.SendMessage(kSearch);
      else
        searchWindow = OpenFileSearch(BMessenger(this), source,
                                      panes[source]->Path(), savedNavigation.SearchStore());
      break;
    case kRevealSearch: {
      int32 which = m->GetInt32("pane", -1);
      if (which < 0 || which > 1)
        break;
      if (split->QuickView() && source != which) {
        if (!split->SetQuickView(false))
          break;
        ApplyWorkspace();
      }
      if (!split->Panel(which).Select(PanelPage::Files))
        break;
      split->SetSource(which);
      split->RefreshPanels();
      if (prefs.SinglePane()) {
        prefs.singlePane = which;
        ApplyWorkspace();
        Save();
      }
      panes[which]->MessageReceived(m);
      Activate();
      split->Focus();
      break;
    }
    case kFilter:
      split->FocusFilter();
      break;
    case kContextMenu: {
      if (actionArea->Pending()) {
        queue->ErrorInfo("Finish or cancel the open question first.");
        break;
      }
      const int32 owner = m->GetInt32("pane", -1);
      if (!split->ActivateFiles(owner))
        break;
      try {
        auto result = Listi::RunPaneContextMenu(
            *m, *panes[owner], *panes[1 - owner], owner,
            prefs.SinglePane(),
            !prefs.SinglePane() &&
                split->Panel(1 - owner).Current() == PanelPage::Files,
            prefs.folderModes.count(panes[owner]->Path()),
            savedNavigation.Places(), savedNavigation.CanEditPlaces());
        // Keep dispatch synchronous so a queued activation cannot redirect it.
        if (result.command.what == Listi::kAddToShelf)
          shelf.Add(*panes[owner], result.shelfSources);
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
        shelf.Refresh();
      }
      break;
    case kShowJobs:
      queue->Toggle();
      break;
    case kCompareFolders:
      OpenFolderCompare(panes[source]->Path(), panes[1 - source]->Path());
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
          split->Focus();
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
      launch.AddString("command", toolbar.CommandText());
      launch.AddString("application", prefs.terminal);
      service.SendMessage(&launch);
      break;
    }
    case kNetwork:
      if (!actionArea->Connect(BMessenger(panes[source])))
        queue->ErrorInfo("Finish or cancel the open question first.");
      break;
    case Listi::kSplitChanged:
      split->RememberWeights(prefs, split->QuickView());
      Save();
      break;
    case kToggleBank:
      if (toolbar.Toggle(actionArea->Pending()))
        Save();
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
      if (!split->PrepareLayoutChange())
        break;
      WorkspaceLayout layout;
      layout.Load(layoutMessage);
      prefs.profiles.Load(profilesMessage);
      if (layout.buttons != prefs.buttons)
        prefs.smidjaButtons = prefs.tviskiptButtons = -1;
      prefs.ApplyLayout(layout);
      if (prefs.SinglePane())
        split->SetSource(prefs.singlePane);
      else
        prefs.singlePane = source;
      ApplyWorkspace();
      split->Focus();
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
