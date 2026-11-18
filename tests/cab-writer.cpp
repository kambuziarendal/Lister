// SPDX-License-Identifier: MIT
#include "ArchiveBackend.h"
#include "CabWriter.h"
#include <Application.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

using namespace Listi;
namespace {
void Check(bool ok, const char *description) {
  if (!ok)
    throw std::runtime_error(description);
  std::cout << "PASS " << description << '\n';
}

struct File {
  int fd;
  explicit File(const std::string &path, int flags)
      : fd(open(path.c_str(), flags, 0600)) {
    if (fd < 0)
      throw std::runtime_error("Cannot open fixture: " + path);
  }
  ~File() { close(fd); }
};
struct Root {
  std::string path;
  Root() {
    char name[] = "/tmp/Listi-cab-writer-XXXXXX";
    auto *created = mkdtemp(name);
    if (!created)
      throw std::runtime_error("Cannot create fixture folder.");
    path = created;
  }
  ~Root() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
void Save(const std::string &path, const std::string &data) {
  std::ofstream file(path, std::ios::binary);
  file.write(data.data(), data.size());
  if (!file)
    throw std::runtime_error("Cannot write fixture.");
}
std::string Read(const std::string &path) {
  std::string data;
  ReadFile(path, [&](const char *bytes, size_t length) {
    data.append(bytes, length);
  });
  return data;
}
template <typename Call> bool Refuses(Call call) {
  try {
    call();
  } catch (const std::runtime_error &) {
    return true;
  }
  return false;
}
} // namespace

int main() {
  BApplication app("application/x-vnd.Listi-CabWriter-Test");
  try {
    Root root;
    auto folder = root.path + "/content";
    std::filesystem::create_directory(folder);
    std::string payload(100003, '\0');
    for (size_t i = 0; i < payload.size(); ++i)
      payload[i] = char((i * 31 + i / 20) & 255);
    const auto name = std::string("Blåbær.bin");
    Save(folder + "/" + name, payload);
    Save(folder + "/tiny.txt", "tiny");
    if (chmod((folder + "/tiny.txt").c_str(), 0444))
      throw std::runtime_error("Cannot set read-only fixture.");
    Save(folder + "/zero.txt", "");
    auto archive = root.path + "/output.cab";
    File out(archive, O_CREAT | O_RDWR | O_TRUNC);
    CabWrite(out.fd, {folder}, {});
    CabValidateEdit(out.fd);
    const auto url = OpenArchive(archive);
    const auto child = ChildLocation(url, "content");
    Check(ListFiles(child).size() == 3,
          "Implicit Cabinet folder lists three files");
    Check(Read(ChildLocation(child, name)) == payload,
          "UTF-8 path and multi-block payload round-trip");
    const auto tiny = ChildLocation(child, "tiny.txt");
    FileEntry metadata;
    Check(Read(tiny) == "tiny" && StatFile(tiny, metadata) &&
              !(metadata.permissions & 0222),
          "Packed small file content and DOS read-only attribute round-trip");
    Check(Read(ChildLocation(child, "zero.txt")).empty(),
          "Packed empty file round-trips");

    auto zeroCab = root.path + "/zero.cab";
    File zeroOut(zeroCab, O_CREAT | O_RDWR | O_TRUNC);
    CabWrite(zeroOut.fd, {folder + "/zero.txt"}, {});
    CabValidateEdit(zeroOut.fd);
    Check(Read(ChildLocation(OpenArchive(zeroCab), "zero.txt")).empty(),
          "Cabinet containing only an empty file round-trips");

    auto empty = root.path + "/empty";
    std::filesystem::create_directory(empty);
    File rejected(root.path + "/rejected.cab", O_CREAT | O_RDWR | O_TRUNC);
    Check(Refuses([&] { CabWrite(rejected.fd, {empty}, {}); }),
          "Empty folders are rejected instead of silently dropped");
    auto link = root.path + "/link";
    symlink((folder + "/" + name).c_str(), link.c_str());
    Check(Refuses([&] { CabWrite(rejected.fd, {link}, {}); }),
          "Symlink source rejected");
    Save(folder + "/TINY.txt", "collision");
    Check(Refuses([&] { CabWrite(rejected.fd, {folder}, {}); }),
          "Case-insensitive member collision rejected");
    std::filesystem::remove(folder + "/TINY.txt");

    auto update = folder + "/" + name;
    Hooks changed;
    bool changedSource = false;
    changed.progress = [&](const auto &path, uint64_t) {
      if (path == update && !changedSource) {
        changedSource = true;
        Save(update, payload + "external change");
      }
    };
    Check(Refuses([&] { CabWrite(rejected.fd, {folder}, changed); }) &&
              changedSource,
          "Source modified during packing rejected");
    Hooks canceled;
    canceled.proceed = [] { return false; };
    try {
      CabWrite(rejected.fd, {folder}, canceled);
      Check(false, "Cabinet cancellation propagated");
    } catch (const Cancelled &) {
      Check(true, "Cabinet cancellation propagated");
    }

    unsigned char attributes = 0;
    const unsigned char hidden = 0x02;
    Check(pread(out.fd, &attributes, 1, 58) == 1 &&
              pwrite(out.fd, &hidden, 1, 58) == 1 &&
              Refuses([&] { CabValidateEdit(out.fd); }) &&
              pwrite(out.fd, &attributes, 1, 58) == 1,
          "Cabinet with unrepresentable DOS attributes refused for edits");
    const unsigned char nextVolume = 2;
    Check(pwrite(out.fd, &nextVolume, 1, 30) == 1 &&
              Refuses([&] { CabValidateEdit(out.fd); }),
          "Spanned Cabinet refused for edits");
    const unsigned char reserve = 4;
    Check(pwrite(out.fd, &reserve, 1, 30) == 1 &&
              Refuses([&] { CabValidateEdit(out.fd); }),
          "Reserved or signed Cabinet refused for edits");
    const unsigned char clear = 0;
    Check(pwrite(out.fd, &clear, 1, 30) == 1 &&
              pwrite(out.fd, &clear, 1, lseek(out.fd, 0, SEEK_END)) == 1 &&
              Refuses([&] { CabValidateEdit(out.fd); }),
          "Unexpected bytes beyond declared Cabinet length refused for edits");
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
