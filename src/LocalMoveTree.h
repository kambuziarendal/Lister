// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"
#include <set>
#include <sys/stat.h>

namespace Lister {
// Frozen ordinary-file trees used only by verified archive/local moves.
// Removal visits the recorded names, never newly discovered children.
class LocalMoveTree {
public:
  LocalMoveTree(const std::vector<std::string> &, const Hooks &,
                bool portableSource);
  void Verify() const;
  void Flush() const;
  void Remove(const std::string &root, const Hooks &);

private:
  std::map<std::string, struct stat> entries, parents;
  bool portable;
  std::map<std::string, std::string> aliases;
  std::set<std::string> removed;
  void Capture(const std::string &, dev_t, const Hooks &, unsigned);
  void GuardParents() const;
};
} // namespace Lister
