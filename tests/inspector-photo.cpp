// SPDX-License-Identifier: MIT
#include "InspectorPhoto.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#ifdef __HAIKU__
#include "ImagePreview.h"
#include <Application.h>
#include <Button.h>
#include <Window.h>
#include <unistd.h>
#endif

namespace {
// Big-endian EXIF generated independently by Pillow's Image.Exif.tobytes().
constexpr char exif[] =
    "\x45\x78\x69\x66\x00\x00\x4d\x4d\x00\x2a\x00\x00\x00\x08\x00\x04"
    "\x01\x0f\x00\x02\x00\x00\x00\x0a\x00\x00\x00\x3e\x01\x10\x00\x02"
    "\x00\x00\x00\x0a\x00\x00\x00\x48\x87\x69\x00\x04\x00\x00\x00\x01"
    "\x00\x00\x00\x52\x88\x25\x00\x04\x00\x00\x00\x01\x00\x00\x00\xac"
    "\x00\x00\x00\x00\x4c\x69\x73\x74\x69\x20\x4c\x61\x62\x00\x46\x69"
    "\x78\x74\x75\x72\x65\x20\x31\x00\x00\x04\x82\x9a\x00\x05\x00\x00"
    "\x00\x01\x00\x00\x00\x88\x82\x9d\x00\x05\x00\x00\x00\x01\x00\x00"
    "\x00\x90\x88\x27\x00\x03\x00\x00\x00\x01\x00\x7d\x00\x00\x90\x03"
    "\x00\x02\x00\x00\x00\x14\x00\x00\x00\x98\x00\x00\x00\x00\x00\x00"
    "\x00\x01\x00\x00\x00\x7d\x00\x00\x00\x0e\x00\x00\x00\x05\x32\x30"
    "\x32\x36\x3a\x30\x31\x3a\x30\x32\x20\x30\x33\x3a\x30\x34\x3a\x30"
    "\x35\x00\x00\x04\x00\x01\x00\x02\x00\x00\x00\x02\x53\x00\x00\x00"
    "\x00\x02\x00\x05\x00\x00\x00\x03\x00\x00\x00\xe2\x00\x03\x00\x02"
    "\x00\x00\x00\x02\x57\x00\x00\x00\x00\x04\x00\x05\x00\x00\x00\x03"
    "\x00\x00\x00\xfa\x00\x00\x00\x00\x00\x00\x00\x0c\x00\x00\x00\x01"
    "\x00\x00\x00\x1e\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x01"
    "\x00\x00\x00\x46\x00\x00\x00\x01\x00\x00\x00\x0f\x00\x00\x00\x01"
    "\x00\x00\x00\x00\x00\x00\x00\x01";

void Segment(std::string &file, uint8_t marker, const std::string &body) {
  unsigned length = body.size() + 2;
  file.append({char(0xff), char(marker), char(length >> 8), char(length)});
  file += body;
}

// Valid baseline JPEG, constant 4800 x 4000 RGB image. Each of the three
// blocks per MCU has zero DC difference and no AC coefficients (two zero bits).
std::string Jpeg(const std::string &metadata) {
  std::string file("\xff\xd8", 2);
  Segment(file, 0xe1, metadata);
  Segment(file, 0xdb, std::string(1, 0) + std::string(64, 1));
  Segment(file, 0xc0,
          std::string("\x08\x0f\xa0\x12\xc0\x03\x01\x11\0\x02\x11\0\x03\x11\0",
                      15));
  std::string table(18, 0);
  table[1] = 1;
  Segment(file, 0xc4, table);
  table[0] = 0x10;
  Segment(file, 0xc4, table);
  Segment(file, 0xda, std::string("\x03\x01\0\x02\0\x03\0\0\x3f\0", 10));
  file.append(4800 / 8 * (4000 / 8) * 6 / 8, 0);
  file.append("\xff\xd9", 2);
  return file;
}

Inspector::PhotoInfo Read(const std::string &file) {
  return Inspector::ReadPhotoInfo([&](size_t p, void *dest, size_t size) {
    if (size > file.size() || p > file.size() - size)
      return false;
    std::memcpy(dest, file.data() + p, size);
    return true;
  });
}

void CheckPhoto(const std::string &metadata, const std::string &file) {
  auto photo = Read(file);
  assert(photo.jpeg && photo.width == 4800 && photo.height == 4000);
  assert(photo.cameraDetails.find("Make: Listi Lab") != std::string::npos);
  assert(photo.cameraDetails.find("Captured: 2026:01:02 03:04:05") !=
         std::string::npos);
  assert(photo.cameraDetails.find("Exposure: 0.008 s") != std::string::npos);
  assert(photo.cameraDetails.find("ISO: 125") != std::string::npos);
  assert(photo.cameraDetails.find("12.5") == std::string::npos);
  assert(photo.hasLocation && photo.latitude == -12.5 &&
         photo.longitude == -70.25);
  auto invalid = metadata;
  // TIFF starts at byte 6; lat. deg. denominator is at 0xe2+4.
  invalid[6 + 0xe2 + 7] = 0;
  assert(!Read(Jpeg(invalid)).hasLocation);
  invalid = metadata;
  invalid[6 + 0xe2 + 3] = 91;
  assert(!Read(Jpeg(invalid)).hasLocation);
  invalid = metadata;
  invalid[6 + 0xac + 10] = 'Q';
  assert(!Read(Jpeg(invalid)).hasLocation);
  invalid = metadata;
  // Corrupt root GPS IFD pointer; ordinary camera fields must survive.
  invalid[6 + 54] = char(0xff);
  photo = Read(Jpeg(invalid));
  assert(!photo.hasLocation &&
         photo.cameraDetails.find("Fixture 1") != std::string::npos);
  // Little-endian IFD, inline ASCII camera model, no GPS.
  std::string tiny("Exif\0\0II\x2a\0\x08\0\0\0\x01\0\x10\x01\x02\0\x04\0\0\0"
                   "ABC\0\0\0\0\0",
                   32);
  photo = Read(Jpeg(tiny));
  assert(photo.cameraDetails == "Model: ABC" && !photo.hasLocation);
  assert(Read(Jpeg("not EXIF")).cameraDetails.empty());
  assert(!Read("not JPEG").jpeg);
  for (size_t n = 0; n < sizeof(exif); ++n)
    assert(!Read(file.substr(0, n)).hasLocation);
}

void CheckGeometry() {
  Inspector::PhotoView view;
  auto fit = view.Region(6000, 4000, 600, 600);
  assert(fit.scale == 0.1 && fit.width == 6000 && fit.y == 100);
  auto small = view.Region(40, 60, 600, 600);
  assert(small.scale == 1 && small.x == 280 && small.y == 270);
  view.zoom = 1;
  auto actual = view.Region(6000, 4000, 600, 600);
  assert(actual.left == 2700 && actual.top == 1700 && actual.width == 600);
  view.Pan(-9000, 9000, 6000, 4000, 600, 600);
  auto edge = view.Region(6000, 4000, 600, 600);
  assert(edge.left == 5400 && edge.top == 0);
  view.zoom = 2;
  view.Pan(200, -200, 6000, 4000, 600, 600);
  auto zoom = view.Region(6000, 4000, 600, 600);
  assert(zoom.left == 5450 && zoom.top == 250 && zoom.width == 300);
  assert(view.Region(6000, 4000, 0, 0).scale == 0);
}
#ifdef __HAIKU__
class PhotoProbe : public ImagePreview {
public:
  sem_id result = create_sem(0, "photo preview result");
  bool loaded = false;
  ~PhotoProbe() override { delete_sem(result); }
  void MessageReceived(BMessage *message) override {
    ImagePreview::MessageReceived(message);
    if (message->what != 'pird')
      return;
    bool visible = false;
    for (int32 i = 0; i < CountChildren(); ++i) {
      auto *button = dynamic_cast<BButton *>(ChildAt(i));
      if (button && !std::strcmp(button->Label(), "Image…"))
        visible = !button->IsHidden(button);
    }
    loaded = visible && message->GetInt64("bitmap", -1) >= 0 &&
             std::string(message->GetString("caption", "")) == "4800 × 4000" &&
             message->GetDouble("latitude", 0) == -12.5;
    if (!loaded)
      std::cerr << "Photo preview: " << message->GetString("caption", "")
                << '\n';
    release_sem(result);
  }
};

void CheckNative(const std::string &file) {
  BApplication app("application/x-vnd.Kambuzi-Listi-inspector-photo-test");
  char path[] = "/tmp/Listi-inspector-photo-XXXXXX";
  int fd = mkstemp(path);
  assert(fd >= 0);
  assert(write(fd, file.data(), file.size()) ==
         static_cast<ssize_t>(file.size()));
  close(fd);
  auto *window = new BWindow(BRect(40, 40, 540, 420), "Photo inspection",
                             B_TITLED_WINDOW, 0);
  auto *preview = new PhotoProbe();
  window->AddChild(preview);
  preview->ShowFile(path);
  window->Show();
  assert(acquire_sem_etc(preview->result, 1, B_RELATIVE_TIMEOUT, 30000000) ==
         B_OK);
  assert(window->Lock());
  bool ok = preview->loaded;
  window->Quit();
  unlink(path);
  assert(ok);
}
#endif
} // namespace

int main() {
  std::string metadata(exif, sizeof(exif) - 1);
  auto file = Jpeg(metadata);
  CheckPhoto(metadata, file);
  CheckGeometry();
#ifdef __HAIKU__
  CheckNative(file);
#endif
  std::cout << "PASS image navigation and JPEG camera metadata\n";
}
