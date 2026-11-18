// SPDX-License-Identifier: MIT
#include "ShelfCollections.h"
#include "MessageStore.h"
#include <StorageDefs.h>
#include <algorithm>
#include <stdexcept>
#include <sys/stat.h>

namespace Listi {
namespace {
int32 Count(const BMessage &message, const char *field, size_t limit) {
  type_code type;
  int32 count = 0;
  auto status = message.GetInfo(field, &type, &count);
  if (status == B_NAME_NOT_FOUND)
    return 0;
  if (status != B_OK || type != B_MESSAGE_TYPE || count < 0 ||
      size_t(count) > limit)
    throw std::runtime_error("Invalid saved Shelf collection.");
  return count;
}

BMessage ReadChild(const BMessage &message, const char *field, int32 index) {
  BMessage child;
  if (message.FindMessage(field, index, &child) != B_OK)
    throw std::runtime_error("Invalid saved Shelf collection.");
  return child;
}

BMessage EncodeStamp(const ShelfNodeStamp &stamp) {
  BMessage message;
  message.AddInt64("inode", stamp.inode);
  message.AddInt64("seconds", stamp.seconds);
  message.AddInt32("nanoseconds", stamp.nanoseconds);
  return message;
}

ShelfNodeStamp DecodeStamp(const BMessage &message) {
  ShelfNodeStamp stamp{message.GetInt64("inode", -1),
                       message.GetInt64("seconds", -1),
                       message.GetInt32("nanoseconds", -1)};
  if (stamp.inode <= 0 || stamp.seconds <= 0 || stamp.nanoseconds < 0 ||
      stamp.nanoseconds >= 1000000000)
    throw std::runtime_error("Invalid saved Shelf identity.");
  return stamp;
}

BMessage EncodeEntry(const ShelfEntry &entry) {
  BMessage message;
  message.AddString("path", entry.path.c_str());
  if (entry.proof) {
    const auto &proof = *entry.proof;
    BMessage identity, root = EncodeStamp(proof.volumeRoot),
                       parent = EncodeStamp(proof.parent),
                       node = EncodeStamp(proof.entry);
    identity.AddString("device", proof.device.c_str());
    identity.AddMessage("root", &root);
    identity.AddMessage("parent", &parent);
    identity.AddMessage("node", &node);
    identity.AddUInt32("kind", proof.kind);
    message.AddMessage("identity", &identity);
  }
  return message;
}

ShelfEntry DecodeEntry(const BMessage &message) {
  ShelfEntry entry;
  entry.path = message.GetString("path", "");
  entry.restored = entry.unresolved = true;
  if (Count(message, "identity", 1)) {
    const auto identity = ReadChild(message, "identity", 0);
    const std::string device = identity.GetString("device", "");
    const auto kind = identity.GetUInt32("kind", 0);
    if (device.empty() || device.size() >= B_DEV_NAME_LENGTH ||
        (kind != S_IFREG && kind != S_IFDIR && kind != S_IFLNK))
      throw std::runtime_error("Invalid saved Shelf identity.");
    entry.directory = kind == S_IFDIR;
    entry.proof = ShelfProof{device,
                             DecodeStamp(ReadChild(identity, "root", 0)),
                             DecodeStamp(ReadChild(identity, "parent", 0)),
                             DecodeStamp(ReadChild(identity, "node", 0)),
                             kind};
  }
  return entry;
}
}

const ShelfCollection *ShelfCollections::Find(const std::string &name) const {
  auto item = std::find_if(items.begin(), items.end(),
                           [&](const auto &i) { return i.name == name; });
  return item == items.end() ? nullptr : &*item;
}

std::string ShelfCollections::Name(const std::string &value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  const auto last = value.find_last_not_of(" \t\r\n");
  const auto name = first == std::string::npos
                        ? std::string()
                        : value.substr(first, last - first + 1);
  if (name.empty() || name.size() > 80 ||
      std::any_of(name.begin(), name.end(),
                  [](unsigned char c) { return c < 32 || c == 127; }))
    throw std::runtime_error(
        "Use a collection name of 1 to 80 bytes without control characters.");
  return name;
}

void ShelfCollections::Put(const std::string &input, const ShelfSnapshot &shelf,
                           bool replace) {
  const auto name = Name(input);
  if (shelf.entries.empty() || shelf.entries.size() > Shelf::kMaximumEntries)
    throw std::runtime_error("Save between 1 and 1024 Shelf references.");
  std::set<std::string> paths;
  for (const auto &entry : shelf.entries) {
    const auto &path = entry.path;
    if (path.size() < 2 || path[0] != '/' || path.back() == '/' ||
        path.size() >= B_PATH_NAME_LENGTH || path.find('\0') != std::string::npos ||
        !paths.insert(path).second)
      throw std::runtime_error("Invalid or duplicate saved Shelf path.");
  }
  for (auto &item : items) {
    if (item.name != name)
      continue;
    if (!replace)
      throw std::runtime_error("That collection name is already in use.");
    item.entries = shelf.entries;
    return;
  }
  if (replace)
    throw std::runtime_error("That saved collection no longer exists.");
  if (items.size() >= kMaximumCollections)
    throw std::runtime_error("Saved collection limit reached (64).");
  items.push_back({name, shelf.entries});
}

void ShelfCollections::Remove(const std::string &name) {
  auto item = std::find_if(items.begin(), items.end(),
                           [&](const auto &i) { return i.name == name; });
  if (item == items.end())
    throw std::runtime_error("That saved collection no longer exists.");
  items.erase(item);
}

ShelfCollections ShelfCollections::Load(const std::string &file) {
  if (file.empty())
    throw std::runtime_error("The user settings folder is unavailable.");
  const auto root = LoadMessageStore(file);
  ShelfCollections result;
  if (root.what == 'none')
    return result;
  if (root.GetInt32("version", 0) != 1)
    throw std::runtime_error("Unsupported saved Shelf collection format.");
  const auto count = Count(root, "collection", kMaximumCollections);
  for (int32 i = 0; i < count; ++i) {
    const auto collection = ReadChild(root, "collection", i);
    const auto entries = Count(collection, "entry", Shelf::kMaximumEntries);
    ShelfSnapshot shelf;
    for (int32 j = 0; j < entries; ++j)
      shelf.entries.push_back(DecodeEntry(ReadChild(collection, "entry", j)));
    result.Put(collection.GetString("name", ""), shelf, false);
  }
  return result;
}

void ShelfCollections::Save(const std::string &file) const {
  if (file.empty())
    throw std::runtime_error("The user settings folder is unavailable.");
  BMessage root;
  root.AddInt32("version", 1);
  for (const auto &item : items) {
    BMessage collection;
    collection.AddString("name", item.name.c_str());
    for (const auto &entry : item.entries) {
      auto encoded = EncodeEntry(entry);
      collection.AddMessage("entry", &encoded);
    }
    root.AddMessage("collection", &collection);
  }
  SaveMessageStore(file, root);
}
}
