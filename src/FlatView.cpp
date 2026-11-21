// SPDX-License-Identifier: MIT
#include "ColumnRows.h"
#include "FlatView.h"
#include "Common.h"
#include "FileSearch.h"
#include "NameFilter.h"
#include "SearchWorker.h"
#include "Shelf.h"
#include "ShelfWindow.h"
#include <Button.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <MessageRunner.h>
#include <StorageDefs.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>
#include <deque>
#include <exception>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Listi {
namespace {
constexpr uint32 kFilterChanged = 'flfc', kCollect = 'flcl', kDeliveryCheck = 'fldc';

class FlatRow : public BRow {
public:
  const SearchHit &hit;
  FlatRow(const SearchHit &file, const std::string &name)
      : BRow(24), hit(file) {
    SetField(new BStringField(name.c_str()), 0);
    SetField(new BStringField(hit.relativeFolder.c_str()), 1);
  }
};

class FlatResults : public BColumnListView {
  BButton *collect;

public:
  explicit FlatResults(BButton *action)
      : BColumnListView("flat results", B_WILL_DRAW | B_NAVIGABLE,
                        B_FANCY_BORDER),
        collect(action) {
    AddColumn(new BStringColumn("Name", 230, 100, 1000, B_TRUNCATE_MIDDLE), 0);
    AddColumn(
        new BStringColumn("Relative folder", 360, 120, 1600, B_TRUNCATE_MIDDLE),
        1);
    SetSelectionMode(B_MULTIPLE_SELECTION_LIST);
  }

  void SelectionChanged() override {
    BColumnListView::SelectionChanged();
    collect->SetEnabled(CurrentSelection() != nullptr);
  }
};

class FlatView : public BGroupView {
  struct File {
    SearchHit hit;
    std::string name;
    FlatRow *row = nullptr;
  };

  const std::string rootPath;
  BMessenger owner;
  std::shared_ptr<Shelf> shelf;
  std::shared_ptr<std::atomic<int64>> generation =
      std::make_shared<std::atomic<int64>>(1);
  BLooper *worker = nullptr;
  std::shared_ptr<std::atomic<int64>> failedSerial = std::make_shared<std::atomic<int64>>(-1);
  std::unique_ptr<BMessageRunner> deliveryTimer;
  // Rows refer to these immutable scan hits. Growth must not invalidate them.
  std::deque<File> files;
  BTextControl *filter;
  BStringView *status, *shelfStatus;
  BButton *stop, *collect;
  FlatResults *results;
  std::string needle, scanState = "Scanning…";
  bool scanning = false;

  static BStringView *Line(const char *name, const char *text) {
    auto line = new BStringView(name, text);
    line->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
    line->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    line->SetTruncation(B_TRUNCATE_MIDDLE);
    line->SetToolTip(text);
    return line;
  }

  void UpdateRow(File &file) {
    const bool visible =
        MatchesNameFilter(file.name.c_str(), needle.c_str()) ||
        MatchesNameFilter(file.hit.relativeFolder.c_str(), needle.c_str());
    if (visible && !file.row) {
      file.row = new FlatRow(file.hit, file.name);
      results->AddRow(file.row);
    } else if (!visible && file.row) {
      results->RemoveRow(file.row);
      delete file.row;
      file.row = nullptr;
    }
  }

  void UpdateStatus() {
    std::string text = std::to_string(results->CountRows()) + " of " +
                       std::to_string(files.size()) + " files shown. " +
                       scanState;
    status->SetText(text.c_str());
    status->SetToolTip(text.c_str());
    collect->SetEnabled(results->CurrentSelection() != nullptr);
    stop->SetEnabled(scanning);
    if (!scanning) deliveryTimer.reset();
  }

  void StartScan() {
    generation->fetch_add(1);
    Listi::ClearColumnRows(*results);
    files.clear();
    shelfStatus->SetText("");
    shelfStatus->SetToolTip("");
    scanState = "Scanning…";

    if (!worker) {
      worker = NewSearchWorker(BMessenger(this), generation, failedSerial);
      if (worker->Run() < 0) {
        if (worker->Lock())
          worker->Quit();
        worker = nullptr;
      }
    }
    BMessage check(kDeliveryCheck);
    deliveryTimer.reset(new BMessageRunner(BMessenger(this), &check, 250000));
    BMessage request(kSearchStart);
    request.AddInt64("serial", generation->load());
    request.AddString("root", rootPath.c_str());
    request.AddBool("flat", true);
    scanning = worker && deliveryTimer->InitCheck() == B_OK &&
               worker->PostMessage(&request) == B_OK;
    if (!scanning)
      scanState = "Incomplete: could not start scan.";
    UpdateStatus();
  }

  void Collect() {
    std::vector<std::string> paths;
    std::map<std::string, NodeIdentity> expected;
    for (auto row = results->CurrentSelection(); row;
         row = results->CurrentSelection(row)) {
      const auto &hit = static_cast<FlatRow *>(row)->hit;
      paths.push_back(hit.path);
      expected.emplace(hit.path, NodeIdentity{int32(hit.device), hit.node});
    }
    if (paths.empty())
      return;
    std::string text;
    try {
      // Do not re-capture identities after the scan or consult the active pane.
      // Shelf validates the entire selected set before adding any references.
      auto count = shelf->Add(paths, expected);
      text = count ? std::to_string(count) + " added to Shelf."
                   : "Selection is already on the Shelf.";
      BMessage refresh(kRefreshShelf);
      owner.SendMessage(&refresh, (BHandler *)nullptr, 0);
    } catch (const std::exception &e) {
      text = e.what();
    }
    shelfStatus->SetText(text.c_str());
    shelfStatus->SetToolTip(text.c_str());
  }

  void Colors() {
    results->SetColor(B_COLOR_BACKGROUND,
                      ui_color(B_DOCUMENT_BACKGROUND_COLOR));
    results->SetColor(B_COLOR_TEXT, ui_color(B_DOCUMENT_TEXT_COLOR));
    results->SetColor(B_COLOR_SELECTION, ui_color(B_CONTROL_HIGHLIGHT_COLOR));
    results->SetColor(B_COLOR_SELECTION_TEXT, ui_color(B_CONTROL_TEXT_COLOR));
    results->SetColor(B_COLOR_NON_FOCUS_SELECTION,
                      tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), .8f));
    results->Invalidate();
  }

public:
  FlatView(BMessenger target, std::shared_ptr<Shelf> collection,
           const char *path)
      : BGroupView("flat view", B_VERTICAL, 5), rootPath(path), owner(target),
        shelf(std::move(collection)) {
    auto root = Line("flat root", (std::string("Folder: ") + path).c_str());
    filter = new BTextControl("flat filter", "Filter:", "", nullptr);
    filter->SetToolTip("Filter by file name or relative folder");
    filter->SetModificationMessage(new BMessage(kFilterChanged));
    filter->TextView()->SetMaxBytes(B_PATH_NAME_LENGTH);
    status = Line("flat status", "");
    shelfStatus = Line("flat Shelf status", "");
    stop =
        new BButton("cancel flat scan", "Stop scan", new BMessage(kSearchStop));
    collect = new BButton("add flat selection", "Add to Shelf",
                          new BMessage(kCollect));
    results = new FlatResults(collect);
    BLayoutBuilder::Group<>(this)
        .SetInsets(5)
        .Add(root)
        .AddGroup(B_HORIZONTAL, 8)
        .Add(filter)
        .Add(stop)
        .End()
        .Add(results)
        .Add(status)
        .AddGroup(B_HORIZONTAL, 8)
        .Add(shelfStatus)
        .Add(collect)
        .End();
    Colors();
  }

  void AllAttached() override {
    BGroupView::AllAttached();
    filter->SetTarget(this);
    stop->SetTarget(this);
    collect->SetTarget(this);
    if (!worker)
      StartScan();
  }

  ~FlatView() {
    generation->fetch_add(1);
    if (worker && worker->Lock())
      worker->Quit();
    results->Clear();
  }

  void MessageReceived(BMessage *message) override {
    switch (message->what) {
    case kFlatView:
      filter->MakeFocus();
      break;
    case kRefresh:
      StartScan();
      break;
    case kFilterChanged:
      needle = filter->Text();
      for (auto &file : files)
        UpdateRow(file);
      UpdateStatus();
      break;
    case kAddToShelf:
    case kCollect:
      Collect();
      break;
    case kDeliveryCheck:
      if (scanning && failedSerial->load() == generation->load()) {
        generation->fetch_add(1);
        scanning = false;
        scanState = "Incomplete: results could not be delivered.";
        UpdateStatus();
      }
      break;
    case kSearchStop:
      if (scanning) {
        generation->fetch_add(1);
        scanning = false;
        scanState = "Incomplete: scan stopped.";
        UpdateStatus();
      }
      break;
    case kSearchBegin:
      break; // No revisions or live updates in Flat View.
    case kSearchRows: {
      if (!scanning || message->GetInt64("serial", -1) != generation->load())
        break;
      BMessage item;
      for (int32 i = 0; message->FindMessage("hit", i, &item) == B_OK; ++i) {
        SearchHit hit{item.GetString("path", ""), item.GetInt64("device", -1),
                      item.GetInt64("node", -1), item.GetString("folder", "")};
        std::string name = hit.path.substr(hit.path.find_last_of('/') + 1);
        auto &file = files.emplace_back(File{std::move(hit), std::move(name)});
        UpdateRow(file);
      }
      UpdateStatus();
      break;
    }
    case kSearchDone: {
      if (!scanning || message->GetInt64("serial", -1) != generation->load())
        break;
      scanning = false;
      const std::string error = message->GetString("error", "");
      const int64 skipped = message->GetInt64("skipped", 0);
      if (!error.empty())
        scanState = "Incomplete: " + error;
      else if (message->GetBool("cancelled", false))
        scanState = "Incomplete: scan interrupted.";
      else if (message->GetBool("limited", false))
        scanState = "Incomplete: scan limit reached.";
      else if (skipped)
        scanState = "Incomplete: " + std::to_string(skipped) +
                    " entries or folders skipped.";
      else
        scanState = "Scan complete.";
      UpdateStatus();
      break;
    }
    case B_COLORS_UPDATED:
      BGroupView::MessageReceived(message);
      Colors();
      break;
    default:
      BGroupView::MessageReceived(message);
      break;
    }
  }
};
}

BView *CreateFlatView(BMessenger owner, std::shared_ptr<Shelf> shelf,
                        const char *root) {
  return new FlatView(owner, std::move(shelf), root);
}
}
