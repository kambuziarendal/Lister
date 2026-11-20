// SPDX-License-Identifier: MIT
#include "PaneModel.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
static void check(bool ok, const char* label) {
  if (!ok) throw std::runtime_error(label);
  printf("PASS %s\n", label);
}
int main() {
  PaneModel model;
  std::vector<std::string> paths;
  for (int i = 0; i < 10000; ++i) {
    auto e = std::make_unique<PaneEntry>();
    char name[24]; snprintf(name, sizeof(name), "item-%05d", i);
    e->name = name; e->path = std::string("/fixture/") + name;
    paths.push_back(e->path); model.Add(std::move(e));
  }
  auto parent = std::make_unique<PaneEntry>();
  parent->name = ".."; parent->path = "/fixture"; parent->parent = true;
  model.Add(std::move(parent)); model.Filter("");
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 20; ++i) model.SetSelection(paths, paths.back());
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
  printf("20 full selections of 10000 entries: %lld us\n", (long long)us);
  check(model.Selected() == paths, "large selection preserves visible order");
  check(!model.OnlySelected(), "multiple rows are not a single inspection target");
  check(model.Focused()->path == paths.back(), "focus preserved");
  model.SetSelection({paths[0], paths[0], "/fixture", "/missing"}, paths[0]);
  check(model.Selected() == std::vector<std::string>{paths[0]}, "duplicate, parent and absent entries excluded");
  check(model.OnlySelected() && model.OnlySelected()->path == paths[0], "one visible target");
  model.RestoreSelection({paths[1], paths[1]});
  check(model.Selected().size() == 2, "restore extends existing selection");
  check(model.MatchPattern("*", false) == paths.size(), "wildcard excludes the parent row");
  const auto beforePreview = model.Selected();
  check(model.MatchPattern("item-0000?", false) == 10 &&
            model.MatchPattern("ITEM-*", false) == 0 &&
            model.MatchPattern("fixture/*", false) == 0,
        "wildcard uses case-sensitive whole names, not paths");
  check(model.Selected() == beforePreview, "preview leaves selection untouched");
  check(model.MatchPattern("item-0000?", true) == 10 &&
            model.Selected() == std::vector<std::string>(paths.begin(), paths.begin() + 10) &&
            model.Focused()->path == paths.front(),
        "confirmed pattern replaces selection and focuses the first match");
  const auto matched = model.Selected();
  check(model.MatchPattern("absent*", true) == 0 && model.Selected() == matched,
        "zero matches preserve the current selection");
  model.Filter("item-00000");
  check(model.MatchPattern("*", true) == 1 &&
            model.Selected() == std::vector<std::string>{paths[0]},
        "pattern selection respects the current literal filter");
  model.SetSelection(paths, paths[0]);
  check(model.Selected() == std::vector<std::string>{paths[0]}, "hidden entries excluded");
  auto duplicate = std::make_unique<PaneEntry>();
  duplicate->name = "item-00000";
  duplicate->path = paths[0];
  model.Add(std::move(duplicate));
  model.Filter("item-00000");
  check(model.Selected().size() == 2 && !model.OnlySelected(), "same-path rows are ambiguous");
  model.SetSelection({}, "");
  check(model.Selected().empty() && model.Focused() == nullptr, "empty selection clears focus");
}
