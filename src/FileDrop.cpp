// SPDX-License-Identifier: MIT
#include "TransferCommand.h"
#include "FileDrop.h"
#include "FileClipboard.h"
#include "VirtualFile.h"
#include <algorithm>
#include <Path.h>
#include <limits.h>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <cstdlib>
namespace Listi {
namespace {
constexpr const char *kSelection = "listi:drag-selection";
void Require(status_t s) {
  if (s != B_OK)
    throw std::runtime_error(
        "The dragged file list is incomplete. Select the files again.");
}
std::vector<ClipboardItem> ReadDragRefs(const BMessage &message) {
  type_code type;
  int32 count;
  if (message.GetInfo("refs", &type, &count) != B_OK || type != B_REF_TYPE ||
      count < 1 || count > 1024)
    throw std::runtime_error("Drop between 1 and 1024 local entries.");
  std::vector<std::string> paths;
  for (int32 i = 0; i < count; ++i) {
    entry_ref ref;
    Require(message.FindRef("refs", i, &ref));
    BEntry entry(&ref, false);
    BPath path;
    Require(entry.GetPath(&path));
    paths.emplace_back(path.Path());
  }
  BMessage selection;
  if (message.HasMessage(kSelection))
    Require(message.FindMessage(kSelection, &selection));
  else
    selection = MakeFileSelection(paths);
  auto items = ReadFileClipboard(selection);
  std::set<std::string> remaining(paths.begin(), paths.end());
  if (remaining.size() != paths.size() || items.size() != paths.size())
    throw std::runtime_error(
        "The dragged file selection does not match its references.");
  for (const auto &item : items) {
    if (!remaining.erase(item.path))
      throw std::runtime_error(
          "The dragged file selection changed. Select it again.");
  }
  return items;
}
}
BMessage MakeFileDrag(const std::vector<std::string> &paths) {
  if (std::any_of(paths.begin(), paths.end(),
                  [](const auto &path) { return IsVirtual(path); })) {
    if (paths.empty() || paths.size() > 1024)
      throw std::runtime_error("Select between 1 and 1024 entries.");
    BMessage drag(B_SIMPLE_DATA);
    for (const auto &path : paths) {
      if (!IsVirtual(path))
        throw std::runtime_error(
            "Drag local and virtual selections separately.");
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
Job PrepareFileDrop(const BMessage &message, const std::string &destination,
                    bool allowSameFolder) {
  if (message.FlattenedSize() > 2 * 1024 * 1024)
    throw std::runtime_error("The dragged selection is too large.");
  Job job;
  job.destination = destination;
  if (message.HasString("listi:path")) {
    if (message.HasRef("refs"))
      throw std::runtime_error("Drag local and virtual selections separately.");
    const char *path;
    for (int32 i = 0; message.FindString("listi:path", i, &path) == B_OK; ++i) {
      if (i >= 1024 || !IsVirtual(path))
        throw std::runtime_error("Invalid virtual drop selection.");
      job.sources.emplace_back(path);
    }
  } else {
    for (const auto &item : ReadDragRefs(message)) {
      job.sources.push_back(item.path);
      job.expectedSources.emplace(item.path, item.node);
    }
  }
  if (!destination.empty() && destination[0] == '/') {
    char canonical[PATH_MAX];
    if (!realpath(destination.c_str(), canonical))
      throw std::runtime_error("The drop destination is unavailable.");
    job.destination = canonical;
  }
  return PrepareTransfer(std::move(job), allowSameFolder);
}
}
