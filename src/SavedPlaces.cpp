// SPDX-License-Identifier: MIT
#include "SavedPlaces.h"
#include "MessageStore.h"
#include "VirtualFile.h"
#include <algorithm>
#include <stdexcept>
namespace Listi {
namespace {
std::string Address(const std::string &path) {
  if (path.empty() || path.size() > 8192 || path.find('\0') != std::string::npos)
    throw std::runtime_error("Invalid saved address.");
  if (IsVirtual(path))
    return ParseLocation(path).URL(); // Reject embedded authentication.
  if (path[0] != '/' && path != "@disks")
    throw std::runtime_error("Saved places require an absolute address.");
  return path;
}
}
const SavedPlace *SavedPlaces::Find(const std::string &path) const {
  for (auto &item : items)
    if (item.path == path) return &item;
  return nullptr;
}
std::string SavedPlaces::Name(const std::string &raw, const std::string &except) const {
  auto first = raw.find_first_not_of(" \t\r\n");
  std::string name = first == std::string::npos ? "" :
      raw.substr(first, raw.find_last_not_of(" \t\r\n") - first + 1);
  if (name.empty() || name.size() > 80 ||
      std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; }))
    throw std::runtime_error("Use a name of 1 to 80 bytes without control characters.");
  for (auto &item : items)
    if (item.path != except && item.name == name)
      throw std::runtime_error("That place name is already in use.");
  return name;
}
void SavedPlaces::Add(const std::string &raw, const std::string &location) {
  auto path = Address(location);
  auto name = Name(raw);
  if (Find(path)) throw std::runtime_error("This place is already saved.");
  if (items.size() >= Limit) throw std::runtime_error("Saved-place limit reached (64).");
  items.push_back({name, path});
}
void SavedPlaces::Rename(const std::string &path, const std::string &raw) {
  auto name = Name(raw, path);
  for (auto &item : items)
    if (item.path == path) { item.name = name; return; }
  throw std::runtime_error("That saved place no longer exists.");
}
void SavedPlaces::Remove(const std::string &path) {
  auto found = std::find_if(items.begin(), items.end(), [&](auto &p) { return p.path == path; });
  if (found == items.end()) throw std::runtime_error("That saved place no longer exists.");
  items.erase(found);
}
BMessage SavedPlaces::Message() const {
  BMessage result;
  result.AddInt32("version", 1);
  for (auto &item : items) {
    BMessage place;
    place.AddString("name", item.name.c_str());
    place.AddString("path", item.path.c_str());
    result.AddMessage("place", &place);
  }
  return result;
}
SavedPlaces SavedPlaces::FromMessage(const BMessage &m) {
  if (m.GetInt32("version", 0) != 1)
    throw std::runtime_error("Unsupported saved-places format.");
  SavedPlaces result;
  BMessage place;
  type_code type; int32 count = 0;
  if (m.GetInfo("place", &type, &count) == B_OK &&
      (type != B_MESSAGE_TYPE || count > int32(Limit)))
    throw std::runtime_error("Invalid saved-places list.");
  for (int32 i = 0; i < count; ++i) {
    if (m.FindMessage("place", i, &place) != B_OK)
      throw std::runtime_error("Invalid saved place.");
    result.Add(place.GetString("name", ""), place.GetString("path", ""));
  }
  return result;
}
SavedPlaces SavedPlaces::Load(const std::string &file) {
  auto message = LoadMessageStore(file);
  if (message.what == 'none') return {};
  return FromMessage(message);
}
void SavedPlaces::Save(const std::string &file) const {
  auto message = Message();
  FromMessage(message);
  SaveMessageStore(file, message);
}
} // namespace Listi
