// SPDX-License-Identifier: MIT
#include "PaneModel.h"
#include "NameFilter.h"
#include <algorithm>
#include <limits>
#include <strings.h>
void PaneModel::Clear() {
  visible.clear();
  entries.clear();
  selection.clear();
  focused.clear();
  anchor.clear();
}
void PaneModel::Add(std::unique_ptr<PaneEntry> entry) {
  entries.push_back(std::move(entry));
}
void PaneModel::Filter(const std::string &query) {
  visible.clear();
  for (auto &entry : entries) {
    if (Lister::MatchesNameFilter(entry->name.c_str(), query.c_str(), entry->parent))
      visible.push_back(entry.get());
    else {
      selection.erase(entry->path);
      if (focused == entry->path)
        focused.clear();
      if (anchor == entry->path)
        anchor.clear();
    }
  }
  std::stable_sort(visible.begin(), visible.end(), [](auto a, auto b) {
    if (a->parent != b->parent)
      return a->parent;
    if ((a->kind == 1) != (b->kind == 1))
      return a->kind == 1;
    int order = strcasecmp(a->name.c_str(), b->name.c_str());
    return order ? order < 0 : a->path < b->path;
  });
}
std::vector<std::string> PaneModel::Selected() const {
  std::vector<std::string> result;
  for (auto entry : visible)
    if (!entry->parent && IsSelected(entry->path))
      result.push_back(entry->path);
  return result;
}
SelectionSummary PaneModel::Summary() const {
  SelectionSummary result;
  // Cached listing metadata only: no filesystem/network calls during selection.
  for (auto entry : visible) {
    if (entry->parent) continue;
    ++result.visible;
    if (!IsSelected(entry->path)) continue;
    ++result.selected;
    if (entry->kind == 2) { ++result.links; continue; }
    if (entry->directory) { ++result.folders; continue; }
    ++result.files;
    if (!entry->sizeKnown || entry->size < 0) { ++result.unknown; continue; }
    const auto bytes = static_cast<uint64_t>(entry->size);
    if (bytes > std::numeric_limits<uint64_t>::max() - result.bytes) {
      result.bytes = std::numeric_limits<uint64_t>::max();
      result.overflow = true;
    } else result.bytes += bytes;
  }
  return result;
}
void PaneModel::Select(int mode) {
  if (mode == 0) {
    selection.clear();
    return;
  }
  for (auto entry : visible) {
    if (entry->parent)
      continue;
    if (mode == 2 && IsSelected(entry->path))
      selection.erase(entry->path);
    else
      selection.insert(entry->path);
  }
}
void PaneModel::Pick(int index, bool extend, bool toggle) {
  if (index < 0 || size_t(index) >= visible.size())
    return;
  auto row = visible[index];
  if (extend && !toggle) {
    auto start = std::find_if(visible.begin(), visible.end(),
                             [&](auto e) { return e->path == anchor; });
    int from = start == visible.end() ? index : int(start - visible.begin());
    if (start == visible.end())
      anchor = row->path;
    selection.clear();
    for (int i = std::min(from, index); i <= std::max(from, index); ++i)
      if (!visible[i]->parent)
        selection.insert(visible[i]->path);
  } else {
    if (!toggle)
      selection.clear();
    if (toggle && IsSelected(row->path))
      selection.erase(row->path);
    else if (!row->parent)
      selection.insert(row->path);
    anchor = row->path;
  }
  focused = row->path;
}
void PaneModel::RestoreSelection(const std::vector<std::string> &paths) {
  const std::set<std::string> wanted(paths.begin(), paths.end());
  for (auto entry : visible)
    if (!entry->parent && wanted.count(entry->path))
      selection.insert(entry->path);
}
void PaneModel::SetSelection(const std::vector<std::string> &paths,
                             const std::string &focus) {
  selection.clear();
  RestoreSelection(paths);
  focused = focus;
  anchor = focus;
}
PaneEntry *PaneModel::Focused() const {
  for (auto entry : visible)
    if (entry->path == focused)
      return entry;
  for (auto entry : visible)
    if (IsSelected(entry->path))
      return entry;
  return nullptr;
}
void PaneModel::Transfer(const std::string &path, int state, float fraction) {
  for (auto &entry : entries)
    if (entry->path == path) {
      entry->transferState = state;
      entry->fraction = fraction;
    }
}
