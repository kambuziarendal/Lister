// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"
#include <Message.h>
namespace Listi {
// Freeze local entry identities before a question or queue delay. Remote
// providers retain their own capability/metadata checks, not local inode claims.
void FreezeLocalJob(Job &, const std::string &destinationFolder = "");
BMessage EncodeJob(const Job &, uint32 what);
Job DecodeJob(const BMessage &);
}
