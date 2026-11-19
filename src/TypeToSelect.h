// SPDX-License-Identifier: MIT
#pragma once

#include "PaneModel.h"
#include <InterfaceDefs.h>
#include <OS.h>
#include <UnicodeChar.h>
#include <View.h>
#include <Window.h>
#include <string>
#include <string_view>

namespace Listi {

inline uint32 TypeSelectModifiers(const BView &view) {
  const auto message = view.Window() ? view.Window()->CurrentMessage() : nullptr;
  return message ? message->GetInt32("modifiers", modifiers()) : modifiers();
}

// A fresh prefix starts at the top. Extending it retains the current match
// when possible. Repeating a single key cycles unless it extends a real name.
class TypeToSelect {
  static constexpr bigtime_t kTimeout = 1000000;
  std::u32string prefix;
  std::string location;
  bigtime_t lastKey = 0;

  static uint32 Next(std::string_view &text) {
    const unsigned char lead = text.front();
    const size_t size = lead < 0x80 ? 1 : lead >= 0xc2 && lead < 0xe0 ? 2
                                     : lead >= 0xe0 && lead < 0xf0 ? 3
                                     : lead >= 0xf0 && lead < 0xf5 ? 4 : 0;
    if (!size || size > text.size())
      return 0;
    for (size_t i = 1; i < size; ++i)
      if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80)
        return 0;
    const char *next = text.data();
    const uint32 value = BUnicodeChar::FromUTF8(&next);
    text.remove_prefix(size);
    return value;
  }

  bool Matches(std::string_view name) const {
    for (uint32 character : prefix) {
      if (name.empty() || BUnicodeChar::ToLower(Next(name)) != character)
        return false;
    }
    return true;
  }

public:
  void Reset() { prefix.clear(); }

  template <class EntryAt>
  bool KeyDown(std::string_view key, uint32 modifiers, bigtime_t now,
               const char *path, int count, const PaneEntry *focused,
               EntryAt entryAt, int &match) {
    match = -1;
    const uint32 character = key.empty() ? 0 : Next(key);
    if ((modifiers & (B_COMMAND_KEY | B_CONTROL_KEY | B_OPTION_KEY | B_MENU_KEY)) ||
        !key.empty() || character == B_SPACE ||
        !BUnicodeChar::IsPrintable(character)) {
      Reset();
      return false;
    }
    if (location != path) {
      location = path;
      Reset();
    }
    if (now - lastKey >= kTimeout)
      Reset();
    lastKey = now;

    const uint32 lower = BUnicodeChar::ToLower(character);
    const bool fresh = prefix.empty();
    const bool repeated = prefix.size() == 1 && prefix.front() == lower;
    int start = 0;
    if (!fresh && focused)
      for (int index = 0; index < count; ++index)
        if (entryAt(index) == focused) {
          start = index;
          break;
        }

    auto find = [&](int first) {
      for (int offset = 0; offset < count; ++offset) {
        const int index = (first + offset) % count;
        const auto entry = entryAt(index);
        if (entry && !entry->parent && Matches(entry->name))
          return index;
      }
      return -1;
    };
    prefix.push_back(lower);
    match = find(start);
    if (match < 0 && repeated) {
      prefix.pop_back();
      match = find(focused ? start + 1 : 0);
    }
    return true;
  }
};

} // namespace Listi
