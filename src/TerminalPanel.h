// SPDX-License-Identifier: MIT
#pragma once

#include <GroupView.h>
#include <Messenger.h>
#include <string>

class BStringView;
class BButton;

namespace Listi {

inline constexpr uint32 kTerminalBelow = 'tlbl';
inline constexpr uint32 kTerminalBeside = 'tlbs';
inline constexpr uint32 kToggleTerminalSide = 'tlts';
inline constexpr uint32 kTerminalWindow = 'tlwn';
inline constexpr uint32 kShowFloatingTerminal = 'tlof';
inline constexpr uint32 kCloseTerminal = 'tlcl';
inline constexpr uint32 kFloatTerminal = 'tlfl';
inline constexpr uint32 kDockTerminal = 'tldk';

class TerminalView;

class TerminalPanel : public BGroupView {
public:
  explicit TerminalPanel(BMessenger owner);
  const std::string &StartFolder() const { return startFolder; }
  int64 Placement() const { return placement; }
  int64 SessionId() const { return sessionId; }
  bool Active() const;
  bool HasFocus() const;
  void Focus();
  int Start(const char *directory);
  void Stop();
  bool HandleKey(BMessage *message);
  void SetPlacement(bool floating, int64 generation);
  void SetDockSide(bool beside);

private:
  std::string startFolder;
  int64 sessionId = 0;
  int64 placement = 0;
  bool floating = false;
  bool dockedBeside = false;
  BGroupLayout *header;
  BStringView *location;
  TerminalView *terminal;
  BButton *position, *move, *close;
};

} // namespace Listi
