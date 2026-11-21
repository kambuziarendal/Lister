// SPDX-License-Identifier: MIT
#pragma once
#include <TextControl.h>
#include <TextView.h>

namespace Listi {
// Use after the native input is attached, when opening a field for appending.
// BTextControl normally selects everything on keyboard/programmatic focus;
// a collapsed selection lets BTextView draw and blink its own caret instead.
inline void FocusTextAtEnd(BTextControl &control) {
  if (!control.IsEnabled())
    return;
  control.MakeFocus(true);
  auto *input = control.TextView();
  const int32 end = input->TextLength();
  input->Select(end, end);
  input->ScrollToSelection();
}
} // namespace Listi
