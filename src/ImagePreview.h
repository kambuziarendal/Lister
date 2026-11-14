// SPDX-License-Identifier: MIT
#pragma once
#include <View.h>
#include <atomic>
#include <memory>
#include <string>
class BBitmap;
class BitmapMailbox;
class BLooper;
class ImagePreview : public BView {
public:
  ImagePreview();
  ~ImagePreview();
  void AttachedToWindow() override;
  void MessageReceived(BMessage *) override;
  void Draw(BRect) override;
  void FrameResized(float, float) override;
  void ShowFile(const std::string &);

private:
  BBitmap *bitmap = nullptr;
  std::shared_ptr<BitmapMailbox> replies;
  BLooper *loader = nullptr;
  std::shared_ptr<std::atomic<int64>> generation;
  std::string path, caption = "Select an image to preview";
};

namespace Lister {
struct TransferControl;
}
BBitmap *LoadPreviewBitmap(const std::string &, const Lister::TransferControl &,
                           int maxSize = 0);
