// SPDX-License-Identifier: MIT
#pragma once
#include "FileClipboard.h"

namespace Listi {
enum class ClipboardContent { Text, Image };
struct ClipboardFile {
  std::string folder, bytes, suggestedName;
  NodeIdentity destination;
};
bool CanSaveClipboardIn(const std::string &folder);
ClipboardFile PrepareClipboardFile(const std::string &folder,
                                   ClipboardContent content,
                                   BClipboard *clipboard = be_clipboard);
void SaveClipboardFile(const ClipboardFile &, const std::string &name);
}
