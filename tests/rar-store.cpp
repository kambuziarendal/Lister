// SPDX-License-Identifier: MIT
#include "RarStore.h"
#include "VirtualFile.h"

#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using namespace Listi;

namespace {
void Require(bool condition, const char *error) {
  if (!condition)
    throw std::runtime_error(error);
}

struct FD {
  int value;
  ~FD() {
    if (value >= 0)
      close(value);
  }
};

struct Scratch {
  char path[sizeof("/tmp/listi-rar-test-XXXXXX")] =
      "/tmp/listi-rar-test-XXXXXX";
  Scratch() { Require(mkdtemp(path), "Cannot create RAR5 test folder"); }
  ~Scratch() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};

void Write(const std::string &path, const std::string &bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(bytes.data(), bytes.size());
  out.close();
  Require(bool(out), "Cannot create RAR5 test source");
}

std::string Read(const std::string &member) {
  std::string bytes;
  ReadFile(member,
           [&](const char *data, size_t count) { bytes.append(data, count); });
  return bytes;
}

void Reject(int fd) {
  bool rejected = false;
  try {
    RarStoreValidateEdit(fd);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  Require(rejected, "RAR5 edit accepted an unsupported archive");
}

void Test() {
  Scratch scratch;
  const auto source = Join(scratch.path, "parent");
  const auto emptyFolder = Join(source, "empty-folder");
  const auto file = Join(source, "blå [a].bin");
  std::filesystem::create_directories(emptyFolder);
  const std::string bytes =
      std::string(250001, '\xff') + std::string("\0\n", 2);
  Write(file, bytes);
  Write(Join(source, ".empty"), "");
  const struct timespec times[2]{{0, 0}, {0, 0}};
  Require(!utimensat(AT_FDCWD, file.c_str(), times, 0) &&
              !chmod(file.c_str(), 0751),
          "Cannot set RAR5 test source metadata");

  const auto archive = Join(scratch.path, "output.rar");
  FD output{open(archive.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600)};
  Require(output.value >= 0, "Cannot create RAR5 test archive");
  RarStoreWrite(output.value, {source}, {});
  RarStoreValidateEdit(output.value);
  const auto root = OpenArchive(archive);
  Require(!root.empty(), "RAR5 Store archive could not be opened");
  const auto parent = ChildLocation(root, "parent");
  const auto member = ChildLocation(parent, "blå [a].bin");
  Require(ListFiles(root).size() == 1 && ListFiles(parent).size() == 3 &&
              ListFiles(ChildLocation(parent, "empty-folder")).empty(),
          "RAR5 listing did not preserve explicit directories");
  Require(Read(member) == bytes &&
              Read(ChildLocation(parent, ".empty")).empty(),
          "RAR5 read-back changed bytes, a Unicode name or an empty file");
  FileEntry entry;
  Require(StatFile(member, entry) && entry.modified == 0 &&
              entry.modifiedKnown && entry.permissions == 0751,
          "RAR5 read-back changed Unix mode or epoch mtime");

  const auto empty = Join(scratch.path, "empty.rar");
  FD emptyOutput{open(empty.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600)};
  Require(emptyOutput.value >= 0, "Cannot create empty RAR5 test archive");
  RarStoreWrite(emptyOutput.value, {}, {});
  RarStoreValidateEdit(emptyOutput.value);
  const auto emptyRoot = OpenArchive(empty);
  Require(!emptyRoot.empty() && ListFiles(emptyRoot).empty(),
          "An empty RAR5 archive could not be reopened");

  const unsigned char unexpectedByte = 42;
  Require(write(output.value, &unexpectedByte, 1) == 1,
          "Cannot add trailing signature data to RAR5 fixture");
  Reject(output.value);
  Require(!ftruncate(output.value, lseek(output.value, 0, SEEK_CUR) - 1) &&
              pwrite(output.value, &unexpectedByte, 1, 8) == 1,
          "Cannot corrupt header CRC in RAR5 fixture");
  Reject(output.value);

  for (const auto *fixture : {"tests/fixtures/sevenzip/rar4-data.rar",
                              "tests/fixtures/sevenzip/rar5-headers.rar",
                              "tests/fixtures/sevenzip/rar5-mixed.rar",
                              "tests/fixtures/multipart/plain.part1.rar"}) {
    FD input{open(fixture, O_RDONLY)};
    Require(input.value >= 0, "Cannot open RAR5 edit guard fixture");
    Reject(input.value);
  }

  FD cancelled{open(Join(scratch.path, "cancelled.rar").c_str(),
                    O_RDWR | O_CREAT | O_EXCL, 0600)};
  Require(cancelled.value >= 0, "Cannot create RAR5 cancellation test archive");
  bool cancel = false, rejected = false;
  Hooks hooks;
  hooks.progress = [&](const std::string &, uint64_t) { cancel = true; };
  hooks.proceed = [&] { return !cancel; };
  try {
    RarStoreWrite(cancelled.value, {source}, hooks);
  } catch (const Cancelled &) {
    rejected = true;
  }
  Require(rejected, "RAR5 writer ignored cancellation during a file");
}
} // namespace

int main() {
  try {
    if (!RarStoreCanCreate()) {
      std::cout << "SKIP optional p7zip RAR5 handler not installed\n";
      return 0;
    }
    Test();
    std::cout << "RAR5 Store writer checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "RAR5 Store: " << error.what() << '\n';
    return 1;
  }
}
