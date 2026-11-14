// SPDX-License-Identifier: MIT
#include "FileDrop.h"
#include "FileGrid.h"
#include "Pane.h"
#include "PaneModel.h"
#include "ImagePreview.h"
#include "BitmapMailbox.h"
#include "VirtualFile.h"
#include <Bitmap.h>
#include <Entry.h>
#include <ScrollBar.h>
#include <Window.h>
#include <algorithm>
#include <set>
namespace {
constexpr uint32 kThumbnail = 'thum', kThumbnailReady = 'thrd';
class ThumbnailLoader : public BLooper {
  BMessenger target;
  std::shared_ptr<BitmapMailbox> replies;
  std::shared_ptr<std::atomic<int64>> generation;

public:
  ThumbnailLoader(BMessenger t, std::shared_ptr<std::atomic<int64>> g,
                  std::shared_ptr<BitmapMailbox> r)
      : BLooper("thumbnail reader"), target(t), replies(r), generation(g) {}
  void MessageReceived(BMessage *m) override {
    if (m->what != kThumbnail) {
      BLooper::MessageReceived(m);
      return;
    }
    int64 serial = m->GetInt64("serial", -1);
    Lister::TransferControl c;
    c.proceed = [&] { return generation->load() == serial; };
    BBitmap *bitmap = nullptr;
    try {
      c.Check();
      bitmap = LoadPreviewBitmap(m->GetString("path", ""), c, 128);
    } catch (const Lister::Cancelled &) {
      return;
    } catch (const std::exception &) {
    }
    BMessage done(kThumbnailReady);
    done.AddString("path", m->GetString("path", ""));
    done.AddInt64("serial", serial);
    auto reply = replies->Put(bitmap);
    done.AddInt64("bitmap", reply);
    if (target.SendMessage(&done, (BHandler *)nullptr, 500000) != B_OK)
      replies->Take(reply);
  }
};
} // namespace
class GridView : public FileGrid {
  Pane *pane;
  PaneModel *model;
  int mode = 1;
  float cellWidth = 112, cellHeight = 100;
  BLooper *loader = nullptr;
  std::shared_ptr<BitmapMailbox> replies = std::make_shared<BitmapMailbox>(64);
  std::shared_ptr<std::atomic<int64>> generation;
  std::map<std::string, std::unique_ptr<BBitmap>> thumbnails;
  std::set<std::string> pending;
  BPoint pressed;
  int pressedIndex = -1;
  bool dragging = false, deferredPick = false;
  int Columns() const {
    return std::max(1, int((Bounds().Width() + 1) / cellWidth));
  }
  BRect Cell(int index) const {
    int cols = Columns();
    float width = (Bounds().Width() + 1) / cols;
    float left = index % cols * width, top = index / cols * cellHeight;
    return BRect(left, top, left + width - 1, top + cellHeight - 1);
  }
  bool Selected(PaneEntry *row) const { return model->IsSelected(row->path); }
  void Pick(int index, bool extend, bool toggle = false) {
    if (index < 0 || index >= int(model->Visible().size()))
      return;
    model->Pick(index, extend, toggle);
    BRect cell = Cell(index);
    if (cell.top < Bounds().top)
      ScrollTo(0, cell.top);
    else if (cell.bottom > Bounds().bottom)
      ScrollTo(0, cell.bottom - Bounds().Height());
    pane->SendSelection();
    Invalidate();
  }

public:
  GridView(Pane *p, PaneModel *state)
      : FileGrid("file grid", B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS |
                               B_FULL_UPDATE_ON_RESIZE),
        pane(p), model(state), generation(std::make_shared<std::atomic<int64>>(0)) {
    SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
  }
  ~GridView() {
    generation->store(-1);
    if (loader)
      loader->PostMessage(B_QUIT_REQUESTED);
  }
  void AttachedToWindow() override {
    loader = new ThumbnailLoader(BMessenger(this), generation, replies);
    loader->Run();
    Update();
  }
  void Reset() {
    pressedIndex = -1;
    dragging = deferredPick = false;
    ++*generation;
    replies->Clear();
    pending.clear();
    thumbnails.clear();
    ScrollTo(0, 0);
    Update();
  }
  void SetMode(int value) {
    if (mode != value) {
      mode = value;
      Reset();
    }
    cellWidth = mode == 1 ? 112 : 156;
    cellHeight = mode == 1 ? 100 : 154;
    Update();
  }
  void Update() {
    if (auto bar = ScrollBar(B_VERTICAL)) {
      float height =
          ((int(model->Visible().size()) + Columns() - 1) / Columns()) * cellHeight;
      float visible = Bounds().Height() + 1;
      bar->SetRange(0, std::max(0.f, height - visible));
      bar->SetProportion(height > 0 ? std::min(1.f, visible / height) : 1.f);
      bar->SetSteps(cellHeight, visible);
    }
    Invalidate();
  }
  void FrameResized(float, float) override { Update(); }
  void MakeFocus(bool focus = true) override {
    BView::MakeFocus(focus);
    if (focus)
      pane->Activate();
    Invalidate();
  }
  void MouseDown(BPoint point) override {
    MakeFocus();
    int index = int(point.y / cellHeight) * Columns() +
                std::min(Columns() - 1,
                         int(point.x / ((Bounds().Width() + 1) / Columns())));
    int32 mods = modifiers();
    int32 clicks = Window()->CurrentMessage()->GetInt32("clicks", 1);
    pressed = point;
    pressedIndex = index;
    dragging = false;
    deferredPick = index >= 0 && index < int(model->Visible().size()) &&
                   model->IsSelected(model->Visible()[index]->path) &&
                   !(mods & (B_SHIFT_KEY | B_COMMAND_KEY | B_CONTROL_KEY)) &&
                   clicks == 1;
    if (!deferredPick)
      Pick(index, mods & B_SHIFT_KEY, mods & (B_COMMAND_KEY | B_CONTROL_KEY));
    SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
    if (clicks == 2 && index >= 0 && index < int(model->Visible().size())) {
      BMessage invoke(kInvoke);
      pane->MessageReceived(&invoke);
    }
  }
  void MouseMoved(BPoint point, uint32, const BMessage *) override {
    if (dragging || pressedIndex < 0 ||
        pressedIndex >= int(model->Visible().size()) ||
        (std::abs(point.x - pressed.x) < 5 && std::abs(point.y - pressed.y) < 5))
      return;
    BPoint current;
    uint32 buttons;
    GetMouse(&current, &buttons);
    if (!(buttons & B_PRIMARY_MOUSE_BUTTON))
      return;
    BMessage drag;
    try { drag = Lister::MakeFileDrag(model->Selected()); }
    catch (const std::exception &e) { pane->ShowStatus(e.what()); return; }
    dragging = true;
    DragMessage(&drag, BRect(point.x, point.y, point.x + 90, point.y + 22));
  }
  void MouseUp(BPoint) override {
    if (deferredPick && !dragging)
      Pick(pressedIndex, false);
    pressedIndex = -1;
    dragging = deferredPick = false;
  }
  void KeyDown(const char *bytes, int32 length) override {
    if (length == 1) {
      int index = 0;
      auto focused = model->Focused();
      for (int i = 0; i < int(model->Visible().size()); ++i)
        if (model->Visible()[i] == focused) {
          index = i;
          break;
        }
      int change = 0;
      switch (bytes[0]) {
      case B_LEFT_ARROW:
        change = -1;
        break;
      case B_RIGHT_ARROW:
        change = 1;
        break;
      case B_UP_ARROW:
        change = -Columns();
        break;
      case B_DOWN_ARROW:
        change = Columns();
        break;
      case B_HOME:
        Pick(0, false);
        return;
      case B_END:
        Pick(int(model->Visible().size()) - 1, false);
        return;
      case B_ENTER: {
        BMessage invoke(kInvoke);
        pane->MessageReceived(&invoke);
        return;
      }
      case B_TAB:
        Window()->PostMessage(kSwitch);
        return;
      case B_BACKSPACE: {
        BMessage parent(kParent);
        pane->MessageReceived(&parent);
        return;
      }
      case ' ':
        Pick(index, true, true);
        return;
      }
      if (change) {
        Pick(std::max(0, std::min(int(model->Visible().size()) - 1, index + change)),
             modifiers() & B_SHIFT_KEY);
        return;
      }
    }
    BView::KeyDown(bytes, length);
  }
  void MessageReceived(BMessage *m) override {
    if (m->what == kThumbnailReady) {
      auto owned = replies->Take(m->GetInt64("bitmap", -1));
      auto bitmap = owned.release();
      if (m->GetInt64("serial", -1) != generation->load()) {
        delete bitmap;
        return;
      }
      std::string path = m->GetString("path", "");
      pending.erase(path);
      if (thumbnails.size() >= 256)
        thumbnails.erase(thumbnails.begin());
      thumbnails[path].reset(bitmap);
      Invalidate();
      return;
    }
    if (m->WasDropped()) {
      BMessage dropped(*m);
      dropped.what = kDrop;
      dropped.RemoveName("destination");
      dropped.AddString("destination", pane->Path());
      Window()->PostMessage(&dropped);
      return;
    }
    BView::MessageReceived(m);
  }
  void Draw(BRect update) override {
    int first = std::max(0, int(update.top / cellHeight) * Columns()),
        last = std::min(int(model->Visible().size()),
                        (int(update.bottom / cellHeight) + 1) * Columns());
    for (int i = first; i < last; ++i) {
      auto row = model->Visible()[i];
      auto cell = Cell(i).InsetByCopy(4, 4);
      bool selected = Selected(row);
      SetHighUIColor(selected ? B_LIST_SELECTED_BACKGROUND_COLOR
                              : B_DOCUMENT_BACKGROUND_COLOR);
      FillRoundRect(cell, 4, 4);
      BBitmap *picture = row->icon.get();
      if (mode == 3 && !row->directory && !row->parent) {
        auto found = thumbnails.find(row->path);
        if (found != thumbnails.end()) {
          if (found->second)
            picture = found->second.get();
        } else if (!pending.count(row->path) && loader && pending.size() < 64) {
          std::string lower = row->name.c_str();
          std::transform(lower.begin(), lower.end(), lower.begin(),
                         [](unsigned char c) { return std::tolower(c); });
          auto dot = lower.find_last_of('.');
          auto ext = dot == std::string::npos ? "" : lower.substr(dot);
          if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
              ext == ".gif" || ext == ".bmp" || ext == ".webp" ||
              ext == ".tiff" || ext == ".tif" || ext == ".avif" ||
              ext == ".heif" || ext == ".svg" || ext == ".ico" ||
              ext == ".tga") {
            pending.insert(row->path);
            BMessage job(kThumbnail);
            job.AddString("path", row->path.c_str());
            job.AddInt64("serial", generation->load());
            loader->PostMessage(&job);
          }
        }
      }
      if (picture) {
        float available = mode == 1 ? 48 : 104;
        auto src = picture->Bounds();
        float scale = std::min(available / (src.Width() + 1),
                               available / (src.Height() + 1));
        float w = (src.Width() + 1) * scale, h = (src.Height() + 1) * scale;
        BRect dest(cell.left + (cell.Width() - w) / 2,
                   cell.top + 4 + (available - h) / 2, 0, 0);
        dest.right = dest.left + w - 1;
        dest.bottom = dest.top + h - 1;
        SetDrawingMode(B_OP_ALPHA);
        DrawBitmap(picture, src, dest);
        SetDrawingMode(B_OP_COPY);
      }
      SetHighUIColor(selected ? B_LIST_SELECTED_ITEM_TEXT_COLOR
                              : B_DOCUMENT_TEXT_COLOR);
      BString label(row->name.c_str());
      TruncateString(&label, B_TRUNCATE_MIDDLE, cell.Width() - 8);
      DrawString(label,
                 BPoint(cell.left + (cell.Width() - StringWidth(label)) / 2,
                        cell.bottom - 10));
      if (row->transferState) {
        SetHighUIColor(row->transferState == 2   ? B_SUCCESS_COLOR
                       : row->transferState == 3 ? B_FAILURE_COLOR
                                                  : B_STATUS_BAR_COLOR);
        float width = std::max(
            5.f, (cell.Width() - 8) *
                     (row->transferState == 1 ? row->fraction : 1.f));
        FillRect(BRect(cell.left + 4, cell.bottom - 3, cell.left + 4 + width,
                       cell.bottom - 1));
      }
      if (IsFocus() && model->Focused() == row) {
        SetHighUIColor(B_KEYBOARD_NAVIGATION_COLOR);
        StrokeRoundRect(cell, 4, 4);
      }
    }
  }
};

FileGrid *CreateFileGrid(Pane *pane, PaneModel *model) {
  return new GridView(pane, model);
}
