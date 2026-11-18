// SPDX-License-Identifier: MIT
#pragma once

#include "FileOperation.h"

namespace Listi {
bool ArjCanCreate();
// Writes single-volume ARJ, method 0 (Store), to a fresh seekable file.
void ArjWrite(int output, const std::vector<std::string> &sources,
              const Hooks &hooks);
// Only the metadata subset emitted by ArjWrite can be safely rebuilt.
void ArjValidateEdit(int input);
} // namespace Listi
