// SPDX-License-Identifier: MIT
#pragma once
#include "Shelf.h"
#include <sys/stat.h>

namespace Listi {
std::optional<ShelfProof> CaptureShelfProof(const std::string &path,
                                            const struct stat &entry);
ShelfState CheckShelfProof(const ShelfProof &, const struct stat &entry);
void ResolveShelfReferences(std::vector<ShelfEntry> &);
}
