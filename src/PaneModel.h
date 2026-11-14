// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>
class BBitmap;
struct PaneEntry {
  std::string name, path;
  bool directory = false, parent = false;
  int kind = 0, transferState = 0;
  int64_t size = 0, modified = 0;
  float fraction = 0;
  std::shared_ptr<BBitmap> icon;
};
// Pane owns entries and selection. Renderers never own each other's state.
// All access occurs on the window thread; scanner messages carry values only.
class PaneModel {
public:
  void Clear();
  void Add(std::unique_ptr<PaneEntry> entry);
  void Filter(const std::string &query);
  const std::vector<PaneEntry *> &Visible() const { return visible; }
  size_t HiddenCount() const { return entries.size() - visible.size(); }
  bool IsSelected(const std::string &path) const { return selection.count(path); }
  std::vector<std::string> Selected() const;
  void Select(int mode);
  void Pick(int index, bool extend, bool toggle);
  void RestoreSelection(const std::vector<std::string> &paths);
  void SetSelection(const std::vector<std::string> &paths,
                    const std::string &focus);
  PaneEntry *Focused() const;
  void Transfer(const std::string &path, int state, float fraction);
private:
  std::vector<std::unique_ptr<PaneEntry>> entries;
  std::vector<PaneEntry *> visible;
  std::set<std::string> selection;
  std::string focused, anchor;
};
