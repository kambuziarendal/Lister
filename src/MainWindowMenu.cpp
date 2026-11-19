// SPDX-License-Identifier: MIT
#include "MainWindowMenu.h"
#include "Common.h"
#include "FlatViewWindow.h"
#include "MainWindow.h"
#include "ShelfWindow.h"
#include "WorkspaceActions.h"
#include "WorkspaceEditor.h"
#include <MenuItem.h>

MainWindowMenu::MainWindowMenu() : BMenuBar("menu") {
  AddFilesMenu();
  AddSelectMenu();
  AddViewMenu();
  AddListiMenu();
}

void MainWindowMenu::AddFilesMenu() {
  auto files = new BMenu("Files");
  struct Item {
    const char *label;
    uint32 action;
    char key;
  };
  files->AddItem(new BMenuItem("Copy", new BMessage(kClipboardCopy), 'C'));
  files->AddItem(new BMenuItem("Cut", new BMessage(kClipboardCut), 'X'));
  files->AddItem(new BMenuItem("Paste into active folder",
                               new BMessage(kClipboardPaste), 'V'));
  auto clipboard = new BMenu("Save clipboard as");
  saveClipboardTextItem = new BMenuItem("Save clipboard text as file...",
                                        new BMessage(kClipboardSaveText));
  clipboard->AddItem(saveClipboardTextItem);
  saveClipboardImageItem = new BMenuItem("Save clipboard image as PNG...",
                                         new BMessage(kClipboardSaveImage));
  clipboard->AddItem(saveClipboardImageItem);
  files->AddItem(clipboard);
  files->AddSeparatorItem();
  Item commands[] = {{"View", kView, 0},
                     {"Edit", kEdit, 'E'},
                     {"Edit in external editor", kEditExternally, 0},
                     {"Copy to other panel", kCopy, 0},
                     {"Move to other panel", kMove, 'M'},
                     {"Rename", kRename, 'R'},
                     {"Bulk rename...", kBulkRename, 0},
                     {"New folder", kMkdir, 'N'},
                     {"Move to Trash", kDelete, 'D'},
                     {"Restore from Trash", kRestore, 0},
                     {"Delete permanently...", kPermanentDelete, 0}};
  for (auto c : commands) {
    if (c.action == kCopy || c.action == kView)
      files->AddItem(new BMenuItem(c.label, new BMessage(c.action),
                                   c.action == kCopy ? 'C' : 'V',
                                   B_SHIFT_KEY));
    else
      files->AddItem(new BMenuItem(c.label, new BMessage(c.action), c.key));
  }
  undoBulkRenameItem = new BMenuItem("Undo last bulk rename", new BMessage(kUndoBulkRename));
  undoBulkRenameItem->SetEnabled(false);
  files->AddItem(undoBulkRenameItem);
  files->AddItem(new BMenuItem("Open with...", new BMessage(kOpenWith), 'O',
                               B_SHIFT_KEY));
  files->AddItem(new BMenuItem("Open with system application",
                               new BMessage(kOpenSystem)));
  files->AddItem(new BMenuItem(
      "Add to Shelf", new BMessage(Listi::kAddToShelf), 'Y', B_SHIFT_KEY));
  auto more = new BMenu("More file actions");
  more->AddItem(new BMenuItem("Duplicate...", new BMessage(kDuplicate), 'D',
                              B_SHIFT_KEY));
  more->AddItem(new BMenuItem("Create symbolic link...",
                              new BMessage(kCreateLink), 'L', B_SHIFT_KEY));
  files->AddItem(more);
  auto archives = new BMenu("Archives");
  archives->AddItem(new BMenuItem(
      "Create archive...", new BMessage(kCreateArchive), 'B', B_SHIFT_KEY));
  archives->AddItem(new BMenuItem("Extract to folder...",
                                  new BMessage(kExtractArchive), 'E',
                                  B_SHIFT_KEY));
  archives->AddItem(new BMenuItem("Resume archive edits...",
                                  new BMessage(kResumeArchiveEdits)));
  archives->AddItem(
      new BMenuItem("Add or replace files...", new BMessage(kAddArchive)));
  archives->AddItem(new BMenuItem("Remove selected entries...",
                                  new BMessage(kRemoveArchive)));
  archives->AddSeparatorItem();
  archives->AddItem(
      new BMenuItem("Forget archive passwords", new BMessage('apfg')));
  files->AddItem(archives);
  files->AddItem(new BMenuItem("Properties...", new BMessage(kProperties),
                               'I', B_SHIFT_KEY));
  files->AddSeparatorItem();
  files->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
  AddItem(files);
}

void MainWindowMenu::AddSelectMenu() {
  auto select = new BMenu("Select");
  select->AddItem(new BMenuItem("All", new BMessage(kAll), 'A'));
  select->AddItem(
      new BMenuItem("None", new BMessage(kNone), 'A', B_SHIFT_KEY));
  select->AddItem(new BMenuItem("Invert", new BMessage(kInvert), 'I'));
  select->AddItem(new BMenuItem("Same file type", new BMessage(kSelectType)));
  select->AddItem(new BMenuItem("By pattern…", new BMessage(kSelectPattern)));
  AddItem(select);
}

void MainWindowMenu::AddViewMenu() {
  auto viewMenu = new BMenu("View");
  auto workspace = new BMenu("Workspace");
  auto einnMessage = new BMessage(kWorkspace);
  einnMessage->AddInt32("workspace", int32(Workspace::einn));
  auto askrMessage = new BMessage(kWorkspace);
  askrMessage->AddInt32("workspace", int32(Workspace::askr));
  auto smidjaMessage = new BMessage(kWorkspace);
  smidjaMessage->AddInt32("workspace", int32(Workspace::smidja));
  askrMenu = new BMenuItem(WorkspaceMenuLabel(Workspace::askr), askrMessage);
  einnMenu = new BMenuItem(WorkspaceMenuLabel(Workspace::einn), einnMessage);
  tviskiptMenu = new BMenuItem(WorkspaceMenuLabel(Workspace::tviskipt),
                               new BMessage(kWorkspace));
  smidjaMenu =
      new BMenuItem(WorkspaceMenuLabel(Workspace::smidja), smidjaMessage);
  workspace->AddItem(einnMenu);
  workspace->AddItem(askrMenu);
  workspace->AddItem(tviskiptMenu);
  workspace->AddItem(smidjaMenu);
  workspace->AddSeparatorItem();
  workspace->AddItem(new BMenuItem("Customize workspace…",
                                    new BMessage(kOpenWorkspaceEditor)));
  viewMenu->AddItem(workspace);
  fileView = new BMenu("File view");
  for (int32 i = 0; i < ViewModeCount; ++i) {
    auto choice = new BMessage(kGridMode);
    choice->AddInt32("mode", i);
    fileView->AddItem(new BMenuItem(ViewModeLabel(i), choice));
  }
  auto resetView = new BMessage(kGridMode);
  resetView->AddInt32("mode", -1);
  fileView->AddSeparatorItem();
  fileView->AddItem(new BMenuItem("Use workspace default", resetView));
  fileView->AddSeparatorItem();
  flatViewItem =
      new BMenuItem("Flat View…", new BMessage(Listi::kFlatView));
  fileView->AddItem(flatViewItem);
  viewMenu->AddItem(fileView);
  bankToggle = new BMenuItem("Show function buttons", new BMessage(kToggleBank));
  viewMenu->AddItem(bankToggle);
  compactActions = new BMenu("Function actions");
  viewMenu->AddItem(compactActions);
  AddItem(viewMenu);
}

void MainWindowMenu::AddListiMenu() {
  auto view = new BMenu("Listi");
  view->AddItem(new BMenuItem("Switch panel", new BMessage(kSwitch), 'S'));
  view->AddItem(new BMenuItem("Same folder in other panel",
                              new BMessage(kSame), 'S', B_SHIFT_KEY));
  previewMenu =
      new BMenuItem("Inspector panel", new BMessage(kPreviewToggle), 'P');
  view->AddItem(previewMenu);
  quickViewMenu = new BMenuItem("Quick view", new BMessage(kQuickView), 'P', B_SHIFT_KEY);
  view->AddItem(quickViewMenu);
  view->AddItem(
      new BMenuItem("Connect to server...", new BMessage(kNetwork), 'K'));
  view->AddItem(new BMenuItem("Refresh", new BMessage(kRefresh), 'L'));
  view->AddItem(new BMenuItem("Filter names", new BMessage(kFilter), 'F'));
  view->AddItem(new BMenuItem("Find files...", new BMessage(kSearch), 'F',
                              B_SHIFT_KEY));
  view->AddItem(
      new BMenuItem("Compare folders...", new BMessage(kCompareFolders)));
  auto terminalMenu = new BMenu("Terminal here");
  terminalMenu->AddItem(
      new BMenuItem("Active lister", new BMessage(kTerminal), 'T'));
  terminalMenu->AddItem(
      new BMenuItem("Left CLI", new BMessage(Listi::kLeftCLI)));
  terminalMenu->AddItem(
      new BMenuItem("Right CLI", new BMessage(Listi::kRightCLI)));
  terminalMenu->AddSeparatorItem();
  auto dockMenu = new BMenu("Dock position");
  dockMenu->AddItem(new BMenuItem(
      "Below file panels", new BMessage(Listi::kTerminalBelow)));
  dockMenu->AddItem(new BMenuItem(
      "Right of file panels", new BMessage(Listi::kTerminalBeside)));
  dockMenu->AddItem(new BMenuItem(
      "Above file panels", new BMessage(Listi::kTerminalAbove)));
  dockMenu->AddItem(new BMenuItem(
      "Left of file panels", new BMessage(Listi::kTerminalLeft)));
  terminalMenu->AddItem(dockMenu);
  terminalMenu->AddItem(new BMenuItem(
      "Detached Listi CLI", new BMessage(Listi::kShowFloatingTerminal),
      'T', B_SHIFT_KEY));
  terminalMenu->AddItem(new BMenuItem(
      "External terminal window", new BMessage(Listi::kTerminalWindow)));
  view->AddItem(terminalMenu);
  view->AddItem(new BMenuItem("Operations", new BMessage(kShowJobs), 'O'));
  view->AddItem(new BMenuItem("Show Shelf", new BMessage(Listi::kShowShelf)));
  view->AddSeparatorItem();
  view->AddItem(new BMenuItem("Settings…", new BMessage(kPreferences), ','));
  AddItem(view);
}

void MainWindowMenu::SetWorkspace(const WorkspaceLayout &layout, bool quickView) {
  for (auto action : {kCopy, kMove, kSame})
    if (auto item = FindItem(action))
      item->SetEnabled(!layout.SinglePane());
  if (auto item = FindItem(kSwitch)) {
    const bool treeFocus = layout.workspace == Workspace::askr;
    item->SetLabel(treeFocus ? "Switch tree/list focus" : "Switch panel");
    item->SetEnabled(!quickView && (treeFocus || !layout.SinglePane()));
  }
  if (auto item = FindItem(kFilter))
    item->SetEnabled(!layout.IsEinn());
  einnMenu->SetMarked(layout.IsEinn());
  askrMenu->SetMarked(layout.workspace == Workspace::askr);
  tviskiptMenu->SetMarked(layout.workspace == Workspace::tviskipt);
  smidjaMenu->SetMarked(layout.IsSmidja());
}

void MainWindowMenu::SetPreview(const WorkspaceLayout &layout, bool showingQuickView) {
  quickViewMenu->SetMarked(showingQuickView);
  previewMenu->SetMarked(layout.Preview() && !layout.SinglePane());
  previewMenu->SetEnabled(!showingQuickView && !layout.SinglePane());
}

void MainWindowMenu::SetFileActions(int32 viewMode, bool canOpenFlatView,
                                    bool canSaveClipboard, bool canUndoBulkRename) {
  for (int32 i = 0; i < ViewModeCount; ++i)
    fileView->ItemAt(i)->SetMarked(i == viewMode);
  flatViewItem->SetEnabled(canOpenFlatView);
  saveClipboardTextItem->SetEnabled(canSaveClipboard);
  saveClipboardImageItem->SetEnabled(canSaveClipboard);
  undoBulkRenameItem->SetEnabled(canUndoBulkRename);
}

void MainWindowMenu::SetFunctionButtonState(bool available, bool requested, bool overflow) {
  bankToggle->SetEnabled(available);
  bankToggle->SetMarked(available && requested);
  compactActions->Superitem()->SetEnabled(overflow);
}

void MainWindowMenu::ClearFunctionActions() {
  while (auto *item = compactActions->RemoveItem(int32(0)))
    delete item;
}

void MainWindowMenu::AddFunctionAction(const WorkspaceAction &action) {
  compactActions->AddItem(new BMenuItem(action.description, new BMessage(action.message)));
}
