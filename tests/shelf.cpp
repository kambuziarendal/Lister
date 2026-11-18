// SPDX-License-Identifier: MIT
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
    Refused([&] { Listi::PrepareShelfMove(shelf.Snapshot(), target.string()); },
            "same name", "same-name entries cannot overwrite each other");
    shelf.Remove({collision});
    Check(fs::exists(collision),
          "removing a shelf row leaves its original file");

    shelf.Add({a.string()});
    Refused([&] { Listi::PrepareShelfMove(shelf.Snapshot(), target.string()); },
            "its contents", "refuse a folder and one of its descendants");
    shelf.Remove({a.string()});
    Refused([&] { Listi::PrepareShelfMove(shelf.Snapshot(), a.string()); },
            "already in", "refuse a move back into an original folder");

    auto job = Listi::PrepareShelfMove(shelf.Snapshot(), target.string());
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
    Refused([&] { Listi::PrepareShelfMove(shelf.Snapshot(), target.string()); },
            "missing or replaced", "reject stale sources before queuing");

    const auto outcome = Listi::Run(job, {});
    Check(outcome.completed == 1 && !outcome.errors.empty() &&
              fs::exists(first) && !fs::exists(target / "first") &&
              fs::exists(target / "second"),
          "guarded job rejects a late replacement and moves the other source");
    Check(shelf.Snapshot().entries.size() == 2 &&
              shelf.Snapshot().entries.back().State() ==
                  Listi::ShelfState::Missing,
          "shelf retains references and shows a moved source as unavailable");

    shelf.Remove({first, second});
    shelf.Add({first});
    auto destinationJob =
        Listi::PrepareShelfMove(shelf.Snapshot(), target.string());
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
