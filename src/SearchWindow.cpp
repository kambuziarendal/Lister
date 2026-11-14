// SPDX-License-Identifier: MIT
#include "SearchWindow.h"
#include "Common.h"
#include "ExplicitSubmit.h"
#include "FileSearch.h"
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
namespace {
constexpr uint32 kStart = 'ssrt', kStop = 'sstp', kHit = 'shit',
                 kFinished = 'sfin', kPicked = 'spik', kShow = 'sshw', kMode = 'smod';
struct HitRow : BRow {
  Lister::SearchHit hit;
  explicit HitRow(const Lister::SearchHit &value) : BRow(24), hit(value) {
    size_t slash = hit.path.find_last_of('/');
    SetField(new BStringField(hit.path.substr(slash + 1).c_str()), 0);
    SetField(new BStringField(hit.path.substr(0, slash ? slash : 1).c_str()), 1);
  }
};
class Results : public BColumnListView {
public:
  Results() : BColumnListView("search results", B_WILL_DRAW | B_NAVIGABLE,
                             B_FANCY_BORDER) {
    AddColumn(new BStringColumn("Name", 220, 80, 1000, B_TRUNCATE_MIDDLE), 0);
    AddColumn(new BStringColumn("Original folder", 440, 120, 2000, B_TRUNCATE_MIDDLE), 1);
    SetSelectionMode(B_SINGLE_SELECTION_LIST);
    SetInvocationMessage(new BMessage(kShow));
  }
  void SelectionChanged() override {
    BColumnListView::SelectionChanged();
    if (Window()) Window()->PostMessage(kPicked);
  }
};
class SearchWorker : public BLooper {
  BMessenger target;
  std::shared_ptr<std::atomic<int64>> generation;
public:
  SearchWorker(BMessenger receiver, std::shared_ptr<std::atomic<int64>> g)
      : BLooper("file search"), target(receiver), generation(g) {}
  void MessageReceived(BMessage *message) override {
    if (message->what != kStart) { BLooper::MessageReceived(message); return; }
    int64 serial = message->GetInt64("serial", -1);
    auto alive = [&] { return generation->load() == serial; };
    if (!alive()) return;
    Lister::SearchRequest request;
    request.root = message->GetString("root", "");
    request.expression = message->GetString("expression", "");
    request.predicate = message->GetBool("predicate", false);
    request.includeTrash = message->GetBool("trash", false);
    BMessage batch(kHit);
    batch.AddInt64("serial", serial);
    int count = 0;
    auto flush = [&] {
      if (!count) return true;
      bool ok = alive() && target.SendMessage(&batch, (BHandler *)nullptr, 500000) == B_OK;
      batch.MakeEmpty(); batch.what = kHit; batch.AddInt64("serial", serial); count = 0;
      return ok;
    };
    auto summary = Lister::SearchFiles(request, alive, [&](const Lister::SearchHit &hit) {
      BMessage item;
      item.AddString("path", hit.path.c_str());
      item.AddInt64("device", hit.device); item.AddInt64("node", hit.node);
      batch.AddMessage("hit", &item);
      return ++count < 40 || flush();
    });
    if (!alive()) return;
    if (!flush()) summary.cancelled = true;
    BMessage done(kFinished);
    done.AddInt64("serial", serial);
    done.AddInt64("visited", summary.visited);
    done.AddInt64("skipped", summary.skipped);
    done.AddBool("limited", summary.limited);
    done.AddBool("cancelled", summary.cancelled);
    done.AddString("error", summary.error.c_str());
    target.SendMessage(&done, (BHandler *)nullptr, 500000);
  }
};
class SearchWindow : public BWindow {
  BMessenger owner;
  int32 pane;
  BTextControl *root, *expression;
  BCheckBox *predicate, *trash;
  BStringView *status, *selected, *scope, *hint;
  BButton *start, *stop, *show;
  Results *results;
  BLooper *worker;
  std::shared_ptr<std::atomic<int64>> generation;
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
  void Status(const std::string &text) { status->SetText(text.c_str()); status->SetToolTip(text.c_str()); }
  void Busy(bool value) {
    busy = value;
    start->SetEnabled(!busy); stop->SetEnabled(busy);
    root->SetEnabled(!busy); expression->SetEnabled(!busy);
    predicate->SetEnabled(!busy); trash->SetEnabled(!busy);
  }
  void Colors() {
    results->SetColor(B_COLOR_BACKGROUND, ui_color(B_DOCUMENT_BACKGROUND_COLOR));
    results->SetColor(B_COLOR_TEXT, ui_color(B_DOCUMENT_TEXT_COLOR));
    results->SetColor(B_COLOR_SELECTION, ui_color(B_CONTROL_HIGHLIGHT_COLOR));
    results->SetColor(B_COLOR_SELECTION_TEXT, ui_color(B_CONTROL_TEXT_COLOR));
    results->SetColor(B_COLOR_NON_FOCUS_SELECTION, tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), .8f));
    results->Invalidate();
  }
public:
  SearchWindow(BMessenger target, int32 which, const char *path)
      : BWindow(BRect(90, 90, 950, 640), "Listi: Find files", B_TITLED_WINDOW,
                B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS), owner(target), pane(which),
        generation(std::make_shared<std::atomic<int64>>(0)) {
    root = new BTextControl("search scope", "Folder:", path, nullptr);
    expression = new BTextControl("search expression", "Name contains:", "", nullptr);
    root->TextView()->SetMaxBytes(B_PATH_NAME_LENGTH - 1);
    expression->TextView()->SetMaxBytes(4096);
    predicate = new BCheckBox("bfs formula", "BFS formula", new BMessage(kMode));
    trash = new BCheckBox("include trash", "Include Trash", nullptr);
    hint = Line("search hint", "Names: literal, case-insensitive. Subfolders on this volume; no link traversal.");
    scope = Line("result scope", which == 0 ? "Show original uses the left panel." : "Show original uses the right panel.");
    status = Line("search status", "Choose a folder and search. Results are a snapshot, not a live query.");
    selected = Line("search selected", "Select a result to see its full original path.");
    start = new BButton("start search", "Search", new BMessage(kStart));
    stop = new BButton("stop search", "Stop", new BMessage(kStop));
    show = new BButton("show original", "Show original in panel", new BMessage(kShow));
    results = new Results;
    BLayoutBuilder::Group<>(this, B_VERTICAL, 8).SetInsets(10)
      .Add(root).Add(expression)
      .AddGroup(B_HORIZONTAL, 8).Add(predicate).Add(trash).AddGlue().Add(start).Add(stop).End()
      .Add(hint).Add(scope).Add(results).Add(selected)
      .AddGroup(B_HORIZONTAL, 8).Add(status).Add(show).End();
    results->SetTarget(this);
    for (auto field : {root, expression})
      field->TextView()->AddFilter(new Lister::ExplicitSubmit(BMessenger(this), BMessage(kStart)));
    AddShortcut(B_ESCAPE, B_NO_COMMAND_KEY, new BMessage(kStop));
    AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
    worker = new SearchWorker(BMessenger(this), generation); worker->Run();
    Busy(false); show->SetEnabled(false); Colors(); expression->MakeFocus();
  }
  ~SearchWindow() {
    generation->fetch_add(1);
    // Worker owns no window pointers. Let in-flight filesystem I/O finish off
    // the UI thread; queued quit releases the worker and its shared token.
    worker->PostMessage(B_QUIT_REQUESTED);
  }
  void MessageReceived(BMessage *m) override {
    switch (m->what) {
    case kSearch:
      Activate(); break;
    case kMode: {
      bool formula = predicate->Value() == B_CONTROL_ON;
      expression->SetLabel(formula ? "BFS formula:" : "Name contains:");
      const char *text = formula ? "Example: name==\"*.txt\". Native indexed BFS syntax; snapshot restricted to this folder."
          : "Names: literal, case-insensitive. Subfolders on this volume; no link traversal.";
      hint->SetText(text); hint->SetToolTip(text);
      break;
    }
    case kStart: {
      if (busy) break;
      if (!expression->Text()[0]) { Status("Enter a name fragment or BFS formula."); expression->MakeFocus(); break; }
      ++serial; generation->store(serial);
      results->Clear(); show->SetEnabled(false); selected->SetText(""); selected->SetToolTip("");
      BMessage request(kStart);
      request.AddInt64("serial", serial); request.AddString("root", root->Text());
      request.AddString("expression", expression->Text());
      request.AddBool("predicate", predicate->Value() == B_CONTROL_ON);
      request.AddBool("trash", trash->Value() == B_CONTROL_ON);
      BString description(pane == 0 ? "Left panel | " : "Right panel | ");
      description << root->Text() << " | " << expression->Text();
      scope->SetText(description); scope->SetToolTip(description);
      Busy(true); Status("Searching... Stop keeps only the results received so far.");
      worker->PostMessage(&request); break;
    }
    case kStop:
      if (busy) {
        ++serial; generation->store(serial); Busy(false);
        Status("Stopped: " + std::to_string(results->CountRows()) + " results received. Incomplete snapshot.");
      }
      break;
    case kHit: {
      if (!busy || m->GetInt64("serial", -1) != serial) break;
      BMessage item;
      for (int32 i = 0; m->FindMessage("hit", i, &item) == B_OK; ++i)
        results->AddRow(new HitRow({item.GetString("path", ""), item.GetInt64("device", -1), item.GetInt64("node", -1)}));
      Status("Searching: " + std::to_string(results->CountRows()) + " results. Snapshot in progress.");
      break;
    }
    case kFinished: {
      if (!busy || m->GetInt64("serial", -1) != serial) break;
      Busy(false);
      std::string error = m->GetString("error", "");
      std::string state = !error.empty() ? "Failed / incomplete" :
          m->GetBool("cancelled", false) ? "Interrupted / incomplete" :
          m->GetBool("limited", false) ? "Limit reached / incomplete" :
          m->GetInt64("skipped", 0) ? "Finished with skipped areas" : "Complete snapshot";
      Status(state + ": " + std::to_string(results->CountRows()) + " results; " +
          std::to_string(m->GetInt64("visited", 0)) + " examined; " +
          std::to_string(m->GetInt64("skipped", 0)) + " skipped." + (error.empty() ? "" : " " + error));
      break;
    }
    case kPicked: {
      auto row = static_cast<HitRow *>(results->CurrentSelection());
      selected->SetText(row ? row->hit.path.c_str() : "");
      selected->SetToolTip(row ? row->hit.path.c_str() : "");
      show->SetEnabled(row != nullptr); break;
    }
    case kShow: {
      auto row = static_cast<HitRow *>(results->CurrentSelection());
      if (!row) break;
      // The receiving pane rechecks identity after its asynchronous scan, too.
      BMessage reveal(kRevealSearch);
      reveal.AddInt32("pane", pane); reveal.AddString("path", row->hit.path.c_str());
      reveal.AddInt64("device", row->hit.device); reveal.AddInt64("node", row->hit.node);
      owner.SendMessage(&reveal);
      break;
    }
    case B_COLORS_UPDATED:
      BWindow::MessageReceived(m); Colors(); break;
    default: BWindow::MessageReceived(m); break;
    }
  }
};
}
BMessenger OpenFileSearch(BMessenger owner, int32 pane, const char *root) {
  auto window = new SearchWindow(owner, pane, root);
  BMessenger result(window);
  window->Show();
  return result;
}
