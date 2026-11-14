// SPDX-License-Identifier: MIT
#pragma once
#include <functional>
#include <map>
#include <utility>
#include <stdint.h>
#include <string>
#include <vector>

namespace Lister {
enum class Action { Copy, Move, Delete, Trash, Restore, MakeDirectory, Rename, Duplicate, Link };
enum class Decision { Replace, ReplaceAll, Skip, SkipAll, Abort };
struct NodeIdentity {
  int32_t device = -1;
  int64_t inode = -1;
};
struct Job {
  Action action = Action::Copy;
  std::vector<std::string> sources;
  std::string destination;
  // Optional identity guards for operations frozen from the system clipboard or a drop.
  std::map<std::string, NodeIdentity> expectedSources;
  NodeIdentity expectedDestination;
  int64_t tag = 0;
  // Only Duplicate/Link use an explicit leaf name, always without replacement.
  std::string entryName;
  bool relativeLink = true;
};
struct Hooks {
  std::function<bool()> proceed;
  std::function<void(const std::string &, uint64_t)> progress;
  std::function<Decision(const std::string &)> conflict;
  std::function<void(const std::string &, const std::string &)> fileDone;
  std::function<void(const std::string &, const std::string &)> fileFailed;
};
struct Result {
  int completed = 0, skipped = 0;
  bool cancelled = false;
  std::vector<std::string> errors;
  std::vector<std::pair<std::string, std::string>> completedRoots;
};
Result Run(const Job &, const Hooks &);
std::string Join(const std::string &, const std::string &);
std::string Leaf(const std::string &);
int SelfTest();
} // namespace Lister
