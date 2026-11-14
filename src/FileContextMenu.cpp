// SPDX-License-Identifier: MIT
#include "FileContextMenu.h"
#include "Common.h"
#include <MenuItem.h>
#include <PopUpMenu.h>
uint32 ShowFileContextMenu(BPoint screen, bool single, bool directory, bool local, bool dual) {
  BPopUpMenu menu("File actions", false, false);
  auto add = [&](const char *label, uint32 action, bool enabled = true) {
    auto item = new BMenuItem(label, new BMessage(action));
    item->SetEnabled(enabled);
    menu.AddItem(item);
  };
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
  auto chosen = menu.Go(screen, false, true, false);
  return chosen && chosen->Message() ? chosen->Message()->what : 0;
}
