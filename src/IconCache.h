// SPDX-License-Identifier: MIT
#pragma once
#include <Bitmap.h>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
// Share identical native pixels, not MIME names: per-file/custom icons remain
// distinct. The bounded index owns no bitmaps after a listing is released.
class IconCache {
  std::map<uint64_t, std::weak_ptr<BBitmap>> cache;
public:
  void Clear() { cache.clear(); }
  std::shared_ptr<BBitmap> Get(const void *data, size_t length, int32 pixels) {
    if (pixels < 16 || pixels > 128 || length != size_t(pixels * pixels * 4)) return {};
    uint64_t hash = 14695981039346656037ULL ^ uint64_t(pixels);
    for (size_t i = 0; i < length; ++i) { hash ^= static_cast<const uint8 *>(data)[i]; hash *= 1099511628211ULL; }
    auto found = cache.find(hash);
    if (found != cache.end()) {
      auto bitmap = found->second.lock();
      if (bitmap && bitmap->Bounds().IntegerWidth() + 1 == pixels &&
          size_t(bitmap->BitsLength()) == length && !memcmp(bitmap->Bits(), data, length)) return bitmap;
    }
    auto bitmap = std::make_shared<BBitmap>(BRect(0, 0, pixels - 1, pixels - 1), B_RGBA32);
    if (bitmap->InitCheck() != B_OK) return {};
    memcpy(bitmap->Bits(), data, length);
    if (cache.size() >= 512) cache.clear();
    cache[hash] = bitmap;
    return bitmap;
  }
};
