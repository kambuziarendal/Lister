// SPDX-License-Identifier: MIT
#pragma once
#include <Bitmap.h>
#include <map>
#include <memory>
#include <mutex>

// Messages carry IDs, never owning pointers. Undelivered replies are reclaimed
// when the view and its worker release this bounded mailbox, including at quit.
class BitmapMailbox {
  std::mutex lock;
  std::map<int64, std::unique_ptr<BBitmap>> images;
  int64 next = 0;
  size_t limit;

public:
  explicit BitmapMailbox(size_t capacity) : limit(capacity) {}
  int64 Put(BBitmap *image) {
    std::lock_guard<std::mutex> guard(lock);
    if (images.size() >= limit)
      images.erase(images.begin());
    images[++next].reset(image);
    return next;
  }
  std::unique_ptr<BBitmap> Take(int64 id) {
    std::lock_guard<std::mutex> guard(lock);
    auto found = images.find(id);
    if (found == images.end())
      return {};
    auto image = std::move(found->second);
    images.erase(found);
    return image;
  }
  void Clear() {
    std::lock_guard<std::mutex> guard(lock);
    images.clear();
  }
};
