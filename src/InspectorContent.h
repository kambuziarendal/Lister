// SPDX-License-Identifier: MIT
#pragma once
#include <DataIO.h>
#include <functional>
#include <memory>
#include <string>

class BBitmap;
namespace Listi {
struct TransferControl;
}

namespace Inspector {
enum class Kind { Unknown, Image, Text, Code, Markdown, Html, Pdf, Audio, Video };

// Pins one file after a nonblocking open; never hands a mutable source path to
// a decoder. Owns the descriptor, not the selected file.
class Input : public BPositionIO {
public:
  explicit Input(const std::string &, off_t limit = INT64_MAX);
  ~Input() override;
  Input(const Input &) = delete;
  Input &operator=(const Input &) = delete;
  ssize_t ReadAt(off_t, void *, size_t) override;
  ssize_t WriteAt(off_t, const void *, size_t) override;
  off_t Seek(off_t, uint32) override;
  off_t Position() const override;
  status_t GetSize(off_t *) const override;

private:
  int fd;
  off_t position = 0, length = 0;
};

Kind Identify(const std::string &, Input &);
std::string ReadText(Input &, bool &truncated, size_t limit = 256 * 1024);
std::unique_ptr<BBitmap> RenderPdf(Input &, int page, const Listi::TransferControl &);
using PresentFrame = std::function<void(std::unique_ptr<BBitmap>)>;
std::string InspectMedia(Input &, Kind, bool play, const Listi::TransferControl &,
                         const PresentFrame &);
}
