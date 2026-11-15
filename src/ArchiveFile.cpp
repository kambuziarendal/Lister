// SPDX-License-Identifier: MIT
#include "ArchiveBackend.h"
#include <algorithm>
#include <cctype>
#include <regex>

namespace Lister {
std::string SafeArchivePath(const char *raw) {
  if (!raw)
    throw std::runtime_error("Archive entry has no name");
  std::string name = raw;
  if (name.size() > 32768)
    throw std::runtime_error("Archive path is too long");
  if (!name.empty() && (name[0] == '/' || name[0] == '\\'))
    throw std::runtime_error("Archive contains an unsafe absolute path");
  if (name.find('\\') != std::string::npos ||
      name.find_first_of("\r\n") != std::string::npos)
    throw std::runtime_error("Archive contains an unsupported path");
  std::string out;
  size_t at = 0;
  while (at < name.size()) {
    auto end = name.find('/', at);
    if (end == std::string::npos)
      end = name.size();
    auto part = name.substr(at, end - at);
    if (part == ".." || (out.empty() && part.size() > 1 && part[1] == ':'))
      throw std::runtime_error(
          "Archive contains an unsafe parent or drive path");
    if (!part.empty() && part != ".") {
      if (!out.empty())
        out += '/';
      out += part;
    }
    at = end + 1;
  }
  return out;
}

// Listi adapters are small routing code, not bundled compression engines.
// Additional system-backed adapters can be added here without changing panes
// or transactional extraction. Never fall back to launching an archive GUI.
namespace {
const ArchiveBackend backends[] = {{"libarchive", LibarchiveAvailable,
                                    LibarchiveOpen, LibarchiveList,
                                    LibarchiveRead}};
void RejectSplitArchive(const std::string &path) {
  static const std::regex split(
      R"((\.part[0-9]+\.rar|\.(7z|zip|tar)\.[0-9]{3}|\.[rz][0-9]{2})$)",
      std::regex::icase);
  if (std::regex_search(path, split))
    throw std::runtime_error("Multipart archives are not supported yet. "
                             "Listi has not extracted an incomplete volume.");
}
const ArchiveBackend &Backend() {
  for (const auto &backend : backends)
    if (backend.available())
      return backend;
  throw std::runtime_error(
      "Install libarchive from HaikuDepot, then try again. "
      "Archive browsing and extraction stay inside Listi.");
}
} // namespace
bool ArchiveExtension(const std::string &path) {
  auto dot = path.find_last_of('.');
  if (dot == std::string::npos)
    return false;
  std::string ext = path.substr(dot);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  for (const char *known :
       {".zip",  ".zipx", ".7z",   ".rar", ".tar", ".tgz",  ".tbz", ".tbz2",
        ".txz",  ".tlz",  ".tzst", ".gz",  ".bz2", ".xz",   ".zst", ".lz",
        ".lzma", ".lz4",  ".z",    ".lzh", ".lha", ".cpio", ".iso", ".cab",
        ".ar",   ".a",    ".deb",  ".rpm", ".xar", ".war",  ".jar", ".epub",
        ".cbz",  ".cbr",  ".arj",  ".ace", ".sit", ".sitx"})
    if (ext == known)
      return true;
  return false;
}
bool ArchiveAvailable() {
  if (SevenZipAvailable())
    return true;
  for (const auto &backend : backends)
    if (backend.available())
      return true;
  return false;
}
std::string OpenArchive(const std::string &path,
                        const TransferControl &control) {
  if (IsVirtual(path))
    return "";
  RejectSplitArchive(path);
  if (!SevenZipFormat(path).empty() && SevenZipAvailable())
    return SevenZipOpen(path, control);
  if (!LibarchiveAvailable() && !ArchiveExtension(path))
    return "";
  return Backend().open(path, control);
}
std::vector<FileEntry> ListArchive(const Location &location,
                                   const TransferControl &control) {
  RejectSplitArchive(location.archive);
  if (!SevenZipFormat(location.archive).empty() && SevenZipAvailable())
    return SevenZipList(location, control);
  return Backend().list(location, control);
}
void ReadArchive(const Location &location, const DataSink &sink,
                 const TransferControl &control) {
  RejectSplitArchive(location.archive);
  if (!SevenZipFormat(location.archive).empty() && SevenZipAvailable())
    SevenZipRead(location, sink, control);
  else
    Backend().read(location, sink, control);
}
} // namespace Lister
