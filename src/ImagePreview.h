// SPDX-License-Identifier: MIT
#pragma once
#include "InspectorContent.h"
#include <View.h>
#include <atomic>
#include <memory>
#include <string>
class BBitmap;
class BitmapMailbox;
class BLooper;
class BButton;
class BScrollView;
class ThemeTextView;
class ImagePreview : public BView {
public:
  explicit ImagePreview(bool standalone = false);
  ~ImagePreview();
  void AttachedToWindow() override;
  void MessageReceived(BMessage *) override;
  void Draw(BRect) override;
  void FrameResized(float, float) override;
  void ShowFile(const std::string &);

private:
  void Request(int page, bool playback = false);
  void UpdateControls();
  BBitmap *bitmap = nullptr;
  std::shared_ptr<BitmapMailbox> replies;
  BLooper *loader = nullptr;
  std::shared_ptr<std::atomic<int64>> generation;
  ThemeTextView *text;
  BScrollView *textScroll;
  BButton *previous, *next, *play;
  Inspector::Kind kind = Inspector::Kind::Unknown;
  const bool standalone;
  int page = 1, lastPage = 0;
  bool playing = false;
  std::string path, caption = "Select a file to inspect";
};

namespace Listi {
struct TransferControl;
}
BBitmap *LoadPreviewBitmap(const std::string &, const Listi::TransferControl &,
                           int maxSize = 0);
