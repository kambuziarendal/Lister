// SPDX-License-Identifier: MIT
#include "ClipboardImage.h"
#include "ClipboardLock.h"
#include <Message.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <TranslationUtils.h>
#include <TranslatorRoster.h>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace Listi {
namespace {
constexpr ssize_t kImageBytes = 64 * 1024 * 1024;
constexpr uint32 kPixels = 16 * 1024 * 1024;
void CheckDimensions(double width, double height) {
  if (!(width >= 1 && height >= 1 && width <= 16384 && height <= 16384 &&
        width * height <= kPixels && width == std::floor(width) &&
        height == std::floor(height)))
    throw std::runtime_error(
        "Unsupported clipboard image dimensions (maximum 16 megapixels).");
}

uint32 BigEndian(const unsigned char *p) {
  return uint32(p[0]) << 24 | uint32(p[1]) << 16 | uint32(p[2]) << 8 | p[3];
}

void VerifyPNG(const std::string &bytes) {
  const auto *p = reinterpret_cast<const unsigned char *>(bytes.data());
  if (bytes.size() < 45 || memcmp(p, "\x89PNG\r\n\x1a\n", 8) ||
      BigEndian(p + 8) != 13 || memcmp(p + 12, "IHDR", 4) ||
      memcmp(p + bytes.size() - 12, "\0\0\0\0IEND\xae\x42\x60\x82", 12))
    throw std::runtime_error(
        "The clipboard contains incomplete or invalid PNG data.");
  CheckDimensions(BigEndian(p + 16), BigEndian(p + 20));
  BMemoryIO input(bytes.data(), bytes.size());
  std::unique_ptr<BBitmap> decoded(BTranslationUtils::GetBitmap(&input));
  if (!decoded || decoded->InitCheck() != B_OK)
    throw std::runtime_error("Could not read the clipboard PNG image.");
}

std::string EncodeBitmap(const BMessage &archive) {
  // Consume only pixel fields. Never instantiate archived child views or flags.
  BRect frame;
  int32 space, stride;
  const void *pixels;
  ssize_t size;
  if (archive.FindRect("_frame", &frame) != B_OK ||
      archive.FindInt32("_cspace", &space) != B_OK ||
      archive.FindInt32("_rowbytes", &stride) != B_OK || stride <= 0 ||
      archive.FindData("_data", B_RAW_TYPE, &pixels, &size) != B_OK ||
      size <= 0 || size > kImageBytes)
    throw std::runtime_error(
        "The clipboard bitmap is incomplete or too large.");
  const double width = double(frame.right) - frame.left + 1;
  const double height = double(frame.bottom) - frame.top + 1;
  CheckDimensions(width, height);
  if (int64(stride) * int64(height) != size)
    throw std::runtime_error("Invalid clipboard bitmap row size.");
  BRect bounds(0, 0, width - 1, height - 1);
  auto bitmap = std::make_unique<BBitmap>(bounds, B_BITMAP_NO_SERVER_LINK,
                                          color_space(space), stride);
  if (bitmap->InitCheck() != B_OK || bitmap->BitsLength() != size ||
      !bitmap->Bits())
    throw std::runtime_error("Unsupported clipboard bitmap format.");
  memcpy(bitmap->Bits(), pixels, size);
  BBitmapStream source(bitmap.release());
  BMallocIO output;
  output.SetBlockSize(256 * 1024);
  auto *roster = BTranslatorRoster::Default();
  if (!roster ||
      roster->Translate(&source, nullptr, nullptr, &output, B_PNG_FORMAT) !=
          B_OK ||
      output.BufferLength() == 0 || output.BufferLength() > size_t(kImageBytes))
    throw std::runtime_error(
        "Could not encode the clipboard bitmap as a PNG up to 64 MiB.");
  return {static_cast<const char *>(output.Buffer()), output.BufferLength()};
}
}

std::string ReadClipboardPNG(BClipboard *clipboard) {
  std::string png;
  BMessage bitmap;
  {
    ClipboardLock lock(clipboard);
    if (!clipboard->Data())
      throw std::runtime_error("Could not read the clipboard.");
    const BMessage &message = *clipboard->Data();
    type_code type;
    int32 count;
    if (message.CountNames(B_REF_TYPE) ||
        message.GetInfo("listi:archive-copy", &type) == B_OK)
      throw std::runtime_error(
          "The clipboard contains files. Use Paste to copy or move them.");
    if (message.GetInfo("image/png", &type, &count) == B_OK) {
      const void *bytes;
      ssize_t size;
      if (type != B_MIME_TYPE || count != 1 ||
          message.FindData("image/png", type, &bytes, &size) != B_OK ||
          size <= 0 || size > kImageBytes)
        throw std::runtime_error(
            "Invalid clipboard PNG data (maximum 64 MiB).");
      png.assign(static_cast<const char *>(bytes), size);
    } else {
      const char *key = message.HasMessage("image/bitmap")
                            ? "image/bitmap"
                            : "image/x-be-bitmap";
      const void *bytes;
      ssize_t size;
      if (message.GetInfo(key, &type, &count) != B_OK || count != 1 ||
          message.FindData(key, B_MESSAGE_TYPE, &bytes, &size) != B_OK ||
          size <= 0 || size > kImageBytes + 65536 ||
          message.FindMessage(key, &bitmap) != B_OK)
        throw std::runtime_error(
            "Copy an image before saving PNG from the clipboard (maximum 64 MiB).");
    }
  }
  if (png.empty())
    return EncodeBitmap(bitmap);
  VerifyPNG(png);
  return png;
}
}
