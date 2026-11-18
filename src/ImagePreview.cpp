// SPDX-License-Identifier: MIT
#include "ImagePreview.h"
#include "BitmapMailbox.h"
#include "InspectorContent.h"
#include "ThemeTextView.h"
#include "VirtualFile.h"
#include <Bitmap.h>
#include <Button.h>
#include <ScrollView.h>
#include <DataIO.h>
#include <Font.h>
#include <Looper.h>
#include <Message.h>
#include <Messenger.h>
#include <String.h>
#include <TranslationUtils.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>
#include <algorithm>
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <unistd.h>
namespace {
constexpr uint32 kLoad = 'pimg', kReady = 'pird', kFrame = 'pifr',
                 kPrevious = 'pipv', kNext = 'pinx', kPlay = 'pipl';

class CachedPreview {
public:
  CachedPreview(const std::string &source, const Listi::TransferControl &control)
      : path(source) {
    if (!Listi::IsVirtual(source))
      return;
    char pattern[] = "/tmp/Listi-preview-XXXXXX";
    int fd = mkstemp(pattern);
    if (fd < 0)
      throw std::runtime_error("Cannot create preview cache");
    try {
      size_t total = 0;
      Listi::ReadFile(
          source,
          [&](const char *data, size_t size) {
            total += size;
            if (total > 32 * 1024 * 1024)
              throw std::runtime_error("File is too large for a network preview");
            while (size) {
              ssize_t written = write(fd, data, size);
              if (written < 0 && errno == EINTR)
                continue;
              if (written <= 0)
                throw std::runtime_error("Preview cache write failed");
              data += written;
              size -= written;
            }
          },
          control);
    } catch (...) {
      close(fd);
      unlink(pattern);
      throw;
    }
    close(fd);
    path = pattern;
    cached = true;
  }
  ~CachedPreview() {
    if (cached)
      unlink(path.c_str());
  }
  std::string path;

private:
  bool cached = false;
};

class BoundedBitmap : public BMallocIO {
  const Listi::TransferControl &control;

public:
  explicit BoundedBitmap(const Listi::TransferControl &c) : control(c) {}
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
BBitmap *DecodeImage(Inspector::Input &input, const Listi::TransferControl &control) {
  constexpr off_t limit = 64 * 1024 * 1024;
  off_t bytes;
  input.GetSize(&bytes);
  if (bytes > limit)
    throw std::runtime_error("Image previews are limited to 64 MiB");
  BoundedBitmap decoded(control);
  const auto status = BTranslatorRoster::Default()->Translate(
      &input, nullptr, nullptr, &decoded, B_TRANSLATOR_BITMAP);
  control.Check();
  if (status != B_OK)
    throw std::runtime_error("No preview available");
  decoded.Seek(0, SEEK_SET);
  auto *image = BTranslationUtils::GetBitmap(&decoded);
  if (!image)
    throw std::runtime_error("Cannot decode this image");
  return image;
}

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
    Listi::TransferControl c;
    c.proceed = [&] { return generation->load() == serial; };
    BMessage result(kReady);
    result.AddInt64("serial", serial);
    std::unique_ptr<BBitmap> image;
    Inspector::Kind kind = Inspector::Kind::Unknown;
    int page = m->GetInt32("page", 1);
    try {
      c.Check();
      const std::string source = m->GetString("path", "");
      CachedPreview file(source, c);
      Inspector::Input input(file.path);
      kind = Inspector::Identify(Listi::LocationLeaf(source), input);
      switch (kind) {
      case Inspector::Kind::Text:
      case Inspector::Kind::Code:
      case Inspector::Kind::Markdown:
      case Inspector::Kind::Html: {
        bool truncated;
        const bool standalone = m->GetBool("standalone", false);
        const auto text = Inspector::ReadText(input, truncated, standalone ? 1024 * 1024 : 256 * 1024);
        BString caption = kind == Inspector::Kind::Markdown ? "Markdown source"
                          : kind == Inspector::Kind::Html ? "HTML source"
                          : kind == Inspector::Kind::Code ? "Code"
                          : "Text";
        if (truncated)
          caption << (standalone ? " (first 1 MiB)" : " (first 256 KiB)");
        result.AddString("caption", caption);
        result.AddString("text", text.c_str());
        break;
      }
      case Inspector::Kind::Pdf: {
        image = Inspector::RenderPdf(input, page, c);
        BString caption("PDF · page ");
        caption << page;
        result.AddString("caption", caption);
        result.AddBool("pageValid", true);
        break;
      }
      case Inspector::Kind::Audio:
      case Inspector::Kind::Video: {
        bool play = m->GetBool("play", false);
        auto caption = Inspector::InspectMedia(
            input, kind, play, c, [&](std::unique_ptr<BBitmap> frame) {
              if (!play) {
                image = std::move(frame);
                return;
              }
              BMessage next(kFrame);
              next.AddInt64("serial", serial);
              Send(next, std::move(frame));
            });
        result.AddString("caption", caption.c_str());
        if (play)
          result.AddBool("keepBitmap", true);
        break;
      }
      default: {
        // Older or extensionless images still use all installed translators.
        image.reset(DecodeImage(input, c));
        BString caption;
        caption << int32(image->Bounds().Width() + 1) << " × "
                << int32(image->Bounds().Height() + 1);
        result.AddString("caption", caption);
        break;
      }
      }
      c.Check();
    } catch (const Listi::Cancelled &) {
      return;
    } catch (const std::exception &e) {
      result.AddString("caption", e.what());
    }
    result.AddInt32("kind", static_cast<int32>(kind));
    result.AddInt32("page", page);
    Send(result, std::move(image));
  }

private:
  void Send(BMessage &message, std::unique_ptr<BBitmap> image) {
    int64 reply = image ? replies->Put(image.release()) : -1;
    message.AddInt64("bitmap", reply);
    if (target.SendMessage(&message, (BHandler *)nullptr, 50000) != B_OK)
      replies->Take(reply);
  }
};
} // namespace
BBitmap *LoadPreviewBitmap(const std::string &source,
                           const Listi::TransferControl &c, int maxSize) {
  BBitmap *image = nullptr;
  try {
    c.Check();
    CachedPreview file(source, c);
    Inspector::Input input(file.path, 64 * 1024 * 1024);
    image = DecodeImage(input, c);

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
    delete image;
    throw;
  }
}
ImagePreview::ImagePreview(bool standalone)
    : BView("inspector", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE |
                             B_FRAME_EVENTS | B_SUPPORTS_LAYOUT),
      replies(std::make_shared<BitmapMailbox>(2)),
      generation(std::make_shared<std::atomic<int64>>(0)), standalone(standalone) {
  SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
  SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
  SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
  SetExplicitMinSize(BSize(180, 128));
  SetExplicitPreferredSize(BSize(280, 400));

  text = new ThemeTextView("preview text", B_DOCUMENT_BACKGROUND_COLOR,
                           B_DOCUMENT_TEXT_COLOR);
  text->SetFontAndColor(be_fixed_font);
  text->MakeEditable(false);
  text->SetWordWrap(true);
  text->SetResizingMode(B_FOLLOW_ALL);
  textScroll = new BScrollView("preview scroll", text, 0, false, true);
  AddChild(textScroll);
  textScroll->Hide();

  auto addButton = [this](const char *label, uint32 command) {
    auto button = new BButton(label, new BMessage(command));
    AddChild(button);
    button->ResizeToPreferred();
    button->Hide();
    return button;
  };
  previous = addButton("Previous", kPrevious);
  next = addButton("Next", kNext);
  play = addButton("Play preview", kPlay);
}
ImagePreview::~ImagePreview() {
  generation->store(-1);
  // Cancel first, then join; a posted quit can outlive BApplication teardown.
  if (loader && loader->Lock())
    loader->Quit();
  delete bitmap;
}
void ImagePreview::AttachedToWindow() {
  previous->SetTarget(this);
  next->SetTarget(this);
  play->SetTarget(this);
  FrameResized(Bounds().Width(), Bounds().Height());
  if (loader)
    return;
  loader = new Loader(BMessenger(this), generation, replies);
  loader->Run();
  if (!path.empty())
    Request(1);
}
void ImagePreview::ShowFile(const std::string &p) {
  if (path == p)
    return;
  path = p;
  page = 1;
  lastPage = 0;
  kind = Inspector::Kind::Unknown;
  playing = false;
  Request(1);
}
void ImagePreview::Request(int requestedPage, bool playback) {
  int64 serial = ++*generation;
  replies->Clear();
  if (kind == Inspector::Kind::Unknown) {
    delete bitmap;
    bitmap = nullptr;
  }
  caption = path.empty() ? "Select a file to inspect"
                        : playback ? "Playing preview (up to 15 seconds)"
                                   : "Loading preview…";
  if (!textScroll->IsHidden(textScroll))
    textScroll->Hide();
  if (!path.empty() && loader) {
    BMessage m(kLoad);
    m.AddInt64("serial", serial);
    m.AddString("path", path.c_str());
    m.AddInt32("page", requestedPage);
    m.AddBool("play", playback);
    m.AddBool("standalone", standalone);
    loader->PostMessage(&m);
  }
  UpdateControls();
}
void ImagePreview::UpdateControls() {
  auto show = [](BView *view, bool visible) {
    if (visible && view->IsHidden(view))
      view->Show();
    else if (!visible && !view->IsHidden(view))
      view->Hide();
  };
  const bool pdf = kind == Inspector::Kind::Pdf;
  const bool media = kind == Inspector::Kind::Audio || kind == Inspector::Kind::Video;
  previous->SetEnabled(pdf && page > 1);
  next->SetEnabled(pdf && page != lastPage);
  show(previous, pdf);
  show(next, pdf);
  show(play, media);
  play->SetLabel(playing ? "Stop preview" : "Play preview");
  Invalidate();
}
void ImagePreview::MessageReceived(BMessage *m) {
  switch (m->what) {
  case kPrevious:
    if (kind == Inspector::Kind::Pdf && page > 1)
      Request(page - 1);
    return;
  case kNext:
    if (kind == Inspector::Kind::Pdf && page != lastPage && page < INT_MAX)
      Request(page + 1);
    return;
  case kPlay:
    if (kind == Inspector::Kind::Audio || kind == Inspector::Kind::Video) {
      playing = !playing;
      Request(1, playing);
    }
    return;
  case kReady:
  case kFrame: {
    auto image = replies->Take(m->GetInt64("bitmap", -1));
    if (m->GetInt64("serial", -1) != generation->load())
      return;
    if (m->what == kFrame) {
      if (image) {
        delete bitmap;
        bitmap = image.release();
        Invalidate();
      }
      return;
    }
    kind = static_cast<Inspector::Kind>(m->GetInt32("kind", 0));
    const int resultPage = m->GetInt32("page", 1);
    const bool validPage = m->GetBool("pageValid", false);
    if (validPage)
      page = resultPage;
    bool beyondLastPage = kind == Inspector::Kind::Pdf && !validPage &&
                          resultPage > page && bitmap;
    if (beyondLastPage)
      lastPage = page;
    if (!beyondLastPage && !m->GetBool("keepBitmap", false)) {
      delete bitmap;
      bitmap = image.release();
    }
    caption = m->GetString("caption", "");
    if (beyondLastPage)
      caption += " (showing page " + std::to_string(page) + ")";
    if (const char *content = m->GetString("text", nullptr)) {
      text->SetText(content);
      text->Select(0, 0);
      text->ScrollToOffset(0);
      textScroll->Show();
    }
    playing = false;
    UpdateControls();
    return;
  }
  default:
    BView::MessageReceived(m);
  }
}

void ImagePreview::FrameResized(float width, float height) {
  textScroll->MoveTo(8, 31);
  textScroll->ResizeTo(std::max(32.f, width - 16), std::max(32.f, height - 70));
  text->SetTextRect(text->Bounds().OffsetToCopy(0, 0).InsetByCopy(4, 3));
  const float buttonTop = height - 44 - previous->Bounds().Height();
  previous->MoveTo(12, buttonTop);
  next->MoveTo(20 + previous->Bounds().Width(), buttonTop);
  play->MoveTo(12, buttonTop);
  Invalidate();
}
void ImagePreview::Draw(BRect) {
  SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
  DrawString("Inspector", BPoint(12, 22));
  BRect area = Bounds().InsetByCopy(12, 12);
  area.top += 22;
  area.bottom = previous->IsHidden(previous) && play->IsHidden(play)
                    ? Bounds().bottom - 53
                    : previous->Frame().top - 6;
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
  BString name(path.empty() ? "" : Listi::LocationLeaf(path).c_str());
  TruncateString(&name, B_TRUNCATE_MIDDLE,
                 std::max(0.f, Bounds().Width() - 24));
  DrawString(name, BPoint(12, Bounds().bottom - 9));
}
