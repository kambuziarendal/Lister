// SPDX-License-Identifier: MIT
#include "BitmapMailbox.h"
#include "ImagePreview.h"
#include "VirtualFile.h"
#include <Application.h>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  BApplication app("application/x-vnd.Kambuzi-Listi-PreviewTest");
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *label) {
    ++checks;
    failures += !ok;
    std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
  };
  Lister::TransferControl control;
  try {
    std::unique_ptr<BBitmap> image(LoadPreviewBitmap(argv[1], control));
    check(image && image->Bounds().Width() == 319 && image->Bounds().Height() == 239,
          "native decoder reads synthetic 320x240 image");
    std::unique_ptr<BBitmap> thumb(LoadPreviewBitmap(argv[1], control, 128));
    check(thumb && thumb->Bounds().Width() < 128 && thumb->Bounds().Height() < 128,
          "native thumbnail is bounded to 128 pixels");
    BitmapMailbox mailbox(1);
    auto discarded = mailbox.Put(image.release());
    auto current = mailbox.Put(thumb.release());
    check(!mailbox.Take(discarded), "bounded replies discard superseded ownership");
    auto received = mailbox.Take(current);
    check(received && !mailbox.Take(current), "reply transfers ownership exactly once");
    auto cleared = mailbox.Put(received.release());
    mailbox.Clear();
    check(!mailbox.Take(cleared), "navigation clears unconsumed bitmap replies");
    int calls = 0;
    control.proceed = [&] { return ++calls < 3; };
    bool cancelled = false;
    try {
      std::unique_ptr<BBitmap> ignored(LoadPreviewBitmap(argv[1], control));
    } catch (const Lister::Cancelled &) {
      cancelled = true;
    }
    check(cancelled && calls >= 3, "cancellation during native translation is propagated");
  } catch (const std::exception &error) {
    std::cerr << error.what() << std::endl;
    ++failures;
  }
  std::cout << checks << " preview checks, " << failures << " failures\n";
  return failures != 0;
}
