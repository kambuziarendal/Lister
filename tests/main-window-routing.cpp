// SPDX-License-Identifier: MIT
// Exercise the actual window and menu without writing real user preferences.
#define main listi_application_entry
#include "../src/App.cpp"
#undef main
#include <List.h>
#include <cassert>
#include <cctype>
#include <fstream>
#include <unistd.h>

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
  // Exercise the installed common filter, not a standalone key mapping.
  Listi::WindowKeyFilter *keys = nullptr;
  auto *filters = window->CommonFilterList();
  for (int32 i = 0; filters && i < filters->CountItems(); ++i) {
    auto *filter = static_cast<BMessageFilter *>(filters->ItemAt(i));
    if ((keys = dynamic_cast<Listi::WindowKeyFilter *>(filter)))
      break;
  }
  assert(keys);
  auto key = [&](BView *focus, const char *bytes, int32 modifiers) {
    focus->MakeFocus();
    auto *targetView = window->CurrentFocus();
    auto *ancestor = targetView;
    while (ancestor && ancestor != focus)
      ancestor = ancestor->Parent();
    assert(ancestor == focus);
    BMessage message(B_KEY_DOWN);
    message.AddString("bytes", bytes);
    message.AddInt32("modifiers", modifiers);
    BHandler *target = targetView;
    return keys->Filter(&message, &target);
  };
  auto *files = window->FindView("files");
  auto *filter = dynamic_cast<BTextControl *>(window->FindView("filter"));
  assert(files && filter);
  assert(key(files, "d", B_COMMAND_KEY) == B_DISPATCH_MESSAGE);
  assert(key(filter->TextView(), "d", B_COMMAND_KEY) == B_SKIP_MESSAGE);
  assert(key(filter->TextView(), "f", B_COMMAND_KEY) == B_DISPATCH_MESSAGE);
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
  auto *left = dynamic_cast<Listi::PanelTools *>(window->FindView("first panel tools"));
  auto *right = dynamic_cast<Listi::PanelTools *>(window->FindView("second panel tools"));
  assert(left && right);
  using Page = Listi::PanelPage;
  auto *original = left->Page(Page::Files);
  auto *tabs = left->FindView("panel tabs");
  assert(tabs && tabs->IsHidden(tabs));
  auto select = [&](Page page) {
    BMessage change(Listi::kPanelPage);
    change.AddInt32("pane", 0);
    change.AddInt32("page", static_cast<int32>(page));
    window->MessageReceived(&change);
  };
  send(kSearch);
  auto *search = left->Page(Page::Search);
  assert(search && left->Current() == Page::Search && !tabs->IsHidden(tabs));
  auto *query = dynamic_cast<BTextControl *>(search->FindView("search expression"));
  assert(query);
  query->SetText("left-only");
  select(Page::Files);
  assert(left->Page(Page::Files) == original && left->Current() == Page::Files);
  send(kSearch);
  assert(left->Page(Page::Search) == search && left->Current() == Page::Search);
  assert(std::string(query->Text()) == "left-only");
  send(kSwitch);
  send(kSearch);
  auto *rightSearch = right->Page(Page::Search);
  assert(rightSearch && rightSearch != search);
  auto *rightQuery = dynamic_cast<BTextControl *>(rightSearch->FindView("search expression"));
  assert(rightQuery && std::string(rightQuery->Text()).empty());
  rightQuery->SetText("right-only");
  assert(std::string(query->Text()) == "left-only");
  send(kSwitch);
  BMessage close(Listi::kClosePanelPage);
  close.AddInt32("pane", 0);
  window->MessageReceived(&close);
  assert(!left->Page(Page::Search) && left->Current() == Page::Files);
  assert(tabs->IsHidden(tabs));
  send(kQuickView);
  assert(right->Current() == Page::Preview && left->Page(Page::Files) == original);
  send(kQuickView);
  assert(right->Current() == Page::Files && right->Page(Page::Search) == rightSearch);
  assert(std::string(rightQuery->Text()) == "right-only");
  // A tool may close immediately, before the result list's first draw.
  for (int i = 0; i < 12; ++i) {
    send(kSearch);
    window->MessageReceived(&close);
    assert(!left->Page(Page::Search));
  }
  // The Inspector's Edit action must not point at a selection on a hidden Files tab.
  char temp[] = "/tmp/Listi-inspector-routing-XXXXXX";
  assert(mkdtemp(temp));
  const std::string file = std::string(temp) + "/visible.txt";
  assert(std::ofstream(file) << "visible selection");
  auto *pane = static_cast<Pane *>(original);
  pane->Navigate(temp);
  window->Show();
  assert(window->Lock());
  auto waitFor = [&](auto condition) {
    for (int i = 0; i < 200 && !condition(); ++i) {
      window->Unlock();
      snooze(25000);
      assert(window->Lock());
    }
    assert(condition());
  };
  waitFor([&] { return !pane->IsScanning(); });
  assert(pane->MatchPattern("visible.txt", true) == 1);
  select(Page::Files);
  send(kPreviewOtherPanel);
  auto *inspect = right->Page(Page::Preview);
  assert(inspect && right->Current() == Page::Preview);
  send(kPreviewOtherPanel);
  assert(right->Page(Page::Preview) == inspect &&
         right->Current() == Page::Preview);
  auto *textPreview = inspect->FindView("preview scroll");
  assert(textPreview);
  waitFor([&] { return !textPreview->IsHidden(); });
  send(kSearch);
  assert(left->Current() == Page::Search && textPreview->IsHidden());
  select(Page::Files);
  waitFor([&] { return !textPreview->IsHidden(); });
  BMessage focusPreview(Listi::kPanelPage);
  focusPreview.AddInt32("pane", 1);
  focusPreview.AddInt32("page", static_cast<int32>(Page::Preview));
  window->MessageReceived(&focusPreview);
  send(kInlineCancel);
  assert(!right->Page(Page::Preview) && right->Current() == Page::Files);
  assert(pane->Selected() == std::vector<std::string>{file});

  select(Page::Files);
  workspace(Workspace::einn);
  waitFor([&] { return !pane->IsScanning(); });
  assert(pane->MatchPattern("visible.txt", true) == 1);
  send(kPreviewOtherPanel);
  assert(item(kQuickView)->IsMarked());
  send(kPreviewOtherPanel);
  assert(item(kQuickView)->IsMarked());
  send(kCloseQuickView);
  send(kCloseQuickView);
  assert(!item(kQuickView)->IsMarked());
  assert(pane->Selected() == std::vector<std::string>{file});
  assert(unlink(file.c_str()) == 0 && rmdir(temp) == 0);
  window->Quit();
  puts("Main window: menu shortcuts and workspace routing OK");
}
