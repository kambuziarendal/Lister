// SPDX-License-Identifier: MIT
#include "FileSearch.h"
#include "SavedSearches.h"
#include "SearchResultsView.h"
#include "SearchWindow.h"
#include "SearchWorker.h"
#include <Application.h>
#include <Button.h>
#include <StringView.h>
#include <TextControl.h>
#include <Window.h>
#include <cassert>
#include <cstdio>
#include <string>

int main() {
  BApplication app("application/x-vnd.Listi-SearchResultsTest");
  auto window = new BWindow(BRect(40, 40, 600, 400), "Search results",
                            B_TITLED_WINDOW, 0);
  auto results = new Listi::SearchResultsView(BMessenger(), 0);
  window->AddChild(results);
  Listi::SearchHit first{"/b/notes", 1, 42};
  Listi::SearchHit second{"/a/notes", 1, 43};
  results->Add(first);
  results->Add(second);
  auto selected = results->RowAt(0);
  results->SetFocusRow(selected, false);
  results->AddToSelection(selected);
  results->Order(3); // Original folder, ascending.
  assert(results->RowAt(1) == selected);
  assert(results->CurrentSelection() == selected && results->FocusRow() == selected);
  results->Reconcile({first, second, {"/c/notes", 1, 44}, first});
  assert(results->CountRows() == 3);
  assert(results->SelectedHit() && results->SelectedHit()->path == first.path);
  // A new file at the same path must not silently inherit the selection.
  auto replacement = first;
  ++replacement.node;
  results->Reconcile({second, replacement});
  assert(results->CountRows() == 2 && !results->SelectedHit());
  results->Order(0); // As found: new live hits follow scan arrival, not path order.
  const Listi::SearchHit earlier{"/z/notes", 1, 45};
  const Listi::SearchHit later{"/d/notes", 1, 46};
  results->Reconcile({second, earlier, later, earlier});
  assert(results->CountRows() == 3);
  results->AddToSelection(results->RowAt(1));
  assert(results->SelectedHit()->path == earlier.path);
  results->DeselectAll();
  results->AddToSelection(results->RowAt(2));
  assert(results->SelectedHit()->path == later.path);
  window->Quit();
  auto searchWindow = new BWindow(BRect(40, 40, 600, 400), "Search",
                                  B_TITLED_WINDOW, 0);
  auto search = CreatePanelSearch(BMessenger(), 0, "/boot/home",
                                  Listi::SavedSearches().Message());
  searchWindow->AddChild(search);
  auto expression =
      dynamic_cast<BTextControl *>(searchWindow->FindView("search expression"));
  auto status =
      dynamic_cast<BStringView *>(searchWindow->FindView("search status"));
  assert(expression && status);
  expression->SetText("notes");
  BMessage start(Listi::kSearchStart);
  search->MessageReceived(&start);
  assert(std::string(status->Text()).find("Searching") == 0);
  // Deliver one batch on the window thread, before the queued worker can reply.
  BMessage begin(Listi::kSearchBegin);
  begin.AddInt64("serial", 1);
  begin.AddInt64("revision", 1);
  begin.AddBool("initial", true);
  search->MessageReceived(&begin);
  BMessage item;
  item.AddString("path", first.path.c_str());
  item.AddInt64("device", first.device);
  item.AddInt64("node", first.node);
  BMessage rows(Listi::kSearchRows);
  rows.AddInt64("serial", 1);
  rows.AddInt64("revision", 1);
  rows.AddMessage("hit", &item);
  search->MessageReceived(&rows);
  BMessage stop(Listi::kSearchStop);
  search->MessageReceived(&stop);
  assert(std::string(status->Text()).find("Stopped: 1 result kept.") == 0);
  auto *save = dynamic_cast<BButton *>(search->FindView("save search"));
  auto *forget = dynamic_cast<BButton *>(search->FindView("forget search"));
  auto *name = dynamic_cast<BTextControl *>(search->FindView("search name"));
  assert(save && forget && name);
  Listi::SavedSearch item;
  item.name = "Notes";
  item.request.root = "/boot/home";
  item.request.expression = "first";
  Listi::SavedSearches store;
  store.Put(item, false);
  int64 revision = 0;
  auto update = [&] {
    auto snapshot = store.Message();
    snapshot.what = kSearchStore;
    snapshot.AddInt64("revision", ++revision);
    snapshot.AddBool("writable", true);
    search->MessageReceived(&snapshot);
  };
  update();
  BMessage load('sqld');
  load.AddString("name", "Notes");
  search->MessageReceived(&load);
  expression->SetText("unfinished work");
  auto other = item;
  other.name = "Music";
  store.Put(other, false);
  update();
  assert(save->IsEnabled() && forget->IsEnabled());

  // Another editor changed only a nested filter of the loaded search.
  // Receiving its new revision is not consent to overwrite that change.
  item.request.filters.extension = "pdf";
  store.Put(item, true);
  update();
  assert(!save->IsEnabled() && !forget->IsEnabled());
  assert(std::string(expression->Text()) == "unfinished work");
  assert(std::string(status->Text()).find("changed elsewhere") != std::string::npos);
  auto *extension = dynamic_cast<BTextControl *>(search->FindView("file extension"));
  assert(extension && std::string(extension->Text()).empty());
  name->SetText("Notes copy");
  BMessage renameDraft('sqnm');
  search->MessageReceived(&renameDraft);
  assert(save->IsEnabled() && !forget->IsEnabled());
  search->MessageReceived(&load);
  assert(save->IsEnabled() && forget->IsEnabled());
  assert(std::string(extension->Text()) == "pdf");

  store.Remove("Notes");
  update();
  assert(!save->IsEnabled() && !forget->IsEnabled());
  expression->SetText("preserve me");
  search->MessageReceived(&load); // A stale menu invocation.
  assert(std::string(expression->Text()) == "preserve me");
  assert(std::string(status->Text()) == "That saved search no longer exists.");
  searchWindow->Quit();
  puts("Search result ordering, live reconciliation and stop status OK");
}
