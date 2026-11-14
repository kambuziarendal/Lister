// SPDX-License-Identifier: MIT
#pragma once
#include <functional>
#include <stdint.h>
#include <string>
#include <vector>

namespace Lister {
enum class Action { Copy, Move, Delete, Trash, Restore, MakeDirectory, Rename };
enum class Decision { Replace, ReplaceAll, Skip, SkipAll, Abort };
struct Job {
  Action action = Action::Copy;
  std::vector<std::string> sources;
  std::string destination;
};
struct Hooks {
  std::function<bool()> proceed;
  std::function<void(const std::string &, uint64_t)> progress;
  std::function<Decision(const std::string &)> conflict;
};
struct Result {
  int completed = 0, skipped = 0;
  bool cancelled = false;
  std::vector<std::string> errors;
};
Result Run(const Job &, const Hooks &);
std::string Join(const std::string &, const std::string &);
std::string Leaf(const std::string &);
int SelfTest();
} // namespace Lister
