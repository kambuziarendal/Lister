// SPDX-License-Identifier: MIT
#pragma once
#include "ThemeTextView.h"
#include <cstring>

// Use Haiku's height-for-width layout so messages can grow and shrink with
// their contents and the space available, like a multi-line label.
class WrappedLabel : public ThemeTextView {
public:
  WrappedLabel(const char *name, const char *text)
      : ThemeTextView(name, B_PANEL_BACKGROUND_COLOR, B_PANEL_TEXT_COLOR) {
    MakeEditable(false);
    MakeSelectable(false);
    SetFlags(Flags() & ~B_NAVIGABLE);
    SetWordWrap(true);
    SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
    SetMessage(text);
  }

  void SetMessage(const char *text) {
    if (std::strcmp(Text(), text) != 0) {
      SetText(text);
      InvalidateLayout();
    }
    SetToolTip(text);
  }
};
