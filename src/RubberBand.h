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

  bool painting = false;
  void Update(BPoint point, bool scroll) {
    BPoint previous = cursor;
    bool wasMoved = moved;
    auto now = system_time();
    auto bounds = view->Bounds();
    float delta = 0;
    if (scroll && now - lastScroll >= 50000) {
      lastScroll = now;
      delta = point.y < bounds.top + 12      ? -18
              : point.y > bounds.bottom - 12 ? 18
                                             : 0;
    }
    if (point == cursor && (!moved || !delta))
      return;
    painting = true;
    if (moved)
      outline(Rect()); // Erase the previous invert-mode border.
    cursor = point;
    if (std::abs(cursor.x - origin.x) >= 4 ||
        std::abs(cursor.y - origin.y) >= 4)
      moved = true;
    if (moved && delta) {
      if (auto bar = view->ScrollBar(B_VERTICAL)) {
        float old = bar->Value();
        bar->SetValue(old + delta);
        cursor.y += bar->Value() - old;
      }
    }
    if (moved && (!wasMoved || cursor != previous))
      select(Rect(), extend ? before : std::vector<std::string>{});
    // Finish any changed rows before overlaying the new border. Moving only
    // the border never invalidates or redraws the underlying native file list.
    view->Window()->UpdateIfNeeded();
    painting = false;
    if (moved)
      outline(Rect());
    view->Flush();
  }
  void End(bool restore) {
    if (!active)
      return;
    if (moved)
      outline(Rect());
    active = false;
    timer.reset();
    if (restore)
      restoreSelection(before);
    finished();
    view->Flush();
  }

public:
  std::function<bool(BPoint)> empty;
  std::function<bool()> ready;
  std::function<std::string()> path;
  std::function<std::vector<std::string>()> selection;
  std::function<void(const std::vector<std::string> &)> restoreSelection;
  std::function<void(BRect, const std::vector<std::string> &)> select;
  std::function<void()> activate, finished;
  std::function<void(BRect)> outline;

  explicit RubberBand(BView *target)
      : BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE), view(target) {}
  bool Active() const { return active && moved && !painting; }
  BRect Rect() const {
    return BRect(std::min(origin.x, cursor.x), std::min(origin.y, cursor.y),
                 std::max(origin.x, cursor.x), std::max(origin.y, cursor.y));
  }
  void Cancel() { End(false); }
  bool Escape() {
    bool was = active;
    End(true);
    return was;
  }
  filter_result Filter(BMessage *message, BHandler **) override {
    if (active && (path() != location || !ready() || view->IsHidden()))
      End(false);
    if (message->what == B_MOUSE_DOWN) {
      if (active)
        End(false);
      BPoint point;
      if (message->GetInt32("buttons", 0) != B_PRIMARY_MOUSE_BUTTON ||
          message->FindPoint("where", &point) != B_OK || !ready() ||
          !empty(point))
        return B_DISPATCH_MESSAGE;
      activate();
      origin = cursor = point;
      before = selection();
      location = path();
      extend = modifiers() & (B_SHIFT_KEY | B_CONTROL_KEY | B_COMMAND_KEY);
      active = true;
      moved = false;
      lastFrame = lastScroll = system_time();
      if (!extend)
        restoreSelection({});
      view->SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
      BMessage tick(kTick);
      timer.reset(new BMessageRunner(BMessenger(view), &tick, 16667));
      return B_SKIP_MESSAGE;
    }
    if (message->what == kTick) {
      if (active && system_time() - lastFrame >= 12000) {
        lastFrame = system_time();
        BPoint point;
        uint32 buttons;
        view->GetMouse(&point, &buttons, false);
        Update(point, (buttons & B_PRIMARY_MOUSE_BUTTON) != 0);
        // Drop overdue ticks after a slow paint instead of replaying them and
        // starving input and window messages with consecutive redraws.
        lastFrame = system_time();
        if (!(buttons & B_PRIMARY_MOUSE_BUTTON))
          End(false);
      }
      return B_SKIP_MESSAGE;
    }
    if (!active)
      return B_DISPATCH_MESSAGE;
    // Pointer events can arrive much faster than the display. Sample the latest
    // position once per frame instead of replaying a growing movement backlog.
    if (message->what == B_MOUSE_MOVED)
      return B_SKIP_MESSAGE;
    if (message->what == B_MOUSE_UP) {
      BPoint point;
      if (message->FindPoint("where", &point) == B_OK)
        Update(point, false);
      if (message->what == B_MOUSE_UP)
        End(false);
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
