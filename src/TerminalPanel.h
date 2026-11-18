// SPDX-License-Identifier: MIT
#pragma once

#include <GroupView.h>
#include <string>

class BStringView;

namespace Listi {

inline constexpr uint32 kTerminalBelow = 'tlbl';
inline constexpr uint32 kTerminalBeside = 'tlbs';
inline constexpr uint32 kTerminalWindow = 'tlwn';
inline constexpr uint32 kCloseTerminal = 'tlcl';

class TerminalView;

class TerminalPanel : public BGroupView {
public:
  TerminalPanel();
  const std::string &StartFolder() const { return startFolder; }
  int64 SessionId() const { return sessionId; }
  bool Active() const;
  bool HasFocus() const;
  void Focus();
  int Start(const char *directory);
  void Stop();
  bool HandleKey(BMessage *message);

private:
  std::string startFolder;
  int64 sessionId = 0;
  BStringView *location;
  TerminalView *terminal;
};

} // namespace Listi
