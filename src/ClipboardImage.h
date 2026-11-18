// SPDX-License-Identifier: MIT
#pragma once
#include <Clipboard.h>
#include <string>

namespace Listi {
// Snapshot image content, not file references, and release the clipboard before encoding.
std::string ReadClipboardPNG(BClipboard *clipboard);
}
