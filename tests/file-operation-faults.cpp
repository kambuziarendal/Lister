// SPDX-License-Identifier: MIT
// Deterministic native syscall faults, including when the test user is root.
#include "FileOperation.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

using namespace Listi;
namespace fs = std::filesystem;
namespace {
std::string deniedStage, watchedStage, deniedRemoval;
int watchedFd = -1, writes = 0;
bool failWrite = false;
int failures = 0;

void Check(bool ok, const char *label) {
  std::cout << (ok ? "PASS " : "FAIL ") << label << '\n';
  failures += !ok;
}
std::string Read(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
bool Has(const Result &result, const fs::path &source, const fs::path &dest,
         const char *status, const char *detail = "") {
  return std::any_of(result.outcomes.begin(), result.outcomes.end(),
                     [&](const EntryOutcome &entry) {
    return entry.source == source.string() && entry.destination == dest.string() &&
           entry.status == status && entry.detail.find(detail) != std::string::npos;
  });
}
}

extern "C" int __real_mkstemp(char *);
extern "C" int __wrap_mkstemp(char *path) {
  if (deniedStage == path) {
    errno = EACCES;
    return -1;
  }
  const bool watch = watchedStage == path;
  const int fd = __real_mkstemp(path);
  if (watch)
    watchedFd = fd;
  return fd;
}
extern "C" ssize_t __real_write(int, const void *, size_t);
extern "C" ssize_t __wrap_write(int fd, const void *data, size_t size) {
  if (fd != watchedFd || !size)
    return __real_write(fd, data, size);
  ++writes;
  if (writes == 1) {
    errno = EINTR;
    return -1;
  }
  if (writes == 2)
    return __real_write(fd, data, std::min<size_t>(size, 7));
  watchedFd = -1;
  if (failWrite) {
    errno = EIO;
    return -1;
  }
  return __real_write(fd, data, size);
}
extern "C" int __real_unlink(const char *);
extern "C" int __wrap_unlink(const char *path) {
  if (deniedRemoval == path) {
    errno = EACCES;
    return -1;
  }
  return __real_unlink(path);
}

int main() {
  char temp[] = "/tmp/Listi-operation-faults-XXXXXX";
  if (!mkdtemp(temp))
    return 2;
  const auto root = fs::canonical(temp);
  const auto out = root / "out", source = root / "file", target = out / "file";
  fs::create_directory(out);
  const std::string payload(1000, 'x');
  std::ofstream(source) << payload;
  std::ofstream(target) << "original";
  Hooks replace;
  replace.conflict = [](const std::string &) { return Decision::Replace; };
  Job move{Action::Move, {source.string()}, out.string()};

  deniedStage = target.string() + ".listi-XXXXXX";
  auto result = Run(move, replace);
  Check(result.errors.size() == 1 && result.completed == 0 &&
            Has(result, source, target, "failed", strerror(EACCES)) &&
            Read(source) == payload && Read(target) == "original",
        "destination permission failure names both paths and preserves both files");
  deniedStage.clear();

  watchedStage = target.string() + ".listi-XXXXXX";
  failWrite = true;
  result = Run(move, replace);
  Check(writes == 3 && result.errors.size() == 1 &&
            Has(result, source, target, "failed", strerror(EIO)) &&
            Read(source) == payload && Read(target) == "original" &&
            std::distance(fs::directory_iterator(out), fs::directory_iterator()) == 1,
        "write failure after partial output discards staging, not the original");

  failWrite = false;
  writes = 0;
  deniedRemoval = source.string();
  result = Run(move, replace);
  Check(writes == 3 && result.errors.size() == 1 && !result.completed &&
            Has(result, source, target, "source-retained", strerror(EACCES)) &&
            Read(source) == payload && Read(target) == payload,
        "retry EINTR and short write; report copied file and denied source cleanup");
  deniedRemoval.clear();
  watchedStage.clear();

  const auto other = root / "second", untouched = root / "third";
  std::ofstream(other) << "second";
  std::ofstream(untouched) << "third";
  std::ofstream(out / "second") << "existing";
  move.sources = {source.string(), other.string(), untouched.string()};
  Hooks conflicts;
  conflicts.conflict = [&](const std::string &path) {
    return path == target.string() ? Decision::Skip : Decision::Abort;
  };
  result = Run(move, conflicts);
  Check(result.cancelled && result.skipped == 1 && !result.completed &&
            Has(result, source, target, "skipped") &&
            Has(result, other, out / "second", "unprocessed") &&
            Has(result, untouched, out / "third", "unprocessed") &&
            Read(out / "second") == "existing" && fs::exists(untouched),
        "skipped, aborted and unstarted roots have individually identifiable outcomes");
  fs::remove_all(root);
  return failures ? 1 : 0;
}
