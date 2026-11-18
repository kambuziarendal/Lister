// SPDX-License-Identifier: MIT
#pragma once
#include <Clipboard.h>
#include <stdexcept>

namespace Listi {
struct ClipboardLock {
  BClipboard *clipboard;
  explicit ClipboardLock(BClipboard *c) : clipboard(c) {
    if (!c || !c->Lock())
      throw std::runtime_error("Could not lock the clipboard.");
  }
  ~ClipboardLock() {
    clipboard->Revert();
    clipboard->Unlock();
  }
  ClipboardLock(const ClipboardLock &) = delete;
  ClipboardLock &operator=(const ClipboardLock &) = delete;
};
}
