// SPDX-License-Identifier: MIT
#include "FileContextMenu.h"
#include "Common.h"
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Screen.h>
#include <algorithm>
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
void FileActions(BMenu &menu, const ContextMenuOptions &options) {
  const bool single = options.selected == 1, directory = options.directory;
  const bool local = options.local, dual = options.dual;
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
  auto archives=new BMenu("Archives");
  auto pack=new BMenuItem("Create archive...",new BMessage(kCreateArchive));pack->SetEnabled(local);archives->AddItem(pack);
  auto extract=new BMenuItem("Extract to folder...",new BMessage(kExtractArchive));extract->SetEnabled(!local || (single&&!directory));archives->AddItem(extract);
  auto update=new BMenuItem("Add or replace files...",new BMessage(kAddArchive));update->SetEnabled(options.archiveAdd);archives->AddItem(update);
  auto remove=new BMenuItem("Remove selected entries...",new BMessage(kRemoveArchive));remove->SetEnabled(options.archiveRemove);archives->AddItem(remove);
  menu.AddItem(archives);
  menu.AddSeparatorItem();
  add("Properties...", kProperties, single && local);
}
constexpr uint32 kSelectFiles = 'cfil', kSelectFolder = 'cfld';
void ContextSelector(BMenu &menu, bool folder, bool hasFiles) {
  auto files = new BMenuItem("Selected files", new BMessage(kSelectFiles));
  files->SetMarked(!folder);
  files->SetEnabled(hasFiles);
  menu.AddItem(files);
  auto currentFolder = new BMenuItem("This folder", new BMessage(kSelectFolder));
  currentFolder->SetMarked(folder);
  menu.AddItem(currentFolder);
  menu.AddSeparatorItem();
}
} // namespace
ContextAction ShowContextMenu(BPoint point, const ContextMenuOptions &options) {
  BPopUpMenu files("File actions", false, false), folder("Folder actions", false, false);
  ContextSelector(files, false, options.selected != 0);
  ContextSelector(folder, true, options.selected != 0);
  FileActions(files, options);
  FolderActions(folder, options.folderLocal, options.sort, options.view);
  // Reserve an anchor that fits either menu. Switching context near an edge
  // must not move the selector under the pointer or lose the original owner.
  float fileWidth, fileHeight, folderWidth, folderHeight;
  files.GetPreferredSize(&fileWidth, &fileHeight);
  folder.GetPreferredSize(&folderWidth, &folderHeight);
  const BRect bounds = BScreen().Frame().InsetByCopy(8, 8);
  const float width = options.selected ? std::max(fileWidth, folderWidth) : folderWidth;
  const float height = options.selected ? std::max(fileHeight, folderHeight) : folderHeight;
  point.x = std::max(bounds.left, std::min(point.x, bounds.right - width));
  point.y = std::max(bounds.top, std::min(point.y, bounds.bottom - height));
  bool showingFolder = options.folder || !options.selected;
  for (;;) {
    const auto choice = Choose(showingFolder ? folder : files, point);
    if (choice.what == kSelectFiles && options.selected) showingFolder = false;
    else if (choice.what == kSelectFolder) showingFolder = true;
    else return choice;
  }
}
