// SPDX-License-Identifier: MIT
#pragma once
#include "FileSearch.h"
#include <Message.h>
#include <vector>
namespace Lister {
struct SavedSearch { std::string name; SearchRequest request; bool live = false; };
class SavedSearches {
public:
  static constexpr size_t Limit = 64;
  const std::vector<SavedSearch> &Items() const { return items; }
  const SavedSearch *Find(const std::string &name) const;
  void Put(SavedSearch item, bool replace);
  void Remove(const std::string &name);
  BMessage Message() const;
  static SavedSearches FromMessage(const BMessage &);
  static SavedSearches Load(const std::string &file);
  void Save(const std::string &file) const;
private:
  std::vector<SavedSearch> items;
};
}
constexpr uint32 kSearchStore = 'qsto', kSearchEdit = 'qedt',
                 kSaveSearches = 'qsav', kSearchesSaved = 'qsdd';
