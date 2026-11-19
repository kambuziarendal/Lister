// SPDX-License-Identifier: MIT
// Opt-in native audit. The destination is an explicitly selected test volume.
#include "ArchiveJobs.h"
#include "FileClipboard.h"
#include "TransferCommand.h"
#include "VirtualFile.h"
#include <Application.h>
#include <Clipboard.h>
#include <algorithm>
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

bool Has(const Result &result, const fs::path &source, const fs::path &target,
         const char *status) {
  return std::any_of(result.outcomes.begin(), result.outcomes.end(),
                     [&](const EntryOutcome &row) {
    return row.source == source.string() && row.destination == target.string() &&
           row.status == status;
  });
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
  BApplication app("application/x-vnd.Listi-cross-volume-test");
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

  const auto item = source.path / "item";
  const auto destItem = dest.path / "item";
  Write(item, "original");
  auto refuses = [](const Job &job) {
    try {
      PrepareTransfer(job);
    } catch (const std::exception &error) {
      return std::string(error.what()).find("Move between volumes") != std::string::npos;
    }
    return false;
  };
  Job move{Action::Move, {item.string()}, dest.path.string()};
  auto result = Run(move, {});
  check(refuses(move) && !result.errors.empty() && !result.completed &&
            fs::is_empty(dest.path) && Read(item) == "original",
        "panel preparation and worker independently reject other-volume move");

  Fixture localTarget("/tmp");
  const auto local = localTarget.path / "first";
  const auto remote = dest.path / "second";
  Write(local, "local");
  Write(remote, "other volume");
  Job mixed{Action::Move, {local.string(), remote.string()}, source.path.string()};
  result = Run(mixed, {});
  check(refuses(mixed) && !result.completed && !result.errors.empty() &&
            !fs::exists(source.path / "first") && Read(local) == "local" &&
            Read(remote) == "other volume",
        "entire mixed selection is refused before moving the first entry");

  auto tree = source.path / "tree";
  fs::create_directory(tree);
  Write(tree / "child", "child");
  result = Run({Action::Move, {tree.string()}, dest.path.string()}, {});
  check(!result.completed && !result.errors.empty() && !fs::exists(dest.path / "tree") &&
            Read(tree / "child") == "child", "other-volume directory move is refused");

  auto archive = [&](const fs::path &directory) {
    auto j = PrepareArchiveJob(false, {item.string()});
    j.archiveFormat = "zip";
    SetArchiveDestination(j, directory.string(), "test.zip");
    auto create = Run(j, {});
    if (create.completed != 1 || !create.errors.empty())
      throw std::runtime_error("Cannot create archive fixture");
    return OpenArchive(j.destination);
  };
  const auto srcArchive = archive(source.path), dstArchive = archive(dest.path);
  const auto srcMember = ChildLocation(srcArchive, "item");
  const auto otherArchiveBefore = Read(dest.path / "test.zip");
  const auto srcArchiveBefore = Read(source.path / "test.zip");
  for (Job attempted : {Job{Action::Move, {item.string()}, dstArchive},
                        Job{Action::Move, {srcMember}, dest.path.string()},
                        Job{Action::Move, {srcMember}, dstArchive}}) {
    bool refused = refuses(attempted);
    attempted.action = Action::MoveArchive;
    auto worker = Run(attempted, {});
    check(refused && worker.completed == 0 && !worker.errors.empty() &&
              !fs::exists(destItem) && Read(item) == "original" &&
              Read(source.path / "test.zip") == srcArchiveBefore &&
              Read(dest.path / "test.zip") == otherArchiveBefore,
          "cross-volume archive move is refused by preparation and worker");
  }

  BClipboard clipboard("listi-volume-move-test", true);
  PutFileClipboard({item.string()}, true, &clipboard);
  bool cutRefused = false;
  try {
    PrepareFilePaste(dest.path.string(), &clipboard);
  } catch (const std::exception &error) {
    cutRefused = std::string(error.what()).find("Move between volumes") != std::string::npos;
  }
  auto retry = PrepareFilePaste(localTarget.path.string(), &clipboard);
  check(cutRefused && !retry.lease.empty() && retry.jobs.size() == 1 &&
            retry.jobs.front().action == Action::Move && !fs::exists(destItem),
        "rejected cut/paste does not consume or reserve its clipboard entries");
  FinishFilePaste(retry, {}, &clipboard);

  result = Run(PrepareTransfer({Action::Copy, {item.string()}, dest.path.string()}), {});
  check(result.completed == 1 && result.errors.empty() && Read(item) == Read(destItem),
        "explicit cross-volume Copy still writes the selected bytes and keeps the source");
  result = Run({Action::Move, {item.string()}, localTarget.path.string()}, {});
  check(result.completed == 1 && result.errors.empty() && !fs::exists(item) &&
            Read(localTarget.path / "item") == "original",
        "same-volume Move still transfers the original");

  // Same-volume folder merges still copy and clean up. Cancellation must
  // distinguish two intact trees from a partially removed source.
  auto merged = localTarget.path / "tree";
  fs::create_directory(merged);
  Write(tree / "z", "last");
  Hooks hooks;
  hooks.proceed = [&] { return !fs::exists(merged / "z") || !fs::exists(merged / "child"); };
  result = Run({Action::Move, {tree.string()}, localTarget.path.string()}, hooks);
  check(result.cancelled && !result.completed && Read(tree / "child") == "child" &&
            Read(tree / "z") == "last", "cancelled merge retains source files");
  hooks.conflict = [](const std::string &) { return Decision::Replace; };
  hooks.proceed = [&] { return fs::exists(tree / "z"); };
  result = Run({Action::Move, {tree.string()}, localTarget.path.string()}, hooks);
  check(result.cancelled && !result.completed && !fs::exists(tree / "z") &&
            Has(result, tree, merged, "partial") &&
            Read(tree / "child") == "child" && Read(merged / "child") == "child" &&
            Read(merged / "z") == "last", "late cancellation reports partial same-volume merge");

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
        Run({Action::Copy, {oversized.string()}, full.path.string()}, hooks);
    check(
        result.errors.size() == 1 &&
            result.errors.front().find(std::strerror(ENOSPC)) !=
                std::string::npos &&
            !result.cancelled && !result.completed &&
            result.completedRoots.empty() && !result.retainedMoveCopy &&
            Has(result, oversized, original, "failed") &&
            Read(oversized) == contents &&
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
