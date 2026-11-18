// SPDX-License-Identifier: MIT
#include "ShelfCollections.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Listi;
namespace fs = std::filesystem;
namespace {
struct Fixture {
  fs::path root;
  Fixture() {
    char path[] = "/boot/home/Listi-collections-XXXXXX";
    const char *created = mkdtemp(path);
    if (!created)
      throw std::runtime_error("Could not create collection fixture.");
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

template <typename Test>
bool Refused(Test test) {
  try {
    test();
    return false;
  } catch (const std::runtime_error &) {
    return true;
  }
}

void Load(Shelf &shelf, const std::string &records, const std::string &name) {
  auto collections = ShelfCollections::Load(records);
  auto item = collections.Find(name);
  if (!item)
    throw std::runtime_error("Missing test collection.");
  shelf.Load(item->entries, shelf.Snapshot().revision);
}

std::string Read(const fs::path &file) {
  std::ifstream stream(file);
  return {std::istreambuf_iterator<char>(stream), {}};
}
}

int main(int argc, char **argv) {
  try {
    // Optional two-process probe for restart and mount-cycle verification.
    if (argc == 4 && std::string(argv[1]) == "--save-reference") {
      Shelf shelf;
      shelf.Add({argv[3]});
      const auto snapshot = shelf.Snapshot();
      Check(snapshot.entries.front().proof.has_value(), "mounted BFS proof");
      ShelfCollections collections;
      collections.Put("mount probe", snapshot, false);
      collections.Save(argv[2]);
      return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--check-reference") {
      Shelf shelf;
      Load(shelf, argv[2], "mount probe");
      const auto entry = shelf.Snapshot().entries.front();
      const std::string expected(argv[4]);
      Check(entry.path == argv[3] &&
                ((expected == "ready" && entry.State() == ShelfState::Ready) ||
                 (expected == "missing" && entry.State() == ShelfState::Missing) ||
                 (expected == "ambiguous" &&
                  entry.State() == ShelfState::Ambiguous)),
            "saved mount probe resolves to the expected path and state");
      return 0;
    }
    Fixture fixture;
    auto folder = fixture.root / "known";
    const auto movedFolder = fixture.root / "known-renamed";
    const auto other = fixture.root / "unrelated";
    const auto records = (fixture.root / "records").string();
    fs::create_directory(folder);
    fs::create_directory(other);
    const auto original = folder / "original";
    const auto renamed = folder / "renamed";
    const auto moved = other / "moved";
    const auto sameName = other / "original";
    std::ofstream(original) << "original bytes";
    const auto link = fixture.root / "dangling-link";
    fs::create_symlink("not-there", link);

    Shelf shelf;
    shelf.Add({original.string(), link.string()});
    const auto initial = shelf.Snapshot();
    Check(initial.entries[0].proof.has_value(),
          "capture original proof on persistent BFS");
    ShelfCollections collections;
    collections.Put("  Daily  ", initial, false);
    Check(Refused([&] { collections.Put("Daily", initial, false); }),
          "duplicate names require explicit replacement");
    collections.Save(records);
    Shelf reopened;
    Load(reopened, records, "Daily");
    auto restored = reopened.Snapshot();
    Check(restored.entries.size() == 2 &&
              restored.entries[0].State() == ShelfState::Ready &&
              restored.entries[1].State() == ShelfState::Ready &&
              restored.entries[1].path == link.string(),
          "load records into a new Shelf and match symlink without following");

    fs::rename(original, sameName);
    std::ofstream(original) << "indexed decoy";
    Load(reopened, records, "Daily");
    restored = reopened.Snapshot();
    Check(restored.entries[0].path == sameName.string() &&
              restored.entries[0].State() == ShelfState::Ready &&
              Read(original) == "indexed decoy",
          "indexed basename lookup follows a same-volume move, not a decoy");
    fs::remove(original);
    fs::rename(sameName, original);
    Load(reopened, records, "Daily");
    restored = reopened.Snapshot();

    fs::rename(original, renamed);
    std::ofstream(original) << "replacement";
    Check(restored.entries[0].State() == ShelfState::Replaced,
          "reusing a live reference path does not bind to the replacement");
    reopened.RefreshSavedReferences();
    restored = reopened.Snapshot();
    Check(restored.entries[0].path == renamed.string() &&
              restored.entries[0].State() == ShelfState::Ready &&
              Read(original) == "replacement",
          "refresh finds the original after a same-parent rename");
    fs::rename(folder, movedFolder);
    Load(reopened, records, "Daily");
    restored = reopened.Snapshot();
    Check(restored.entries[0].path == (movedFolder / "renamed").string() &&
              restored.entries[0].State() == ShelfState::Ready,
          "load follows the original parent after that directory moved");

    fs::rename(movedFolder / "renamed", moved);
    Load(reopened, records, "Daily");
    Check(reopened.Snapshot().entries[0].State() == ShelfState::Missing,
          "an entry outside the known parent is unavailable, not guessed");
    fs::create_directory(folder);
    std::ofstream(original) << "decoy";
    Load(reopened, records, "Daily");
    restored = reopened.Snapshot();
    Check(restored.entries[0].path == original.string() &&
              restored.entries[0].State() != ShelfState::Ready &&
              Refused([&] { CheckShelfSources(restored, Action::Copy); }),
          "move outside the known parent requires relink; decoy is not queued");
    Check(Refused([&] {
            reopened.Relink(original.string(), moved.string(),
                            restored.revision - 1);
          }) &&
              Refused([&] {
                reopened.Relink(original.string(), link.string(),
                                restored.revision);
              }),
          "reject stale relink requests and duplicate paths");
    reopened.Relink(original.string(), moved.string(), restored.revision);
    auto relinked = reopened.Snapshot();
    Check(relinked.entries[0].path == moved.string() &&
              relinked.entries[0].State() == ShelfState::Ready &&
              Read(original) == "decoy" && Read(moved) == "original bytes",
          "explicit relink changes the reference only");

    auto stale = relinked;
    ++stale.entries[0].proof->entry.seconds;
    collections.Put("Stale birth stamp", stale, false);
    auto untrusted = relinked;
    untrusted.entries[0].proof.reset();
    collections.Put("No proof", untrusted, false);
    collections.Put("Daily", relinked, true);
    collections.Save(records);
    Load(reopened, records, "Stale birth stamp");
    Check(reopened.Snapshot().entries[0].State() == ShelfState::Replaced,
          "path, volume and inode alone do not prove a reused node is original");
    Load(reopened, records, "No proof");
    Check(reopened.Snapshot().entries[0].State() == ShelfState::Ambiguous,
          "a filesystem without a supported proof must be explicitly relinked");
    Load(reopened, records, "Daily");
    Check(reopened.Snapshot().entries[0].State() == ShelfState::Ready,
          "explicitly updated collection keeps the relink across reloads");
    reopened.Remove({moved.string()});
    collections.Remove("Daily");
    collections.Save(records);
    Check(!ShelfCollections::Load(records).Find("Daily") &&
              Read(moved) == "original bytes" && fs::is_symlink(link),
          "removing from Shelf and deleting collection never change originals");
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
  return 0;
}
