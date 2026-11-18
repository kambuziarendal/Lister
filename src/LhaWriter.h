// SPDX-License-Identifier: MIT
#pragma once

#include "FileOperation.h"

namespace Listi {
bool LhaCanCreate();
void LhaCreate(int outputFd, const std::vector<std::string> &sources, int level,
               const Hooks &);
} // namespace Listi
