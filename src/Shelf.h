// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"
#include <mutex>
#include <set>

namespace Listi {
enum class ShelfState { Ready, Missing, Replaced };

struct ShelfEntry {
  std::string path;
  NodeIdentity identity;
  bool directory;

  ShelfState State() const;
};

struct ShelfSnapshot {
  std::vector<ShelfEntry> entries;
  size_t revision = 0;
};

// Session-only collection. The browser and the Shelf have separate loopers.
class Shelf {
public:
  static constexpr size_t kMaximumEntries = 1024;

  size_t Add(const std::vector<std::string> &paths,
             const std::map<std::string, NodeIdentity> &expected = {});
  void Remove(const std::set<std::string> &paths);
  ShelfSnapshot Snapshot() const;

private:
  mutable std::mutex mutex;
  ShelfSnapshot current;
};

void CheckShelfSources(const ShelfSnapshot &);
Job PrepareShelfMove(const ShelfSnapshot &, const std::string &folder);
} // namespace Listi
