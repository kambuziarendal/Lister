// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"
#include <Message.h>
namespace Lister {
// Local refs for Tracker interoperability, with a private identity snapshot.
BMessage MakeFileDrag(const std::vector<std::string> &paths);
// Captures identities at drop time for external refs, at drag time for Listi.
Job PrepareFileDrop(const BMessage &, const std::string &destination);
}
