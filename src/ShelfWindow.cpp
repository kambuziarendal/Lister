// SPDX-License-Identifier: MIT
#include "ShelfWindow.h"
#include "Shelf.h"
#include "FileClipboard.h"
#include "JobSnapshot.h"
#include <Alert.h>
#include <Button.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <Path.h>
#include <StringView.h>
#include <Window.h>
#include <stdexcept>

namespace Listi {
namespace {
constexpr uint32 kRemove = 'sfrm', kSelect = 'sfsl', kPickFolder = 'sfbw',
                 kFolderChosen = 'sfdc', kDropped = 'sfdp';

std::string RefPath(const entry_ref &ref, bool traverse) {
  BEntry entry(&ref, traverse);
  BPath path;
  if (entry.GetPath(&path) != B_OK)
    throw std::runtime_error("The selected entry is no longer available.");
  return path.Path();
}

size_t AddDrop(Shelf &shelf, const BMessage &drop) {
  type_code type;
  int32 count;
  if (drop.GetInfo("refs", &type, &count) != B_OK || type != B_REF_TYPE ||
      count < 1 || size_t(count) > Shelf::kMaximumEntries ||
      drop.FlattenedSize() > 2 * 1024 * 1024)
    throw std::runtime_error("Drop between 1 and 1024 local entries.");
  std::vector<std::string> paths;
  for (int32 i = 0; i < count; ++i) {
    entry_ref ref;
    if (drop.FindRef("refs", i, &ref) != B_OK)
      throw std::runtime_error("A dropped entry is unavailable.");
    paths.push_back(RefPath(ref, false));
  }
  // Listi drags carry the identities from drag start; Tracker drops have refs
  // only. Capture Tracker identities here, but never refresh Listi's snapshot.
  BMessage selection;
  std::map<std::string, NodeIdentity> expected;
  if (drop.FindMessage("listi:drag-selection", &selection) == B_OK) {
    for (const auto &item : ReadFileClipboard(selection))
      expected.emplace(item.path, item.node);
    std::set<std::string> unique(paths.begin(), paths.end());
    if (expected.size() != paths.size() || unique.size() != paths.size())
      throw std::runtime_error(
          "The dragged selection does not match its references.");
  }
  return shelf.Add(paths, expected);
}

const char *StateLabel(ShelfState state) {
  switch (state) {
  case ShelfState::Missing:
    return "Unavailable";
  case ShelfState::Replaced:
    return "Replaced";
  default:
    return "Ready";
  }
}

class ShelfRow : public BRow {
  ShelfState state;

public:
  std::string path;
  explicit ShelfRow(const ShelfEntry &entry)
      : state(entry.State()), path(entry.path) {
    const auto slash = path.find_last_of('/');
    SetField(new BStringField(path.substr(slash + 1).c_str()), 0);
    SetField(new BStringField(path.substr(0, slash ? slash : 1).c_str()), 1);
    SetField(new BStringField(StateLabel(state)), 2);
  }

  bool UpdateState(ShelfState current) {
    if (current == state)
      return false;
    state = current;
    static_cast<BStringField *>(GetField(2))->SetString(StateLabel(state));
    return true;
  }
};

class ShelfList : public BColumnListView {
public:
  ShelfList()
      : BColumnListView("shelf entries", B_WILL_DRAW | B_NAVIGABLE,
                        B_FANCY_BORDER) {}

  void MessageDropped(BMessage *message, BPoint) override {
    BMessage drop(*message);
    drop.what = kDropped;
    Window()->PostMessage(&drop);
  }
};

class ShelfWindow : public BWindow {
  BMessenger owner;
  std::shared_ptr<Shelf> shelf;
  ShelfList *rows;
  BButton *remove, *move;
  BStringView *status;
  std::map<std::string, ShelfRow *> displayed;
  std::unique_ptr<BFilePanel> picker;
  size_t pickerRevision = 0;
  int64 pickerSerial = 0;

  void Status(const std::string &text) {
    status->SetText(text.c_str());
    status->SetToolTip(text.c_str());
  }

  std::set<std::string> Selected() const {
    std::set<std::string> selected;
    for (auto row = rows->CurrentSelection(); row;
         row = rows->CurrentSelection(row))
      selected.insert(static_cast<ShelfRow *>(row)->path);
    return selected;
  }

  void Render() {
    const auto snapshot = shelf->Snapshot();
    std::set<std::string> present;
    for (const auto &entry : snapshot.entries) {
      present.insert(entry.path);
      auto existing = displayed.find(entry.path);
      if (existing != displayed.end()) {
        if (existing->second->UpdateState(entry.State()))
          rows->UpdateRow(existing->second);
      } else {
        auto row = new ShelfRow(entry);
        displayed.emplace(entry.path, row);
        rows->AddRow(row);
      }
    }
    // Remove through the control so it adjusts its focus and selection pointers.
    for (auto it = displayed.begin(); it != displayed.end();) {
      if (present.count(it->first)) {
        ++it;
      } else {
        rows->RemoveRow(it->second);
        delete it->second;
        it = displayed.erase(it);
      }
    }
    remove->SetEnabled(rows->CurrentSelection() != nullptr);
    move->SetEnabled(!snapshot.entries.empty());
  }

  void Add(const BMessage &message) {
    const auto added = AddDrop(*shelf, message);
    Render();
    Status(added ? "Added " + std::to_string(added) + " to the Shelf."
                 : "Already on the Shelf.");
  }

  void PickFolder() {
    if (picker) {
      picker->Show();
      return;
    }
    const auto snapshot = shelf->Snapshot();
    CheckShelfSources(snapshot);
    pickerRevision = snapshot.revision;
    BMessenger here(this);
    BMessage chosen(kFolderChosen);
    chosen.AddInt64("shelf picker", ++pickerSerial);
    picker = std::make_unique<BFilePanel>(B_OPEN_PANEL, &here, nullptr,
                                          B_DIRECTORY_NODE, false, &chosen);
    picker->SetButtonLabel(B_DEFAULT_BUTTON, "Choose folder");
    picker->Window()->SetTitle("Listi: Choose Shelf destination");
    picker->Show();
  }

  void ConfirmFolder(const BMessage &message) {
    if (!picker || message.GetInt64("shelf picker", -1) != pickerSerial)
      return;
    picker.reset();
    const auto snapshot = shelf->Snapshot();
    if (snapshot.revision != pickerRevision)
      throw std::runtime_error(
          "The Shelf changed. Choose a destination again.");
    entry_ref ref;
    if (message.FindRef("refs", &ref) != B_OK)
      throw std::runtime_error("Choose a destination folder.");
    auto job = PrepareShelfMove(snapshot, RefPath(ref, true));
    const auto question = "Move all " + std::to_string(job.sources.size()) +
                          " Shelf entries to this folder?\n\n" +
                          job.destination;
    auto alert = new BAlert("Move Shelf entries", question.c_str(), "Cancel",
                            "Move", nullptr, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
    alert->SetDefaultButton(alert->ButtonAt(0));
    alert->SetShortcut(0, B_ESCAPE);
    if (alert->Go() != 1)
      return;
    if (shelf->Snapshot().revision != snapshot.revision)
      throw std::runtime_error(
          "The Shelf changed. Choose a destination again.");
    CheckShelfSources(snapshot);
    // Queue validation checks those same identities again before each move.
    auto moveJob = EncodeJob(job, kSubmitShelfMove);
    if (owner.SendMessage(&moveJob, (BHandler *)nullptr, 0) != B_OK)
      throw std::runtime_error(
          "Could not submit the move. The Shelf is unchanged.");
    Status("Move submitted. See Operations for progress.");
  }

public:
  ShelfWindow(BMessenger receiver, std::shared_ptr<Shelf> collection)
      : BWindow(BRect(150, 115, 700, 455), "Listi: Shelf", B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS),
        owner(receiver), shelf(std::move(collection)) {
    // A normal titled window can remain independent or join Stack & Tile.
    rows = new ShelfList();
    rows->AddColumn(new BStringColumn("Name", 145, 72, 500, B_TRUNCATE_MIDDLE),
                    0);
    rows->AddColumn(
        new BStringColumn("Original folder", 246, 120, 1000, B_TRUNCATE_MIDDLE),
        1);
    rows->AddColumn(new BStringColumn("Status", 96, 80, 200, B_TRUNCATE_END),
                    2);
    rows->SetExplicitMinSize(BSize(365, 130));
    rows->SetSelectionMode(B_MULTIPLE_SELECTION_LIST);
    rows->SetSelectionMessage(new BMessage(kSelect));
    rows->SetTarget(this);
    status = new BStringView(
        "shelf status", "Drop local entries, or use Files > Add to Shelf.");
    status->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
    status->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    status->SetTruncation(B_TRUNCATE_END);
    remove = new BButton("Remove from Shelf", new BMessage(kRemove));
    move = new BButton("Move all...", new BMessage(kPickFolder));
    BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
        .SetInsets(10)
        .Add(rows)
        .Add(status)
        .AddGroup(B_HORIZONTAL, 8)
        .Add(new BButton("Refresh", new BMessage(kRefreshShelf)))
        .Add(remove)
        .AddGlue()
        .Add(move)
        .End();
    AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
    Render();
  }

  void WindowActivated(bool active) override {
    BWindow::WindowActivated(active);
    if (active)
      Render();
  }

  void MessageReceived(BMessage *message) override {
    try {
      switch (message->what) {
      case kShowShelf:
        Render();
        Activate();
        return;
      case kRefreshShelf:
        Render();
        return;
      case kSelect:
        remove->SetEnabled(rows->CurrentSelection() != nullptr);
        return;
      case kRemove:
        shelf->Remove(Selected());
        Render();
        Status("Removed references only. Original files were not changed.");
        return;
      case kPickFolder:
        PickFolder();
        return;
      case kFolderChosen:
        ConfirmFolder(*message);
        return;
      case B_CANCEL:
        if (message->GetInt64("shelf picker", -1) == pickerSerial)
          picker.reset();
        return;
      case kDropped:
        Add(*message);
        return;
      default:
        if (message->WasDropped()) {
          Add(*message);
          return;
        }
        BWindow::MessageReceived(message);
      }
    } catch (const std::exception &e) {
      Render();
      Status(e.what());
    }
  }
};
} // namespace

void OpenShelfWindow(BMessenger owner, const std::shared_ptr<Shelf> &shelf,
                     BMessenger &window) {
  if (window.SendMessage(kShowShelf) == B_OK)
    return;
  auto shelfWindow = new ShelfWindow(owner, shelf);
  window = BMessenger(shelfWindow);
  shelfWindow->Show();
}
} // namespace Listi
