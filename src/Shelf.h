// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"
#include <mutex>
#include <optional>
#include <set>

namespace Listi {
enum class ShelfState { Ready, Missing, Replaced, Ambiguous };

struct ShelfNodeStamp {
  int64_t inode;
  int64_t seconds;
  int32_t nanoseconds;
  bool operator==(const ShelfNodeStamp &other) const {
    return inode == other.inode && seconds == other.seconds &&
           nanoseconds == other.nanoseconds;
  }
};

struct ShelfProof {
  std::string device;
  ShelfNodeStamp volumeRoot, parent, entry;
  uint32_t kind;
};

struct ShelfEntry {
  std::string path;
  NodeIdentity identity{};
  bool directory = false;
  std::optional<ShelfProof> proof;
  bool restored = false;
  bool unresolved = false;
  ShelfState restoredState = ShelfState::Ambiguous;

  ShelfState State() const;
  ShelfState DisplayState() const { return restored ? restoredState : State(); }
};

struct ShelfSnapshot {
  std::vector<ShelfEntry> entries;
  size_t revision = 0;
  std::set<std::string> RetainSelection(const ShelfSnapshot &previous,
                                        const std::set<std::string> &paths) const;
};

// Session-only collection. The browser and the Shelf have separate loopers.
class Shelf {
public:
  static constexpr size_t kMaximumEntries = 1024;

  size_t Add(const std::vector<std::string> &paths,
             const std::map<std::string, NodeIdentity> &expected = {});
  void Remove(const std::set<std::string> &paths,
              std::optional<size_t> expectedRevision = std::nullopt);
  void Clear();
  void Load(std::vector<ShelfEntry> entries, size_t revision);
  void RefreshSavedReferences();
  void Relink(const std::string &oldPath, const std::string &newPath,
              size_t revision,
              const std::optional<NodeIdentity> &expected = std::nullopt,
              const std::optional<ShelfNodeStamp> &stamp = std::nullopt);
  ShelfSnapshot Snapshot() const;

private:
  mutable std::mutex mutex;
  ShelfSnapshot current;
};

void CheckShelfSources(const ShelfSnapshot &, Action);
Job PrepareShelfTransfer(const ShelfSnapshot &, Action,
                         const std::string &folder);
Job PrepareShelfArchive(const ShelfSnapshot &);
} // namespace Listi
