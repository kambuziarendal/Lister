// SPDX-License-Identifier: MIT
#include "ImagePreview.h"
#include "BitmapMailbox.h"
#include "VirtualFile.h"
#include <Bitmap.h>
#include <DataIO.h>
#include <File.h>
#include <Font.h>
#include <Looper.h>
#include <Message.h>
#include <Messenger.h>
#include <String.h>
#include <TranslationUtils.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
namespace {
constexpr uint32 kLoad = 'pimg', kReady = 'pird';
class BoundedBitmap : public BMallocIO {
  const Lister::TransferControl &control;

public:
  explicit BoundedBitmap(const Lister::TransferControl &c) : control(c) {}
  status_t SetSize(off_t size) override {
    if (size < 0 || size > 64 * 1024 * 1024)
      return B_NO_MEMORY;
    return BMallocIO::SetSize(size);
  }
  ssize_t WriteAt(off_t position, const void *data, size_t size) override {
    try {
      control.Check();
    } catch (...) {
      return B_CANCELED;
    }
    constexpr off_t limit = 64 * 1024 * 1024;
    if (position < 0 || position > limit ||
        size > static_cast<size_t>(limit - position))
      return B_NO_MEMORY;
    return BMallocIO::WriteAt(position, data, size);
  }
};
class Loader : public BLooper {
  BMessenger target;
  std::shared_ptr<BitmapMailbox> replies;
  std::shared_ptr<std::atomic<int64>> generation;

public:
  Loader(BMessenger t, std::shared_ptr<std::atomic<int64>> g,
         std::shared_ptr<BitmapMailbox> r)
      : BLooper("image preview reader"), target(t), replies(r), generation(g) {}
  void MessageReceived(BMessage *m) override {
    if (m->what != kLoad) {
      BLooper::MessageReceived(m);
      return;
    }
    int64 serial = m->GetInt64("serial", -1);
    Lister::TransferControl c;
    c.proceed = [&] { return generation->load() == serial; };
    BMessage result(kReady);
    result.AddInt64("serial", serial);
    BBitmap *image = nullptr;
    std::string temporary;
    try {
      image = LoadPreviewBitmap(m->GetString("path", ""), c);
      BString text;
      text << int32(image->Bounds().Width() + 1) << " x "
           << int32(image->Bounds().Height() + 1);
      result.AddString("caption", text);
    } catch (const Lister::Cancelled &) {
      if (!temporary.empty())
        unlink(temporary.c_str());
      delete image;
      return;
    } catch (const std::exception &e) {
      result.AddString("caption", e.what());
    }
    if (!temporary.empty())
      unlink(temporary.c_str());
    auto reply = replies->Put(image);
    result.AddInt64("bitmap", reply);
    if (target.SendMessage(&result, (BHandler *)nullptr, 500000) != B_OK)
      replies->Take(reply);
  }
};
} // namespace
BBitmap *LoadPreviewBitmap(const std::string &source,
                           const Lister::TransferControl &c, int maxSize) {
  BBitmap *image = nullptr;
  std::string path = source, temporary;
  try {
    c.Check();
    if (Lister::IsVirtual(path)) {
      char pattern[] = "/tmp/Listi-preview-XXXXXX";
      int fd = mkstemp(pattern);
      if (fd < 0)
        throw std::runtime_error("Cannot create preview cache");
      temporary = pattern;
      try {
        size_t total = 0;
        Lister::ReadFile(
            path,
            [&](const char *data, size_t n) {
              total += n;
              if (total > 32 * 1024 * 1024)
                throw std::runtime_error(
                    "Image is too large for a network preview");
              while (n) {
                ssize_t written = write(fd, data, n);
                if (written <= 0)
                  throw std::runtime_error("Preview cache write failed");
                data += written;
                n -= written;
              }
            },
            c);
        close(fd);
      } catch (...) {
        close(fd);
        throw;
      }
      path = temporary;
    }
    BFile input(path.c_str(), B_READ_ONLY);
    if (input.InitCheck() != B_OK)
      throw std::runtime_error("Cannot read image");
    off_t size = 0;
    input.GetSize(&size);
    if (size > 64 * 1024 * 1024)
      throw std::runtime_error("Image is too large for a preview");
    BoundedBitmap decoded(c);
    status_t status = BTranslatorRoster::Default()->Translate(
        &input, nullptr, nullptr, &decoded, B_TRANSLATOR_BITMAP);
    c.Check();
    if (status != B_OK)
      throw std::runtime_error("No image preview available");
    decoded.Seek(0, SEEK_SET);
    image = BTranslationUtils::GetBitmap(&decoded);
    if (!image)
      throw std::runtime_error("Cannot decode this image");

    if (!temporary.empty())
      unlink(temporary.c_str());
    if (maxSize > 0 && (image->Bounds().Width() + 1 > maxSize ||
                        image->Bounds().Height() + 1 > maxSize)) {
      auto bounds = image->Bounds();
      float scale = std::min(float(maxSize) / (bounds.Width() + 1),
                             float(maxSize) / (bounds.Height() + 1));
      auto small = new BBitmap(
          BRect(0, 0, std::max(1.f, (bounds.Width() + 1) * scale) - 1,
                std::max(1.f, (bounds.Height() + 1) * scale) - 1),
          B_BITMAP_ACCEPTS_VIEWS, B_RGBA32);
      if (small->InitCheck() != B_OK) {
        delete small;
        throw std::runtime_error("Cannot allocate thumbnail");
      }
      auto drawing =
          new BView(small->Bounds(), "thumbnail", B_FOLLOW_NONE, B_WILL_DRAW);
      small->AddChild(drawing);
      small->Lock();
      drawing->SetHighColor(0, 0, 0, 0);
      drawing->FillRect(drawing->Bounds());
      drawing->SetDrawingMode(B_OP_ALPHA);
      drawing->DrawBitmap(image, image->Bounds(), drawing->Bounds());
      drawing->Sync();
      small->Unlock();
      small->RemoveChild(drawing);
      delete drawing;
      delete image;
      image = small;
    }
    c.Check();
    return image;
  } catch (...) {
    if (!temporary.empty())
      unlink(temporary.c_str());
    delete image;
    throw;
  }
}
ImagePreview::ImagePreview()
    : BView("image preview", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE |
                                 B_FRAME_EVENTS | B_SUPPORTS_LAYOUT),
      replies(std::make_shared<BitmapMailbox>(1)),
      generation(std::make_shared<std::atomic<int64>>(0)) {
  SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
  SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
  SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
  SetExplicitMinSize(BSize(160, 96));
  SetExplicitPreferredSize(BSize(280, 400));
}
ImagePreview::~ImagePreview() {
  generation->store(-1);
  if (loader)
    loader->PostMessage(B_QUIT_REQUESTED);
  delete bitmap;
}
void ImagePreview::AttachedToWindow() {
  loader = new Loader(BMessenger(this), generation, replies);
  loader->Run();
}
void ImagePreview::ShowFile(const std::string &p) {
  if (path == p)
    return;
  path = p;
  int64 serial = ++*generation;
  replies->Clear();
  delete bitmap;
  bitmap = nullptr;
  caption = p.empty() ? "Select an image to preview" : "Loading image...";
  Invalidate();
  if (!p.empty() && loader) {
    BMessage m(kLoad);
    m.AddInt64("serial", serial);
    m.AddString("path", p.c_str());
    loader->PostMessage(&m);
  }
}
void ImagePreview::MessageReceived(BMessage *m) {
  if (m->what == kReady) {
    auto owned = replies->Take(m->GetInt64("bitmap", -1));
    BBitmap *image = owned.release();
    if (m->GetInt64("serial", -1) != generation->load()) {
      delete image;
      return;
    }
    delete bitmap;
    bitmap = image;
    caption = m->GetString("caption", "");
    Invalidate();
  } else
    BView::MessageReceived(m);
}
void ImagePreview::FrameResized(float, float) { Invalidate(); }
void ImagePreview::Draw(BRect) {
  SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
  DrawString("Preview", BPoint(12, 22));
  BRect area = Bounds().InsetByCopy(12, 12);
  area.top += 22;
  area.bottom -= 45;
  if (bitmap && area.Width() > 0 && area.Height() > 0) {
    BRect src = bitmap->Bounds();
    float scale = std::min(1.f, std::min(area.Width() / (src.Width() + 1),
                                         area.Height() / (src.Height() + 1)));
    float width = (src.Width() + 1) * scale,
          height = (src.Height() + 1) * scale;
    BRect dest(area.left + (area.Width() - width) / 2,
               area.top + (area.Height() - height) / 2, 0, 0);
    dest.right = dest.left + width - 1;
    dest.bottom = dest.top + height - 1;
    SetDrawingMode(B_OP_ALPHA);
    DrawBitmap(bitmap, src, dest);
    SetDrawingMode(B_OP_COPY);
  }
  BString line(caption.c_str());
  TruncateString(&line, B_TRUNCATE_END, std::max(0.f, Bounds().Width() - 24));
  DrawString(line, BPoint(12, Bounds().bottom - 26));
  BString name(path.empty() ? "" : Lister::LocationLeaf(path).c_str());
  TruncateString(&name, B_TRUNCATE_MIDDLE,
                 std::max(0.f, Bounds().Width() - 24));
  DrawString(name, BPoint(12, Bounds().bottom - 9));
}
