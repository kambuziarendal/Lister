// SPDX-License-Identifier: MIT
#pragma once
#include <View.h>

namespace Listi {
// Haiku counts Hide()/Show() calls. Change only the view's own state,
// even when its parent or window is also hidden.
inline bool SetViewVisible(BView *view, bool visible) {
  const bool wasVisible = !view->IsHidden(view);
  if (wasVisible == visible)
    return false;
  if (visible)
    view->Show();
  else
    view->Hide();
  return true;
}
} // namespace Listi
