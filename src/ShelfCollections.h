// SPDX-License-Identifier: MIT
#pragma once
#include "Shelf.h"
#include <Message.h>

namespace Listi {
struct ShelfCollection {
  std::string name;
  std::vector<ShelfEntry> entries;
};

class ShelfCollections {
public:
  static constexpr size_t kMaximumCollections = 64;
  const std::vector<ShelfCollection> &Items() const { return items; }
  const ShelfCollection *Find(const std::string &name) const;
  void Put(const std::string &name, const ShelfSnapshot &, bool replace);
  void Remove(const std::string &name);
  static std::string Name(const std::string &);
  static ShelfCollections Load(const std::string &file);
  void Save(const std::string &file) const;

private:
  std::vector<ShelfCollection> items;
};
}
