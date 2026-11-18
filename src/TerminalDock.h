// SPDX-License-Identifier: MIT
#pragma once

#include "TerminalPanel.h"

class BSplitView;

namespace Listi {

// Called only with the file window locked. The detached window never locks
// the file window; its controls send requests back to this owner.
class TerminalDock {
public:
  TerminalDock(BSplitView *split, BMessenger owner);
  ~TerminalDock();

  TerminalPanel *Docked();
  bool FocusFloating();
  bool Float() { return Float(placement); }
  bool Float(int64 placement);
  bool Dock(int64 placement);
  bool Close(int64 placement);

private:
  bool ReturnPanel();

  BSplitView *split;
  BMessenger owner;
  BGroupView *slot;
  TerminalPanel *panel = nullptr;
  BMessenger detached;
  int64 placement = 0;
};

} // namespace Listi
