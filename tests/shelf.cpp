// SPDX-License-Identifier: MIT
#include "ArchiveJobs.h"
#include "JobSnapshot.h"
#include "Shelf.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
struct Fixture {
  fs::path root;
  Fixture() {
    char path[] = "/tmp/Listi-shelf-XXXXXX";
    const char *created = mkdtemp(path);
    if (!created)
      throw std::runtime_error("Could not create test fixture");
    root = fs::canonical(created);
  }
  ~Fixture() {
    std::error_code error;
    fs::remove_all(root, error);
  }
};

void Check(bool success, const char *name) {
  if (!success)
    throw std::runtime_error(name);
  std::cout << "PASS " << name << '\n';
}

std::string Read(const fs::path &path) {
  std::ifstream stream(path);
  return std::string(std::istreambuf_iterator<char>(stream), {});
}

template <typename Action>
void Refused(Action action, const char *reason, const char *name) {
  try {
    action();
  } catch (const std::runtime_error &e) {
    Check(std::string(e.what()).find(reason) != std::string::npos, name);
    return;
  }
  Check(false, name);
}
} // namespace

int main() {
  try {
    Fixture fixture;
    const auto a = fixture.root / "a", b = fixture.root / "b",
               target = fixture.root / "target", alias = fixture.root / "alias";
    fs::create_directory(a);
    fs::create_directory(b);
    fs::create_directory(target);
    fs::create_directory_symlink(a, alias);
    auto first = (a / "first").string(), second = (b / "second").string(),
         collision = (b / "first").string(), link = (a / "link").string();
    std::ofstream(first) << "original";
    std::ofstream(second) << "second";
    std::ofstream(collision) << "collision";
    fs::create_symlink(first, link);

    Listi::Shelf shelf;
    Check(shelf.Add({first, second}) == 2, "collect from two source folders");
    auto ready = shelf.Snapshot();
    Check(shelf.Add({(alias / "first").string(), first}) == 0 &&
              shelf.Snapshot().revision == ready.revision,
          "duplicate refs and parent aliases do not refresh the snapshot");
    Check(shelf.Add({link}) == 1 &&
              shelf.Snapshot().entries.back().path == link,
          "collect a symlink without following the selected entry");
    shelf.Remove({link});
    Check(fs::is_symlink(link), "removing a shelf row does not touch the link");

    shelf.Add({collision});
    Refused(
        [&] {
          Listi::PrepareShelfTransfer(shelf.Snapshot(), Listi::Action::Move,
                                      target.string());
        },
        "same name", "same-name entries cannot overwrite each other");
    Refused([&] { Listi::PrepareShelfArchive(shelf.Snapshot()); }, "same name",
            "ambiguous ZIP root names are refused");
    shelf.Remove({collision});
    Check(fs::exists(collision),
          "removing a shelf row leaves its original file");

    shelf.Add({a.string()});
    Refused(
        [&] {
          Listi::PrepareShelfTransfer(shelf.Snapshot(), Listi::Action::Move,
                                      target.string());
        },
        "its contents", "refuse a folder and one of its descendants");
    shelf.Remove({a.string()});
    Refused(
        [&] {
          Listi::PrepareShelfTransfer(shelf.Snapshot(), Listi::Action::Move,
                                      a.string());
        },
        "already in", "refuse a move back into an original folder");

    const auto copyFolder = fixture.root / "copies";
    fs::create_directory(copyFolder);
    const auto copySnapshot = shelf.Snapshot();
    auto copy = Listi::PrepareShelfTransfer(copySnapshot, Listi::Action::Copy,
                                            copyFolder.string());
    const auto submitted =
        Listi::DecodeSubmittedJob(Listi::EncodeJob(copy, 'test'),
                                  {Listi::Action::Copy, Listi::Action::Move});
    Check(submitted.has_value(),
          "Shelf copy uses the guarded transfer message");
    const auto copied = Listi::Run(*submitted, {});
    Check(
        copied.completed == 2 && copied.errors.empty() &&
            Read(copyFolder / "first") == "original" &&
            Read(copyFolder / "second") == "second" &&
            Read(first) == "original" && Read(second) == "second" &&
            shelf.Snapshot().revision == copySnapshot.revision,
        "copy entries from different folders without changing originals or Shelf");

    const auto zipSnapshot = shelf.Snapshot();
    auto zip = Listi::PrepareShelfArchive(zipSnapshot);
    Listi::SetArchiveDestination(zip, target.string(), "shelf.zip");
    zip = Listi::DecodeJob(Listi::EncodeJob(zip, 'test'));
    auto packed = Listi::Run(zip, {});
    Check(packed.completed == 1 && packed.errors.empty() &&
              zip.archiveFormat == "zip" && Read(first) == "original" &&
              Read(second) == "second" &&
              shelf.Snapshot().revision == zipSnapshot.revision,
          "create a ZIP without modifying originals or Shelf");
    auto unpack = Listi::PrepareArchiveJob(true, {zip.destination});
    Listi::SetArchiveDestination(unpack, target.string(), "unpacked");
    auto extracted = Listi::Run(unpack, {});
    Check(extracted.completed == 1 && extracted.errors.empty() &&
              Read(target / "unpacked" / "first") == "original" &&
              Read(target / "unpacked" / "second") == "second",
          "round trip ZIP members collected from different folders");
    auto tar = Listi::PrepareShelfArchive(shelf.Snapshot());
    tar.archiveFormat = "tar";
    Listi::SetArchiveDestination(tar, target.string(), "shelf.tar");
    tar = Listi::DecodeJob(Listi::EncodeJob(tar, 'test'));
    const auto packedTar = Listi::Run(tar, {});
    auto unpackTar = Listi::PrepareArchiveJob(true, {tar.destination});
    Listi::SetArchiveDestination(unpackTar, target.string(), "unpacked-tar");
    const auto extractedTar = Listi::Run(unpackTar, {});
    Check(
        packedTar.completed == 1 && packedTar.errors.empty() &&
            extractedTar.completed == 1 && extractedTar.errors.empty() &&
            Read(target / "unpacked-tar" / "first") == "original" &&
            Read(target / "unpacked-tar" / "second") == "second" &&
            shelf.Snapshot().revision == zipSnapshot.revision,
        "selected non-ZIP format survives submission and round-trips Shelf entries");

    auto occupied = Listi::PrepareShelfArchive(shelf.Snapshot());
    Listi::SetArchiveDestination(occupied, target.string(), "occupied.zip");
    Listi::Hooks occupyTarget;
    occupyTarget.progress = [&](const std::string &, uint64_t) {
      std::ofstream(occupied.destination) << "do not replace";
    };
    auto refused = Listi::Run(occupied, occupyTarget);
    Check(refused.completed == 0 && !refused.errors.empty() &&
              Read(occupied.destination) == "do not replace",
          "ZIP job cannot replace a target created while packing");

    auto lateCopyFolder = fixture.root / "late-copy";
    fs::create_directory(lateCopyFolder);
    const auto guardedCopy = Listi::PrepareShelfTransfer(
        shelf.Snapshot(), Listi::Action::Copy, lateCopyFolder.string());
    auto guardedZip = Listi::PrepareShelfArchive(shelf.Snapshot());
    Listi::SetArchiveDestination(guardedZip, target.string(), "stale.zip");
    auto job = Listi::PrepareShelfTransfer(
        shelf.Snapshot(), Listi::Action::Move, target.string());
    job = Listi::DecodeJob(Listi::EncodeJob(job, 'test'));
    const auto original = first + ".original";
    fs::rename(first, original);
    std::ofstream(first) << "replacement";
    Check(shelf.Snapshot().entries.front().State() ==
              Listi::ShelfState::Replaced,
          "same-path replacement is visibly stale");
    Check(shelf.Add({first}) == 0 && shelf.Snapshot().entries.front().State() ==
                                         Listi::ShelfState::Replaced,
          "adding the same path does not silently accept a replacement");
    Refused([&] { shelf.Add({first}, job.expectedSources); }, "dragged entry",
            "a drag with the old inode cannot collect its replacement");
    Refused(
        [&] {
          Listi::PrepareShelfTransfer(shelf.Snapshot(), Listi::Action::Move,
                                      target.string());
        },
        "missing or replaced", "reject stale sources before queuing");
    Refused([&] { Listi::PrepareShelfArchive(shelf.Snapshot()); },
            "missing or replaced", "reject a stale Shelf reference for ZIP");
    auto stale = Listi::Run(guardedZip, {});
    Check(stale.completed == 0 && !stale.errors.empty() &&
              !fs::exists(guardedZip.destination),
          "ZIP job refuses a source replaced after picking");

    const auto staleCopy = Listi::Run(guardedCopy, {});
    Check(
        staleCopy.completed == 1 && !staleCopy.errors.empty() &&
            !fs::exists(lateCopyFolder / "first") &&
            Read(lateCopyFolder / "second") == "second" &&
            Read(first) == "replacement" && Read(second) == "second",
        "guarded copy rejects a late replacement and leaves originals untouched");

    const auto outcome = Listi::Run(job, {});
    Check(outcome.completed == 1 && !outcome.errors.empty() &&
              fs::exists(first) && !fs::exists(target / "first") &&
              fs::exists(target / "second"),
          "guarded job rejects a late replacement and moves the other source");
    Check(shelf.Snapshot().entries.size() == 2 &&
              shelf.Snapshot().entries.back().State() ==
                  Listi::ShelfState::Missing,
          "shelf retains references and shows a moved source as unavailable");
    shelf.Remove({first});
    Refused([&] { Listi::PrepareShelfArchive(shelf.Snapshot()); },
            "missing or replaced", "reject a missing Shelf reference for ZIP");

    shelf.Remove({second});
    shelf.Add({first});
    auto destinationJob = Listi::PrepareShelfTransfer(
        shelf.Snapshot(), Listi::Action::Move, target.string());
    fs::rename(target, fixture.root / "old-target");
    fs::create_directory(target);
    const auto replacedDestination = Listi::Run(destinationJob, {});
    Check(!replacedDestination.errors.empty() && fs::exists(first) &&
              !fs::exists(target / "first"),
          "guard rejects a destination replaced after confirmation");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
