// SPDX-License-Identifier: MIT
// Exercise the actual window and menu without writing real user preferences.
#include "../src/Common.h"
#include "../src/FunctionButtonBank.h"
#include "../src/MainWindow.h"
#include "../src/MainWindowMenu.h"
#include "../src/Pane.h"
#include "../src/PanelTools.h"
#include "../src/PaneSidebar.h"
#include "../src/SavedPlaces.h"
#include "../src/SavedSearches.h"
#include "../src/WindowKeyFilter.h"
#include <Application.h>
#include <Button.h>
#include <GridView.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>
#include <List.h>
#include <ListView.h>
#include <StringItem.h>
#include <cassert>
#include <cctype>
#include <fstream>
#include <unistd.h>

static void CheckFunctionButtons() {
  MainWindowMenu menu;
  Listi::FunctionButtonBank bank(menu);
  auto *fallbackItem = menu.FindItem("Function actions");
  assert(fallbackItem && fallbackItem->Submenu());
  auto *fallback = fallbackItem->Submenu();
  assert(bank.FindView("shortcuts"));

  auto grid = [&]() -> BGridView * {
    for (int32 i = 0; i < bank.CountChildren(); ++i)
      if (auto *view = dynamic_cast<BGridView *>(bank.ChildAt(i)))
        return view;
    return nullptr;
  };
  auto button = [&](int32 index, uint32 action, const char *label) {
    assert(grid());
    auto *control = dynamic_cast<BButton *>(grid()->ChildAt(index));
    assert(control && std::string(control->Label()) == label);
    assert(control->Message()->what == action);
    assert(fallback->ItemAt(index)->Message()->what == action);
  };

  const std::vector<int32> actions = {2, -1, 7, 99, 1};
  bank.SetActions(actions, false);
  assert(!bank.IsCompact());
  assert(grid() && grid()->CountChildren() == 3 && fallback->CountItems() == 3);
  button(0, kCopy, "Copy [Shift+C]");
  button(1, kNone, "None");
  button(2, kEdit, "Edit [E]");
  assert(std::string(fallback->ItemAt(0)->Label()) == "Copy to other panel");

  bank.SetActions(actions, true);
  assert(bank.IsCompact());
  button(0, kCopy, "Copy");
  button(1, kNone, "None");
  button(2, kEdit, "Edit");
  bank.SetActions({}, true);
  assert(grid() && grid()->CountChildren() == 0 && fallback->CountItems() == 0);
}

int main() {
  BApplication app("application/x-vnd.Listi-MainWindowRoutingTest");
  CheckFunctionButtons();
  auto *window = CreateMainWindow(Preferences{}, BMessenger{}, {}, false, {}, false);
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
  auto *leftPane = static_cast<Pane *>(original);
  auto *rightPane = static_cast<Pane *>(right->Page(Page::Files));
  const SidebarMode originalRight = rightPane->Sidebar();
  auto changeSidebar = [&](SidebarMode mode) {
    BMessage change(kSidebarMode);
    change.AddInt32("mode", static_cast<int32>(mode));
    leftPane->MessageReceived(&change);
  };
  Listi::SavedPlaces places;
  places.Add("Home shortcut", "/boot/home");
  leftPane->SetPlaces(places, true);
  changeSidebar(SidebarMode::Favorites);
  auto *sidebar = leftPane->FindView("navigation sidebar");
  auto *favorites = dynamic_cast<BListView *>(leftPane->FindView("favorite destinations"));
  assert(sidebar && favorites && !favorites->IsHidden(sidebar));
  auto *shortcut = dynamic_cast<BStringItem *>(favorites->ItemAt(0));
  assert(shortcut && std::string(shortcut->Text()) == "Home shortcut");
  changeSidebar(SidebarMode::Shelf);
  auto *shelf = leftPane->FindView("sidebar shelf");
  assert(shelf && !shelf->IsHidden(sidebar) && favorites->IsHidden(sidebar));
  changeSidebar(SidebarMode::Hidden);
  assert(sidebar->IsHidden(sidebar) && leftPane->SidebarCanBeRestored());
  BMessage restore(kRestoreSidebar);
  leftPane->MessageReceived(&restore);
  assert(leftPane->Sidebar() == SidebarMode::Shelf && !shelf->IsHidden(sidebar));
  assert(rightPane->Sidebar() == originalRight);
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
  // Haiku can resolve /tmp through an alias when loading the folder.
  const auto selected = pane->Selected();
  assert(selected == std::vector<std::string>{
                         std::string(pane->Path()) + "/visible.txt"});
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
  auto verifyClose = [&] {
    assert(!right->Page(Page::Preview) && right->Current() == Page::Files);
    assert(pane->Selected() == selected);
    auto *focus = window->CurrentFocus();
    while (focus && focus != pane)
      focus = focus->Parent();
    assert(focus == pane);
  };
  for (uint32 closeCommand : {uint32(kInlineCancel), kQuickView}) {
    window->MessageReceived(&focusPreview);
    send(closeCommand);
    verifyClose();
    send(kPreviewOtherPanel);
    assert(right->Current() == Page::Preview);
  }
  BMessage closePreview(Listi::kClosePanelPage);
  closePreview.AddInt32("pane", 1);
  window->MessageReceived(&focusPreview);
  window->MessageReceived(&closePreview);
  verifyClose();

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
  assert(pane->Selected() == selected);
  assert(unlink(file.c_str()) == 0 && rmdir(temp) == 0);
  window->Quit();
  puts("Main window: menu shortcuts and workspace routing OK");
}
