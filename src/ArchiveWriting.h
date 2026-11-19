// SPDX-License-Identifier: MIT
#pragma once
#include "ArchivePasswords.h"
#include <string>
#include <vector>

namespace Listi {
struct Hooks;
void CreateArchiveOutput(int output, const std::string &format,
                         const std::vector<std::string> &sources, int level,
                         bool encryptNames, const ArchiveSecretPtr &secret,
                         const Hooks &);
std::string DetectArchiveEditFormat(const std::string &path,
                                   const Hooks &);
}
