// SPDX-License-Identifier: MIT
#pragma once
#include <string>
namespace Listi {
// Haiku backend. These functions perform I/O and must run off the UI thread.
std::string TrashDirectory(const std::string &path, bool create);
std::string MoveToTrash(const std::string &path);
std::string RestoreFromTrash(const std::string &path, std::string *warning = nullptr);
} // namespace Listi
