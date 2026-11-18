// SPDX-License-Identifier: MIT
#include "ArjWriter.h"
#include "VirtualFile.h"

#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace Listi;

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

struct FD {
  int value;
  ~FD() {
    if (value >= 0)
      close(value);
  }
};

struct Scratch {
  char path[sizeof("/tmp/listi-arj-test-XXXXXX")] =
      "/tmp/listi-arj-test-XXXXXX";
  Scratch() { Require(mkdtemp(path), "Cannot create ARJ test folder"); }
  ~Scratch() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};

void Write(const std::string &path, const std::string &bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(bytes.data(), bytes.size());
  out.close();
  Require(bool(out), "Cannot create ARJ test source");
}

std::string Read(const std::string &member) {
  std::string bytes;
  ReadFile(member,
           [&](const char *data, size_t count) { bytes.append(data, count); });
  return bytes;
}

void Test(int level) {
  Scratch scratch;
  const auto source = Join(scratch.path, "parent");
  const auto emptyFolder = Join(source, "empty-folder");
  const auto file = Join(source, "space [a].bin");
  std::filesystem::create_directories(emptyFolder);
  const std::string bytes =
      std::string(250001, '\xff') + std::string("\0\n", 2);
  Write(file, bytes);
  Write(Join(source, ".empty"), "");
  constexpr time_t stamp = 1767357296;
  const struct timespec times[2]{{stamp, 0}, {stamp, 0}};
  Require(!utimensat(AT_FDCWD, file.c_str(), times, 0),
          "Cannot set ARJ test source timestamp");

  const auto archive = Join(scratch.path, "output.arj");
  FD output{open(archive.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600)};
  Require(output.value >= 0, "Cannot create ARJ test archive");
  ArjWrite(output.value, {source}, {}, level);
  if (level > 0 && ArjCanCompress()) {
    struct stat result;
    Require(!fstat(output.value, &result) &&
                result.st_size < off_t(bytes.size() / 4),
            "ARJ codec stored compressible data instead of compressing it");
  }
  ArjValidateEdit(output.value);
  const auto root = OpenArchive(archive);
  const auto parent = ChildLocation(root, "parent");
  const auto member = ChildLocation(parent, "space [a].bin");
  Require(ListFiles(root).size() == 1 && ListFiles(parent).size() == 3 &&
              ListFiles(ChildLocation(parent, "empty-folder")).empty(),
          "ARJ listing did not preserve explicit directories");
  Require(Read(member) == bytes &&
              Read(ChildLocation(parent, ".empty")).empty(),
          "ARJ read-back changed bytes, a literal name or a hidden empty file");
  FileEntry entry;
  Require(StatFile(member, entry) && entry.modified == stamp,
          "ARJ read-back did not preserve a representable DOS timestamp");

  bool cancelled = false;
  if (level > 0) {
    const auto cancelledPath = Join(scratch.path, "cancelled.arj");
    FD cancelledOutput{
        open(cancelledPath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600)};
    Hooks stop;
    stop.proceed = [&] { return !cancelled; };
    stop.progress = [&](const std::string &, uint64_t) { cancelled = true; };
    try {
      ArjWrite(cancelledOutput.value, {source}, stop, level);
      Require(false, "ARJ compression ignored cancellation");
    } catch (const Cancelled &) {
    }
  }
  const auto unsuitable = Join(scratch.path, "føø.bin");
  Write(unsuitable, "must not silently rename");
  FD rejectedOutput{open(Join(scratch.path, "unsupported.arj").c_str(),
                         O_WRONLY | O_CREAT | O_EXCL, 0600)};
  Require(rejectedOutput.value >= 0, "Cannot create rejected ARJ output");
  bool unsupportedName = false;
  try {
    ArjWrite(rejectedOutput.value, {unsuitable}, {}, level);
  } catch (const std::runtime_error &) {
    unsupportedName = true;
  }
  Require(unsupportedName, "ARJ writer accepted a nonportable Unicode name");

  const unsigned char unexpectedByte = 42;
  Require(write(output.value, &unexpectedByte, 1) == 1,
          "Cannot add trailing data to ARJ fixture");
  bool rejected = false;
  try {
    ArjValidateEdit(output.value);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  Require(rejected, "ARJ edit accepted an archive with trailing data");
}
} // namespace

int main() {
  try {
    if (!ArjCanCreate()) {
      std::cout << "SKIP optional p7zip ARJ handler not installed\n";
      return 0;
    }
    Test(0);
    if (ArjCanCompress()) {
      for (int level : {2, 6, 9})
        Test(level);
      std::cout << "ARJ compression and cancellation checks passed\n";
    }
    std::cout << "ARJ writer checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "ARJ writer: " << error.what() << '\n';
    return 1;
  }
}
