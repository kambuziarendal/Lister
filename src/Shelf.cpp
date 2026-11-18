// SPDX-License-Identifier: MIT
#include "TransferCommand.h"
#include "Shelf.h"
#include "ArchiveJobs.h"
#include "ShelfIdentity.h"
#include <algorithm>
#include <cstdlib>
#include <limits.h>
#include <stdexcept>
#include <sys/stat.h>

namespace Listi {
namespace {
NodeIdentity Identity(const struct stat &st) {
  return {int32_t(st.st_dev), int64_t(st.st_ino)};
}

bool Matches(const NodeIdentity &identity, const struct stat &st) {
  return identity.device == int32_t(st.st_dev) &&
         identity.inode == int64_t(st.st_ino);
}

std::string Parent(const std::string &path) {
  return path.substr(0, std::max<size_t>(1, path.find_last_of('/')));
}

std::string Resolve(const std::string &directory) {
  char path[PATH_MAX];
  if (directory.empty() || directory.front() != '/' ||
      !realpath(directory.c_str(), path))
    throw std::runtime_error("Folder is unavailable: " + directory);
  return path;
}

ShelfEntry Capture(const std::string &path) {
  auto leaf = Leaf(path);
  if (path.empty() || path.front() != '/' || leaf.empty() || leaf == "." ||
      leaf == "..")
    throw std::runtime_error("The Shelf accepts ordinary local entries only.");

  // Resolve parent aliases, but never follow the selected entry if it is a
  // symbolic link. Distinct hard-link names are also distinct references.
  auto parent = Resolve(Parent(path));
  auto source = Join(parent, leaf);
  struct stat st;
  if (lstat(source.c_str(), &st) ||
      !(S_ISREG(st.st_mode) || S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)))
    throw std::runtime_error("Entry is unavailable or unsupported: " + source);
  if (S_ISDIR(st.st_mode)) {
    struct stat parentStat;
    if (stat(parent.c_str(), &parentStat) || st.st_dev != parentStat.st_dev)
      throw std::runtime_error("The Shelf cannot collect volume roots.");
  }
  return {source, Identity(st), S_ISDIR(st.st_mode),
          CaptureShelfProof(source, st)};
}
} // namespace

ShelfState ShelfEntry::State() const {
  struct stat st;
  if (lstat(path.c_str(), &st))
    return ShelfState::Missing;
  if (restored || proof) {
    if (!proof)
      return ShelfState::Ambiguous;
    auto state = CheckShelfProof(*proof, st);
    if (state != ShelfState::Ready)
      return state;
    if (unresolved || !Matches(identity, st))
      return ShelfState::Ambiguous;
  }
  return Matches(identity, st) ? ShelfState::Ready : ShelfState::Replaced;
}

size_t Shelf::Add(const std::vector<std::string> &paths,
                  const std::map<std::string, NodeIdentity> &expected) {
  if (paths.empty() || paths.size() > kMaximumEntries)
    throw std::runtime_error("Select between 1 and 1024 local entries first.");

  std::vector<ShelfEntry> additions;
  for (const auto &path : paths) {
    auto entry = Capture(path);
    if (!expected.empty()) {
      auto original = expected.find(path);
      if (original == expected.end() ||
          entry.identity.device != original->second.device ||
          entry.identity.inode != original->second.inode)
        throw std::runtime_error("An entry changed after it was selected.");
    }
    additions.push_back(std::move(entry));
  }
  std::lock_guard<std::mutex> lock(mutex);
  std::set<std::string> existing;
  for (const auto &entry : current.entries)
    existing.insert(entry.path);
  additions.erase(std::remove_if(additions.begin(), additions.end(),
                                 [&](const auto &entry) {
                                   return !existing.insert(entry.path).second;
                                 }),
                  additions.end());
  if (current.entries.size() + additions.size() > kMaximumEntries)
    throw std::runtime_error("The Shelf can hold up to 1024 entries.");
  if (!additions.empty()) {
    current.entries.insert(current.entries.end(), additions.begin(),
                           additions.end());
    ++current.revision;
  }
  return additions.size();
}

void Shelf::Remove(const std::set<std::string> &paths) {
  std::lock_guard<std::mutex> lock(mutex);
  auto &entries = current.entries;
  auto end = std::remove_if(entries.begin(), entries.end(),
                            [&](const auto &e) { return paths.count(e.path); });
  if (end != entries.end()) {
    entries.erase(end, entries.end());
    ++current.revision;
  }
}

void Shelf::Clear() {
  std::lock_guard<std::mutex> lock(mutex);
  if (!current.entries.empty()) {
    current.entries.clear();
    ++current.revision;
  }
}

void Shelf::Load(std::vector<ShelfEntry> entries, size_t revision) {
  ResolveShelfReferences(entries);
  std::lock_guard<std::mutex> lock(mutex);
  if (current.revision != revision)
    throw std::runtime_error("The Shelf changed. Load the collection again.");
  current.entries = std::move(entries);
  ++current.revision;
}

void Shelf::RefreshSavedReferences() {
  auto next = Snapshot();
  ResolveShelfReferences(next.entries);
  std::lock_guard<std::mutex> lock(mutex);
  if (current.revision != next.revision)
    return;
  for (size_t i = 0; i < next.entries.size(); ++i) {
    auto &old = current.entries[i];
    auto &updated = next.entries[i];
    if (old.path != updated.path || old.unresolved != updated.unresolved ||
        old.identity.device != updated.identity.device) {
      current.entries = std::move(next.entries);
      ++current.revision;
      return;
    }
  }
}

void Shelf::Relink(const std::string &oldPath, const std::string &newPath,
                   size_t revision) {
  auto replacement = Capture(newPath);
  std::lock_guard<std::mutex> lock(mutex);
  if (current.revision != revision)
    throw std::runtime_error("The Shelf changed. Choose the reference again.");
  auto &entries = current.entries;
  auto selected = std::find_if(entries.begin(), entries.end(),
                               [&](const auto &entry) {
                                 return entry.path == oldPath;
                               });
  if (selected == entries.end())
    throw std::runtime_error("That reference is no longer on the Shelf.");
  if (selected->State() == ShelfState::Ready)
    throw std::runtime_error("That reference is already available.");
  if (std::any_of(entries.begin(), entries.end(), [&](const auto &entry) {
        return entry.path != oldPath && entry.path == replacement.path;
      }))
    throw std::runtime_error("That entry is already on the Shelf.");
  *selected = std::move(replacement);
  ++current.revision;
}

ShelfSnapshot Shelf::Snapshot() const {
  std::lock_guard<std::mutex> lock(mutex);
  return current;
}

void CheckShelfSources(const ShelfSnapshot &shelf, Action action) {
  if (shelf.entries.empty())
    throw std::runtime_error("Add entries to the Shelf first.");
  std::set<std::string> names;
  for (const auto &entry : shelf.entries) {
    if (entry.State() != ShelfState::Ready)
      throw std::runtime_error("Relink or remove the missing or replaced reference (including ambiguous identity): " +
                               entry.path);
    const auto name = Leaf(entry.path);
    if (!names.insert(name).second)
      throw std::runtime_error(
          "Two Shelf entries have the same name: " + name +
          ". Remove one reference and try again.");
  }
  if (action != Action::Move)
    return;
  std::set<std::string> paths;
  for (const auto &entry : shelf.entries)
    paths.insert(entry.path);
  for (const auto &entry : shelf.entries) {
    const auto prefix = entry.path + "/";
    const auto child = paths.lower_bound(prefix);
    if (entry.directory && child != paths.end() && child->rfind(prefix, 0) == 0)
      throw std::runtime_error(
          "Remove either a folder or its contents from the Shelf before moving.");
  }
}

Job PrepareShelfTransfer(const ShelfSnapshot &shelf, Action action,
                         const std::string &folder) {
  CheckShelfSources(shelf, action);
  Job job;
  job.action = action;
  job.destination = job.destinationFolder = Resolve(folder);
  struct stat st;
  if (stat(job.destination.c_str(), &st) || !S_ISDIR(st.st_mode))
    throw std::runtime_error("Choose an available local destination folder.");
  job.expectedDestination = Identity(st);
  for (const auto &entry : shelf.entries) {
    job.sources.push_back(entry.path);
    job.expectedSources.emplace(entry.path, entry.identity);
  }
  return PrepareTransfer(std::move(job));
}

Job PrepareShelfArchive(const ShelfSnapshot &shelf) {
  CheckShelfSources(shelf, Action::CreateArchive);
  std::vector<std::string> paths;
  for (const auto &entry : shelf.entries)
    paths.push_back(entry.path);
  auto job = PrepareArchiveJob(false, paths);
  for (const auto &entry : shelf.entries) {
    const auto &captured = job.expectedSources.at(entry.path);
    if (captured.device != entry.identity.device ||
        captured.inode != entry.identity.inode)
      throw std::runtime_error("Relink or remove the missing or replaced reference (including ambiguous identity): " +
                               entry.path);
  }
  return job;
}
} // namespace Listi
