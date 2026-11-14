// SPDX-License-Identifier: MIT
#pragma once
#include <MessageFilter.h>
#include <MessageRunner.h>
#include <ScrollBar.h>
#include <View.h>
#include <Window.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Shared gesture for both renderers. Coordinates stay in the scrolling content.
// The renderers supply hit testing and draw the outline in their normal paint pass.
class RubberBand : public BMessageFilter {
  static constexpr uint32 kTick = 'rbtk';
  BView *view;
  std::unique_ptr<BMessageRunner> timer;
  BPoint origin, cursor;
  std::vector<std::string> before;
  std::string location;
  bool active = false, moved = false, extend = false;

  void Update(BPoint point, bool scroll) {
    BPoint previous = cursor;
    bool wasMoved = moved;
    cursor = point;
    if (std::abs(cursor.x - origin.x) >= 4 || std::abs(cursor.y - origin.y) >= 4)
      moved = true;
    if (scroll && moved) {
      auto bounds = view->Bounds();
      float delta = cursor.y < bounds.top + 12 ? -18 :
                    cursor.y > bounds.bottom - 12 ? 18 : 0;
      if (delta) {
        if (auto bar = view->ScrollBar(B_VERTICAL)) {
          float old = bar->Value();
          bar->SetValue(old + delta);
          cursor.y += bar->Value() - old;
        }
      }
    }
    if (moved && (!wasMoved || cursor != previous)) {
      select(Rect(), extend ? before : std::vector<std::string>{});
      redraw();
    }
  }
  void End(bool restore) {
    if (!active) return;
    active = false;
    timer.reset();
    if (restore) restoreSelection(before);
    redraw();
  }

public:
  std::function<bool(BPoint)> empty;
  std::function<bool()> ready;
  std::function<std::string()> path;
  std::function<std::vector<std::string>()> selection;
  std::function<void(const std::vector<std::string> &)> restoreSelection;
  std::function<void(BRect, const std::vector<std::string> &)> select;
  std::function<void()> activate, redraw;

  explicit RubberBand(BView *target)
      : BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE), view(target) {}
  bool Active() const { return active && moved; }
  BRect Rect() const {
    return BRect(std::min(origin.x, cursor.x), std::min(origin.y, cursor.y),
                 std::max(origin.x, cursor.x), std::max(origin.y, cursor.y));
  }
  void Cancel() { End(false); }
  bool Escape() { bool was = active; End(true); return was; }
  filter_result Filter(BMessage *message, BHandler **) override {
    if (active && (path() != location || !ready() || view->IsHidden())) End(false);
    if (message->what == B_MOUSE_DOWN) {
      if (active) End(false);
      BPoint point;
      if (message->GetInt32("buttons", 0) != B_PRIMARY_MOUSE_BUTTON ||
          message->FindPoint("where", &point) != B_OK || !ready() || !empty(point))
        return B_DISPATCH_MESSAGE;
      activate();
      origin = cursor = point;
      before = selection();
      location = path();
      extend = modifiers() & (B_SHIFT_KEY | B_CONTROL_KEY | B_COMMAND_KEY);
      active = true;
      moved = false;
      if (!extend) restoreSelection({});
      view->SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
      BMessage tick(kTick);
      timer.reset(new BMessageRunner(BMessenger(view), &tick, 50000));
      return B_SKIP_MESSAGE;
    }
    if (message->what == kTick) {
      if (active) {
        BPoint point; uint32 buttons;
        view->GetMouse(&point, &buttons, false);
        if (!(buttons & B_PRIMARY_MOUSE_BUTTON)) End(false);
        else Update(point, true);
      }
      return B_SKIP_MESSAGE;
    }
    if (!active) return B_DISPATCH_MESSAGE;
    if (message->what == B_MOUSE_MOVED || message->what == B_MOUSE_UP) {
      BPoint point;
      if (message->FindPoint("where", &point) == B_OK) Update(point, false);
      if (message->what == B_MOUSE_UP) End(false);
      return B_SKIP_MESSAGE;
    }
    if (message->what == B_KEY_DOWN) {
      const char *bytes = message->GetString("bytes", "");
      End(bytes[0] == B_ESCAPE);
      return B_SKIP_MESSAGE;
    }
    return B_DISPATCH_MESSAGE;
  }
};
