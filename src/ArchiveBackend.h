// SPDX-License-Identifier: MIT
#pragma once
#include "VirtualFile.h"
namespace Lister {
struct ArchiveBackend {
  const char *package;
  bool (*available)();
  std::string (*open)(const std::string &, const TransferControl &);
  std::vector<FileEntry> (*list)(const Location &, const TransferControl &);
  void (*read)(const Location &, const DataSink &, const TransferControl &);
};
std::string SafeArchivePath(const char *);
bool SevenZipAvailable();
std::string SevenZipFormat(const std::string &);
std::string SevenZipOpen(const std::string &, const TransferControl &);
std::vector<FileEntry> SevenZipList(const Location &, const TransferControl &);
void SevenZipRead(const Location &, const DataSink &, const TransferControl &);
bool ArchiveExtension(const std::string &);
bool LibarchiveAvailable();
std::string LibarchiveOpen(const std::string &, const TransferControl &);
std::vector<FileEntry> LibarchiveList(const Location &,
                                      const TransferControl &);
void LibarchiveRead(const Location &, const DataSink &,
                    const TransferControl &);
} // namespace Lister
