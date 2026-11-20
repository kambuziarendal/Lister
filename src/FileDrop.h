// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"
#include <Message.h>
namespace Listi {
// Local refs for Tracker interoperability, with a private identity snapshot.
BMessage MakeFileDrag(const std::vector<std::string> &paths,
                      const std::map<std::string, NodeIdentity> &expected = {});
// Captures identities at drop time for external refs, at drag time for Listi.
Job PrepareFileDrop(const BMessage &, const std::string &destination,
                    bool allowSameFolder = false);
bool CanMoveFileDrop(const Job &job);
}
