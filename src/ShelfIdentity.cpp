// SPDX-License-Identifier: MIT
#include "ShelfIdentity.h"
#include "VirtualFile.h"
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Query.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <fs_info.h>
#include <algorithm>
#include <cstring>

namespace Listi {
namespace {
constexpr size_t kMaximumVolumes = 64;
constexpr size_t kDirectoryScanBudget = 4096;

ShelfNodeStamp Stamp(const struct stat &st) {
  return {int64(st.st_ino), int64(st.st_crtim.tv_sec),
          int32(st.st_crtim.tv_nsec)};
}

struct VolumeProof {
  std::string device, trash;
  ShelfNodeStamp root;
};

std::optional<VolumeProof> IdentifyVolume(dev_t device, bool withTrash = false) {
  fs_info info{};
  if (fs_stat_dev(device, &info) != 0 || strcmp(info.fsh_name, "bfs") ||
      !(info.flags & B_FS_IS_PERSISTENT) || !info.device_name[0])
    return std::nullopt;
  BDirectory root;
  BVolume mounted(device);
  struct stat st;
  if (mounted.GetRootDirectory(&root) != B_OK ||
      root.GetStat(&st) != B_OK || st.st_dev != device ||
      st.st_ino != info.root || st.st_crtim.tv_sec <= 0)
    return std::nullopt;
  BPath trash;
  std::string excluded;
  if (withTrash &&
      find_directory(B_TRASH_DIRECTORY, &trash, false, &mounted) == B_OK)
    excluded = trash.Path();
  return VolumeProof{info.device_name, std::move(excluded), Stamp(st)};
}

bool MatchesVolume(const ShelfProof &proof, const VolumeProof &volume) {
  return volume.device == proof.device && volume.root == proof.volumeRoot;
}

bool MatchesEntry(const ShelfProof &proof, const struct stat &st) {
  return (st.st_mode & S_IFMT) == proof.kind && Stamp(st) == proof.entry;
}

void Bind(ShelfEntry &entry, const struct stat &st) {
  entry.identity = {int32(st.st_dev), int64(st.st_ino)};
  entry.unresolved = false;
  entry.restoredState = ShelfState::Ready;
}

std::string LeafName(const std::string &path) {
  return path.substr(path.find_last_of('/') + 1);
}

bool SafeCandidate(const std::string &path, const std::string &trash,
                   const std::set<std::string> &reserved) {
  if (reserved.count(path) || path.size() < 2 ||
      (!trash.empty() &&
       (path == trash || path.rfind(trash + "/", 0) == 0)))
    return false;
  for (size_t start = 1, end; start < path.size(); start = end + 1) {
    end = path.find('/', start);
    if (IsListiPrivateEntry(path.substr(start, end - start).c_str()))
      return false;
    if (end == std::string::npos)
      break;
  }
  return true;
}

// Follow the known parent directory, not a filesystem-wide scan. The shared
// budget caps total enumeration per refresh, including absent entries.
void ResolveInParent(ShelfEntry &entry, dev_t volume, size_t &budget,
                     const std::set<std::string> &reserved,
                     const std::string &trash) {
  const auto &proof = *entry.proof;
  node_ref parentRef(volume, ino_t(proof.parent.inode));
  BDirectory parent(&parentRef);
  struct stat st;
  if (parent.GetStat(&st) != B_OK || st.st_dev != volume ||
      !(Stamp(st) == proof.parent))
    return;
  // Try the original leaf directly if only the parent moved. Otherwise scan
  // for the original BFS inode, verifying its birth time as well.
  const auto leaf = LeafName(entry.path);
  BEntry candidate(&parent, leaf.c_str(), false);
  if (candidate.GetStat(&st) != B_OK || st.st_dev != volume ||
      !MatchesEntry(proof, st)) {
    for (;;) {
      if (!budget)
        return;
      --budget;
      if (parent.GetNextEntry(&candidate, false) != B_OK)
        return;
      if (candidate.GetStat(&st) == B_OK && st.st_dev == volume &&
          MatchesEntry(proof, st))
        break;
    }
  }
  BPath found;
  struct stat current;
  if (candidate.GetPath(&found) != B_OK ||
      !SafeCandidate(found.Path(), trash, reserved) ||
      lstat(found.Path(), &current) || current.st_dev != volume ||
      !MatchesEntry(proof, current))
    return;
  entry.path = found.Path();
  Bind(entry, current);
}

void ResolveByName(ShelfEntry &entry, dev_t device, size_t &budget,
                   const std::set<std::string> &reserved,
                   const std::string &trash) {
  const auto leaf = LeafName(entry.path);
  // Query equality interprets glob metacharacters. Don't accidentally start
  // a broad wildcard query when an unusual leaf cannot be queried literally.
  if (!budget || leaf.find_first_of("*?[]\\") != std::string::npos)
    return;
  BVolume volume(device);
  BQuery query;
  if (!volume.KnowsQuery() || query.SetVolume(&volume) != B_OK ||
      query.PushAttr("name") != B_OK ||
      query.PushString(leaf.c_str()) != B_OK || query.PushOp(B_EQ) != B_OK ||
      query.Fetch() != B_OK)
    return;

  std::string matched;
  const auto &proof = *entry.proof;
  for (;;) {
    BEntry candidate;
    const auto status = query.GetNextEntry(&candidate, false);
    if (status == B_ENTRY_NOT_FOUND)
      break;
    if (status != B_OK || !budget)
      return;
    --budget;
    BPath path;
    struct stat st;
    if (candidate.GetPath(&path) != B_OK ||
        LeafName(path.Path()) != leaf || lstat(path.Path(), &st) ||
        st.st_dev != device || !MatchesEntry(proof, st))
      continue;
    if (!matched.empty() || !SafeCandidate(path.Path(), trash, reserved))
      return;
    matched = path.Path();
  }

  struct stat current;
  if (matched.empty() || lstat(matched.c_str(), &current) ||
      current.st_dev != device || !MatchesEntry(proof, current))
    return;
  auto updated = CaptureShelfProof(matched, current);
  if (!updated || !(updated->volumeRoot == proof.volumeRoot) ||
      updated->device != proof.device || !(updated->entry == proof.entry))
    return;
  entry.path = std::move(matched);
  entry.proof = std::move(updated);
  Bind(entry, current);
}
}

std::optional<ShelfProof> CaptureShelfProof(const std::string &path,
                                            const struct stat &entry) {
  const auto volume = IdentifyVolume(entry.st_dev);
  struct stat parent, current;
  auto slash = path.find_last_of('/');
  const auto folder = path.substr(0, slash ? slash : 1);
  if (!volume || entry.st_crtim.tv_sec <= 0 ||
      stat(folder.c_str(), &parent) != 0 || parent.st_dev != entry.st_dev ||
      parent.st_crtim.tv_sec <= 0 || lstat(path.c_str(), &current) != 0 ||
      current.st_dev != entry.st_dev || !(Stamp(current) == Stamp(entry)) ||
      (current.st_mode & S_IFMT) != (entry.st_mode & S_IFMT))
    return std::nullopt;
  return ShelfProof{volume->device, volume->root, Stamp(parent), Stamp(entry),
                    uint32(entry.st_mode & S_IFMT)};
}

ShelfState CheckShelfProof(const ShelfProof &proof, const struct stat &entry) {
  const auto volume = IdentifyVolume(entry.st_dev);
  if (!volume || !MatchesVolume(proof, *volume))
    return ShelfState::Ambiguous;
  return MatchesEntry(proof, entry) ? ShelfState::Ready : ShelfState::Replaced;
}

void ResolveShelfReferences(std::vector<ShelfEntry> &entries) {
  if (std::none_of(entries.begin(), entries.end(), [](const auto &entry) {
        return entry.restored && entry.proof;
      }))
    return;
  std::set<std::string> reserved;
  for (const auto &entry : entries)
    reserved.insert(entry.path);
  std::vector<std::pair<dev_t, VolumeProof>> volumes;
  BVolumeRoster roster;
  BVolume mounted;
  for (size_t i = 0; i < kMaximumVolumes &&
                     roster.GetNextVolume(&mounted) == B_OK;
       ++i) {
    if (auto volume = IdentifyVolume(mounted.Device(), true))
      volumes.emplace_back(mounted.Device(), std::move(*volume));
  }
  size_t budget = kDirectoryScanBudget;
  for (auto &entry : entries) {
    if (!entry.restored || !entry.proof)
      continue;
    entry.unresolved = true;
    entry.restoredState = ShelfState::Missing;
    const auto &proof = *entry.proof;
    const std::pair<dev_t, VolumeProof> *matched = nullptr;
    for (const auto &volume : volumes) {
      if (!MatchesVolume(proof, volume.second))
        continue;
      if (matched) {
        entry.restoredState = ShelfState::Ambiguous;
        matched = nullptr;
        break;
      }
      matched = &volume;
    }
    if (!matched)
      continue;
    const auto &[device, volume] = *matched;
    struct stat st;
    if (lstat(entry.path.c_str(), &st) == 0) {
      if (st.st_dev == device && MatchesEntry(proof, st)) {
        Bind(entry, st);
        continue;
      }
      entry.restoredState = st.st_dev == device ? ShelfState::Replaced
                                                : ShelfState::Ambiguous;
    }
    ResolveInParent(entry, device, budget, reserved, volume.trash);
    if (entry.unresolved)
      ResolveByName(entry, device, budget, reserved, volume.trash);
    reserved.insert(entry.path);
  }
}
} // namespace Listi
