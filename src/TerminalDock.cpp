// SPDX-License-Identifier: MIT
#include "TerminalDock.h"

#include <GroupLayout.h>
#include <Screen.h>
#include <SplitView.h>
#include <Window.h>
#include <algorithm>

namespace Listi {
namespace {

constexpr uint32 kFocusFloating = 'tlfc';

class FloatingTerminal : public BWindow {
public:
  FloatingTerminal(BRect frame, BMessenger receiver, int64 placement)
      : BWindow(frame, "Listi: CLI", B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS),
        owner(receiver), placement(placement) {
    SetLayout(new BGroupLayout(B_VERTICAL, 0));
    AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
  }

  TerminalPanel *Panel() const {
    return static_cast<TerminalPanel *>(ChildAt(0));
  }

  void DispatchMessage(BMessage *message, BHandler *target) override {
    if (message->what == B_QUIT_REQUESTED && target == this) {
      // The title-bar close button and Command+W dock the session. Haiku's
      // application shutdown instead calls QuitRequested/Quit directly.
      BMessage dock(kDockTerminal);
      dock.AddInt64("terminal placement", placement);
      owner.SendMessage(&dock, static_cast<BHandler *>(nullptr), 0);
      return;
    }
    BWindow::DispatchMessage(message, target);
  }

  void MessageReceived(BMessage *message) override {
    if (message->what == kFocusFloating) {
      if (IsMinimized())
        Minimize(false);
      Activate();
      if (auto *panel = Panel())
        panel->Focus();
      return;
    }
    BWindow::MessageReceived(message);
  }

private:
  const BMessenger owner;
  const int64 placement;
};

BRect FloatingFrame(BWindow *owner, const TerminalPanel &panel) {
  const BRect workingArea = BScreen(owner).Frame().InsetByCopy(20, 40);
  const BRect previous = owner->Frame();
  if (!workingArea.IsValid())
    return previous.OffsetByCopy(24, 24);
  const float width =
      std::min(workingArea.Width(), std::max(640.0f, panel.Bounds().Width()));
  const float height =
      std::min(workingArea.Height(), std::max(330.0f, panel.Bounds().Height()));
  const float left = std::clamp(previous.left + 35, workingArea.left,
                                workingArea.right - width);
  const float top = std::clamp(previous.top + 45, workingArea.top,
                               workingArea.bottom - height);
  return BRect(left, top, left + width, top + height);
}

} // namespace

TerminalDock::TerminalDock(BSplitView *split, BMessenger owner)
    : split(split), owner(owner), slot(new BGroupView(B_VERTICAL, 0)) {
  Docked();
  split->AddChild(slot, 0.32f);
  split->SetItemCollapsed(1, true);
}

TerminalDock::~TerminalDock() {
  if (detached.LockTarget()) {
    BLooper *looper;
    detached.Target(&looper);
    // Quit directly: requesting a normal window close would redock instead.
    looper->Quit();
  }
}

TerminalPanel *TerminalDock::Docked() {
  if (!panel && !detached.IsValid()) {
    panel = new TerminalPanel(owner);
    panel->SetPlacement(false, ++placement);
    slot->GroupLayout()->AddView(panel);
  }
  return panel;
}

bool TerminalDock::FocusFloating() {
  if (!detached.IsValid())
    return false;
  BMessage focus(kFocusFloating);
  detached.SendMessage(&focus, static_cast<BHandler *>(nullptr), 0);
  return true;
}

bool TerminalDock::Float(int64 requested) {
  if (!panel || requested != placement || split->IsItemCollapsed(1))
    return false;
  auto *window = new FloatingTerminal(FloatingFrame(split->Window(), *panel),
                                      owner, placement + 1);
  // Remove through the layout; BView::RemoveSelf can report failure after
  // removal of the last layout item has already detached the view.
  if (!slot->GroupLayout()->RemoveView(panel)) {
    window->Quit();
    return false;
  }
  if (!window->GetLayout()->AddView(panel)) {
    slot->GroupLayout()->AddView(panel);
    window->Quit();
    return false;
  }
  panel->SetPlacement(true, ++placement);
  split->SetItemCollapsed(1, true);
  panel->Focus();
  detached = BMessenger(window);
  panel = nullptr;
  window->Show();
  return true;
}

bool TerminalDock::ReturnPanel() {
  if (!detached.LockTarget())
    return false;
  BLooper *looper;
  detached.Target(&looper);
  auto *window = static_cast<FloatingTerminal *>(looper);
  auto *returning = window->Panel();
  if (!returning || !window->GetLayout()->RemoveView(returning)) {
    window->Unlock();
    return false;
  }
  if (!slot->GroupLayout()->AddView(returning)) {
    window->GetLayout()->AddView(returning);
    window->Unlock();
    return false;
  }
  panel = returning;
  panel->SetPlacement(false, ++placement);
  detached = {};
  window->Quit();
  return true;
}

bool TerminalDock::Dock(int64 requested) {
  if (requested != placement || !ReturnPanel())
    return false;
  split->SetItemCollapsed(1, false);
  split->Window()->Activate();
  panel->Focus();
  return true;
}

bool TerminalDock::Close(int64 requested) {
  if (requested != placement || (!panel && !ReturnPanel()))
    return false;
  panel->Stop();
  panel->SetPlacement(false, ++placement);
  split->SetItemCollapsed(1, true);
  return true;
}

} // namespace Listi
