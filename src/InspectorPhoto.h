// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace Inspector {
struct PhotoInfo {
  bool jpeg = false;
  unsigned width = 0, height = 0;
  std::string cameraDetails; // Never contains GPS tags.
  bool hasLocation = false;
  double latitude = 0, longitude = 0;
};

// Reads only selected JPEG header segments, never compressed pixels.
using PhotoReader = std::function<bool(size_t, void *, size_t)>;
PhotoInfo ReadPhotoInfo(const PhotoReader &read);

struct PhotoRegion {
  double left, top, width, height; // Source pixels, with exclusive far edges.
  double x, y, scale; // Destination origin and scale within the viewport.
};
struct PhotoView {
  double zoom = 0; // Zero means fit, without upscaling a small image.
  double centerX = 0.5, centerY = 0.5;
  PhotoRegion Region(double width, double height, double viewWidth,
                     double viewHeight) const;
  void Pan(double dx, double dy, double width, double height, double viewWidth,
           double viewHeight);
};
}
