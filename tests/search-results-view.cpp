// SPDX-License-Identifier: MIT
#include "FileSearch.h"
#include "SearchResultsView.h"
#include <Application.h>
#include <Window.h>
#include <cassert>
#include <cstdio>

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
  window->Quit();
  puts("Search result ordering and live identity reconciliation OK");
}
