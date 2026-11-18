// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Listi {
// Only the latest successful scan may commit a visit. Addresses are normalized
// by the caller, with authentication kept separately in the connection store.
class NavigationHistory {
public:
  static constexpr size_t Limit = 64;
  int Back() const { return cursor < 0 ? -1 : detached ? cursor : cursor - 1; }
  int Forward() const {
    return !detached && cursor >= 0 && cursor + 1 < int(visits.size())
               ? cursor + 1
               : -1;
  }
  const std::string &At(int index) const { return visits.at(index); }
  size_t Size() const { return visits.size(); }
  int Position() const { return cursor; }
  void Begin(int64_t serial, const std::string &requested, int target = -1,
             bool unresolved = false) {
    pending = serial;
    destination = target;
    waiting = true;
    detached = unresolved || cursor < 0 || requested != visits[cursor];
  }
  bool Complete(int64_t serial, const std::string &resolved, bool success) {
    if (!waiting || serial != pending)
      return false;
    waiting = false;
    if (!success) {
      detached = true;
      return true;
    }
    if (destination >= 0 && destination < int(visits.size())) {
      cursor = destination;
      visits[cursor] = resolved;
    } else if (cursor < 0 || visits[cursor] != resolved) {
      visits.resize(cursor + 1);
      visits.push_back(resolved);
      if (visits.size() > Limit)
        visits.erase(visits.begin());
      cursor = int(visits.size()) - 1;
    }
    detached = false;
    return true;
  }

private:
  std::vector<std::string> visits;
  int cursor = -1, destination = -1;
  int64_t pending = 0;
  bool waiting = false, detached = false;
};
} // namespace Listi
