// SPDX-License-Identifier: MIT
#pragma once
#include "FileOperation.h"

namespace Listi {
struct BulkRenameOptions {
  std::string prefix, suffix, search, replacement;
  std::map<std::string, std::string> overrides; // Frozen source path to exact new name.
  bool number = false;
  unsigned start = 1, digits = 1;
};
struct BulkRenameRow {
  std::string before, after, error;
};
struct BulkRenamePreview {
  Job job;
  std::vector<BulkRenameRow> rows;
  std::string error;
  size_t changed = 0, blocked = 0;

  bool CanApply() const { return error.empty() && changed && !blocked; }
};
Job FreezeBulkRename(const std::string &folder,
                     const std::vector<std::string> &selection,
                     const std::map<std::string, NodeIdentity> &expected = {});
BulkRenamePreview PreviewBulkRename(const Job &, const BulkRenameOptions &);
Result RunBulkRename(const Job &, const Hooks &);
}
