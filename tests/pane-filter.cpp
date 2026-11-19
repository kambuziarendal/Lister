// SPDX-License-Identifier: MIT
#include "Pane.h"
#include <Application.h>
#include <LayoutBuilder.h>
#include <ScrollBar.h>
#include <TextControl.h>
#include <Window.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <unistd.h>

int main() {
  BApplication app("application/x-vnd.Listi-PaneFilterTest");
  char temp[] = "/tmp/Listi-grid-filter-XXXXXX", canonical[PATH_MAX];
  if (!mkdtemp(temp) || !realpath(temp, canonical))
    return 2;
  std::string root(canonical);
  const int count = 80;
  for (int i = 0; i < count; ++i)
    if (!(std::ofstream(root + "/" + std::to_string(i)) << i))
      return 2;

  Preferences prefs;
  prefs.SetPaneMode(0, 1);
  auto pane = new Pane(0, prefs);
  auto window = new BWindow(BRect(0, 0, 640, 400), "Filter", B_TITLED_WINDOW, 0);
  BLayoutBuilder::Group<>(window, B_VERTICAL).Add(pane);
  window->Show();
  window->Lock();
  pane->Navigate(root.c_str());
  window->Unlock();
  bool ready = false;
  for (int i = 0; i < 200 && !ready; ++i) {
    snooze(25000);
    window->Lock();
    ready = !pane->IsScanning() && root == pane->Path();
    window->Unlock();
  }

  window->Lock();
  auto grid = pane->FindView("file grid");
  auto bar = grid ? grid->ScrollBar(B_VERTICAL) : nullptr;
  auto filter = dynamic_cast<BTextControl *>(pane->FindView("filter"));
  bool passed = false;
  if (ready && bar && filter) {
    float low, initial, filtered, restored;
    bar->GetRange(&low, &initial);
    bar->SetValue(initial);
    BMessage changed(kFilterChanged);
    filter->SetText("no matches");
    pane->MessageReceived(&changed);
    bar->GetRange(&low, &filtered);
    filter->SetText("");
    pane->MessageReceived(&changed);
    bar->GetRange(&low, &restored);
    passed = initial > 0 && filtered == 0 && restored == initial;
    const auto revision = pane->SelectionRevision();
    const bool preview = pane->MatchPattern("7?", false) == 10 && pane->Selected().empty();
    const bool selected = pane->MatchPattern("7?", true) == 10 && pane->Selected().size() == 10;
    filter->SetText("79");
    pane->MessageReceived(&changed);
    passed = passed && preview && selected && revision >= 0 && pane->SelectionRevision() != revision &&
             pane->MatchPattern("*", false) == 1 && pane->Selected().size() == 1;
  }
  window->Quit();
  for (int i = 0; i < count; ++i)
    unlink((root + "/" + std::to_string(i)).c_str());
  rmdir(root.c_str());
  std::cout << (passed ? "PASS " : "FAIL ")
            << "icon grid filter, wildcard selection and preview invalidation\n";
  return passed ? 0 : 1;
}
