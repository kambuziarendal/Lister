// SPDX-License-Identifier: MIT
#include "ArchiveBackend.h"
#include <algorithm>
#include <cctype>
#include <regex>

namespace Lister {
// Listi adapters are small routing code, not bundled compression engines.
// Additional system-backed adapters can be added here without changing panes
// or transactional extraction. Never fall back to launching an archive GUI.
namespace {
const ArchiveBackend backends[] = {
    {"libarchive", LibarchiveAvailable, LibarchiveOpen, LibarchiveList, LibarchiveRead}};
void RejectSplitArchive(const std::string &path) {
  static const std::regex split(R"((\.part[0-9]+\.rar|\.(7z|zip|tar)\.[0-9]{3}|\.[rz][0-9]{2})$)",
                                std::regex::icase);
  if (std::regex_search(path, split))
    throw std::runtime_error("Multipart archives are not supported yet. "
                            "Listi has not extracted an incomplete volume.");
}
const ArchiveBackend &Backend() {
  for (const auto &backend : backends)
    if (backend.available()) return backend;
  throw std::runtime_error("Install libarchive from HaikuDepot, then try again. "
                           "Archive browsing and extraction stay inside Listi.");
}
}
bool ArchiveExtension(const std::string &path) {
  auto dot = path.find_last_of('.');
  if (dot == std::string::npos) return false;
  std::string ext = path.substr(dot);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  for (const char *known : {".zip", ".zipx", ".7z", ".rar", ".tar", ".tgz",
       ".tbz", ".tbz2", ".txz", ".tlz", ".tzst", ".gz", ".bz2", ".xz",
       ".zst", ".lz", ".lzma", ".lz4", ".z", ".lzh", ".lha", ".cpio",
       ".iso", ".cab", ".ar", ".a", ".deb", ".rpm", ".xar", ".war",
       ".jar", ".epub", ".cbz", ".cbr", ".arj", ".ace", ".sit", ".sitx"})
    if (ext == known) return true;
  return false;
}
bool ArchiveAvailable() {
  for (const auto &backend : backends)
    if (backend.available()) return true;
  return false;
}
std::string OpenArchive(const std::string &path, const TransferControl &control) {
  if (IsVirtual(path)) return "";
  RejectSplitArchive(path);
  if (!ArchiveAvailable() && !ArchiveExtension(path)) return "";
  return Backend().open(path, control);
}
std::vector<FileEntry> ListArchive(const Location &location, const TransferControl &control) {
  RejectSplitArchive(location.archive);
  return Backend().list(location, control);
}
void ReadArchive(const Location &location, const DataSink &sink, const TransferControl &control) {
  RejectSplitArchive(location.archive);
  Backend().read(location, sink, control);
}
}
