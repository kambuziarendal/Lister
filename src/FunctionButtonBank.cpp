// SPDX-License-Identifier: MIT
#include "FunctionButtonBank.h"

#include "MainWindowMenu.h"
#include "WorkspaceActions.h"
#include <Button.h>
#include <GridLayout.h>
#include <GridView.h>
#include <GroupLayout.h>
#include <StringView.h>

namespace Listi {

FunctionButtonBank::FunctionButtonBank(MainWindowMenu &menu)
    : BGroupView(B_VERTICAL, 5), fMenu(menu) {
  SetName("workspace action buttons");
  auto shortcuts = new BStringView(
      "shortcuts",
      "Command + letter (normally Alt). Tab: other panel. Command+F: filter. "
      "C/X/V: clipboard. Shift+C: panel copy. Escape: close dialog.");
  shortcuts->SetTruncation(B_TRUNCATE_END);
  shortcuts->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
  shortcuts->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
  AddChild(shortcuts);
  SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
}

void FunctionButtonBank::SetActions(const std::vector<int32> &actions,
                                    bool compact) {
  if (fGrid && fCompact == compact && fActions == actions)
    return;
  fCompact = compact;
  fActions = actions;
  fMenu.ClearFunctionActions();
  if (fGrid) {
    GroupLayout()->RemoveView(fGrid);
    delete fGrid;
  }

  const int columns = compact ? 3 : 6;
  fGrid = new BGridView(compact ? 3 : 5, 5);
  int index = 0;
  for (auto id : fActions) {
    if (id < 0 || id >= WorkspaceLayout::ActionButtonCount)
      continue;
    const auto &action = kWorkspaceActions[id];
    fMenu.AddFunctionAction(action);
    BString label(action.label);
    if (!compact && action.message != kNone)
      label << " [" << action.shortcut << "]";
    auto button = new BButton(label, new BMessage(action.message));
    BString hint("Command+");
    hint << action.shortcut
         << " (normally Alt). File shortcuts apply outside text fields.";
    button->SetToolTip(hint);
    button->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    fGrid->GridLayout()->AddView(button, index % columns, index / columns);
    ++index;
  }
  GroupLayout()->AddView(int32(0), fGrid);
}

}
