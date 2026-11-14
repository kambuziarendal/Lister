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
  bigtime_t lastFrame = 0, lastScroll = 0;
  std::vector<std::string> before;
  std::string location;
  bool active = false, moved = false, extend = false;

  void Update(BPoint point, bool scroll) {
    BRect oldRect = Rect();
    BPoint previous = cursor;
    bool wasMoved = moved;
    cursor = point;
    if (std::abs(cursor.x - origin.x) >= 4 || std::abs(cursor.y - origin.y) >= 4)
      moved = true;
    bigtime_t now = system_time();
    if (scroll && moved && now - lastScroll >= 50000) {
      lastScroll = now;
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
      view->Window()->BeginViewTransaction();
      select(Rect(), extend ? before : std::vector<std::string>{});
      RedrawOutline(oldRect);
      RedrawOutline(Rect());
      view->Window()->EndViewTransaction();
    }
  }
  void End(bool restore) {
    if (!active) return;
    BRect oldRect = Rect();
    bool hadOutline = moved;
    active = false;
    timer.reset();
    if (restore) restoreSelection(before);
    if (hadOutline) RedrawOutline(oldRect);
    finished();
  }
  void RedrawOutline(BRect rect) {
    // Invalidate only the old/new border, never the entire file view.
    redraw(BRect(rect.left - 1, rect.top - 1, rect.right + 1, rect.top + 1));
    redraw(BRect(rect.left - 1, rect.bottom - 1, rect.right + 1, rect.bottom + 1));
    redraw(BRect(rect.left - 1, rect.top - 1, rect.left + 1, rect.bottom + 1));
    redraw(BRect(rect.right - 1, rect.top - 1, rect.right + 1, rect.bottom + 1));
  }

public:
  std::function<bool(BPoint)> empty;
  std::function<bool()> ready;
  std::function<std::string()> path;
  std::function<std::vector<std::string>()> selection;
  std::function<void(const std::vector<std::string> &)> restoreSelection;
  std::function<void(BRect, const std::vector<std::string> &)> select;
  std::function<void()> activate, finished;
  std::function<void(BRect)> redraw;

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
      lastFrame = lastScroll = system_time();
      if (!extend) restoreSelection({});
      view->SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
      BMessage tick(kTick);
      timer.reset(new BMessageRunner(BMessenger(view), &tick, 16667));
      return B_SKIP_MESSAGE;
    }
    if (message->what == kTick) {
      if (active && system_time() - lastFrame >= 12000) {
        lastFrame = system_time();
        BPoint point; uint32 buttons;
        view->GetMouse(&point, &buttons, false);
        Update(point, (buttons & B_PRIMARY_MOUSE_BUTTON) != 0);
        if (!(buttons & B_PRIMARY_MOUSE_BUTTON)) End(false);
      }
      return B_SKIP_MESSAGE;
    }
    if (!active) return B_DISPATCH_MESSAGE;
    // Pointer events can arrive much faster than the display. Sample the latest
    // position once per frame instead of replaying a growing movement backlog.
    if (message->what == B_MOUSE_MOVED) return B_SKIP_MESSAGE;
    if (message->what == B_MOUSE_UP) {
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
