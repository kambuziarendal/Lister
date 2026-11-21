// SPDX-License-Identifier: MIT
#include "ArchiveJobs.h"
#include "MillerColumns.h"
#include "Pane.h"
#include "VirtualFile.h"
#include <Application.h>
#include <LayoutBuilder.h>
#include <Messenger.h>
#include <StringView.h>
#include <TextView.h>
#include <TextControl.h>
#include <Window.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  BApplication app("application/x-vnd.Listi-MillerColumnsTest");
  char temp[] = "/tmp/Listi-miller-XXXXXX";
  char canonical[PATH_MAX];
  if (!mkdtemp(temp) || !realpath(temp, canonical))
    return 2;
  const std::string root = canonical, child = root + "/child";
  const std::string file = root + "/root.txt", inner = child + "/inside.txt";
  if (mkdir(child.c_str(), 0700) != 0)
    return 2;
  std::ofstream(file) << "outer";
  std::ofstream(inner) << "inner";
  auto archive = Listi::PrepareArchiveJob(false, {child});
  archive.archiveFormat = "zip";
  Listi::SetArchiveDestination(archive, root, "fixture.zip");
  if (Listi::Run(archive, {}).completed != 1)
    return 2;
  const auto archiveRoot = Listi::OpenArchive(archive.destination);

  Preferences prefs;
  prefs.icons = false;
  prefs.SetPaneMode(0, MillerMode);
  auto window = new BWindow(BRect(20, 20, 1040, 690), "Miller columns test",
                            B_TITLED_WINDOW, 0);
  auto pane = new Pane(0, prefs);
  BLayoutBuilder::Group<>(window, B_VERTICAL).Add(pane);
  window->Show();

  unsigned failures = 0;
  auto check = [&](bool success, const char *label) {
    failures += !success;
    std::cout << (success ? "PASS " : "FAIL ") << label << std::endl;
  };
  auto ready = [&](const std::string &path) {
    for (int i = 0; i < 200; ++i) {
      window->Lock();
      const bool done = !pane->IsScanning() && path == pane->Path();
      window->Unlock();
      if (done)
        return true;
      snooze(25000);
    }
    return false;
  };
  auto key = [&](char key) {
    auto focused = window->CurrentFocus();
    if (!focused || std::string(focused->Name()) != "Miller rows")
      return false;
    focused->KeyDown(&key, 1);
    return true;
  };
  auto heading = [](BView *column) {
    return column
               ? dynamic_cast<BStringView *>(column->FindView("Miller heading"))
               : nullptr;
  };

  window->Lock();
  pane->Navigate(root.c_str());
  window->Unlock();
  check(ready(root), "native pane lists the fixture");
  window->Lock();
  auto columns =
      dynamic_cast<MillerColumns *>(pane->FindView("Miller columns"));
  const int columnCount = columns ? columns->CountChildren() : 0;
  auto current = columns ? heading(columns->ChildAt(columnCount - 1)) : nullptr;
  check(pane->CurrentView() == MillerMode && columnCount >= 3 && current &&
            current->Text() == Listi::LocationLeaf(root),
        "direct navigation builds native ancestor and current columns");

  BMessenger ancestor(columns ? columns->ChildAt(0) : nullptr);
  pane->NavigateFromColumn(root);
  pane->NavigateFromColumn(child);
  window->Unlock();
  check(ready(child),
        "clicking the open branch does not discard the next queued folder click");
  window->Lock();
  check(columns && ancestor == BMessenger(columns->ChildAt(0)),
        "navigation keeps shared native ancestor views attached");
  pane->NavigateFromColumn(root);
  window->Unlock();
  check(ready(root), "ancestor click returns to the fixture");
  window->Lock();
  pane->FocusList();
  check(key(B_HOME) && key(B_DOWN_ARROW) &&
            pane->Selected() == std::vector<std::string>{child},
        "keyboard selects a directory without changing the action source");
  auto preview = columns ? heading(columns->ChildAt(columnCount)) : nullptr;
  check(preview && std::string(preview->Text()) == "child" &&
            root == pane->Path(),
        "selection adds a preview beside the active directory");
  check(key(B_RIGHT_ARROW), "right arrow enters the focused directory");
  window->Unlock();
  check(ready(child),
        "right arrow commits navigation through the pane scanner");
  window->Lock();
  check(key(B_END) && pane->Selected() == std::vector<std::string>{inner},
        "preview does not replace selectable file rows in the active column");
  auto filter = dynamic_cast<BTextControl *>(pane->FindView("filter"));
  if (filter) {
    filter->SetText("no-match");
    BMessage changed(kFilterChanged);
    pane->MessageReceived(&changed);
  }
  check(filter && pane->Selected().empty(),
        "filter removes the hidden file from normal actions");
  check(key(B_LEFT_ARROW), "left arrow returns to the parent");
  window->Unlock();
  check(ready(root), "left arrow updates the active path");
  window->Lock();
  check(pane->Selected() == std::vector<std::string>{child},
        "parent retains the branch directory as actionable selection");
  // A click captured in a preview must not be reinterpreted after the
  // file is replaced. Hold the window lock until the replacement is visible.
  struct stat before;
  if (lstat(inner.c_str(), &before) != 0)
    return 2;
  PaneEntry clicked;
  clicked.path = inner;
  clicked.device = before.st_dev;
  clicked.node = before.st_ino;
  pane->NavigateFromColumn(child, inner, true, false, {}, &clicked);
  const auto previous = child + "/previous.txt";
  if (rename(inner.c_str(), previous.c_str()) != 0)
    return 2;
  std::ofstream(inner) << "replacement";
  auto *status = dynamic_cast<BTextView *>(pane->FindView("status"));
  bool rejected = false;
  for (int i = 0; i < 200 && !rejected; ++i) {
    window->Unlock();
    snooze(25000);
    window->Lock();
    rejected = status && std::string(status->Text()) ==
                             "Column entry changed. Select it again.";
  }
  check(rejected && root == pane->Path() &&
            pane->Selected() == std::vector<std::string>{child},
        "stale preview click cannot select or open a replacement file");
  if (lstat(inner.c_str(), &before) != 0)
    return 2;
  clicked.device = before.st_dev;
  clicked.node = before.st_ino;
  pane->NavigateFromColumn(child, inner, false, false, {}, &clicked);
  window->Unlock();
  check(ready(child), "fresh preview click reaches the intended folder");
  window->Lock();
  check(pane->Selected() == std::vector<std::string>{inner},
        "fresh preview click selects the matching file after the rescan");
  pane->NavigateFromColumn(root, file);
  pane->Navigate(archiveRoot.c_str());
  window->Unlock();

  check(ready(archiveRoot),
        "archive navigation wins over a stale column click");
  window->Lock();
  auto archiveHeading =
      columns ? heading(columns->ChildAt(columns->CountChildren() - 1))
              : nullptr;
  check(
      archiveHeading && std::string(archiveHeading->Text()) == "fixture.zip" &&
          pane->Selected().empty(),
      "archive location renders a native current column without stale selection");
  pane->FocusList();
  check(key(B_HOME) && key(B_DOWN_ARROW) && key(B_RIGHT_ARROW),
        "archive directory uses the same keyboard navigation");
  window->Unlock();
  const auto archiveChild = Listi::ChildLocation(archiveRoot, "child");
  check(ready(archiveChild),
        "Miller columns enter directories inside archives");
  window->Lock();
  check(key(B_END) && pane->Selected() ==
                          std::vector<std::string>{
                              Listi::ChildLocation(archiveChild, "inside.txt")},
        "files inside archives remain normal pane selections");
  window->Quit();

  unlink(archive.destination.c_str());
  unlink(previous.c_str());
  unlink(inner.c_str());
  unlink(file.c_str());
  rmdir(child.c_str());
  rmdir(root.c_str());
  return failures != 0;
}
