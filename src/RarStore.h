// SPDX-License-Identifier: MIT
#pragma once

#include "FileOperation.h"

namespace Listi {
// A RAR5 decoder is needed to verify new archives and rebuild old ones.
bool RarStoreCanCreate();
// Writes a single-volume RAR5 archive, method 0 (Store), to a fresh file.
void RarStoreWrite(int output, const std::vector<std::string> &sources,
                   const Hooks &);
// Checks that a RAR5 archive can be rebuilt without discarding metadata
// that the Listi archive reader and staging tree cannot represent.
void RarStoreValidateEdit(int input, const Hooks & = {});
} // namespace Listi
