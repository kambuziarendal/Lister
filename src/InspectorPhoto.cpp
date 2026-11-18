// SPDX-License-Identifier: MIT
#include "InspectorPhoto.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Inspector {
namespace {
class Tiff {
  const std::vector<uint8_t> &data;
  bool little;

  uint16_t U16(size_t p) const {
    return little ? data[p] | (uint16_t(data[p + 1]) << 8)
                  : (uint16_t(data[p]) << 8) | data[p + 1];
  }
  uint32_t U32(size_t p) const {
    return little ? U16(p) | (uint32_t(U16(p + 2)) << 16)
                  : (uint32_t(U16(p)) << 16) | U16(p + 2);
  }

  // Offsets are relative to the TIFF header, never to the JPEG or a prior IFD.
  template <typename Visitor>
  void Directory(uint32_t offset, Visitor visit) const {
    if (offset < 8 || offset > data.size() - 2)
      return;
    unsigned entries = U16(offset);
    size_t first = size_t(offset) + 2;
    if (entries > (data.size() - first) / 12)
      return;
    for (unsigned i = 0; i < entries; ++i) {
      size_t p = first + i * 12;
      uint16_t type = U16(p + 2);
      unsigned unit;
      switch (type) {
      case 2:
        unit = 1;
        break; // ASCII
      case 3:
        unit = 2;
        break; // SHORT
      case 4:
        unit = 4;
        break; // LONG
      case 5:
        unit = 8;
        break; // RATIONAL
      default:
        continue;
      }
      uint32_t count = U32(p + 4);
      if (!count || count > data.size() / unit)
        continue;
      size_t length = size_t(count) * unit;
      size_t value = length <= 4 ? p + 8 : U32(p + 8);
      if (value > data.size() - length)
        continue;
      visit(U16(p), type, count, value);
    }
  }

  std::string String(size_t offset, size_t size) const {
    std::string result;
    for (size_t i = 0; i < std::min<size_t>(size, 128); ++i) {
      unsigned ch = data[offset + i];
      if (!ch)
        break;
      // EXIF specifies ASCII. Do not pass control characters to the UI.
      result += ch >= 32 && ch < 127 ? char(ch) : '?';
    }
    while (!result.empty() && result.back() == ' ')
      result.pop_back();
    return result;
  }

  double Rational(size_t p) const {
    return U32(p + 4) ? double(U32(p)) / U32(p + 4) : -1;
  }

public:
  explicit Tiff(const std::vector<uint8_t> &bytes)
      : data(bytes), little(!bytes.empty() && bytes[0] == 'I') {}
  void Extract(PhotoInfo &photo) const {
    if (data.size() < 8 ||
        (std::memcmp(data.data(), "II", 2) &&
         std::memcmp(data.data(), "MM", 2)) ||
        U16(2) != 42)
      return;
    uint32_t exif = 0, gps = 0;
    std::string make, model, captured, lens;
    unsigned iso = 0;
    double exposure = 0, aperture = 0, focal = 0;
    Directory(U32(4), [&](unsigned tag, unsigned type, size_t count, size_t p) {
      if (type == 2 && tag == 0x010f)
        make = String(p, count);
      if (type == 2 && tag == 0x0110)
        model = String(p, count);
      if (type == 4 && count == 1 && tag == 0x8769)
        exif = U32(p);
      if (type == 4 && count == 1 && tag == 0x8825)
        gps = U32(p);
    });
    Directory(exif, [&](unsigned tag, unsigned type, size_t count, size_t p) {
      if (type == 2 && tag == 0x9003)
        captured = String(p, count);
      if (type == 2 && tag == 0xa434)
        lens = String(p, count);
      if (type == 3 && count == 1 && tag == 0x8827)
        iso = U16(p);
      if (type == 5 && count == 1) {
        if (tag == 0x829a)
          exposure = Rational(p);
        if (tag == 0x829d)
          aperture = Rational(p);
        if (tag == 0x920a)
          focal = Rational(p);
      }
    });
    std::string details;
    auto line = [&](const char *label, const std::string &value) {
      if (!value.empty()) {
        if (!details.empty())
          details += '\n';
        details += label;
        details += value;
      }
    };
    line("Make: ", make);
    line("Model: ", model);
    line("Captured: ", captured);
    line("Lens: ", lens);
    if (iso)
      line("ISO: ", std::to_string(iso));
    auto number = [&](const char *label, double value, const char *suffix) {
      if (value > 0) {
        char text[48];
        std::snprintf(text, sizeof(text), "%.4g%s", value, suffix);
        line(label, text);
      }
    };
    number("Exposure: ", exposure, " s");
    number("Aperture: f/", aperture, "");
    number("Focal length: ", focal, " mm");
    photo.cameraDetails = details;

    char latitudeRef = 0, longitudeRef = 0;
    double latitude = -1, longitude = -1;
    Directory(gps, [&](unsigned tag, unsigned type, size_t count, size_t p) {
      if (type == 2 && count == 2 && data[p + 1] == 0) {
        if (tag == 1)
          latitudeRef = data[p];
        if (tag == 3)
          longitudeRef = data[p];
      }
      if (type == 5 && count == 3 && (tag == 2 || tag == 4)) {
        double degrees = Rational(p), minutes = Rational(p + 8),
               seconds = Rational(p + 16);
        if (degrees >= 0 && minutes >= 0 && minutes < 60 && seconds >= 0 &&
            seconds < 60) {
          double coordinate = degrees + minutes / 60 + seconds / 3600;
          if (tag == 2)
            latitude = coordinate;
          else
            longitude = coordinate;
        }
      }
    });
    if (latitude >= 0 && latitude <= 90 && longitude >= 0 && longitude <= 180 &&
        (latitudeRef == 'N' || latitudeRef == 'S') &&
        (longitudeRef == 'E' || longitudeRef == 'W')) {
      photo.hasLocation = true;
      photo.latitude = latitudeRef == 'S' ? -latitude : latitude;
      photo.longitude = longitudeRef == 'W' ? -longitude : longitude;
    }
  }
};
} // namespace

PhotoInfo ReadPhotoInfo(const PhotoReader &read) {
  PhotoInfo photo;
  uint8_t marker[2];
  if (!read(0, marker, 2) || marker[0] != 0xff || marker[1] != 0xd8)
    return photo;
  photo.jpeg = true;
  size_t offset = 2;
  // Bound both seeks and address range on malformed/pathological metadata.
  bool foundExif = false;
  for (unsigned i = 0; i < 1024 && offset < 4 * 1024 * 1024; ++i) {
    if (!read(offset, marker, 2) || marker[0] != 0xff)
      break;
    if (marker[1] == 0xff) {
      ++offset;
      continue;
    } // JPEG fill byte
    if (marker[1] == 0xda || marker[1] == 0xd9 || marker[1] == 0)
      break;
    offset += 2;
    if (marker[1] == 1 || (marker[1] >= 0xd0 && marker[1] <= 0xd7))
      continue;
    uint8_t length[2];
    if (!read(offset, length, 2))
      break;
    unsigned size = unsigned(length[0]) * 256 + length[1];
    if (size < 2)
      break;
    if (marker[1] == 0xe1 && !foundExif && size >= 16) {
      uint8_t label[6];
      if (read(offset + 2, label, 6) && !std::memcmp(label, "Exif\0\0", 6)) {
        std::vector<uint8_t> tiff(size - 8);
        if (read(offset + 8, tiff.data(), tiff.size())) {
          Tiff(tiff).Extract(photo);
          foundExif = true;
        }
      }
    }
    bool frame = marker[1] >= 0xc0 && marker[1] <= 0xcf && marker[1] != 0xc4 &&
                 marker[1] != 0xc8 && marker[1] != 0xcc;
    if (frame && size >= 8) {
      uint8_t dims[4];
      if (read(offset + 3, dims, 4)) {
        photo.height = unsigned(dims[0]) * 256 + dims[1];
        photo.width = unsigned(dims[2]) * 256 + dims[3];
      }
    }
    offset += size;
  }
  return photo;
}

PhotoRegion PhotoView::Region(double width, double height, double viewWidth,
                              double viewHeight) const {
  if (width <= 0 || height <= 0 || viewWidth <= 0 || viewHeight <= 0)
    return {};
  double scale =
      zoom > 0 ? zoom : std::min({1., viewWidth / width, viewHeight / height});
  double w = std::min(width, viewWidth / scale);
  double h = std::min(height, viewHeight / scale);
  return {std::clamp(width * centerX - w / 2, 0., width - w),
          std::clamp(height * centerY - h / 2, 0., height - h),
          w,
          h,
          (viewWidth - w * scale) / 2,
          (viewHeight - h * scale) / 2,
          scale};
}

void PhotoView::Pan(double dx, double dy, double width, double height,
                    double viewWidth, double viewHeight) {
  auto r = Region(width, height, viewWidth, viewHeight);
  if (r.scale <= 0)
    return;
  centerX =
      (std::clamp(r.left - dx / r.scale, 0., width - r.width) + r.width / 2) /
      width;
  centerY =
      (std::clamp(r.top - dy / r.scale, 0., height - r.height) + r.height / 2) /
      height;
}
}
