// SPDX-License-Identifier: MIT
#pragma once
#include <String.h>

namespace Lister {
// Literal name filtering, never shell expansion or a recursive search.
inline bool MatchesNameFilter(const char *name, const char *filter,
                              bool parent = false) {
  return parent || !filter[0] || BString(name).IFindFirst(filter) >= 0;
}
}
