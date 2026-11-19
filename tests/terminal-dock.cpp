// SPDX-License-Identifier: MIT
#include "TerminalDock.h"

#include <Application.h>
#include <Button.h>
#include <Clipboard.h>
#include <Layout.h>
#include <SplitView.h>
#include <Window.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace Listi;
namespace fs = std::filesystem;
namespace {

struct Fixture {
  fs::path path;
  Fixture() {
    char directory[] = "/tmp/Listi-terminal-dock-XXXXXX";
    const char *created = mkdtemp(directory);
    if (!created)
      throw std::runtime_error("Could not create terminal fixture.");
    path = fs::canonical(created);
  }
  ~Fixture() {
    std::error_code error;
    fs::remove_all(path, error);
  }
};

class ClipboardFixture {
public:
  ClipboardFixture() {
    if (!be_clipboard->Lock())
      throw std::runtime_error("Could not lock test clipboard.");
    previous = *be_clipboard->Data();
    be_clipboard->Clear();
    // Ignore an overlong printable character while preserving valid UTF-8.
    constexpr char pasted[] = "cont" "\xe0\x81\x81" "inuéd";
    auto status = be_clipboard->Data()->AddData(
        "text/plain", B_MIME_TYPE, pasted, sizeof(pasted) - 1);
    if (status == B_OK)
      status = be_clipboard->Commit(true);
    sequence = be_clipboard->SystemCount();
    be_clipboard->Unlock();
    if (status != B_OK)
      throw std::runtime_error("Could not seed test clipboard.");
  }
  ~ClipboardFixture() {
    if (be_clipboard->Lock()) {
      if (be_clipboard->SystemCount() == sequence) {
        *be_clipboard->Data() = previous;
        be_clipboard->Commit(true);
      }
      be_clipboard->Unlock();
    }
  }

private:
  BMessage previous;
  uint32 sequence;
};

class Owner : public BWindow {
public:
  Owner() : BWindow(BRect(35, 55, 850, 620), "Terminal dock test",
                    B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
            split(new BSplitView(B_VERTICAL, 6)) {
    auto *workspace = new BGroupView(B_VERTICAL);
    workspace->SetName("file panels");
    dock = std::make_unique<TerminalDock>(split, workspace, BMessenger(this));
    AddChild(split);
    split->ResizeTo(Bounds().Width(), Bounds().Height());
  }

  TerminalPanel *Start(const fs::path &path, int lister = 0) {
    auto *panel = dock->Docked(lister);
    if (!panel || panel->Start(path.c_str()) != 0)
      return nullptr;
    dock->Show(lister);
    panel->Focus();
    return panel;
  }

  bool DockIs(orientation direction, int32 workspaceIndex) const {
    return split->Orientation() == direction &&
           split->GetLayout()->IndexOfView(FindView("file panels")) ==
               workspaceIndex;
  }

  bool StartFloating(const fs::path &path) {
    auto *panel = Start(path);
    return panel && dock->Float(panel->Placement());
  }

  void MessageReceived(BMessage *message) override {
    const int64 serial = message->GetInt64("terminal placement", -1);
    switch (message->what) {
    case kToggleTerminalSide:
      dock->Move(serial);
      return;
    case kFloatTerminal:
      dock->Float(serial);
      return;
    case kDockTerminal:
      dock->Dock(serial);
      return;
    case kCloseTerminal:
      dock->Close(serial);
      return;
    default:
      BWindow::MessageReceived(message);
    }
  }

private:
  BSplitView *split;
  std::unique_ptr<TerminalDock> dock;
};

void Check(bool success, const char *label) {
  if (!success)
    throw std::runtime_error(label);
  std::cout << "PASS " << label << std::endl;
}

template <typename CheckState> bool Until(CheckState ready) {
  for (int i = 0; i < 250; ++i) {
    if (ready())
      return true;
    snooze(20000);
  }
  return false;
}

template <typename ReadWindow>
bool WithWindow(const BMessenger &target, ReadWindow read) {
  if (target.LockTargetWithTimeout(20000) != B_OK)
    return false;
  BLooper *looper;
  target.Target(&looper);
  const bool result = read(*static_cast<BWindow *>(looper));
  looper->Unlock();
  return result;
}

BButton *Button(BView *view, const char *label) {
  if (auto *button = dynamic_cast<BButton *>(view);
      button && button->Label() && !std::strcmp(button->Label(), label))
    return button;
  for (int32 i = 0; i < view->CountChildren(); ++i)
    if (auto *button = Button(view->ChildAt(i), label))
      return button;
  return nullptr;
}

bool Click(const BMessenger &target, const char *label,
           BMessage *capture = nullptr, int lister = 0) {
  return WithWindow(target, [&](BWindow &window) {
    auto *panel = window.FindView(TerminalName(lister));
    auto *button = panel ? Button(panel, label) : nullptr;
    if (!button)
      return false;
    if (capture)
      *capture = *button->Message();
    return button->Invoke() == B_OK;
  });
}

BMessenger Floating(BApplication &app, int lister = 0) {
  for (int32 i = 0; i < app.CountWindows(); ++i) {
    auto *window = app.WindowAt(i);
    if (window && !std::strcmp(window->Title(), TerminalName(lister)))
      return BMessenger(window);
  }
  return {};
}

bool PanelMatches(const BMessenger &target, TerminalPanel *original,
                  int64 session, bool active, int lister = 0) {
  return WithWindow(target, [&](BWindow &window) {
    auto *panel =
        dynamic_cast<TerminalPanel *>(window.FindView(TerminalName(lister)));
    return panel && panel == original && panel->HasFocus() &&
           panel->SessionId() == session && panel->Active() == active;
  });
}

bool Key(const BMessenger &target, const char *bytes, uint32 what = B_KEY_DOWN,
         int32 modifiers = 0, int lister = 0) {
  BMessenger toScreen;
  WithWindow(target, [&](BWindow &window) {
    auto *panel = window.FindView(TerminalName(lister));
    if (auto *screen = panel ? panel->FindView("terminal screen") : nullptr)
      toScreen = BMessenger(screen);
    return toScreen.IsValid();
  });
  BMessage key(what);
  key.AddString("bytes", bytes);
  key.AddInt32("raw_char", bytes[0]);
  key.AddInt32("key", 0);
  key.AddInt32("modifiers", modifiers);
  return toScreen.SendMessage(&key) == B_OK;
}

bool Enter(const BMessenger &target, const char *command, int lister = 0) {
  return Key(target, command, B_KEY_DOWN, 0, lister) &&
         Key(target, "\n", B_KEY_DOWN, 0, lister);
}

std::string Read(const fs::path &path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), {}};
}

} // namespace

int main() {
  try {
    BApplication app("application/x-vnd.Listi-TerminalDockTest");
    Fixture fixture, rightFixture;
    ClipboardFixture clipboard;
    auto *owner = new Owner();
    BMessenger main(owner);
    owner->Show();
    TerminalPanel *panel = nullptr;
    Check(WithWindow(main, [&](BWindow &) {
            panel = owner->Start(fixture.path);
            return panel != nullptr;
          }),
          "start CLI in the file window");
    Check(Enter(main, "token=live; printf ready > ready; read answer; "
                      "printf '%s:%s' \"$token\" \"$answer\" > result") &&
              Until([&] { return Read(fixture.path / "ready") == "ready"; }),
          "start an interactive command that waits for input");

    TerminalPanel *right = nullptr;
    Check(WithWindow(main, [&](BWindow &) {
            right = owner->Start(rightFixture.path, 1);
            return right && right != panel && !panel->IsHidden() &&
                   !right->IsHidden();
          }) &&
              Enter(main, "token=right; printf '%s' \"$token\" > ready", 1) &&
              Until([&] { return Read(rightFixture.path / "ready") == "right"; }),
          "both listers can run separate shells in their own folders");
    Check(Click(main, "Move right", nullptr, 1) && Until([&] {
            return WithWindow(main, [&](BWindow &) {
              return owner->DockIs(B_HORIZONTAL, 0);
            });
          }) &&
              Click(main, "Move above", nullptr, 1) && Until([&] {
                return WithWindow(main, [&](BWindow &) {
                  return owner->DockIs(B_VERTICAL, 1);
                });
              }) &&
              Click(main, "Move left", nullptr, 1) && Until([&] {
                return WithWindow(main, [&](BWindow &) {
                  return owner->DockIs(B_HORIZONTAL, 1) && !panel->IsHidden() &&
                         !right->IsHidden();
                });
              }),
          "shared dock moves right, above and left without hiding either CLI");

    BMessage stale;
    Check(Click(main, "Detach CLI", &stale) && Until([&] {
            return PanelMatches(Floating(app), panel, 1, true);
          }),
          "Detach CLI moves the same live view and focuses it");
    auto detached = Floating(app);
    Check(Click(main, "Detach CLI", nullptr, 1) && Until([&] {
            return PanelMatches(Floating(app, 1), right, 1, true, 1);
          }),
          "both CLIs can detach into independent windows");
    auto rightDetached = Floating(app, 1);
    Check(main.SendMessage(&stale) == B_OK &&
              Key(detached, "v", B_KEY_DOWN, B_COMMAND_KEY) &&
              Key(detached, "\n") && Until([&] {
                return Read(fixture.path / "result") == "live:continuéd";
              }),
          "stale placement is ignored; detached clipboard completes the command");
    Check(Click(rightDetached, "Dock CLI", nullptr, 1) && Until([&] {
            return !rightDetached.IsValid() &&
                   PanelMatches(main, right, 1, true, 1);
          }),
          "right CLI docks without disturbing the detached left CLI");
    Check(Click(detached, "Dock CLI") && Until([&] {
            return !detached.IsValid() && PanelMatches(main, panel, 1, true);
          }),
          "Dock CLI returns focus to the same terminal and closes its window");
    Check(Enter(main, "printf '%s' \"$token\" > after") && Until([&] {
            return Read(fixture.path / "after") == "live";
          }),
          "shell variables and working directory survive both moves");

    Check(Click(main, "Detach CLI") && Until([&] {
            return PanelMatches(Floating(app), panel, 1, true);
          }),
          "can detach again");
    detached = Floating(app);
    Check(detached.SendMessage(B_QUIT_REQUESTED) == B_OK && Until([&] {
            return !detached.IsValid() && PanelMatches(main, panel, 1, true);
          }),
          "window close docks instead of stopping the shell");

    Check(Click(main, "Detach CLI") && Until([&] {
            return PanelMatches(Floating(app), panel, 1, true);
          }),
          "can detach after closing the floating window");
    detached = Floating(app);
    Check(Enter(detached, "exit") &&
              Until([&] { return PanelMatches(detached, panel, 1, false); }),
          "rebound polling notices PTY exit while detached");
    Check(Click(detached, "Close CLI") &&
              Until([&] { return !detached.IsValid(); }) &&
              Enter(main, "printf '%s' \"$token\" > after", 1) &&
              Until([&] { return Read(rightFixture.path / "after") == "right"; }),
          "closing the floating left CLI leaves the right shell running");
    Check(WithWindow(main, [&](BWindow &) {
            return owner->StartFloating(fixture.path);
          }) && Until([&] {
                return PanelMatches(Floating(app), panel, 2, true);
              }),
          "direct button starts a closed CLI already detached in a single dispatch");
    detached = Floating(app);
    Check(main.SendMessage(B_QUIT_REQUESTED) == B_OK &&
              Until([&] { return !main.IsValid() && !detached.IsValid(); }),
          "quitting the owner also closes its floating CLI");
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
  return 0;
}
