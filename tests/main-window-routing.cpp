// SPDX-License-Identifier: MIT
// Exercise the actual window and menu without writing real user preferences.
#define main listi_application_entry
#include "../src/App.cpp"
#undef main
#include <cassert>
#include <cctype>

int main() {
  BApplication app("application/x-vnd.Listi-MainWindowRoutingTest");
  auto *window = new MainWindow(Preferences{}, BMessenger{}, {}, false, {}, false);
  auto *menu = dynamic_cast<BMenuBar *>(window->FindView("menu"));
  assert(menu);
  auto item = [&](uint32 action) {
    auto *entry = menu->FindItem(action);
    assert(entry);
    return entry;
  };
  uint32 quickModifiers, archiveModifiers;
  const char quick = item(kQuickView)->Shortcut(&quickModifiers);
  const char archive = item(kCreateArchive)->Shortcut(&archiveModifiers);
  assert(std::toupper(quick) == 'P' && (quickModifiers & B_SHIFT_KEY));
  assert(std::toupper(archive) != 'P' || quickModifiers != archiveModifiers);

  auto send = [&](uint32 what) {
    BMessage message(what);
    window->MessageReceived(&message);
  };
  auto workspace = [&](Workspace value) {
    BMessage message(kWorkspace);
    message.AddInt32("workspace", static_cast<int32>(value));
    window->MessageReceived(&message);
  };
  assert(item(kSelectPattern)->IsEnabled());
  assert(item(kSwitch)->IsEnabled() && item(kCopy)->IsEnabled());
  send(kQuickView);
  assert(item(kQuickView)->IsMarked() && !item(kSwitch)->IsEnabled());
  workspace(Workspace::einn);
  assert(!item(kQuickView)->IsMarked() && !item(kSwitch)->IsEnabled());
  assert(!item(kCopy)->IsEnabled() && !item(kMove)->IsEnabled());
  workspace(Workspace::askr);
  assert(item(kSwitch)->IsEnabled() && !item(kSame)->IsEnabled());
  assert(std::string(item(kSwitch)->Label()) == "Switch tree/list focus");
  workspace(Workspace::smidja);
  assert(item(kSwitch)->IsEnabled() && item(kSame)->IsEnabled());
  assert(std::string(item(kSwitch)->Label()) == "Switch panel");
  window->Quit();
  puts("Main window: menu shortcuts and workspace routing OK");
}
