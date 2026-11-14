// SPDX-License-Identifier: MIT
#include "NavigationHistory.h"
#include <iostream>
using Lister::NavigationHistory;
int main() {
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *name) {
    ++checks; if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
  };
  NavigationHistory h, other;
  int64_t serial = 0;
  auto visit = [&](const std::string &path, bool ok = true) {
    h.Begin(++serial, path); h.Complete(serial, path, ok);
  };
  auto travel = [&](int index, bool ok = true) {
    auto path = h.At(index);
    h.Begin(++serial, path, index); h.Complete(serial, path, ok);
  };
  check(h.Back() == -1 && h.Forward() == -1 && !h.Size(), "empty history has no actions");
  visit("A", false);
  check(!h.Size() && h.Back() == -1, "failed first visit creates no history");
  visit("A");
  check(h.Size() == 1 && h.Back() == -1 && h.Forward() == -1, "first successful visit anchors history");
  visit("B"); visit("C"); travel(h.Back());
  check(h.At(h.Position()) == "B" && h.At(h.Forward()) == "C", "Back from C restores B with Forward available");
  travel(h.Forward());
  check(h.At(h.Position()) == "C" && h.Size() == 3, "Forward traverses without duplicating visits");
  travel(h.Back()); visit("B");
  check(h.Size() == 3 && h.Forward() >= 0, "refresh preserves forward branch");
  h.Begin(++serial, "B/."); h.Complete(serial, "B", true);
  check(h.Size() == 3 && h.Forward() >= 0, "resolved alias of current folder is not a visit");
  visit("D", false);
  check(h.At(h.Back()) == "B" && h.Forward() == -1 && h.Size() == 3, "failed open returns to last valid place first");
  travel(h.Back());
  check(h.At(h.Position()) == "B" && h.At(h.Forward()) == "C", "recovering failed open keeps original forward branch");
  visit("D");
  check(h.Size() == 3 && h.At(h.Position()) == "D" && h.Forward() == -1, "successful new visit discards forward branch");
  travel(h.Back(), false);
  check(h.At(h.Position()) == "D" && h.At(h.Back()) == "D", "failed traversal never commits cursor or skips valid place");
  travel(h.Back());
  check(h.At(h.Position()) == "D", "explicit recovery after offline history target");
  h.Begin(++serial, "slow"); auto old = serial;
  h.Begin(++serial, "new");
  check(!h.Complete(old, "slow", true) && h.At(h.Position()) == "D", "late successful scan cannot commit");
  check(!h.Complete(old, "slow", false), "late failed scan cannot detach newer request");
  h.Complete(serial, "new", true);
  check(h.At(h.Position()) == "new" && h.Size() == 4, "only latest request becomes a visit");
  check(!h.Complete(serial, "duplicate", true) && h.Size() == 4, "duplicate completion is ignored");
  h.Begin(++serial, "cancelled");
  check(h.At(h.Back()) == "new", "Back during pending scan targets last committed place");
  travel(h.Back());
  check(h.Size() == 4 && h.At(h.Position()) == "new", "cancelled visit is not recorded");
  other.Begin(1, "Other"); other.Complete(1, "Other", true);
  travel(h.Back());
  check(other.Size() == 1 && other.At(other.Position()) == "Other" && other.Back() == -1, "pane histories remain independent");
  h.Begin(++serial, "D", -1, true); h.Complete(serial, "Trash", true);
  check(h.At(h.Position()) == "Trash", "worker-resolved Trash address commits as a new visit");
  visit("@disks");
  check(h.At(h.Position()) == "@disks", "volume overview is a revisitable location");
  NavigationHistory bounded;
  for (int i = 0; i < 65; ++i) {
    bounded.Begin(i + 1, std::to_string(i));
    bounded.Complete(i + 1, std::to_string(i), true);
  }
  check(bounded.Size() == 64 && bounded.At(0) == "1" && bounded.At(63) == "64", "65 visits retain only newest 64");
  for (int i = 62; i >= 0; --i) {
    bounded.Begin(100 + i, bounded.At(i), i);
    bounded.Complete(100 + i, bounded.At(i), true);
  }
  check(bounded.Back() == -1 && bounded.Forward() == 1, "bounded history stops at oldest retained visit");
  std::cout << checks << " checks, " << failures << " failures" << std::endl;
  return failures ? 1 : 0;
}
