// SPDX-License-Identifier: MIT
// Opt-in native audit. The destination is an explicitly selected test volume.
#include "FileOperation.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace Listi;

namespace {
struct Fixture {
  fs::path path;
  explicit Fixture(const fs::path &parent) {
    std::string pattern = (parent / "Listi-move-XXXXXX").string();
    if (!mkdtemp(pattern.data()))
      throw std::runtime_error("Cannot create test folder in " +
                               parent.string());
    path = fs::canonical(pattern);
  }
  ~Fixture() {
    std::error_code error;
    fs::remove_all(path, error);
    if (error)
      std::cerr << "Could not clean up " << path << ": " << error.message()
                << '\n';
  }
};

std::string Read(const fs::path &file) {
  std::ifstream stream(file, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}

void Write(const fs::path &file, const std::string &contents) {
  std::ofstream stream;
  stream.exceptions(std::ios::failbit | std::ios::badbit);
  stream.open(file, std::ios::binary);
  stream << contents;
  stream.close();
}
}

int main() try {
  const char *volume = std::getenv("LISTI_TEST_VOLUME");
  struct stat from, to;
  if (!volume || stat("/tmp", &from) || stat(volume, &to) ||
      !S_ISDIR(to.st_mode) || from.st_dev == to.st_dev)
    throw std::runtime_error(
        "LISTI_TEST_VOLUME must be a directory on a different volume from /tmp");
  Fixture source("/tmp"), dest(volume);
  int failures = 0;
  auto check = [&](bool ok, const char *label) {
    std::cout << (ok ? "PASS " : "FAIL ") << label << '\n';
    failures += !ok;
  };

  // Cancellation after removing the source cannot undo a committed move.
  for (bool batch : {false, true}) {
    auto first = source.path / (batch ? "batch" : "single");
    auto next = source.path / "next";
    auto moved = dest.path / first.filename();
    Write(first, "first");
    Job job{Action::Move, {first.string()}, dest.path.string()};
    if (batch) {
      Write(next, "next");
      job.sources.push_back(next.string());
    }
    Hooks hooks;
    hooks.proceed = [&] { return fs::exists(first); };
    int notifications = 0;
    hooks.fileDone = [&](const std::string &src, const std::string &dst) {
      if (src == first && dst == moved && !fs::exists(first))
        ++notifications;
    };
    auto result = Run(job, hooks);
    const std::vector<std::pair<std::string, std::string>> expected{
        {first.string(), moved.string()}};
    check(result.errors.empty() && result.cancelled == batch &&
              result.completed == 1 && notifications == 1 &&
              result.completedRoots == expected && !fs::exists(first) &&
              Read(moved) == "first" &&
              (!batch ||
               (Read(next) == "next" && !fs::exists(dest.path / "next"))),
          batch ? "count committed move and leave next source alone"
                : "count last move even when cancelled after removal");
  }

  auto large = source.path / "large";
  const std::string payload(512 * 1024, 'x');
  Write(large, payload);
  Fixture interrupted(volume);
  Hooks hooks;
  bool stop = false;
  hooks.proceed = [&] { return !stop; };
  hooks.progress = [&](const std::string &, uint64_t bytes) {
    stop |= bytes > 0;
  };
  auto result =
      Run({Action::Move, {large.string()}, interrupted.path.string()}, hooks);
  check(
      result.cancelled && result.errors.empty() && !result.completed &&
          Read(large) == payload && fs::is_empty(interrupted.path),
      "interrupt copying without exposing a partial destination or losing the source");

  hooks = {};
  hooks.proceed = [&] { return !fs::exists(dest.path / "large"); };
  result = Run({Action::Move, {large.string()}, dest.path.string()}, hooks);
  check(result.cancelled && !result.completed && result.errors.empty() &&
            result.completedRoots.empty() && Read(large) == payload &&
            Read(dest.path / "large") == payload,
        "cancel after copy but before removal retains both complete files");
  // Never fill a general-purpose volume. This case is explicitly enabled
  // for a small disposable filesystem, and writes only in the unique fixture.
  const char *diskFull = std::getenv("LISTI_TEST_DISK_FULL");
  if (diskFull && std::string(diskFull) == "1") {
    const auto space = fs::space(dest.path);
    if (space.capacity > 64 * 1024 * 1024)
      throw std::runtime_error(
          "Disk-full test requires a disposable volume of at most 64 MiB");
    Fixture full(volume);
    const auto oversized = source.path / "oversized";
    const auto original = full.path / "oversized";
    Write(original, "existing destination");
    const std::string contents(space.capacity + 1, 'z');
    Write(oversized, contents);
    hooks = {};
    hooks.conflict = [](const std::string &) { return Decision::Replace; };
    result =
        Run({Action::Move, {oversized.string()}, full.path.string()}, hooks);
    check(
        result.errors.size() == 1 &&
            result.errors.front().find(std::strerror(ENOSPC)) !=
                std::string::npos &&
            !result.cancelled && !result.completed &&
            result.completedRoots.empty() && Read(oversized) == contents &&
            Read(original) == "existing destination" &&
            std::distance(fs::directory_iterator(full.path),
                          fs::directory_iterator()) == 1,
        "full disk retains the source and existing destination without a staging file");
  }
  return failures ? 1 : 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 2;
}
