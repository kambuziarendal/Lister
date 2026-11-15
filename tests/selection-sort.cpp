// SPDX-License-Identifier: MIT
#include "PaneModel.h"
#include <cstdio>
#include <stdexcept>
static void check(bool ok, const char *label) {
  if (!ok) throw std::runtime_error(label);
  printf("PASS %s\n", label);
}
int main() {
  PaneModel model;
  auto add = [&](const char *name, int size, int date, bool directory = false) {
    auto e = std::make_unique<PaneEntry>();
    e->name = e->path = name; e->size = size; e->modified = date;
    e->directory = directory; e->kind = directory ? 1 : 0;
    model.Add(std::move(e));
  };
  add("alpha", 300, 10); add("beta", 100, 30); add("gamma", 200, 20);
  model.Filter(""); model.SetSelection({"alpha", "gamma"}, "gamma");
  model.Sort({1, true});
  check(model.Visible()[0]->name == "beta" && model.Visible()[2]->name == "alpha", "numeric size ascending");
  model.Sort({1, false});
  check(model.Visible()[0]->name == "alpha" && model.Visible()[2]->name == "beta", "numeric size descending");
  model.Sort({2, true});
  check(model.Visible()[0]->name == "alpha" && model.Visible()[2]->name == "beta", "modified ascending");
  model.Sort({2, false});
  check(model.Visible()[0]->name == "beta" && model.Visible()[2]->name == "alpha", "modified descending");
  check(model.IsSelected("alpha") && model.IsSelected("gamma") && model.Focused()->path == "gamma", "sorting preserves selected paths and focus");
  model.Filter("a");
  check(model.Visible()[0]->name == "beta", "filter retains chosen sort");
  add("delta", 50, 40); model.Filter("");
  check(model.Visible()[0]->name == "delta", "scan batch retains sort");
  model.Sort({0, false});
  check(model.Visible()[0]->name == "gamma", "name descending");
  model.Sort({0, true});
  check(model.Visible()[0]->name == "alpha", "name ascending");
  model.Clear(); add("folder", 1000, 1, true); add("empty", 0, 2);
  model.Filter(""); model.Sort({1, true});
  check(model.Visible()[0]->name == "folder", "directory size is unavailable, not recursive byte count");
  puts("10 sorting checks passed");
}
