// SPDX-License-Identifier: MIT
#include "PaneModel.h"
#include <cstdio>
#include <limits>
#include <stdexcept>
static void check(bool ok, const char* label) {
  if (!ok) throw std::runtime_error(label);
  printf("PASS %s\n", label);
}
int main() {
  PaneModel model;
  auto add = [&](const char* name, int64_t bytes, int kind = 0, bool known = true, bool parent = false) {
    auto e = std::make_unique<PaneEntry>();
    e->name = name; e->path = name; e->size = bytes; e->kind = kind;
    e->directory = kind != 0; e->sizeKnown = known; e->parent = parent;
    model.Add(std::move(e));
  };
  add("..", 999, 1, true, true); add("a", 1024); add("b", 2048);
  add("empty", 0); add("folder", 8192, 1); add("link", 123, 2);
  add("unknown", 777, 0, false); add("negative", -1);
  model.Filter(""); auto s = model.Summary();
  check(s.visible == 7 && s.selected == 0 && s.bytes == 0, "empty selection excludes parent from visible count");
  model.SetSelection({"a", "b", "a", "..", "missing"}, "a"); s = model.Summary();
  check(s.selected == 2 && s.files == 2 && s.bytes == 3072, "selection sums exact cached bytes without duplicates or parent");
  model.RestoreSelection({"folder", "link"}); s = model.Summary();
  check(s.selected == 4 && s.folders == 1 && s.links == 1 && s.bytes == 3072, "folders and directory links counted separately without target sizes");
  model.RestoreSelection({"unknown", "negative"}); s = model.Summary();
  check(s.files == 4 && s.unknown == 2 && s.bytes == 3072, "unavailable sizes never masquerade as zero or subtract bytes");
  model.Filter("a"); s = model.Summary();
  check(s.selected == 2 && s.bytes == 1024 && s.unknown == 1, "filter removes hidden sizes from selection");
  model.Filter(""); model.SetSelection({"empty"}, "empty");s = model.Summary();
  check(s.files == 1 && s.bytes == 0 && s.unknown == 0, "empty file has a known zero size");
  model.Select(0); check(model.Summary().selected == 0, "clear removes selection totals");
  model.Clear(); check(model.Summary().visible == 0, "navigation clears cached totals");
  add("huge-a", INT64_MAX); add("huge-b", INT64_MAX); add("huge-c", INT64_MAX);
  model.Filter("");model.Select(1);s = model.Summary();
  check(s.overflow && s.bytes == std::numeric_limits<uint64_t>::max(), "huge totals saturate rather than wrap");
  puts("9 selection summary checks passed");
}
