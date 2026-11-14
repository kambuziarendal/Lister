// SPDX-License-Identifier: MIT
#pragma once
#include <TextView.h>
// BTextView stores glyph colours independently of BView's HighColor.
// Refresh existing text runs as well as the background on a live theme change.
class ThemeTextView : public BTextView {
public:
  ThemeTextView(const char *name, color_which background, color_which foreground)
      : BTextView(name), foreground(foreground) {
    SetViewUIColor(background);
    SetLowUIColor(background);
    SetHighUIColor(foreground);
  }
  void AttachedToWindow() override {
    BTextView::AttachedToWindow();
    RefreshTextColor();
  }
  void MessageReceived(BMessage *message) override {
    BTextView::MessageReceived(message);
    if (message->what == B_COLORS_UPDATED) RefreshTextColor();
  }
private:
  color_which foreground;
  void RefreshTextColor() {
    rgb_color color = ui_color(foreground);
    SetFontAndColor(nullptr, B_FONT_ALL, &color);
    Invalidate();
  }
};
