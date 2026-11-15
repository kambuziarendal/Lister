// SPDX-License-Identifier: MIT
#include "FileContextMenu.h"
#include "Common.h"
#include <MenuItem.h>
#include <PopUpMenu.h>
namespace {
void FolderActions(BMenu &menu, bool local) {
  auto add = [&](const char *label, uint32 what, bool enabled) {
    auto message = new BMessage(what);
    message->AddBool("folderContext", true);
    auto item = new BMenuItem(label, message);
    item->SetEnabled(enabled); menu.AddItem(item);
  };
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
  return {chosen->Message()->what, chosen->Message()->GetBool("folderContext", false)};
}
}
ContextAction ShowFolderContextMenu(BPoint screen, bool local) {
  BPopUpMenu menu("This folder", false, false);
  FolderActions(menu, local);
  return Choose(menu, screen);
}
ContextAction ShowFileContextMenu(BPoint screen, bool single, bool directory,
    bool local, bool dual, bool folderLocal) {
  BPopUpMenu menu("File actions", false, false);
  auto add = [&](const char *label, uint32 action, bool enabled = true) {
    auto item = new BMenuItem(label, new BMessage(action));
    item->SetEnabled(enabled);
    menu.AddItem(item);
  };
  auto folder = new BMenu("This folder");
  FolderActions(*folder, folderLocal);
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
