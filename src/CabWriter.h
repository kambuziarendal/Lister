// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"

namespace Listi {
// MSZIP uses the optional system zlib. Level zero always uses Store.
bool CabCanCompress();
void CabWrite(int output, const std::vector<std::string> &sources, int level,
              const Hooks &hooks);
inline void CabWrite(int output, const std::vector<std::string> &sources,
                     const Hooks &hooks) {
  CabWrite(output, sources, 0, hooks);
}
// Refuses to rewrite Cabinet features that would be lost during an edit.
void CabValidateEdit(int input);
} // namespace Listi
