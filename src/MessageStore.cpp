// SPDX-License-Identifier: MIT
#include "MessageStore.h"
#include <DataIO.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
namespace Listi {
namespace {
constexpr size_t MaxFile = 1024 * 1024;
void Fail() { throw std::runtime_error(strerror(errno)); }
}
BMessage LoadMessageStore(const std::string &file) {
  int fd = open(file.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    if (errno == ENOENT) { BMessage absent; absent.what = 'none'; return absent; }
    Fail();
  }
  struct stat st;
  if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > off_t(MaxFile)) {
    close(fd); throw std::runtime_error("Invalid settings file.");
  }
  std::vector<char> bytes(st.st_size);
  size_t at = 0;
  while (at < bytes.size()) {
    ssize_t n = read(fd, bytes.data() + at, bytes.size() - at);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) { close(fd); throw std::runtime_error("Could not read settings."); }
    at += n;
  }
  close(fd);
  BMemoryIO input(bytes.data(), bytes.size()); BMessage message;
  if (message.Unflatten(&input) != B_OK)
    throw std::runtime_error("Could not decode settings.");
  if (message.what == 'none') throw std::runtime_error("Invalid settings message.");
  return message;
}
void SaveMessageStore(const std::string &file, const BMessage &message) {
  ssize_t size = message.FlattenedSize();
  if (size <= 0 || size > ssize_t(MaxFile)) throw std::runtime_error("Settings are too large.");
  std::vector<char> bytes(size);
  if (message.Flatten(bytes.data(), size) != B_OK) throw std::runtime_error("Could not encode settings.");
  std::string pattern = file + ".tmp.XXXXXX";
  std::vector<char> temp(pattern.begin(), pattern.end()); temp.push_back(0);
  int fd = mkstemp(temp.data());
  if (fd < 0) Fail();
  try {
    size_t at = 0;
    while (at < bytes.size()) {
      ssize_t n = write(fd, bytes.data() + at, bytes.size() - at);
      if (n < 0 && errno == EINTR) continue;
      if (n < 0) Fail();
      if (!n) throw std::runtime_error("Could not write settings.");
      at += n;
    }
    if (fsync(fd)) Fail();
    if (close(fd)) { fd = -1; Fail(); }
    fd = -1;
    if (rename(temp.data(), file.c_str())) Fail();
  } catch (...) {
    if (fd >= 0) close(fd);
    unlink(temp.data());
    throw;
  }
}
} // namespace Listi
