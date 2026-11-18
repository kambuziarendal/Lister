// SPDX-License-Identifier: MIT
#pragma once

#include "FileOperation.h"
#include "VirtualFile.h"

namespace Listi {
// Recognizes regular and thin Unix ar by signature, independent of extension.
void ClearArReadCache();
bool ArRecognized(const std::string &path);
std::string ArOpen(const std::string &path, const TransferControl &);
std::vector<FileEntry> ArList(const Location &, const TransferControl &);
void ArRead(const Location &, const DataSink &, const TransferControl &);

// Returns "" for non-ar, "ar" for GNU/SVR4 or empty archives,
// and "ar-bsd" for BSD-style headers. Refuses non-rebuildable ar containers.
std::string ArEditFormat(int inputFd, const std::string &path,
                         const Hooks & = {});
void ArCreate(int outputFd, const std::vector<std::string> &sources, bool bsd,
              const Hooks &);
} // namespace Listi
