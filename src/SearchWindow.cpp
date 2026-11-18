// SPDX-License-Identifier: MIT
#include "SearchWindow.h"
#include "Common.h"
#include "ExplicitSubmit.h"
#include "FileSearch.h"
#include "SearchWorker.h"
#include "SavedSearches.h"
#include <MessageRunner.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <map>
#include <tuple>
#include <Button.h>
#include <CheckBox.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <LayoutBuilder.h>
#include <Looper.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>
#include <memory>
#include <stdexcept>
namespace {
using namespace Listi;
constexpr uint32 kStart = 'ssrt', kStop = 'sstp', kHit = 'shit',
                 kFinished = 'sfin', kSave = 'sqsv', kForget = 'sqfg',
                 kLoad = 'sqld', kName = 'sqnm', kPicked = 'spik',
                 kShow = 'sshw', kMode = 'smod', kClearFilters = 'sclr',
                 kDeliveryCheck = 'sdck';
struct HitRow : BRow {
  Listi::SearchHit hit;
  explicit HitRow(const Listi::SearchHit &value) : BRow(24), hit(value) {
    size_t slash = hit.path.find_last_of('/');
    SetField(new BStringField(hit.path.substr(slash + 1).c_str()), 0);
    SetField(new BStringField(hit.path.substr(0, slash ? slash : 1).c_str()),
             1);
  }
};
class Results : public BColumnListView {
public:
  Results()
      : BColumnListView("search results", B_WILL_DRAW | B_NAVIGABLE,
                        B_FANCY_BORDER) {
    AddColumn(new BStringColumn("Name", 220, 80, 1000, B_TRUNCATE_MIDDLE), 0);
    AddColumn(
        new BStringColumn("Original folder", 440, 120, 2000, B_TRUNCATE_MIDDLE),
        1);
    SetSelectionMode(B_SINGLE_SELECTION_LIST);
    SetInvocationMessage(new BMessage(kShow));
  }
  void SelectionChanged() override {
    BColumnListView::SelectionChanged();
    if (Window())
      Window()->PostMessage(kPicked);
  }
};
class SearchWindow : public BWindow {
  BMessenger owner;
  int32 pane;
  BTextControl *root, *expression, *name;
  BTextControl *extension, *minimum, *maximum, *fromDate, *throughDate;
  BCheckBox *createdDate;
  BButton *clearFilters;
  BPopUpMenu *savedMenu;
  BMenuField *savedField;
  BButton *save, *forget;
  SavedSearches saved;
  std::string chosen;
  int64 storeRevision = 0, resultRevision = 0;
  bool writable = true, saving = false, initial = true;
  std::vector<SearchHit> pending;
  BCheckBox *predicate, *trash, *live;
  BStringView *status, *selected, *scope, *hint;
  BButton *start, *stop, *show;
  Results *results;
  BLooper *worker;
  std::shared_ptr<std::atomic<int64>> generation;
  std::shared_ptr<std::atomic<int64>> failedSerial =
      std::make_shared<std::atomic<int64>>(-1);
  std::unique_ptr<BMessageRunner> deliveryTimer;
  bool workerReady = false;
  int64 serial = 0;
  bool busy = false;
  static BStringView *Line(const char *name, const char *text) {
    auto view = new BStringView(name, text);
    view->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
    view->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    view->SetTruncation(B_TRUNCATE_MIDDLE);
    view->SetToolTip(text);
    return view;
  }
  void Status(const std::string &text) {
    status->SetText(text.c_str());
    status->SetToolTip(text.c_str());
  }
  void Busy(bool value) {
    busy = value;
    if (!busy)
      deliveryTimer.reset();
    start->SetEnabled(!busy);
    stop->SetEnabled(busy);
    root->SetEnabled(!busy);
    expression->SetEnabled(!busy);
    predicate->SetEnabled(!busy);
    trash->SetEnabled(!busy);
    for (auto field : {extension, minimum, maximum, fromDate, throughDate})
      field->SetEnabled(!busy);
    createdDate->SetEnabled(!busy);
    clearFilters->SetEnabled(!busy);
    live->SetEnabled(!busy && predicate->Value() == B_CONTROL_ON);
  }
  SearchFilters Filters() const {
    return {extension->Text(),   minimum->Text(),
            maximum->Text(),     fromDate->Text(),
            throughDate->Text(), createdDate->Value() == B_CONTROL_ON};
  }
  void Filters(const SearchFilters &f) {
    extension->SetText(f.extension.c_str());
    minimum->SetText(f.minimumSize.c_str());
    maximum->SetText(f.maximumSize.c_str());
    fromDate->SetText(f.fromDate.c_str());
    throughDate->SetText(f.throughDate.c_str());
    createdDate->SetValue(f.created);
  }
  bool AddFilters(BMessage &message) {
    try {
      auto f = Filters();
      CompileSearchFilters(f);
      if (!expression->Text()[0] &&
          (predicate->Value() == B_CONTROL_ON || !f.Active()))
        throw std::runtime_error(
            "Enter a name, at least one file filter, or a BFS formula.");
      WriteSearchFilters(message, f);
      return true;
    } catch (const std::exception &e) {
      Status(e.what());
      return false;
    }
  }
  void StoreControls() {
    save->SetEnabled(writable && !saving);
    forget->SetEnabled(writable && !saving && !chosen.empty() &&
                       chosen == name->Text());
    name->SetEnabled(!saving);
    savedField->SetEnabled(!saving);
  }
  void Store(const BMessage &m) {
    saved = SavedSearches::FromMessage(m);
    if (m.HasString("chosen")) {
      chosen = m.GetString("chosen", "");
      auto first = chosen.find_first_not_of(" \t\r\n");
      chosen = first == std::string::npos
                   ? ""
                   : chosen.substr(first, chosen.find_last_not_of(" \t\r\n") -
                                              first + 1);
      name->SetText(chosen.c_str());
    }
    storeRevision = m.GetInt64("revision", 0);
    writable = m.GetBool("writable", false);
    saving = m.GetBool("saving", false);
    while (auto item = savedMenu->RemoveItem(int32(0)))
      delete item;
    auto fresh = new BMessage(kLoad);
    fresh->AddString("name", "");
    savedMenu->AddItem(new BMenuItem("New search", fresh));
    for (const auto &item : saved.Items()) {
      auto pick = new BMessage(kLoad);
      pick->AddString("name", item.name.c_str());
      auto entry = new BMenuItem(item.name.c_str(), pick);
      savedMenu->AddItem(entry);
      entry->SetMarked(item.name == chosen);
    }
    savedMenu->SetTargetForItems(this);
    if (!saved.Find(chosen)) {
      chosen.clear();
      savedMenu->ItemAt(0)->SetMarked(true);
    }
    StoreControls();
    if (m.HasString("error"))
      Status(m.GetString("error", "Save failed."));
    else if (m.GetBool("committed", false))
      Status("Search criteria saved. Results are never stored.");
  }
  void Cancel() {
    if (!busy)
      return;
    BMessage request(kStop);
    request.AddInt64("serial", serial);
    ++serial;
    generation->store(serial);
    worker->PostMessage(&request);
    pending.clear();
    Busy(false);
    Status("Stopped: " + std::to_string(results->CountRows()) +
           " results. No longer updating.");
  }
  void Reconcile() {
    using Key = std::tuple<std::string, int64, int64>;
    std::map<Key, SearchHit> next;
    for (const auto &hit : pending)
      next.emplace(Key(hit.path, hit.device, hit.node), hit);
    for (int32 i = results->CountRows() - 1; i >= 0; --i) {
      auto row = static_cast<HitRow *>(results->RowAt(i));
      auto found =
          next.find(Key(row->hit.path, row->hit.device, row->hit.node));
      if (found == next.end()) {
        results->RemoveRow(row);
        delete row;
      } else
        next.erase(
            found); // Preserve selection only for identical path AND identity.
    }
    for (const auto &item : next)
      results->AddRow(new HitRow(item.second));
    pending.clear();
    PostMessage(kPicked);
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
  SearchWindow(BMessenger target, int32 which, const char *path,
               const BMessage &store)
      : BWindow(BRect(90, 90, 1000, 720), "Listi: Find files", B_TITLED_WINDOW,
                B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
        owner(target), pane(which),
        generation(std::make_shared<std::atomic<int64>>(0)) {
    root = new BTextControl("search scope", "Folder:", path, nullptr);
    expression =
        new BTextControl("search expression", "Name contains:", "", nullptr);
    root->TextView()->SetMaxBytes(B_PATH_NAME_LENGTH - 1);
    expression->TextView()->SetMaxBytes(4096);
    predicate =
        new BCheckBox("bfs formula", "BFS formula", new BMessage(kMode));
    live = new BCheckBox("live results", "Live", nullptr);
    trash = new BCheckBox("include trash", "Include Trash", nullptr);
    extension = new BTextControl("file extension", "Extension:", "", nullptr);
    minimum = new BTextControl("minimum size", "Min size:", "", nullptr);
    maximum = new BTextControl("maximum size", "Max size:", "", nullptr);
    fromDate = new BTextControl("from date", "From:", "", nullptr);
    throughDate = new BTextControl("through date", "Through:", "", nullptr);
    createdDate = new BCheckBox("creation date", "Use creation date", nullptr);
    clearFilters = new BButton("clear file filters", "Clear filters",
                               new BMessage(kClearFilters));
    for (auto field : {extension, minimum, maximum, fromDate, throughDate}) {
      field->TextView()->SetMaxBytes(255);
      field->SetExplicitMinSize(BSize(180, B_SIZE_UNSET));
    }
    extension->SetToolTip(
        "File extension, case-insensitive, e.g. pdf or tar.gz. Not MIME or wildcards.");
    for (auto field : {minimum, maximum})
      field->SetToolTip(
          "Inclusive size: whole bytes or B, KiB, MiB, GiB. Blank means no bound.");
    for (auto field : {fromDate, throughDate})
      field->SetToolTip(
          "YYYY-MM-DD in local time. Both calendar days included. Blank means no bound.");
    createdDate->SetToolTip(
        "Unchecked: last modification. Checked: native creation time, not metadata-change time.");
    hint = Line(
        "search hint",
        "Names: literal, case-insensitive. Subfolders on this volume; no link traversal.");
    scope = Line("result scope", which == 0
                                     ? "Show original uses the left panel."
                                     : "Show original uses the right panel.");
    status = Line(
        "search status",
        "Choose a folder and search. Results are a snapshot, not a live query.");
    selected = Line("search selected",
                    "Select a result to see its full original path.");
    start = new BButton("start search", "Search", new BMessage(kStart));
    stop = new BButton("stop search", "Stop", new BMessage(kStop));
    show = new BButton("show original", "Show original in panel",
                       new BMessage(kShow));
    results = new Results;
    savedMenu = new BPopUpMenu("saved searches");
    savedField = new BMenuField("saved searches", "Saved:", savedMenu);
    savedField->SetExplicitMinSize(BSize(130, B_SIZE_UNSET));
    savedField->SetExplicitMaxSize(BSize(220, B_SIZE_UNSET));
    name = new BTextControl("search name", "Name:", "", nullptr);
    name->TextView()->SetMaxBytes(80);
    name->SetModificationMessage(new BMessage(kName));
    save = new BButton("save search", "Save", new BMessage(kSave));
    forget = new BButton("forget search", "Forget", new BMessage(kForget));
    BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
        .SetInsets(10)
        .AddGroup(B_HORIZONTAL, 8)
        .Add(savedField)
        .Add(name)
        .Add(save)
        .Add(forget)
        .End()
        .Add(root)
        .Add(expression)
        .Add(Line(
            "file filters hint",
            "Optional file filters (regular files only): sizes in B / KiB / MiB / GiB; dates YYYY-MM-DD, local time."))
        .AddGroup(B_HORIZONTAL, 8)
        .Add(extension)
        .Add(minimum)
        .Add(maximum)
        .End()
        .AddGroup(B_HORIZONTAL, 8)
        .Add(fromDate)
        .Add(throughDate)
        .Add(createdDate)
        .Add(clearFilters)
        .End()
        .AddGroup(B_HORIZONTAL, 8)
        .Add(predicate)
        .Add(trash)
        .Add(live)
        .AddGlue()
        .Add(start)
        .Add(stop)
        .End()
        .Add(hint)
        .Add(scope)
        .Add(results)
        .Add(selected)
        .AddGroup(B_HORIZONTAL, 8)
        .Add(status)
        .Add(show)
        .End();
    results->SetTarget(this);
    for (auto field :
         {root, expression, extension, minimum, maximum, fromDate, throughDate})
      field->TextView()->AddFilter(
          new Listi::ExplicitSubmit(BMessenger(this), BMessage(kStart)));
    AddShortcut(B_ESCAPE, B_NO_COMMAND_KEY, new BMessage(kStop));
    AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
    worker = NewSearchWorker(BMessenger(this), generation, failedSerial);
    workerReady = worker->Run() >= 0;
    Store(store);
    Busy(false);
    show->SetEnabled(false);
    Colors();
    expression->MakeFocus();
  }
  ~SearchWindow() {
    generation->fetch_add(1);
    // Invalidate in-flight work before joining. A merely posted quit could
    // destroy the worker's BHandler after BApplication's token space is gone.
    if (worker->Lock())
      worker->Quit();
  }
  void MessageReceived(BMessage *m) override {
    switch (m->what) {
    case kSearchStore:
      Store(*m);
      break;
    case kName:
      StoreControls();
      break;
    case kLoad: {
      if (saving)
        break;
      Cancel();
      chosen = m->GetString("name", "");
      name->SetText(chosen.c_str());
      if (const auto *item = saved.Find(chosen)) {
        root->SetText(item->request.root.c_str());
        expression->SetText(item->request.expression.c_str());
        predicate->SetValue(item->request.predicate);
        trash->SetValue(item->request.includeTrash);
        live->SetValue(item->live);
        Filters(item->request.filters);
      } else {
        expression->SetText("");
        predicate->SetValue(0);
        trash->SetValue(0);
        live->SetValue(0);
        Filters({});
      }
      results->Clear();
      show->SetEnabled(false);
      selected->SetText("");
      selected->SetToolTip("");
      scope->SetText(pane == 0 ? "Show original uses the left panel."
                               : "Show original uses the right panel.");
      scope->SetToolTip(scope->Text());
      PostMessage(kMode);
      StoreControls();
      Status("Criteria loaded. Press Search to obtain current results.");
      break;
    }
    case kSave:
    case kForget: {
      if (!writable || saving)
        break;
      if (m->what == kForget && (chosen.empty() || chosen != name->Text()))
        break;
      BMessage edit(kSearchEdit);
      edit.AddInt64("revision", storeRevision);
      edit.AddString("name", name->Text());
      edit.AddBool("remove", m->what == kForget);
      edit.AddBool("replace", !chosen.empty() && chosen == name->Text());
      edit.AddString("root", root->Text());
      edit.AddString("expression", expression->Text());
      edit.AddBool("predicate", predicate->Value() == B_CONTROL_ON);
      edit.AddBool("trash", trash->Value() == B_CONTROL_ON);
      edit.AddBool("live", live->Value() == B_CONTROL_ON);
      if (m->what == kSave && !AddFilters(edit))
        break;
      if (owner.SendMessage(&edit) == B_OK) {
        saving = true;
        StoreControls();
      } else
        Status(
            "Could not queue saved search. Previous criteria are unchanged.");
      break;
    }
    case kClearFilters:
      if (!busy) {
        Filters({});
        Status("File filters cleared. Name or BFS formula is unchanged.");
      }
      break;
    case kSearch:
      Activate();
      break;
    case kMode: {
      bool formula = predicate->Value() == B_CONTROL_ON;
      if (!formula)
        live->SetValue(0);
      live->SetEnabled(formula && !busy);
      expression->SetLabel(formula ? "BFS formula:" : "Name contains:");
      const char *text =
          formula
              ? "Example: name==\"*.txt\". Native indexed BFS syntax; Live keeps this folder updated."
              : "Names: literal, case-insensitive. Subfolders on this volume; no link traversal.";
      hint->SetText(text);
      hint->SetToolTip(text);
      break;
    }
    case kStart: {
      if (busy)
        break;
      BMessage request(kStart);
      if (!AddFilters(request))
        break;
      ++serial;
      generation->store(serial);
      resultRevision = 0;
      pending.clear();
      results->Clear();
      show->SetEnabled(false);
      selected->SetText("");
      selected->SetToolTip("");
      request.AddInt64("serial", serial);
      request.AddString("root", root->Text());
      request.AddString("expression", expression->Text());
      request.AddBool("predicate", predicate->Value() == B_CONTROL_ON);
      request.AddBool("trash", trash->Value() == B_CONTROL_ON);
      request.AddBool("live", live->Value() == B_CONTROL_ON);
      BString description(pane == 0 ? "Left panel | " : "Right panel | ");
      description << root->Text() << " | " << expression->Text();
      auto filters = Filters();
      if (filters.Active()) {
        description << " | Files";
        if (!filters.extension.empty())
          description << " | extension: " << filters.extension.c_str();
        if (!filters.minimumSize.empty())
          description << " | min: " << filters.minimumSize.c_str();
        if (!filters.maximumSize.empty())
          description << " | max: " << filters.maximumSize.c_str();
        if (!filters.fromDate.empty() || !filters.throughDate.empty()) {
          description << (filters.created ? " | created: " : " | modified: ");
          description << filters.fromDate.c_str() << " through "
                      << filters.throughDate.c_str();
        }
      }
      scope->SetText(description);
      scope->SetToolTip(description);
      Busy(true);
      Status("Searching... Stop keeps only the results received so far.");
      BMessage check(kDeliveryCheck);
      deliveryTimer = std::make_unique<BMessageRunner>(BMessenger(this), &check, 250000);
      if (!workerReady || deliveryTimer->InitCheck() != B_OK ||
          worker->PostMessage(&request) != B_OK) {
        Busy(false);
        Status("Could not start search.");
      }
      break;
    }
    case kDeliveryCheck:
      if (busy && failedSerial->load() == serial) {
        Cancel();
        Status("Incomplete: search results could not be delivered. Start again to retry.");
      }
      break;
    case kStop:
      Cancel();
      break;
    case kSearchBegin:
      if (!busy || m->GetInt64("serial", -1) != serial)
        break;
      resultRevision = m->GetInt64("revision", -1);
      initial = m->GetBool("initial", false);
      pending.clear();
      break;
    case kHit: {
      if (!busy || m->GetInt64("serial", -1) != serial)
        break;
      if (m->GetInt64("revision", -1) != resultRevision)
        break;
      BMessage item;
      for (int32 i = 0; m->FindMessage("hit", i, &item) == B_OK; ++i) {
        SearchHit hit{item.GetString("path", ""), item.GetInt64("device", -1),
                      item.GetInt64("node", -1)};
        if (initial)
          results->AddRow(new HitRow(hit));
        else
          pending.push_back(hit);
      }
      if (initial)
        Status("Searching: " + std::to_string(results->CountRows()) +
               " results. Snapshot in progress.");
      break;
    }
    case kFinished: {
      if (!busy || m->GetInt64("serial", -1) != serial)
        break;
      if (m->GetInt64("revision", -1) < resultRevision)
        break;
      bool updating = m->GetBool("live", false);
      if (!initial)
        Reconcile();
      Busy(updating);
      std::string error = m->GetString("error", "");
      std::string state =
          !error.empty()                   ? "Failed / incomplete"
          : m->GetBool("cancelled", false) ? "Interrupted / incomplete"
          : m->GetBool("limited", false)   ? "Limit reached / incomplete"
          : m->GetInt64("skipped", 0)      ? "Finished with skipped areas"
          : updating                       ? "Live"
                                           : "Complete snapshot";
      Status(state + ": " + std::to_string(results->CountRows()) +
             " results; " + std::to_string(m->GetInt64("visited", 0)) +
             " examined; " + std::to_string(m->GetInt64("skipped", 0)) +
             " skipped." + (error.empty() ? "" : " " + error));
      break;
    }
    case kPicked: {
      auto row = static_cast<HitRow *>(results->CurrentSelection());
      selected->SetText(row ? row->hit.path.c_str() : "");
      selected->SetToolTip(row ? row->hit.path.c_str() : "");
      show->SetEnabled(row != nullptr);
      break;
    }
    case kShow: {
      auto row = static_cast<HitRow *>(results->CurrentSelection());
      if (!row)
        break;
      // The receiving pane rechecks identity after its asynchronous scan, too.
      BMessage reveal(kRevealSearch);
      reveal.AddInt32("pane", pane);
      reveal.AddString("path", row->hit.path.c_str());
      reveal.AddInt64("device", row->hit.device);
      reveal.AddInt64("node", row->hit.node);
      owner.SendMessage(&reveal);
      break;
    }
    case B_COLORS_UPDATED:
      BWindow::MessageReceived(m);
      Colors();
      break;
    default:
      BWindow::MessageReceived(m);
      break;
    }
  }
};
}
BMessenger OpenFileSearch(BMessenger owner, int32 pane, const char *root,
                          const BMessage &store) {
  auto window = new SearchWindow(owner, pane, root, store);
  BMessenger result(window);
  window->Show();
  return result;
}
