// SPDX-License-Identifier: MIT
#include "FileDrop.h"
#include "FileClipboard.h"
#include "JobSnapshot.h"
#include "ArchiveJobs.h"
#include "VirtualFile.h"
#include <algorithm>
#include <Path.h>
#include <limits.h>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <cstdlib>
namespace Lister {
namespace {
constexpr const char *kSelection = "listi:drag-selection";
void Require(status_t s) {
  if (s != B_OK) throw std::runtime_error("The dragged file list is incomplete. Select the files again.");
}
}
BMessage MakeFileDrag(const std::vector<std::string> &paths) {
  if (std::any_of(paths.begin(), paths.end(), [](const auto &path) { return IsVirtual(path); })) {
    if (paths.empty() || paths.size() > 1024) throw std::runtime_error("Select between 1 and 1024 entries.");
    BMessage drag(B_SIMPLE_DATA);
    for (const auto &path : paths) {
      if (!IsVirtual(path)) throw std::runtime_error("Drag local and virtual selections separately.");
      Require(drag.AddString("listi:path", path.c_str()));
    }
    return drag;
  }
  auto selection = MakeFileSelection(paths);
  BMessage drag(B_SIMPLE_DATA);
  for (const auto &item : ReadFileClipboard(selection))
    Require(drag.AddRef("refs", &item.ref));
  Require(drag.AddMessage(kSelection, &selection));
  return drag;
}
Job PrepareFileDrop(const BMessage &message, const std::string &destination) {
  if (message.FlattenedSize() > 2 * 1024 * 1024)
    throw std::runtime_error("The dragged selection is too large.");
  // Preserve existing provider copy support, but never imply remote move safety.
  if (message.HasString("listi:path") || IsVirtual(destination)) {
    if (destination.empty() || destination == "@disks") throw std::runtime_error("Open a destination folder first.");
    Job job; job.destination = destination;
    if (message.HasString("listi:path")) {
      if (message.HasRef("refs")) throw std::runtime_error("Drag local and virtual selections separately.");
      const char *path;
      for (int32 i = 0; message.FindString("listi:path", i, &path) == B_OK; ++i) {
        if (i >= 1024 || !IsVirtual(path)) throw std::runtime_error("Invalid virtual drop selection.");
        job.sources.emplace_back(path);
      }
    } else {
      BMessage selection;
      if (message.FindMessage(kSelection, &selection) == B_OK) {
        for (const auto &item : ReadFileClipboard(selection)) job.sources.push_back(item.path);
      } else {
        entry_ref ref;
        for (int32 i = 0; message.FindRef("refs", i, &ref) == B_OK; ++i) {
          if (i >= 1024) throw std::runtime_error("Select at most 1024 entries.");
          BEntry entry(&ref, false); BPath path; Require(entry.GetPath(&path));
          job.sources.emplace_back(path.Path());
        }
        MakeFileSelection(job.sources);
      }
    }
    if (job.sources.empty()) throw std::runtime_error("No files were dropped.");
    FreezeLocalJob(job, destination);
    FreezeArchiveTargets(job);
    if (ParseLocation(destination).Archive()) return PrepareArchiveCopy(job);
    return job;
  }
  if (destination.empty() || destination[0] != '/')
    throw std::runtime_error("Drop into a local folder. Use Copy for network or archive destinations.");
  char canonical[PATH_MAX];
  struct stat st;
  if (!realpath(destination.c_str(), canonical) || stat(canonical, &st) || !S_ISDIR(st.st_mode))
    throw std::runtime_error("The drop destination is not an available folder.");
  Job job;
  job.destination = canonical;
  job.expectedDestination = {int32_t(st.st_dev), int64_t(st.st_ino)};
  type_code type; int32 count;
  if (message.GetInfo("refs", &type, &count) != B_OK || type != B_REF_TYPE || count < 1 || count > 1024)
    throw std::runtime_error("Drop between 1 and 1024 local entries.");
  std::vector<std::string> paths;
  for (int32 i = 0; i < count; ++i) {
    entry_ref ref; Require(message.FindRef("refs", i, &ref));
    BEntry entry(&ref, false); BPath path;
    Require(entry.GetPath(&path)); paths.emplace_back(path.Path());
  }
  BMessage selection;
  if (message.HasMessage(kSelection)) Require(message.FindMessage(kSelection, &selection));
  else selection = MakeFileSelection(paths);
  auto items = ReadFileClipboard(selection);
  std::set<std::string> remaining(paths.begin(), paths.end());
  if (remaining.size() != paths.size() || items.size() != paths.size())
    throw std::runtime_error("The dragged file selection does not match its references.");
  for (const auto &item : items) {
    if (!remaining.erase(item.path))
      throw std::runtime_error("The dragged file selection changed. Select it again.");
    if (Join(job.destination, Leaf(item.path)) == item.path)
      throw std::runtime_error("Drop into a different folder, not the source folder.");
    // realpath on directories detects aliases into the source tree as well.
    struct stat source;
    if (!lstat(item.path.c_str(), &source) && S_ISDIR(source.st_mode)) {
      char resolved[PATH_MAX];
      if (!realpath(item.path.c_str(), resolved)) throw std::runtime_error("The source folder is unavailable.");
      std::string root(resolved);
      if (job.destination == root || job.destination.compare(0, root.size()+1, root+"/") == 0)
        throw std::runtime_error("A folder cannot be dropped into itself or its contents.");
    }
    job.sources.push_back(item.path);
    job.expectedSources.emplace(item.path, item.node);
  }
  return job;
}
}
