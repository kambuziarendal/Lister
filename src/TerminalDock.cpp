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
  FloatingTerminal(BRect frame, BMessenger receiver, int lister,
                   int64 placement)
      : BWindow(frame, TerminalName(lister), B_TITLED_WINDOW,
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

TerminalDock::TerminalDock(BSplitView *split, BView *workspace,
                           BMessenger owner)
    : split(split), owner(owner), band(new BGroupView(B_HORIZONTAL, 6)) {
  split->AddChild(workspace, 0.68f);
  split->SetCollapsible(0, false);
  split->AddChild(band, 0.32f);
  for (int lister = 0; lister < 2; ++lister) {
    auto &session = sessions[lister];
    session.panel = new TerminalPanel(owner, lister);
    session.panel->SetPlacement(false, session.placement = ++generation);
    band->GroupLayout()->AddView(session.panel);
    session.panel->Hide();
  }
  UpdateVisibility();
}

TerminalDock::~TerminalDock() {
  for (auto &session : sessions) {
    if (session.detached.LockTarget()) {
      BLooper *looper;
      session.detached.Target(&looper);
      // Quit directly: requesting a normal window close would redock instead.
      looper->Quit();
    }
  }
}

TerminalPanel *TerminalDock::Docked(int lister) const {
  return sessions[lister].panel;
}

bool TerminalDock::HasFocus() const {
  for (const auto &session : sessions)
    if (session.visible && session.panel->HasFocus())
      return true;
  return false;
}

int TerminalDock::FindSession(int64 requested) const {
  for (int lister = 0; lister < 2; ++lister)
    if (sessions[lister].placement == requested)
      return lister;
  return -1;
}

void TerminalDock::UpdateVisibility() {
  split->SetItemCollapsed(split->GetLayout()->IndexOfView(band),
                          !sessions[0].visible && !sessions[1].visible);
}

void TerminalDock::Show(int lister) {
  auto &session = sessions[lister];
  if (!session.visible) {
    session.panel->Show();
    session.visible = true;
  }
  UpdateVisibility();
}

void TerminalDock::SetSide(TerminalSide requested) {
  if (side == requested)
    return;
  TerminalPanel *focused = nullptr;
  for (auto &session : sessions)
    if (session.panel && session.panel->HasFocus())
      focused = session.panel;
  const auto leading = [](TerminalSide position) {
    return position == TerminalSide::Above || position == TerminalSide::Left;
  };
  if (leading(side) != leading(requested)) {
    const int32 oldIndex = split->GetLayout()->IndexOfView(band);
    const float weight = split->ItemWeight(oldIndex);
    split->GetLayout()->RemoveView(band);
    split->Window()->Sync();
    split->AddChild(leading(requested) ? 0 : 1, band, weight);
  }
  side = requested;
  const bool lateral = IsSideDock(side);
  split->SetOrientation(lateral ? B_HORIZONTAL : B_VERTICAL);
  band->GroupLayout()->SetOrientation(lateral ? B_VERTICAL : B_HORIZONTAL);
  for (auto &session : sessions)
    if (session.panel)
      session.panel->SetDockSide(side);
  UpdateVisibility();
  if (focused)
    focused->Focus();
}

bool TerminalDock::Move(int64 requested) {
  const int lister = FindSession(requested);
  if (lister < 0 || !sessions[lister].visible)
    return false;
  switch (side) {
  case TerminalSide::Below:
    SetSide(TerminalSide::Right);
    break;
  case TerminalSide::Right:
    SetSide(TerminalSide::Above);
    break;
  case TerminalSide::Above:
    SetSide(TerminalSide::Left);
    break;
  case TerminalSide::Left:
    SetSide(TerminalSide::Below);
    break;
  }
  sessions[lister].panel->Focus();
  return true;
}

bool TerminalDock::FocusFloating(int lister) {
  auto &detached = sessions[lister].detached;
  if (!detached.IsValid())
    return false;
  BMessage focus(kFocusFloating);
  detached.SendMessage(&focus, static_cast<BHandler *>(nullptr), 0);
  return true;
}

bool TerminalDock::Float(int64 requested) {
  const int lister = FindSession(requested);
  if (lister < 0)
    return false;
  auto &session = sessions[lister];
  auto *panel = session.panel;
  if (!panel || !session.visible)
    return false;
  auto *window = new FloatingTerminal(FloatingFrame(split->Window(), *panel),
                                      owner, lister, generation + 1);
  // Remove through the layout; BView::RemoveSelf can report failure after
  // removal of the last layout item has already detached the view.
  if (!band->GroupLayout()->RemoveView(panel)) {
    window->Quit();
    return false;
  }
  // app_server tracks view tokens across the application, not per window.
  // Finish removing the old server views before creating their counterparts.
  split->Window()->Sync();
  if (!window->GetLayout()->AddView(panel)) {
    const int32 index = lister == 0 || !sessions[0].panel ? 0 : 1;
    band->GroupLayout()->AddView(index, panel);
    window->Quit();
    return false;
  }
  panel->SetPlacement(true, session.placement = ++generation);
  panel->Focus();
  session.detached = BMessenger(window);
  session.panel = nullptr;
  session.visible = false;
  UpdateVisibility();
  window->Show();
  return true;
}

bool TerminalDock::ReturnPanel(int lister) {
  auto &session = sessions[lister];
  if (!session.detached.LockTarget())
    return false;
  BLooper *looper;
  session.detached.Target(&looper);
  auto *window = static_cast<FloatingTerminal *>(looper);
  auto *returning = window->Panel();
  if (!returning || !window->GetLayout()->RemoveView(returning)) {
    window->Unlock();
    return false;
  }
  window->Sync();
  const int32 index = lister == 0 || !sessions[0].panel ? 0 : 1;
  if (!band->GroupLayout()->AddView(index, returning)) {
    window->GetLayout()->AddView(returning);
    window->Unlock();
    return false;
  }
  session.panel = returning;
  session.visible = true;
  returning->SetDockSide(side);
  returning->SetPlacement(false, session.placement = ++generation);
  session.detached = {};
  window->Quit();
  return true;
}

bool TerminalDock::Dock(int64 requested) {
  const int lister = FindSession(requested);
  if (lister < 0 || !ReturnPanel(lister))
    return false;
  UpdateVisibility();
  split->Window()->Activate();
  sessions[lister].panel->Focus();
  return true;
}

bool TerminalDock::Close(int64 requested) {
  const int lister = FindSession(requested);
  if (lister < 0)
    return false;
  auto &session = sessions[lister];
  if (!session.visible && !ReturnPanel(lister))
    return false;
  session.panel->Stop();
  session.panel->SetPlacement(false, session.placement = ++generation);
  session.panel->Hide();
  session.visible = false;
  UpdateVisibility();
  return true;
}

} // namespace Listi
