// SPDX-License-Identifier: MIT
#include "FileContextMenu.h"
#include "Common.h"
#include <MenuItem.h>
#include <PopUpMenu.h>
namespace {
void FolderActions(BMenu &menu, bool local, PaneSort sort, int view) {
  auto add = [&](const char *label, uint32 what, bool enabled) {
    auto message = new BMessage(what);
    message->AddBool("folderContext", true);
    auto item = new BMenuItem(label, message);
    item->SetEnabled(enabled); menu.AddItem(item);
  };
  auto sorting = new BMenu("Sort by");
  auto sortItem = [&](const char *label, int column, bool ascending, bool marked) {
    auto message = new BMessage(kSort);
    message->AddBool("folderContext", true);
    message->AddInt32("value", column);
    message->AddBool("ascending", ascending);
    auto item = new BMenuItem(label, message);
    item->SetMarked(marked); sorting->AddItem(item);
  };
  const char *columns[] = {"Name", "Size", "Modified"};
  for (int i = 0; i < 3; ++i) sortItem(columns[i], i, sort.ascending, sort.column == i);
  sorting->AddSeparatorItem();
  sortItem("Ascending", sort.column, true, sort.ascending);
  sortItem("Descending", sort.column, false, !sort.ascending);
  menu.AddItem(sorting);
  auto views = new BMenu("View");
  const char *names[] = {"Details", "Icons", "Large icons", "Thumbnails"};
  for (int i = 0; i < 4; ++i) {
    auto message = new BMessage(kGridMode);
    message->AddBool("folderContext", true);
    message->AddInt32("value", i);
    auto item = new BMenuItem(names[i], message);
    item->SetMarked(view == i); views->AddItem(item);
  }
  menu.AddItem(views);
  menu.AddSeparatorItem();
  add("New folder...", kMkdir, local);
  add("Paste here", kClipboardPaste, local);
  menu.AddSeparatorItem();
  add("Refresh", kRefresh, true);
  add("Terminal here", kTerminal, local);
  menu.AddSeparatorItem();
  add("Properties...", kProperties, local);
}
ContextAction Choose(BPopUpMenu &menu, BPoint screen) {
  auto chosen = menu.Go(screen, false, true, false);
  if (!chosen || !chosen->Message()) return {};
  return {chosen->Message()->what, chosen->Message()->GetBool("folderContext", false),
    chosen->Message()->GetInt32("value", 0), chosen->Message()->GetBool("ascending", true)};
}
}
ContextAction ShowFolderContextMenu(BPoint screen, bool local, PaneSort sort, int view) {
  BPopUpMenu menu("This folder", false, false);
  FolderActions(menu, local, sort, view);
  return Choose(menu, screen);
}
ContextAction ShowFileContextMenu(BPoint screen, bool single, bool directory,
    bool local, bool dual, bool folderLocal, PaneSort sort, int view) {
  BPopUpMenu menu("File actions", false, false);
  auto add = [&](const char *label, uint32 action, bool enabled = true) {
    auto item = new BMenuItem(label, new BMessage(action));
    item->SetEnabled(enabled);
    menu.AddItem(item);
  };
  auto folder = new BMenu("This folder");
  FolderActions(*folder, folderLocal, sort, view);
  menu.AddItem(folder);
  menu.AddSeparatorItem();
  add("Open", kInvoke, single);
  if (!directory) {
    add("Open with...", kOpenWith, single && local);
    add("View", kView, single);
    add("Edit", kEdit, single && local);
  }
  menu.AddSeparatorItem();
  add("Copy", kClipboardCopy, local);
  add("Cut", kClipboardCut, local);
  add("Copy to other panel", kCopy, dual);
  add("Move to other panel", kMove, local && dual);
  menu.AddSeparatorItem();
  add("Rename...", kRename, single && local);
  add("Duplicate...", kDuplicate, single && local);
  add("Create symbolic link...", kCreateLink, single && local);
  add("Move to Trash...", kDelete, local);
  menu.AddSeparatorItem();
  add("Properties...", kProperties, single && local);
  return Choose(menu, screen);
}
