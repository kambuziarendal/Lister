// SPDX-License-Identifier: MIT
#pragma once
#include "FileClipboard.h"

namespace Listi {
struct ClipboardTextFile {
  std::string folder, bytes, suggestedName;
  NodeIdentity destination;
};
bool CanSaveClipboardTextIn(const std::string &folder);
ClipboardTextFile PrepareClipboardTextFile(const std::string &folder,
    BClipboard *clipboard = be_clipboard);
void SaveClipboardTextFile(const ClipboardTextFile &, const std::string &name);
}
