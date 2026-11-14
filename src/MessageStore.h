// SPDX-License-Identifier: MIT
#pragma once
#include <Message.h>
#include <string>
namespace Lister {
// Bounded regular-file read. Missing file returns an empty message.
BMessage LoadMessageStore(const std::string &file);
// Atomic replacement: failed writes leave the previous file untouched.
void SaveMessageStore(const std::string &file, const BMessage &message);
}
