// SPDX-License-Identifier: MIT
#pragma once
#include <InterfaceDefs.h>
namespace Lister {
inline rgb_color PanelBlend(rgb_color base, rgb_color accent, float amount) {
  auto channel = [amount](uint8 a, uint8 b) {
    return uint8(a + (int(b) - int(a)) * amount + 0.5f);
  };
  return {channel(base.red, accent.red), channel(base.green, accent.green),
          channel(base.blue, accent.blue), 255};
}
inline rgb_color PanelBackground(color_which base, bool active) {
  return PanelBlend(ui_color(base), ui_color(B_KEYBOARD_NAVIGATION_COLOR),
                    active ? 0.035f : 0.f);
}
}
