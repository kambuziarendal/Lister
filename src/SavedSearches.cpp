// SPDX-License-Identifier: MIT
#include "SavedSearches.h"
#include "MessageStore.h"
#include <algorithm>
#include <stdexcept>
namespace Listi {
void SavedSearch::WriteTo(BMessage &message) const {
  message.AddString("name", name.c_str());
  message.AddString("root", request.root.c_str());
  message.AddString("expression", request.expression.c_str());
  message.AddBool("predicate", request.predicate);
  message.AddBool("trash", request.includeTrash);
  message.AddBool("live", live);
  WriteSearchFilters(message, request.filters);
}

SavedSearch SavedSearch::FromMessage(const BMessage &message) {
  SavedSearch item;
  item.name = message.GetString("name", "");
  item.request.root = message.GetString("root", "");
  item.request.expression = message.GetString("expression", "");
  item.request.predicate = message.GetBool("predicate", false);
  item.request.includeTrash = message.GetBool("trash", false);
  item.live = message.GetBool("live", false);
  item.request.filters = ReadSearchFilters(message);
  return item;
}

const SavedSearch *SavedSearches::Find(const std::string &name) const {
  for (const auto &item : items)
    if (item.name == name)
      return &item;
  return nullptr;
}
void SavedSearches::Put(SavedSearch item, bool replace) {
  auto first = item.name.find_first_not_of(" \t\r\n");
  item.name =
      first == std::string::npos
          ? ""
          : item.name.substr(first,
                             item.name.find_last_not_of(" \t\r\n") - first + 1);
  if (item.name.empty() || item.name.size() > 80 ||
      std::any_of(item.name.begin(), item.name.end(),
                  [](unsigned char c) { return c < 32 || c == 127; }))
    throw std::runtime_error(
        "Use a search name of 1 to 80 bytes without control characters.");
  const auto &r = item.request;
  auto filters = CompileSearchFilters(r.filters);
  if (r.root.empty() || r.root[0] != '/' ||
      r.root.size() >= B_PATH_NAME_LENGTH ||
      r.root.find('\0') != std::string::npos ||
      (r.expression.empty() && (r.predicate || !filters.active)) ||
      r.expression.size() > 4096 ||
      r.expression.find('\0') != std::string::npos ||
      (item.live && !r.predicate))
    throw std::runtime_error(
        "Use a local folder and valid search criteria. Live results require BFS.");
  for (auto &old : items)
    if (old.name == item.name) {
      if (!replace)
        throw std::runtime_error("That search name is already in use.");
      old = item;
      return;
    }
  if (replace)
    throw std::runtime_error("That saved search no longer exists.");
  if (items.size() >= Limit)
    throw std::runtime_error("Saved-search limit reached (64).");
  items.push_back(item);
}
void SavedSearches::Remove(const std::string &name) {
  auto found = std::find_if(items.begin(), items.end(),
                            [&](const auto &i) { return i.name == name; });
  if (found == items.end())
    throw std::runtime_error("That saved search no longer exists.");
  items.erase(found);
}
BMessage SavedSearches::Message() const {
  BMessage result;
  result.AddInt32("version", 2);
  for (const auto &item : items) {
    BMessage entry;
    item.WriteTo(entry);
    result.AddMessage("search", &entry);
  }
  return result;
}
SavedSearches SavedSearches::FromMessage(const BMessage &m) {
  if (m.GetInt32("version", 0) != 1 && m.GetInt32("version", 0) != 2)
    throw std::runtime_error("Unsupported saved-search format.");
  type_code type;
  int32 count = 0;
  if (m.GetInfo("search", &type, &count) == B_OK &&
      (type != B_MESSAGE_TYPE || count > int32(Limit)))
    throw std::runtime_error("Invalid saved-search list.");
  SavedSearches result;
  for (int32 i = 0; i < count; ++i) {
    BMessage entry;
    bool predicate, trash, live;
    if (m.FindMessage("search", i, &entry) != B_OK ||
        entry.FindBool("predicate", &predicate) != B_OK ||
        entry.FindBool("trash", &trash) != B_OK ||
        entry.FindBool("live", &live) != B_OK)
      throw std::runtime_error("Invalid saved search.");
    if (m.GetInt32("version", 0) == 2 && !entry.HasMessage("filters"))
      throw std::runtime_error("Missing saved file filters.");
    result.Put(SavedSearch::FromMessage(entry), false);
  }
  return result;
}
SavedSearches SavedSearches::Load(const std::string &file) {
  auto message = LoadMessageStore(file);
  return message.what == 'none' ? SavedSearches() : FromMessage(message);
}
void SavedSearches::Save(const std::string &file) const {
  auto message = Message();
  FromMessage(message);
  SaveMessageStore(file, message);
}
}
