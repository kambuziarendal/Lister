// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"

namespace Listi {
// Writes an unsigned, single-volume Cabinet with uncompressed CFDATA blocks.
void CabWrite(int output, const std::vector<std::string> &sources,
              const Hooks &hooks);
// Refuses to rewrite Cabinet features that would be lost during an edit.
void CabValidateEdit(int input);
} // namespace Listi
