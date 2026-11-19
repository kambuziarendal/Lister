// SPDX-License-Identifier: MIT
#include "TypeToSelect.h"
#include <cassert>

int main() {
  PaneEntry parent, alpha, alpine, beta, island, aachen;
  parent.name = "..";
  parent.parent = true;
  alpha.name = "Alpha";
  alpine.name = "Alpine";
  beta.name = "Beta";
  island.name = "Øya";
  aachen.name = "Aachen";
  std::vector<const PaneEntry *> rows{&parent, &alpha, &alpine, &beta, &island};
  Listi::TypeToSelect select;
  bigtime_t now = 0;
  const char *path = "/fixture";
  const PaneEntry *focused = nullptr;
  auto type = [&](std::string_view key, uint32 modifiers = 0) {
    int match;
    now += 100000;
    const bool handled = select.KeyDown(
        key, modifiers, now, path, rows.size(), focused,
        [&](int index) { return rows[index]; }, match);
    if (!handled)
      return -2;
    if (match >= 0)
      focused = rows[match];
    return match;
  };

  assert(type(".") == -1); // Synthetic parent row is never picked.
  select.Reset();
  assert(type("a") == 1);
  assert(type("l") == 1);
  assert(type("p") == 1);
  assert(type("i") == 2);
  assert(type("z") == -1 && focused == &alpine);
  now += 1000000;
  assert(type("a") == 1);
  assert(type("A", B_SHIFT_KEY) == 2);
  assert(type("a") == 1); // Repeat wraps in visible order.

  rows.push_back(&aachen);
  assert(type("a") == 5); // A real repeated-letter prefix takes precedence.
  path = "/another-folder";
  assert(type("b") == 3); // Directory changes discard the previous prefix.
  assert(type("f", B_COMMAND_KEY) == -2);
  assert(type("ø") == 4);
  assert(type("y") == 4);
  assert(type(" ", B_SHIFT_KEY) == -2);
  assert(type(std::string_view("\x1c", 1)) == -2); // Left arrow.
  assert(type(std::string_view("\xc3", 1)) == -2); // Truncated UTF-8.
  assert(type("a", B_CONTROL_KEY) == -2);
  assert(type("a", B_OPTION_KEY) == -2);
  assert(type("B", B_SHIFT_KEY) == 3);
  rows.clear();
  assert(type("b") == -1);
}
