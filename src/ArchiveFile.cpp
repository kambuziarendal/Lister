// SPDX-License-Identifier: MIT
#include "DynamicLibrary.h"
#include "VirtualFile.h"
#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <map>
#include <unistd.h>

namespace Lister {
namespace {
DynamicLibrary &Library() {
  static DynamicLibrary lib({"libarchive.so", "libarchive.so.13"});
  return lib;
}
struct Reader {
  DynamicLibrary &lib = Library();
  struct archive *handle = nullptr;
  int fd = -1;
  const TransferControl &control;
  std::exception_ptr failure;
  char buffer[131072];
  explicit Reader(const std::string &path, const TransferControl &c)
      : control(c) {
    if (!lib.Available())
      throw std::runtime_error(
          "Install libarchive from HaikuDepot to browse archives");
    fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
      throw std::runtime_error(path + ": " + strerror(errno));
    handle = LIBCALL(lib, archive_read_new)();
    if (!handle) {
      close(fd);
      fd = -1;
      throw std::runtime_error("Could not allocate archive reader");
    }
    try {
      LIBCALL(lib, archive_read_support_filter_all)(handle);
      // Compressed single files must not be guessed as mtree (ordinary text
      // can look like an mtree manifest). Explicit tar containers still use
      // format detection; standalone compression exposes one decoded file.
      std::string lower = path;
      std::transform(lower.begin(), lower.end(), lower.begin(),
                     [](unsigned char ch) { return std::tolower(ch); });
      auto dot = lower.find_last_of('.');
      auto extension = dot == std::string::npos ? "" : lower.substr(dot);
      auto stem = dot == std::string::npos ? lower : lower.substr(0, dot);
      bool compressed = extension == ".gz" || extension == ".bz2" ||
                        extension == ".xz" || extension == ".zst" ||
                        extension == ".lz";
      bool tar = stem.size() >= 4 && stem.substr(stem.size() - 4) == ".tar";
      if (!compressed || tar)
        LIBCALL(lib, archive_read_support_format_all)(handle);
      LIBCALL(lib, archive_read_support_format_raw)(handle);
      int result = LIBCALL(lib, archive_read_open)(handle, this, nullptr, &Read,
                                                   nullptr);
      Rethrow();
      if (result != ARCHIVE_OK)
        Error();
    } catch (...) {
      LIBCALL(lib, archive_read_free)(handle);
      handle = nullptr;
      close(fd);
      fd = -1;
      throw;
    }
  }
  ~Reader() {
    if (handle)
      LIBCALL(lib, archive_read_free)(handle);
    if (fd >= 0)
      close(fd);
  }
  static la_ssize_t Read(struct archive *, void *data, const void **out) {
    auto &self = *static_cast<Reader *>(data);
    try {
      self.control.Check();
      *out = self.buffer;
      ssize_t n;
      do {
        n = read(self.fd, self.buffer, sizeof(self.buffer));
      } while (n < 0 && errno == EINTR);
      if (n < 0)
        throw std::runtime_error(std::string("Archive read failed: ") +
                                 strerror(errno));
      return n;
    } catch (...) {
      self.failure = std::current_exception();
      return -1;
    }
  }
  void Rethrow() {
    if (failure)
      std::rethrow_exception(failure);
  }
  [[noreturn]] void Error() {
    Rethrow();
    const char *s = LIBCALL(lib, archive_error_string)(handle);
    throw std::runtime_error(s ? s : "Unsupported or damaged archive");
  }
  struct archive_entry *Next() {
    control.Check();
    struct archive_entry *entry = nullptr;
    int r = LIBCALL(lib, archive_read_next_header)(handle, &entry);
    Rethrow();
    if (r == ARCHIVE_EOF)
      return nullptr;
    if (r != ARCHIVE_OK)
      Error();
    return entry;
  }
  bool CompressedOrArchive() const {
    return LIBCALL(lib, archive_format)(handle) != ARCHIVE_FORMAT_RAW ||
           LIBCALL(lib, archive_filter_code)(handle, 0) != ARCHIVE_FILTER_NONE;
  }
};
std::string SafeName(const char *raw) {
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
std::string EntryName(Reader &reader, struct archive_entry *entry,
                      const Location &location) {
  auto name = SafeName(LIBCALL(reader.lib, archive_entry_pathname)(entry));
  if (LIBCALL(reader.lib, archive_format)(reader.handle) ==
      ARCHIVE_FORMAT_RAW) {
    name = LocationLeaf(location.archive);
    auto dot = name.find_last_of('.');
    if (dot != std::string::npos)
      name.resize(dot);
    if (name.empty())
      name = "contents";
  }
  return name;
}
} // namespace
bool ArchiveAvailable() { return Library().Available(); }
std::string OpenArchive(const std::string &path, const TransferControl &c) {
  if (IsVirtual(path))
    return "";
  std::string lower = path;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  auto dot = lower.find_last_of('.');
  std::string ext = dot == std::string::npos ? "" : lower.substr(dot);
  bool expected = ext == ".zip" || ext == ".7z" || ext == ".rar" ||
                  ext == ".tar" || ext == ".tgz" || ext == ".gz" ||
                  ext == ".bz2" || ext == ".xz" || ext == ".zst" ||
                  ext == ".lz" || ext == ".lzh" || ext == ".lha" ||
                  ext == ".cpio" || ext == ".iso";
  if (!ArchiveAvailable()) {
    if (expected)
      throw std::runtime_error(
          "Install libarchive from HaikuDepot to browse this archive");
    return "";
  }
  try {
    Reader reader(path, c);
    reader.Next();
    if (!reader.CompressedOrArchive())
      return "";
    Location location;
    location.scheme = "archive";
    location.archive = path;
    location.path = "/";
    return location.URL();
  } catch (const Cancelled &) {
    throw;
  } catch (const std::exception &) {
    if (expected)
      throw;
    return "";
  }
}
std::vector<FileEntry> ListArchive(const Location &location,
                                   const TransferControl &c) {
  Reader reader(location.archive, c);
  std::map<std::string, FileEntry> children;
  std::string prefix =
      location.path == "/" ? "" : location.path.substr(1) + "/";
  size_t count = 0;
  while (auto entry = reader.Next()) {
    if (++count > 200000)
      throw std::runtime_error("Archive has too many entries for this view");
    auto name = EntryName(reader, entry, location);
    if (name.empty() || name.compare(0, prefix.size(), prefix) != 0)
      continue;
    auto rest = name.substr(prefix.size());
    if (rest.empty())
      continue;
    auto slash = rest.find('/');
    std::string leaf = rest.substr(0, slash);
    auto type = LIBCALL(reader.lib, archive_entry_filetype)(entry);
    auto &item = children[leaf];
    item.name = leaf;
    item.path = ChildLocation(location.URL(), leaf);
    bool implied = slash != std::string::npos;
    if (implied) {
      if (!item.directory)
        item.permissions = 0755;
      item.directory = true;
    } else if (type == AE_IFDIR) {
      item.directory = true;
      item.permissions = LIBCALL(reader.lib, archive_entry_perm)(entry) & 0777;
      item.modified = LIBCALL(reader.lib, archive_entry_mtime)(entry);
    } else {
      item.link = type == AE_IFLNK ||
                  LIBCALL(reader.lib, archive_entry_hardlink)(entry) != nullptr;
      item.size = LIBCALL(reader.lib, archive_entry_size)(entry);
      item.sizeKnown = LIBCALL(reader.lib, archive_entry_size_is_set)(entry);
      item.modified = LIBCALL(reader.lib, archive_entry_mtime)(entry);
      item.permissions = LIBCALL(reader.lib, archive_entry_perm)(entry) & 0777;
      if (type != AE_IFREG && !item.link)
        throw std::runtime_error("Archive contains a special device entry");
    }
  }
  std::vector<FileEntry> out;
  for (auto &pair : children)
    out.push_back(pair.second);
  return out;
}
void ReadArchive(const Location &location, const DataSink &sink,
                 const TransferControl &c) {
  Reader reader(location.archive, c);
  std::string wanted = location.path.substr(1);
  while (auto entry = reader.Next()) {
    if (EntryName(reader, entry, location) != wanted)
      continue;
    if (LIBCALL(reader.lib, archive_entry_filetype)(entry) != AE_IFREG ||
        LIBCALL(reader.lib, archive_entry_hardlink)(entry))
      throw std::runtime_error(
          "Archive links and special entries are not extracted");
    char data[131072];
    for (;;) {
      c.Check();
      la_ssize_t n = LIBCALL(reader.lib, archive_read_data)(reader.handle, data,
                                                            sizeof(data));
      reader.Rethrow();
      if (n < 0)
        reader.Error();
      if (!n)
        return;
      sink(data, n);
      c.Tick(n);
    }
  }
  throw std::runtime_error("Archive entry no longer exists");
}
} // namespace Lister
