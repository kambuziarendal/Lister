// SPDX-License-Identifier: MIT
#pragma once

#include <sys/types.h>

namespace Listi {

class TerminalSession {
public:
  TerminalSession() = default;
  ~TerminalSession();
  TerminalSession(const TerminalSession &) = delete;
  TerminalSession &operator=(const TerminalSession &) = delete;

  // Returns zero on success or an errno value.
  int Start(const char *directory, int rows, int columns);
  void Stop();
  void Resize(int rows, int columns);
  int Descriptor() const { return master; }
  bool Reap();

private:
  int master = -1;
  pid_t shell = -1;
};

} // namespace Listi
