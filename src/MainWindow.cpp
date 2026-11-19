// SPDX-License-Identifier: MIT
#include "MainWindow.h"
#include "MainWindowMenu.h"
#include "ApplicationServices.h"
#include "TransferCommand.h"
#include "ArchivePasswords.h"
#include "Common.h"
#include "BulkRename.h"
#include "BulkRenameWindow.h"
#include "ClipboardTextFile.h"
#include "FileClipboard.h"
#include "FileDrop.h"
#include "FlatViewWindow.h"
#include "FileContextMenu.h"
#include "NamedEntry.h"
#include "JobSnapshot.h"
#include "ArchiveWindow.h"
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
#include "ImageConvert.h"
#include "InlineAction.h"
#include "OperationQueue.h"
#include "Pane.h"
#include "PanelTools.h"
#include "TerminalDock.h"
#include "VirtualFile.h"
#include "WindowKeyFilter.h"
#include "WorkspaceEditor.h"
#include "WorkspaceActions.h"
#include <array>
#include <Alert.h>
#include <Application.h>
#include <algorithm>
#include <exception>
#include <Button.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <GridView.h>
#include <GroupView.h>
#include <Invoker.h>
#include <LayoutBuilder.h>
#include <Path.h>
#include <PropertyInfo.h>
#include <SplitView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
using Listi::PanelPage;

void PostMainWindowNotice(const char *text) {
  BMessage error(kServiceError);
  error.AddString("error", text);
  be_app->PostMessage(&error);
}

namespace {
constexpr uint32 kPreviewPattern = 'sppv', kApplyPattern = 'spap';
constexpr uint32 kSplitChanged = 'swgt';

class WorkspaceSplit : public BSplitView {
public:
  WorkspaceSplit() : BSplitView(B_HORIZONTAL, 6) {}

  void MouseDown(BPoint where) override {
    for (int32 i = 0; i < CountItems(); ++i)
      before[i] = ItemWeight(i);
    BSplitView::MouseDown(where);
  }

  void MouseUp(BPoint where) override {
    BSplitView::MouseUp(where);
    for (int32 i = 0; i < CountItems(); ++i) {
      if (before[i] != ItemWeight(i)) {
        Window()->PostMessage(kSplitChanged);
        break;
      }
    }
  }

private:
  std::array<float, 3> before{};
};
constexpr uint32 kImageDropConvert = 'dcvi';
constexpr uint32 kClipboardFileAccepted = 'ctac';
constexpr uint32 kNamedEntryAccepted = 'nacc', kDropAccepted = 'dacc',
                 kConfirm = 'cfir',
                 kArchiveAddChosen = 'aadc', kArchiveEditAccepted = 'aeda';
class MainWindow : public BWindow {
  Preferences prefs;
  BMessenger searchWindow;
  std::map<std::string, BMessenger> flatViews;
  BMessenger bulkRenameWindow;
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
  Listi::PanelTools *hosts[2];
  ImagePreview *preview;
  bool PanelPreviewVisible() const {
    return hosts[0]->Current() == PanelPage::Preview ||
           hosts[1]->Current() == PanelPage::Preview;
  }
  bool ToolAllowsCommand(uint32 action) const {
    switch (action) {
    case kCopy: case kMove: case kSame: case kCreateLink: case kDuplicate:
    case kCompareFolders: case kCreateArchive: case kExtractArchive:
      return hosts[0]->Current() == PanelPage::Files &&
             hosts[1]->Current() == PanelPage::Files;
    case kClipboardCopy: case kClipboardCut: case kClipboardPaste: case kCopyPaths:
    case kClipboardSaveText: case kClipboardSaveImage: case kUndoBulkRename:
    case kEdit: case kEditExternally: case kView: case kPreviewOtherPanel:
    case kOpenWith: case kOpenSystem: case kProperties:
    case kDelete: case kRestore: case kPermanentDelete: case kRename:
    case kBulkRename: case kMkdir: case kAddArchive: case kRemoveArchive:
    case kAll: case kNone: case kInvert: case kSelectType: case kSelectPattern:
    case kApplyPattern: case kGridMode: case Listi::kAddToShelf:
    case Listi::kFlatView:
      return hosts[source]->Current() == PanelPage::Files;
    default:
      return true;
    }
  }
  void FocusPage(int pane) {
    switch (hosts[pane]->Current()) {
    case PanelPage::Files:
      panes[pane]->FocusList();
      break;
    case PanelPage::Search: {
      BMessage focus(kSearch);
      hosts[pane]->Page(PanelPage::Search)->MessageReceived(&focus);
      break;
    }
    case PanelPage::Preview:
      hosts[pane]->Page(PanelPage::Preview)->MakeFocus();
      break;
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
    for (auto host : hosts)
      if (auto *page = host->Page(PanelPage::Search))
        send(BMessenger(page));
  }
  bool quickView = false;
  MainWindowMenu *menu;
  BSplitView *split;
  BSplitView *terminalSplit;
  std::unique_ptr<Listi::TerminalDock> terminalDock;
  BGroupView *topTools, *bottomTools;
  bool inspectorOnLeft = false;
  bool bankOnTop = false, commandOnTop = false;
  float splitterSize = 0;
  bool paneVisible[2] = {true, true};
  bool previewVisible = true;
  void Save() {
    auto saved = prefs.Save();
    saved.what = kSavePreferences;
    service.SendMessage(&saved);
  }
  void UpdatePreview() {
    auto visibleFile = [&](int pane) -> std::string {
      if (hosts[pane]->Current() != PanelPage::Files)
        return {};
      auto selection = panes[pane]->Selected();
      return selection.size() == 1 ? selection.front() : "";
    };
    for (int pane = 0; pane < 2; ++pane) {
      if (auto *inspector = static_cast<ImagePreview *>(hosts[pane]->Page(PanelPage::Preview)))
        inspector->ShowFile(hosts[pane]->Current() == PanelPage::Preview
                                ? visibleFile(1 - pane) : "");
    }
    preview->ShowFile(previewVisible ? visibleFile(source) : "");
  }
  InlineAction *actionArea;
  void CloseAction() {
    actionArea->Close();
    pendingClipboardFile.reset();
    if (archiveClipboardPending) {
      try {
        Listi::FinishFilePaste(clipboardPaste, {});
      } catch (const std::exception &e) {
        PostMainWindowNotice(e.what());
      }
      clipboardPaste = {};
      archiveClipboardPending = false;
    }
    RefreshBank();
    FocusPage(source);
  }
  void AskInline(const char *text, const char *button, const BMessage &result,
                 const char *initial = nullptr) {
    if (!actionArea->Pending())
      ShowBank(false);
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
  int64 clipboardSerial = 0;
  int64 dropSerial = 0;
  std::unique_ptr<BFilePanel> archiveAddPicker;
  BMessenger archiveWindow;
  void ArchiveAction(bool extract) {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    if (archiveWindow.IsValid()) {
      archiveWindow.SendMessage('arav');
      return;
    }
    try {
      auto job = Listi::PrepareArchiveJob(extract, panes[source]->Selected());
      std::string folder =
          panes[prefs.SinglePane() ? source : 1 - source]->Path();
      if (folder == "@disks" || Listi::IsVirtual(folder)) {
        auto loc = Listi::ParseLocation(job.sources[0]);
        folder =
            Listi::ParentLocation(loc.Archive() ? loc.archive : job.sources[0]);
      }
      archiveWindow = OpenArchiveWindow(job, folder, BMessenger(this));
    } catch (const std::exception &e) {
      PostMainWindowNotice(e.what());
    }
  }
  void ConfirmArchiveAddition(const Listi::Job &job,
                              const std::string &lease = {}) {
    auto accepted = Listi::EncodeJob(job, kArchiveEditAccepted);
    if (!lease.empty())
      accepted.AddString("clipboardLease", lease.c_str());
    BString text;
    if (job.action == Listi::Action::MoveArchive) {
      text << "Move " << int32(job.sources.size()) << " selected entries to:\n"
           << job.destination.c_str();
      const bool sameArchive =
          !job.sources.empty() &&
          Listi::ParseLocation(job.sources.front()).archive ==
              Listi::ParseLocation(job.destination).archive;
      text
          << (sameArchive
                  ? "\nEntries move within this archive. Existing names are never replaced."
                  : "\nSource entries are removed only after the saved destination is verified."
                    " If interrupted after copying, both copies may remain. Existing names require a choice.");
      if (!job.sources.empty() &&
          Listi::ParseLocation(job.sources.front()).Local())
        text
            << "\nFiles with links or filesystem attributes must be copied instead, so the original metadata is retained.";
      AskInline(text.String(), "Move entries", accepted);
      return;
    }
    if (!job.archiveRenames.empty())
      text << "Move selected entries inside this archive:\n";
    else
      text << "Add " << int32(job.sources.size()) << " selected entries to:\n";
    text << job.destination.c_str();
    if (!job.archivePath.empty())
      text << "\nInside: /" << job.archivePath.c_str();
    const auto shown = std::min<size_t>(job.sources.size(), 3);
    for (size_t i = 0; i < shown; ++i)
      text << "\n" << Listi::LocationLeaf(job.sources[i]).c_str();
    if (job.sources.size() > shown)
      text << "\n…and " << int32(job.sources.size() - shown) << " more";
    text
        << (job.archiveRenames.empty()
                ? "\nOriginal files are kept. Existing names require a Replace or Skip choice."
                : "\nEntries move within this archive. Existing names are never replaced.");
    AskInline(text.String(), "Update archive", accepted);
  }
  void AddToArchive() {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    Listi::Job target;
    std::string archiveAddTarget, archiveAddFolder;
    try {
      const auto folder = panes[source]->Path();
      auto location = Listi::ParseLocation(folder);
      if (location.Archive()) {
        archiveAddTarget = location.archive;
        archiveAddFolder = location.path;
      } else {
        const auto selected = panes[source]->Selected();
        if (selected.size() != 1 || Listi::IsVirtual(selected[0]))
          throw std::runtime_error(
              "Select one local writable archive, or open a folder inside it.");
        archiveAddTarget = selected[0];
        archiveAddFolder = "/";
      }
      struct stat st;
      if (lstat(archiveAddTarget.c_str(), &st) || !S_ISREG(st.st_mode))
        throw std::runtime_error("Choose one local writable archive.");
      target =
          Listi::PrepareArchiveEditTarget(archiveAddTarget, archiveAddFolder);
    } catch (const std::exception &e) {
      PostMainWindowNotice(e.what());
      return;
    }
    BMessenger here(this);
    auto chosen = Listi::EncodeJob(target, kArchiveAddChosen);
    archiveAddPicker.reset(new BFilePanel(
        B_OPEN_PANEL, &here, nullptr,
        node_flavor(B_FILE_NODE | B_DIRECTORY_NODE), true, &chosen));
    archiveAddPicker->SetPanelDirectory(
        Listi::ParentLocation(archiveAddTarget).c_str());
    archiveAddPicker->Show();
  }
  void RemoveFromArchive() {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    try {
      const auto selected = panes[source]->Selected();
      if (selected.empty())
        throw std::runtime_error("Select entries inside one writable archive.");
      auto location = Listi::ParseLocation(selected[0]);
      if (!location.Archive())
        throw std::runtime_error(
            "Open an archive and select the entries to remove.");
      auto job = Listi::PrepareArchiveEditJob(location.archive, {}, selected,
                                              location.path);
      const auto accepted = Listi::EncodeJob(job, kArchiveEditAccepted);
      BString text;
      text
          << "Remove " << int32(selected.size())
          << " selected entries from this archive?\n\n"
          << location.archive.c_str()
          << "\n\nThe archive is rebuilt safely. The original remains unchanged if the job fails.";
      AskInline(text.String(), "Remove from archive", accepted);
    } catch (const std::exception &e) {
      PostMainWindowNotice(e.what());
    }
  }
  void NamedEntry(Listi::Action action) {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    try {
      auto job = Listi::PrepareNamedEntry(
          action, panes[source]->Selected(),
          panes[prefs.SinglePane() ? source : 1 - source]->Path());
      const bool duplicate = action == Listi::Action::Duplicate;
      std::string text = std::string(duplicate ? "Duplicate: " : "Link to: ") +
                         job.sources[0] + "\nIn: " + job.destination;
      if (!duplicate)
        text +=
            "\nRelative: path from this folder. Absolute: full target path.";
      auto accepted = Listi::EncodeJob(job, kNamedEntryAccepted);
      accepted.AddString("newEntryIn", job.destination.c_str());
      ShowBank(false);
      actionArea->Ask(text.c_str(), duplicate ? "Duplicate" : "Relative link",
                      accepted, job.entryName.c_str(),
                      duplicate ? nullptr : "Absolute link");
    } catch (const std::exception &e) {
      PostMainWindowNotice(e.what());
    }
  }
  std::optional<Listi::ClipboardFile> pendingClipboardFile;
  void AskToSaveClipboardFile(Listi::ClipboardContent content) {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    try {
      auto file = Listi::PrepareClipboardFile(panes[source]->Path(), content);
      const auto prompt = (content == Listi::ClipboardContent::Image
                               ? "Save clipboard PNG in: "
                               : "Save clipboard text in: ") +
                          file.folder;
      BMessage accepted(kClipboardFileAccepted);
      accepted.AddString("newEntryIn", file.folder.c_str());
      ShowBank(false);
      if (actionArea->Ask(prompt.c_str(), "Save", accepted,
                          file.suggestedName.c_str()))
        pendingClipboardFile = std::move(file);
    } catch (const std::exception &e) {
      PostMainWindowNotice(e.what());
    }
  }
  void SaveClipboardFile(const std::string &name) {
    if (!pendingClipboardFile)
      return;
    auto file = std::move(*pendingClipboardFile);
    pendingClipboardFile.reset();
    try {
      Listi::SaveClipboardFile(file, name);
      const auto status = "Saved " + name;
      for (auto pane : panes) {
        if (pane->Path() == file.folder) {
          pane->Reload();
          pane->ShowStatus(status.c_str());
        }
      }
    } catch (const std::exception &e) {
      PostMainWindowNotice(e.what());
    }
  }
  Listi::ClipboardPaste clipboardPaste;
  bool archiveClipboardPending = false;
  std::set<int64> clipboardJobs;
  std::vector<std::string> clipboardMoved;
  void ClipboardAction(uint32 action) {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    try {
      if (action != kClipboardPaste) {
        auto selected = panes[source]->Selected();
        if (action == kCopyPaths) {
          Listi::PutPathsAsText(selected);
          panes[source]->ShowStatus(selected.size() == 1
                                        ? "Path copied as text."
                                        : "Paths copied as text.");
          return;
        }
        Listi::PutFileClipboard(selected, action == kClipboardCut);
        BString status;
        status
            << int32(selected.size())
            << (action == kClipboardCut
                    ? " marked for moving. Paste in a destination folder."
                    : " copied to clipboard. Paste in a destination folder.");
        panes[source]->ShowStatus(status);
        return;
      }
      if (!clipboardJobs.empty()) {
        queue->ErrorInfo(
            "A clipboard transfer is still running. Let it finish before pasting again.");
        return;
      }
      if (auto content = Listi::DetectClipboardSaveContent()) {
        AskToSaveClipboardFile(*content);
        return;
      }
      clipboardPaste = Listi::PrepareFilePaste(panes[source]->Path());
      if (clipboardPaste.jobs.size() == 1 &&
          (clipboardPaste.jobs[0].action == Listi::Action::EditArchive ||
           clipboardPaste.jobs[0].action == Listi::Action::MoveArchive)) {
        archiveClipboardPending = !clipboardPaste.lease.empty();
        ConfirmArchiveAddition(clipboardPaste.jobs[0], clipboardPaste.lease);
        if (!archiveClipboardPending)
          clipboardPaste = {};
        return;
      }
      clipboardMoved.clear();
      for (auto job : clipboardPaste.jobs) {
        job.tag = ++clipboardSerial;
        clipboardJobs.insert(job.tag);
        queue->Enqueue(job);
      }
    } catch (const std::exception &e) {
      PostMainWindowNotice(e.what());
    }
  }
  OperationQueue *queue;
  BGroupView *bank = nullptr;
  BGridView *bankGrid = nullptr;
  std::vector<int32> renderedButtons;
  int bankColumns = 0;
  BTextControl *command;
  BMessenger service;
  bool bankVisible = true, commandVisible = true;
  bool bankOverflow = false;
  void UpdateToolZones() {
    const bool topVisible = (bankVisible && bankOnTop) ||
                            (commandVisible && commandOnTop);
    const bool bottomVisible = (bankVisible && !bankOnTop) ||
                               (commandVisible && !commandOnTop);
    for (const auto &[zone, visible] :
         {std::make_pair(topTools, topVisible),
          std::make_pair(bottomTools, bottomVisible)}) {
      if (visible && zone->IsHidden(zone))
        zone->Show();
      else if (!visible && !zone->IsHidden(zone))
        zone->Hide();
    }
  }
  void SetBankVisible(bool visible) {
    if (bankVisible == visible)
      return;
    bankVisible = visible;
    if (visible)
      bank->Show();
    else
      bank->Hide();
    UpdateToolZones();
  }
  void ShowBank(bool visible) {
    const bool dual = !prefs.SinglePane() && !prefs.actionButtons.empty();
    visible = visible && dual;
    bankOverflow = false;
    SetBankVisible(visible);
    if (visible) {
      // Showing a layout item invalidates layout but does not run it inline.
      // Measure before the window's auto-size pass can impose a larger frame.
      auto minimum = GetLayout()->MinSize();
      if (minimum.width > Bounds().Width() && bankColumns == 6) {
        UpdateBank(Bounds().Width(), true);
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
  void UpdateBank(float width, bool forceCompact = false) {
    const int columns = forceCompact || (prefs.adaptive && width < 850) ? 3 : 6;
    if (columns == bankColumns && renderedButtons == prefs.actionButtons)
      return;
    bankColumns = columns;
    renderedButtons = prefs.actionButtons;
    menu->ClearFunctionActions();
    if (bankGrid) {
      bank->GroupLayout()->RemoveView(bankGrid);
      delete bankGrid;
    }
    bankGrid = new BGridView(columns == 3 ? 3 : 5, 5);
    int index = 0;
    for (auto id : renderedButtons) {
      if (id < 0 || id >= WorkspaceLayout::ActionButtonCount)
        continue;
      const auto &action = kWorkspaceActions[id];
      menu->AddFunctionAction(action);
      BString label(action.label);
      if (columns == 6 && action.message != kNone)
        label << " [" << action.shortcut << "]";
      auto button = new BButton(label, new BMessage(action.message));
      BString hint("Command+");
      hint << action.shortcut
           << " (normally Alt). File shortcuts apply outside text fields.";
      button->SetToolTip(hint);
      button->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
      bankGrid->GridLayout()->AddView(button, index % columns, index / columns);
      ++index;
    }
    bank->GroupLayout()->AddView(int32(0), bankGrid);
  }
  void PlaceTools() {
    if (bankOnTop != prefs.bankTop) {
      auto oldGroup = bankOnTop ? topTools : bottomTools;
      oldGroup->GroupLayout()->RemoveView(bank);
      bankOnTop = prefs.bankTop;
      auto newGroup = bankOnTop ? topTools : bottomTools;
      newGroup->GroupLayout()->AddView(int32(0), bank);
    }
    if (commandOnTop != prefs.commandTop) {
      auto oldGroup = commandOnTop ? topTools : bottomTools;
      oldGroup->GroupLayout()->RemoveView(command);
      commandOnTop = prefs.commandTop;
      (commandOnTop ? topTools : bottomTools)->AddChild(command);
    }
  }
  int SplitIndex(int logicalIndex) const {
    if (inspectorOnLeft)
      return logicalIndex == 2 ? 0 : logicalIndex + 1;
    return logicalIndex;
  }
  void SetSplitWeights() {
    for (int i = 0; i < 3; ++i)
      split->SetItemWeight(SplitIndex(i), prefs.splitWeights[i], true);
  }
  void RememberSplitWeights() {
    if (prefs.SinglePane() || quickView)
      return;
    // Keep the inspector's previous share when it is not participating.
    const bool inspectorExpanded =
        previewVisible && !preview->IsHidden(preview);
    const int count = inspectorExpanded ? 3 : 2;
    const float totalShare = inspectorExpanded ? 1 : 1 - prefs.splitWeights[2];
    float raw[3]{}, total = 0;
    for (int i = 0; i < count; ++i) {
      raw[i] = split->ItemWeight(SplitIndex(i));
      total += raw[i];
    }
    if (total <= 0)
      return;
    for (int i = 0; i < count; ++i)
      prefs.splitWeights[i] = raw[i] * totalShare / total;
  }
  void ApplyPreviewVisibility() {
    const bool showPreview = quickView || (prefs.Preview() && !prefs.SinglePane() && !PanelPreviewVisible());
    preview->SetQuickViewCloseVisible(quickView);
    if (showPreview != previewVisible) {
      previewVisible = showPreview;
      if (showPreview && preview->IsHidden(preview))
        preview->Show();
      else if (!showPreview && !preview->IsHidden(preview))
        preview->Hide();
    }
    menu->SetPreview(prefs, quickView || PanelPreviewVisible());
    UpdatePreview();
  }
  void OpenPanelPreview(int pane) {
    if (hosts[pane]->Page(PanelPage::Preview))
      hosts[pane]->Select(PanelPage::Preview);
    else
      hosts[pane]->Open(PanelPage::Preview, new ImagePreview());
  }
  ImagePreview *PanelPreview(int pane) const {
    return static_cast<ImagePreview *>(hosts[pane]->Page(PanelPage::Preview));
  }
  bool LeavePanelPage(int pane) {
    return hosts[pane]->Current() != PanelPage::Preview ||
           PanelPreview(pane)->CloseEditor();
  }
  bool CloseWorkspaceEditors() {
    if (!preview->CloseEditor())
      return false;
    for (int pane = 0; pane < 2; ++pane)
      if (auto *page = PanelPreview(pane); page && !page->CloseEditor())
        return false;
    return true;
  }
  void ClosePanelPreview(int pane) {
    if (!PanelPreview(pane)->CloseEditor())
      return;
    hosts[pane]->Close(PanelPage::Preview);
    ActivatePanel(1 - pane);
    ApplyPreviewVisibility();
    FocusPage(source);
  }
  void SetQuickView(bool enabled) {
    if (quickView != enabled) {
      if (!enabled && !preview->CloseEditor())
        return;
      quickView = enabled;
      ApplyWorkspace();
    }
    FocusPage(source);
  }
  void ApplyWorkspace() {
    const bool previewFirst = quickView ? source == 1 : prefs.inspectorLeft;
    if (inspectorOnLeft != previewFirst) {
      split->GetLayout()->RemoveView(preview);
      inspectorOnLeft = previewFirst;
      split->AddChild(SplitIndex(2), preview, prefs.splitWeights[2]);
      for (int i = 0; i < 3; ++i)
        split->SetCollapsible(SplitIndex(i), i == 2);
    }
    PlaceTools();
    UpdateBank(Bounds().Width());
    for (int i = 0; i < 2; ++i) {
      hosts[i]->EnableTools(prefs.IsSmidja());
      const bool visible = i == source || (!quickView && !prefs.SinglePane());
      if (visible != paneVisible[i]) {
        paneVisible[i] = visible;
        if (visible)
          hosts[i]->Show();
        else
          hosts[i]->Hide();
      }
      panes[i]->SetWorkspace(prefs);
      panes[i]->SetSource(i == source);
    }
    ApplyPreviewVisibility();
    split->SetOrientation(prefs.panelsStacked ? B_VERTICAL : B_HORIZONTAL);
    const bool oneView = prefs.SinglePane() && !quickView;
    split->SetSpacing(oneView ? 0 : 6);
    split->SetSplitterSize(oneView ? 0 : splitterSize);
    SetSplitWeights();
    if (quickView)
      split->SetItemWeight(SplitIndex(2), prefs.splitWeights[1 - source], true);
    menu->SetWorkspace(prefs, quickView);
    queue->SetCompact(prefs.SinglePane());
    const bool showCommand = prefs.command && !prefs.SinglePane();
    if (commandVisible != showCommand) {
      commandVisible = showCommand;
      if (showCommand)
        command->Show();
      else
        command->Hide();
    }
    UpdateToolZones();
    RefreshBank();
  }
  void SetWorkspace(Workspace value) {
    if (prefs.workspace == value || !CloseWorkspaceEditors())
      return;
    quickView = false;
    prefs.workspace = value;
    prefs.singlePane = source;
    ApplyWorkspace();
    if (!actionArea->Pending())
      FocusPage(source);
    Save();
  }
  void Apply() {
    for (auto p : panes) {
      p->Apply(prefs);
      p->Reload();
    }
    ApplyWorkspace();
  }
  Listi::Job Snapshot(Listi::Action a) {
    Listi::Job j;
    j.action = a;
    j.sources = panes[source]->Selected();
    if (a == Listi::Action::Copy || a == Listi::Action::Move)
      j.destination = panes[1 - source]->Path();
    return j;
  }
  void BulkRename() {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    if (bulkRenameWindow.IsValid() &&
        bulkRenameWindow.SendMessage(kBulkRename) == B_OK)
      return;
    try {
      auto job = Listi::FreezeBulkRename(panes[source]->Path(),
                                         panes[source]->Selected());
      bulkRenameWindow =
          Listi::OpenBulkRenameWindow(std::move(job), BMessenger(this));
    } catch (const std::exception &e) {
      PostMainWindowNotice(e.what());
    }
  }
  void Operation(Listi::Action a) {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    if (prefs.SinglePane() &&
        (a == Listi::Action::Copy || a == Listi::Action::Move)) {
      PostMainWindowNotice(
          "Use Copy or Cut, then navigate and Paste. Panel transfers need Tvískipt or Smiðja.");
      return;
    }
    auto job = Snapshot(a);
    if ((a == Listi::Action::Trash || a == Listi::Action::Delete) &&
        !job.sources.empty() &&
        Listi::ParseLocation(job.sources.front()).Archive()) {
      RemoveFromArchive();
      return;
    }
    if (a != Listi::Action::MakeDirectory && job.sources.empty()) {
      PostMainWindowNotice("Select one or more files in the active panel first.");
      return;
    }
    if (a == Listi::Action::Rename || a == Listi::Action::MakeDirectory) {
      if (a == Listi::Action::Rename && job.sources.size() > 1) {
        BulkRename();
        return;
      }
      job.destination = panes[source]->Path();
      if (a == Listi::Action::MakeDirectory)
        job.sources.clear();
      if (job.destination == "@disks") {
        PostMainWindowNotice("Open a folder first.");
        return;
      }
      try {
        Listi::FreezeLocalJob(job, job.destination);
        Listi::FreezeArchiveTargets(job);
        if (Listi::ParseLocation(job.destination).Archive())
          Listi::PrepareArchiveEditTarget(
              Listi::ParseLocation(job.destination).archive);
      } catch (const std::exception &e) {
        PostMainWindowNotice(e.what());
        return;
      }
      auto message = Listi::EncodeJob(job, kNameAccepted);
      AskInline(
          a == Listi::Action::Rename ? "Rename selected entry"
                                     : "Create a folder in the active panel",
          a == Listi::Action::Rename ? "Rename" : "Create folder", message,
          a == Listi::Action::Rename
              ? Listi::LocationLeaf(job.sources[0]).c_str()
              : "New folder");
      return;
    }
    try {
      if (a == Listi::Action::Copy || a == Listi::Action::Move) {
        auto transfer = Listi::PrepareTransfer(std::move(job));
        if (transfer.action == Listi::Action::MoveArchive ||
            transfer.action == Listi::Action::EditArchive)
          ConfirmArchiveAddition(transfer);
        else
          queue->Enqueue(transfer);
        return;
      }
      Listi::FreezeLocalJob(job);
      Listi::FreezeArchiveTargets(job);
    } catch (const std::exception &e) {
      PostMainWindowNotice(e.what());
      return;
    }
    auto m = Listi::EncodeJob(job, kConfirm);
    BString text;
    if (a == Listi::Action::Trash)
      text << "Move " << int32(job.sources.size())
           << " selected entries to this volume's Trash?\n\nYou can restore "
              "them from Trash.\n"
           << job.sources[0].c_str();
    else if (a == Listi::Action::Restore)
      text << "Restore " << int32(job.sources.size())
           << " selected entries to their original folders?\n\nExisting files "
              "will not be overwritten.";
    else if (a == Listi::Action::Delete)
      text << "Permanently delete " << int32(job.sources.size())
           << " selected entries?\n\nThis cannot be undone.\n"
           << job.sources[0].c_str();
    AskInline(text,
              a == Listi::Action::Trash     ? "Move to Trash"
              : a == Listi::Action::Restore ? "Restore"
                                            : "Delete permanently",
              m);
  }
  void Open(uint32 action) {
    auto selected = panes[source]->Selected();
    if (selected.size() != 1) {
      PostMainWindowNotice("Select exactly one file.");
      return;
    }
    const bool editInPanel = action == kEdit && !Listi::IsVirtual(selected.front());
    if (action == kPreviewOtherPanel || editInPanel) {
      ImagePreview *target = preview;
      if (prefs.IsSmidja()) {
        if (previewVisible && !preview->CloseEditor())
          return;
        OpenPanelPreview(1 - source);
        target = PanelPreview(1 - source);
        ApplyPreviewVisibility();
        FocusPage(source);
      } else {
        SetQuickView(true);
      }
      if (editInPanel)
        target->BeginEditing(selected.front());
      return;
    }
    if (action == kView) {
      auto window = new BWindow(BRect(120, 100, 900, 650), selected[0].c_str(),
                                B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS);
      auto inspector = new ImagePreview(true);
      inspector->ShowFile(selected[0]);
      BLayoutBuilder::Group<>(window, B_VERTICAL, 0).Add(inspector);
      window->AddShortcut(B_ESCAPE, B_NO_COMMAND_KEY,
                          new BMessage(B_QUIT_REQUESTED));
      window->AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
      window->Show();
      return;
    }
    if (Listi::ParseLocation(selected[0]).Archive()) {
      OpenArchiveEditor(selected[0], prefs.editor.String());
      return;
    }
    BMessage m(kOpenPaths);
    m.AddString("path", selected[0].c_str());
    m.AddString("mode", "edit");
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
    split = new WorkspaceSplit();
    splitterSize = split->SplitterSize();
    for (int pane = 0; pane < 2; ++pane) {
      hosts[pane] = new Listi::PanelTools(pane, panes[pane]);
      split->AddChild(hosts[pane]);
    }
    preview = new ImagePreview();
    split->AddChild(preview);
    split->SetItemWeight(0, 0.39, false);
    split->SetItemWeight(1, 0.39, false);
    split->SetItemWeight(2, 0.22, true);
    split->SetCollapsible(0, false);
    split->SetCollapsible(1, false);
    terminalSplit = new BSplitView(B_VERTICAL, 6);
    terminalDock = std::make_unique<Listi::TerminalDock>(
        terminalSplit, split, BMessenger(this));
    bank = new BGroupView(B_VERTICAL, 5);
    bank->SetName("workspace action buttons");
    auto shortcuts = new BStringView(
        "shortcuts",
        "Command + letter (normally Alt). Tab: other panel. Command+F: filter. "
        "C/X/V: clipboard. Shift+C: panel copy. Escape: close dialog.");
    shortcuts->SetTruncation(B_TRUNCATE_END);
    shortcuts->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
    shortcuts->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    bank->AddChild(shortcuts);
    bank->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    command = new BTextControl("Command:", "", nullptr);
    command->TextView()->AddFilter(
        new Listi::ExplicitSubmit(BMessenger(this), BMessage(kCommand)));
    command->SetToolTip(
        "Runs a shell command in the active directory using Terminal.");
    queue = new OperationQueue(BMessenger(this));
    actionArea = new InlineAction(BMessenger(this));
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
    FocusPage(source);
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
    static property_info properties[] = {{"Workspace",
                                          {B_GET_PROPERTY, 0},
                                          {B_DIRECT_SPECIFIER, 0},
                                          "Current workspace",
                                          0},
                                         {"SourcePath",
                                          {B_GET_PROPERTY, B_SET_PROPERTY, 0},
                                          {B_DIRECT_SPECIFIER, 0},
                                          "Source directory",
                                          0},
                                         {"DestinationPath",
                                          {B_GET_PROPERTY, B_SET_PROPERTY, 0},
                                          {B_DIRECT_SPECIFIER, 0},
                                          "Destination directory",
                                          0},
                                         {"PathReady",
                                          {B_GET_PROPERTY, 0},
                                          {B_DIRECT_SPECIFIER, 0},
                                          "Both directory scans complete",
                                          0},
                                         {"Busy",
                                          {B_GET_PROPERTY, 0},
                                          {B_DIRECT_SPECIFIER, 0},
                                          "Operations are queued or running",
                                          0},
                                         {nullptr}};
    BPropertyInfo info(properties);
    m->AddString("suites", "suite/vnd.Kambuzi-Lister");
    m->AddFlat("messages", &info);
    return BWindow::GetSupportedSuites(m);
  }
  BHandler *ResolveSpecifier(BMessage *msg, int32 index, BMessage *spec,
                             int32 what, const char *property) override {
    if (!strcmp(property, "Workspace") || !strcmp(property, "SourcePath") ||
        !strcmp(property, "DestinationPath") || !strcmp(property, "Busy") ||
        !strcmp(property, "PathReady"))
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
      for (auto *view = receiver; view; view = view->Parent()) {
        for (int pane = 0; pane < 2; ++pane)
          if (view == hosts[pane] && source != pane) {
            ActivatePanel(pane);
            UpdatePreview();
          }
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
    if (queue->Busy() || !clipboardJobs.empty()) {
      queue->Show();
      PostMainWindowNotice("File operations are still running. Let them finish, or use "
             "Cancel all before quitting.");
      return false;
    }
    if (archiveClipboardPending)
      CloseAction();
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
  }
  void MessageReceived(BMessage *m) override {
    if (m->what == B_GET_PROPERTY || m->what == B_SET_PROPERTY) {
      int32 index, what;
      BMessage spec;
      const char *property;
      if (m->GetCurrentSpecifier(&index, &spec, &what, &property) == B_OK) {
        BMessage reply(B_REPLY);
        if (!strcmp(property, "SourcePath") ||
            !strcmp(property, "DestinationPath")) {
          Pane *pane =
              panes[!strcmp(property, "SourcePath") ? source : 1 - source];
          if (m->what == B_SET_PROPERTY) {
            const char *value;
            if (m->FindString("data", &value) == B_OK)
              pane->Navigate(value);
            else {
              reply.AddInt32("error", B_BAD_VALUE);
              m->SendReply(&reply);
              return;
            }
          }
          reply.AddString("result", pane->Path());
        } else if (m->what == B_SET_PROPERTY &&
                   (!strcmp(property, "Workspace") || !strcmp(property, "Busy") ||
                    !strcmp(property, "PathReady"))) {
          reply.AddInt32("error", B_NOT_ALLOWED);
          m->SendReply(&reply);
          return;
        } else if (!strcmp(property, "Workspace"))
          reply.AddString("result", WorkspaceName(prefs.workspace));
        else if (!strcmp(property, "PathReady"))
          reply.AddBool("result",
                        !panes[0]->IsScanning() && !panes[1]->IsScanning());
        else if (!strcmp(property, "Busy"))
          reply.AddBool("result", queue->Busy());
        else {
          BWindow::MessageReceived(m);
          return;
        }
        reply.AddInt32("error", B_OK);
        m->SendReply(&reply);
        return;
      }
    }
    if (prefs.IsSmidja() && !ToolAllowsCommand(m->what)) {
      queue->ErrorInfo("Switch to Files in the affected panel before using this command.");
      return;
    }
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
      if (!actionArea->Pending())
        pendingClipboardFile.reset();
      if (!actionArea->Pending() && archiveClipboardPending)
        CloseAction();
      RefreshBank();
      if (!actionArea->Pending() && !terminalDock->HasFocus())
        FocusPage(source);
      break;
    case kInlineCancel:
      if (panes[source]->CancelSelectionGesture())
        break;
      if (actionArea->Pending())
        CloseAction();
      else if (hosts[source]->Current() == PanelPage::Preview)
        ClosePanelPreview(source);
      else if (hosts[source]->Current() != PanelPage::Files) {
        hosts[source]->Select(PanelPage::Files);
        ApplyPreviewVisibility();
        FocusPage(source);
      } else if (PanelPreviewVisible()) {
        ClosePanelPreview(1 - source);
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
        if (hosts[pane]->Current() == PanelPage::Preview) {
          ClosePanelPreview(pane);
          break;
        }
        hosts[pane]->Close(hosts[pane]->Current());
      } else {
        const int32 page = m->GetInt32("page", -1);
        if (page < 0 || page > 2 || !hosts[pane]->Page(PanelPage(page)))
          break;
        if (PanelPage(page) != hosts[pane]->Current() && !LeavePanelPage(pane))
          break;
        hosts[pane]->Select(PanelPage(page));
      }
      ActivatePanel(pane);
      ApplyPreviewVisibility();
      FocusPage(pane);
      break;
    }
    case kQuickView:
      if (actionArea->Pending())
        break;
      if (prefs.IsSmidja()) {
        int pane = hosts[source]->Current() == PanelPage::Preview ? source : 1 - source;
        if (hosts[pane]->Current() == PanelPage::Preview) {
          ClosePanelPreview(pane);
        } else {
          if (previewVisible && !preview->CloseEditor())
            break;
          OpenPanelPreview(pane);
          ApplyPreviewVisibility();
          FocusPage(source);
        }
        break;
      }
      SetQuickView(!quickView);
      break;
    case kCloseQuickView:
      if (quickView && !actionArea->Pending())
        SetQuickView(false);
      break;
    case kPreviewToggle:
      if (prefs.SinglePane() || quickView || PanelPreviewVisible())
        break;
      if (previewVisible && !preview->CloseEditor())
        break;
      prefs.SetPreview(!prefs.Preview());
      ApplyPreviewVisibility();
      Save();
      break;
    case kPreviewSelection:
      if (m->GetInt32("pane", -1) == source || PanelPreviewVisible())
        UpdatePreview();
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
      if (m->GetInt32("pane", source) < 0 || m->GetInt32("pane", source) > 1)
        break;
      if (hosts[m->GetInt32("pane", source)]->Current() != PanelPage::Files ||
          ((quickView || prefs.SinglePane()) && m->GetInt32("pane", source) != source))
        break;
      source = m->GetInt32("pane", source);
      for (int i = 0; i < 2; i++)
        panes[i]->SetSource(i == source);
      UpdatePreview();
      break;
    case kSwitch:
      if (quickView)
        break;
      if (prefs.workspace == Workspace::askr) {
        panes[source]->ToggleTreeFocus();
        break;
      }
      if (prefs.SinglePane())
        break;
      source = 1 - source;
      for (int i = 0; i < 2; i++)
        panes[i]->SetSource(i == source);
      FocusPage(source);
      UpdatePreview();
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
        if (!LeavePanelPage(source))
          break;
        if (!hosts[source]->Page(PanelPage::Search))
          hosts[source]->Open(PanelPage::Search, CreatePanelSearch(
              BMessenger(this), source, panes[source]->Path(), savedNavigation.SearchStore()));
        else
          hosts[source]->Select(PanelPage::Search);
        ApplyPreviewVisibility();
        FocusPage(source);
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
        if (!preview->CloseEditor())
          break;
        quickView = false;
        ApplyWorkspace();
      }
      if (!LeavePanelPage(which))
        break;
      hosts[which]->Select(PanelPage::Files);
      source = which;
      ApplyPreviewVisibility();
      if (prefs.SinglePane()) {
        prefs.singlePane = source;
        ApplyWorkspace();
        Save();
      }
      for (int i = 0; i < 2; ++i)
        panes[i]->SetSource(i == source);
      panes[source]->MessageReceived(m);
      Activate();
      FocusPage(source);
      break;
    }
    case kFilter:
      if (!LeavePanelPage(source))
        break;
      hosts[source]->Select(PanelPage::Files);
      ApplyPreviewVisibility();
      panes[source]->FocusFilter();
      break;
    case kContextMenu: {
      if (actionArea->Pending()) {
        queue->ErrorInfo("Finish or cancel the open question first.");
        break;
      }
      const int32 owner = m->GetInt32("pane", -1);
      if (owner < 0 || owner > 1 || hosts[owner]->Current() != PanelPage::Files ||
          ((prefs.SinglePane() || quickView) && owner != source))
        break;
      source = owner;
      for (int i = 0; i < 2; ++i)
        panes[i]->SetSource(i == source);
      const auto selected = panes[source]->Selected();
      const std::string folder = panes[source]->Path(),
                        destination = panes[1 - source]->Path();
      const bool background = m->GetBool("folderContext", false);
      if (!background && selected.empty())
        break;
      const bool folderLocal = folder != "@disks" && !Listi::IsVirtual(folder);
      bool local = true;
      for (const auto &path : selected)
        local &= !Listi::IsVirtual(path);
      try {
        BMessage snapshot;
        std::exception_ptr fileSnapshotError;
        Listi::Job shelfSnapshot{Listi::Action::Copy, selected, ""};
        bool canAddToShelf = false;
        if (!selected.empty() && local) {
          try {
            snapshot = Listi::MakeFileSelection(selected);
          } catch (...) {
            fileSnapshotError = std::current_exception();
          }
          // Shelf accepts overlapping folders and distinct hard-link names;
          // the native transfer clipboard has stricter rules for those.
          if (folder != "@disks" &&
              selected.size() <= Listi::Shelf::kMaximumEntries) {
            try {
              FreezeLocalJob(shelfSnapshot);
              canAddToShelf = true;
            } catch (const std::runtime_error &) {
              // An unavailable selection cannot be saved to the Shelf.
            }
          }
        }
        struct stat folderBefore {};
        if (folderLocal && (stat(folder.c_str(), &folderBefore) ||
                            !S_ISDIR(folderBefore.st_mode)))
          throw std::runtime_error(
              "The folder is unavailable. Open the menu again.");
        struct stat before {};
        const bool destinationKnown = !stat(destination.c_str(), &before);
        BPoint screen;
        if (m->FindPoint("screen", &screen) != B_OK)
          break;
        ContextMenuOptions options;
        options.folder = background;
        options.selected = selected.size();
        options.directory = m->GetBool("directory", false);
        options.local = local;
        options.dual = !prefs.SinglePane() && hosts[1 - owner]->Current() == PanelPage::Files;
        options.folderLocal = folderLocal;
        const auto folderLocation = Listi::ParseLocation(folder);
        options.folderArchive = folderLocation.Archive();
        options.archiveAdd =
            folderLocation.Archive() ||
            (selected.size() == 1 && local && !options.directory);
        options.archiveRemove = !selected.empty();
        std::string selectedArchive;
        for (const auto &path : selected) {
          auto selectedLocation = Listi::ParseLocation(path);
          if (!selectedLocation.Archive() ||
              (!selectedArchive.empty() &&
               selectedArchive != selectedLocation.archive)) {
            options.archiveRemove = false;
            break;
          }
          selectedArchive = selectedLocation.archive;
        }
        options.fileType = panes[owner]->SelectedFileType();
        options.sort = panes[owner]->Sorting();
        options.view = panes[owner]->CurrentView();
        options.singlePane = prefs.SinglePane();
        options.rememberView = prefs.folderModes.count(folder);
        options.addToShelf = canAddToShelf;
        options.restoreSidebar = panes[owner]->SidebarCanBeRestored();
        options.addShortcut =
            options.directory && selected.size() == 1 && savedNavigation.CanEditPlaces() &&
            !savedNavigation.Places().Find(selected.front()) &&
            savedNavigation.Places().Items().size() < Listi::SavedPlaces::Limit;
        const auto choice = ShowContextMenu(screen, options);
        const uint32 action = choice.what;
        if (!action)
          break;
        if (source != owner || selected != panes[owner]->Selected() ||
            folder != panes[owner]->Path() ||
            destination != panes[1 - owner]->Path())
          throw std::runtime_error(
              "The panel changed. Open the file menu again.");
        if (action == kRestoreSidebar) {
          BMessage restore(kRestoreSidebar);
          panes[owner]->MessageReceived(&restore);
          break;
        }
        if (action == kAddShortcut) {
          if (!options.addShortcut)
            break;
          // A shortcut stores an address, not a clipboard reference. Volume
          // roots and other browsable folders need no clipboard eligibility.
          struct stat shortcut {};
          if (local && (stat(selected.front().c_str(), &shortcut) ||
                        !S_ISDIR(shortcut.st_mode)))
            throw std::runtime_error("The selected folder is unavailable.");
          BMessage request(kPlaceAction);
          request.AddInt32("action", 0);
          request.AddString("path", selected.front().c_str());
          request.AddInt32("pane", owner);
          MessageReceived(&request);
          break;
        }
        if (action == Listi::kAddToShelf) {
          if (options.addToShelf)
            AddToShelf(shelfSnapshot.expectedSources);
          break;
        }
        if (choice.folder) {
          struct stat folderAfter {};
          if (folderLocal && (stat(folder.c_str(), &folderAfter) ||
                              folderBefore.st_dev != folderAfter.st_dev ||
                              folderBefore.st_ino != folderAfter.st_ino))
            throw std::runtime_error(
                "The folder changed. Open the menu again.");
          if (action == kSort) {
            panes[owner]->Sort({choice.value, choice.ascending});
            break;
          }
          if (action == kGridMode || action == kRememberView ||
              action == kForgetView) {
            BMessage command(action);
            if (action == kGridMode)
              command.AddInt32("mode", choice.value);
            panes[owner]->MessageReceived(&command);
            break;
          }
          if (action == kProperties) {
            OpenFileProperties(folder, BMessenger(panes[owner]));
            break;
          }
          if (action == kRefresh) {
            panes[owner]->Reload();
            break;
          }
        } else if (local && action != kCopyPaths) {
          if (fileSnapshotError)
            std::rethrow_exception(fileSnapshotError);
          Listi::ReadFileClipboard(snapshot);
        }
        if (action == kCopy || action == kMove || action == kCreateLink) {
          struct stat after {};
          if (destinationKnown &&
              (stat(destination.c_str(), &after) ||
               before.st_dev != after.st_dev || before.st_ino != after.st_ino))
            throw std::runtime_error(
                "The destination changed. Open the file menu again.");
        }
        BMessage command(action);
        // Dispatch before any queued activation/navigation message can redirect it.
        if (action == kInvoke)
          panes[owner]->MessageReceived(&command);
        else
          MessageReceived(&command);
      } catch (const std::exception &e) {
        PostMainWindowNotice(e.what());
      }
      break;
    }
    case kCreateArchive:
      ArchiveAction(false);
      break;
    case kExtractArchive:
      ArchiveAction(true);
      break;
    case kAddArchive:
      AddToArchive();
      break;
    case kRemoveArchive:
      RemoveFromArchive();
      break;
    case kArchiveAddChosen: {
      try {
        std::vector<std::string> additions;
        entry_ref ref;
        BPath path;
        for (int32 i = 0; m->FindRef("refs", i, &ref) == B_OK; ++i) {
          if (path.SetTo(&ref) != B_OK)
            throw std::runtime_error("A selected file is unavailable.");
          additions.emplace_back(path.Path());
        }
        auto job = Listi::DecodeJob(*m);
        Listi::SetArchiveEditAdditions(job, additions);
        ConfirmArchiveAddition(job);
      } catch (const std::exception &e) {
        PostMainWindowNotice(e.what());
      }
      break;
    }
    case kArchiveEditAccepted: {
      auto job = Listi::DecodeSubmittedJob(
          *m, {Listi::Action::EditArchive, Listi::Action::MoveArchive});
      if (!job || m->GetInt32("which", 0) != 1)
        break;
      const std::string lease = m->GetString("clipboardLease", "");
      if (!lease.empty()) {
        if (!archiveClipboardPending || lease != clipboardPaste.lease)
          break;
        job->tag = ++clipboardSerial;
        clipboardJobs.insert(job->tag);
        clipboardMoved.clear();
        archiveClipboardPending = false;
      }
      queue->Enqueue(*job);
      break;
    }
    case 'apfg':
      Listi::ForgetArchivePasswords();
      break;
    case kArchiveAccepted: {
      if (auto job =
              Listi::DecodeSubmittedJob(*m, {Listi::Action::CreateArchive,
                                             Listi::Action::ExtractArchive}))
        queue->Enqueue(*job);
      break;
    }
    case kResumeArchiveEdits:
      ResumeArchiveEditors(prefs.editor.String());
      break;
    case kOpenWith: {
      auto selected = panes[source]->Selected();
      if (selected.size() != 1)
        PostMainWindowNotice("Select one local file for Open with.");
      else
        OpenWithFile(selected[0]);
      break;
    }
    case kProperties: {
      auto selected = panes[source]->Selected();
      if (selected.size() != 1)
        PostMainWindowNotice("Select one local file or folder for Properties.");
      else
        OpenFileProperties(selected[0], BMessenger(panes[source]));
      break;
    }
    case kClipboardSaveText:
      AskToSaveClipboardFile(Listi::ClipboardContent::Text);
      break;
    case kClipboardSaveImage:
      AskToSaveClipboardFile(Listi::ClipboardContent::Image);
      break;
    case kClipboardFileAccepted:
      SaveClipboardFile(m->GetString("name", ""));
      break;
    case kClipboardCopy:
    case kClipboardCut:
    case kClipboardPaste:
    case kCopyPaths:
      ClipboardAction(m->what);
      break;
    case kCopy:
      Operation(Listi::Action::Copy);
      break;
    case kMove:
      Operation(Listi::Action::Move);
      break;
    case kDelete:
      Operation(Listi::Action::Trash);
      break;
    case kRestore:
      Operation(Listi::Action::Restore);
      break;
    case kPermanentDelete:
      Operation(Listi::Action::Delete);
      break;
    case kDuplicate:
      NamedEntry(Listi::Action::Duplicate);
      break;
    case kCreateLink:
      NamedEntry(Listi::Action::Link);
      break;
    case kNamedEntryAccepted: {
      auto job = Listi::DecodeSubmittedJob(
          *m, {Listi::Action::Link, Listi::Action::Duplicate});
      if (job) {
        job->entryName = m->GetString("name", "");
        job->relativeLink = m->GetInt32("which", 1) != 2;
        queue->Enqueue(*job);
      }
      break;
    }
    case kRename:
      Operation(Listi::Action::Rename);
      break;
    case kBulkRename:
      BulkRename();
      break;
    case kUndoBulkRename:
      if (!m->IsSourceRemote() && !actionArea->Pending() && queue->EnqueueBulkRenameUndo())
        queue->Show();
      break;
    case kBulkRenameAccepted: {
      if (auto job = Listi::DecodeSubmittedJob(*m, {Listi::Action::BulkRename}))
        queue->Enqueue(*job);
      break;
    }
    case kMkdir:
      Operation(Listi::Action::MakeDirectory);
      break;
    case kConfirm: {
      auto job = Listi::DecodeSubmittedJob(*m, {Listi::Action::Trash,
                                                Listi::Action::Restore,
                                                Listi::Action::Delete});
      if (job && m->GetInt32("which", 0) == 1)
        queue->Enqueue(*job);
      break;
    }
    case kNameAccepted: {
      auto job = Listi::DecodeSubmittedJob(
          *m, {Listi::Action::Rename, Listi::Action::MakeDirectory});
      if (!job)
        break;
      try {
        job->destination =
            Listi::ChildLocation(job->destination, m->GetString("name", ""));
        if (Listi::ParseLocation(job->destination).Archive())
          *job = Listi::PrepareArchiveMutation(*job);
        queue->Enqueue(*job);
      } catch (const std::exception &e) {
        PostMainWindowNotice(e.what());
      }
      break;
    }
    case kDrop: {
      if (actionArea->Pending()) {
        queue->ErrorInfo(
            "Finish or cancel the open question before dropping files.");
        break;
      }
      try {
        ++dropSerial;
        auto job =
            Listi::PrepareFileDrop(*m, m->GetString("destination", ""), true);
        if (job.action == Listi::Action::EditArchive) {
          ConfirmArchiveAddition(job);
          break;
        }
        const bool canConvert = Listi::CanProbeImageDrop(job);
        const bool sameFolder = Listi::InTransferSourceFolder(job);
        auto accepted = Listi::EncodeJob(job, kDropAccepted);
        accepted.AddInt64("request", dropSerial);
        if (sameFolder) {
          if (!canConvert)
            throw std::runtime_error(
                "Drop into a different folder, not the source folder.");
          accepted.AddBool("convertOnly", true);
          AskInline(
              "Convert an image in its current folder. The original is kept.",
              "Convert here", accepted);
          break;
        }
        const bool canMove = !Listi::IsVirtual(job.destination) &&
                             job.expectedSources.size() == job.sources.size();
        std::string text =
            "Drop " + std::to_string(job.sources.size()) + " item(s) into:\n" +
            job.destination +
            (!canMove
                 ? "\nCopy only for network/archive transfers. Originals are kept."
                 : "\nCopy keeps originals. Move removes originals after transfer.");
        ShowBank(false);
        actionArea->Ask(text.c_str(), "Copy here", accepted, nullptr,
                        canMove ? "Move here" : nullptr,
                        canConvert ? "Convert here" : nullptr);
      } catch (const std::exception &e) {
        PostMainWindowNotice(e.what());
      }
      break;
    }
    case kDropAccepted: {
      if (m->GetInt64("request", -1) != dropSerial)
        break;
      const auto which = m->GetInt32("which", 0);
      if (which == 3 || m->GetBool("convertOnly", false)) {
        BMessage probe(*m);
        probe.what = kImageDropProbe;
        probe.AddMessenger("replyTo", BMessenger(this));
        if (service.SendMessage(&probe, (BHandler *)nullptr, 500000) != B_OK)
          PostMainWindowNotice("Could not check installed image translators.");
      } else if (which == 1 || which == 2) {
        auto job = Listi::DecodeSubmittedJob(*m, {Listi::Action::Copy});
        if (!job)
          break;
        if (which == 2) {
          if (Listi::IsVirtual(job->destination) ||
              job->expectedSources.size() != job->sources.size())
            break;
          job->action = Listi::Action::Move;
        }
        queue->Enqueue(*job);
      }
      break;
    }
    case kImageDropOutputs: {
      if (m->GetInt64("request", -1) != dropSerial)
        break;
      if (actionArea->Pending()) {
        queue->ErrorInfo("Finish the open question and drop the image again.");
        break;
      }
      if (m->HasString("error")) {
        PostMainWindowNotice(m->GetString("error", ""));
        break;
      }
      auto job = Listi::DecodeJob(*m);
      if (job.sources.size() != 1)
        break;
      std::vector<std::string> labels;
      BMessage output;
      for (int32 i = 0; m->FindMessage("output", i, &output) == B_OK; ++i) {
        labels.push_back(std::string(output.GetString("name", "")) + " (." +
                         output.GetString("extension", "") + ")");
      }
      if (labels.empty())
        break;
      BMessage accepted(*m);
      accepted.what = kImageDropConvert;
      accepted.RemoveName("replyTo");
      accepted.RemoveName("which");
      std::string text = "Convert " + Listi::Leaf(job.sources.front()) +
                         " into:\n" + job.destination +
                         "\nThe original and existing files are kept.";
      ShowBank(false);
      actionArea->Ask(text.c_str(), "Convert", accepted, nullptr, nullptr,
                      nullptr, labels);
      break;
    }
    case kImageDropConvert: {
      if (m->GetInt64("request", -1) != dropSerial)
        break;
      BMessage output;
      const auto option = m->GetInt32("option", -1);
      if (option < 0 || m->FindMessage("output", option, &output) != B_OK)
        break;
      auto job = Listi::DecodeSubmittedJob(*m, {Listi::Action::Copy});
      if (!job)
        break;
      job->action = Listi::Action::ConvertImage;
      job->imageFormat = output.GetInt32("type", 0);
      job->imageMime = output.GetString("mime", "");
      queue->Enqueue(*job);
      break;
    }
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
        if (clipboardJobs.erase(done.GetInt64("tag", 0))) {
          const char *path;
          for (int32 i = 0; done.FindString("moved", i, &path) == B_OK; ++i)
            clipboardMoved.emplace_back(path);
          if (clipboardJobs.empty()) {
            try {
              Listi::FinishFilePaste(clipboardPaste, clipboardMoved);
            } catch (const std::exception &e) {
              PostMainWindowNotice(e.what());
            }
            clipboardPaste = {};
            clipboardMoved.clear();
          }
        }
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
      Open(m->what);
      break;
    case kOpenPaths:
      service.SendMessage(m);
      break;
    case Listi::kTerminalBelow:
      terminalDock->SetSide(Listi::TerminalSide::Below);
      break;
    case Listi::kTerminalBeside:
      terminalDock->SetSide(Listi::TerminalSide::Right);
      break;
    case Listi::kTerminalAbove:
      terminalDock->SetSide(Listi::TerminalSide::Above);
      break;
    case Listi::kTerminalLeft:
      terminalDock->SetSide(Listi::TerminalSide::Left);
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
      auto *terminalPanel = terminalDock->Docked(lister);
      if (!terminalPanel) {
        terminalDock->FocusFloating(lister);
        break;
      }
      const bool floating = m->what == Listi::kShowFloatingTerminal;
      const bool hasSession = terminalPanel->Active();
      const bool here = m->what == kTerminal;
      const char *path = m->GetString("folder", panes[lister]->Path());
      if (m->HasString("folder") &&
          (m->GetInt64("session", -1) != terminalPanel->SessionId() ||
           m->GetInt64("terminal placement", -1) !=
               terminalPanel->Placement()))
        break;
      if (!hasSession || here) {
        if (!strcmp(path, "@disks") || Listi::IsVirtual(path)) {
          PostMainWindowNotice("Open a local folder first.");
          break;
        }
        if (hasSession && terminalPanel->StartFolder() != path) {
          if (!m->HasString("folder")) {
            BMessage accepted(m->what);
            accepted.AddString("folder", path);
            accepted.AddInt32("terminal lister", lister);
            accepted.AddInt64("session", terminalPanel->SessionId());
            accepted.AddInt64("terminal placement", terminalPanel->Placement());
            BString prompt("Close the current CLI and start a new shell in ");
            prompt << path << "? Any running command will stop.";
            AskInline(prompt.String(), "Start new CLI", accepted);
            break;
          }
          terminalPanel->Stop();
        }
        if (!terminalPanel->Active()) {
          if (const int error = terminalPanel->Start(path)) {
            BString detail("Could not open Listi CLI: ");
            detail << strerror(error);
            PostMainWindowNotice(detail);
            break;
          }
        }
      }
      terminalDock->Show(lister);
      if (!floating || !terminalDock->Float(terminalPanel->Placement()))
        terminalPanel->Focus();
      break;
    }
    case Listi::kToggleTerminalSide:
      terminalDock->Move(m->GetInt64("terminal placement", -1));
      break;
    case Listi::kFloatTerminal:
      terminalDock->Float(m->GetInt64("terminal placement", -1));
      break;
    case Listi::kDockTerminal:
      terminalDock->Dock(m->GetInt64("terminal placement", -1));
      break;
    case Listi::kCloseTerminal:
      if (terminalDock->Close(m->GetInt64("terminal placement", -1))) {
        Activate();
        if (!terminalDock->HasFocus())
          FocusPage(source);
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
      if (!actionArea->Pending())
        ShowBank(false);
      if (!actionArea->Connect(BMessenger(panes[source])))
        queue->ErrorInfo("Finish or cancel the open question first.");
      break;
    case kOpenSystem: {
      auto selected = panes[source]->Selected();
      if (selected.size() != 1) {
        PostMainWindowNotice("Select exactly one file.");
        break;
      }
      BMessage open(kOpenPaths);
      open.AddString("path", selected[0].c_str());
      open.AddString("mode", "system");
      service.SendMessage(&open);
      break;
    }
    case kSplitChanged:
      RememberSplitWeights();
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
      if (!CloseWorkspaceEditors())
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
      FocusPage(source);
      Save();
      break;
    }
    case kPreferences:
      OpenPreferences(BMessenger(this), prefs);
      break;
    case kApplyPreferences: {
      prefs.ApplyDialog(*m);
      Apply();
      BMessage save(prefs.Save());
      save.what = kSavePreferences;
      service.SendMessage(&save);
      break;
    }
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
