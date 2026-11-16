// SPDX-License-Identifier: MIT
#pragma once
#include <Message.h>
#include <string>
namespace Listi {
BMessage LoadArchiveSettings(const std::string &path);
void SaveArchiveSettings(const std::string &path,const BMessage &settings);
}
