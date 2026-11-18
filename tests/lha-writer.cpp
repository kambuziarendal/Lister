// SPDX-License-Identifier: MIT
#include "LhaWriter.h"
#include "VirtualFile.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace Listi;

namespace {

void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

struct Scratch {
  std::string path;
  Scratch() {
    char directory[] = "/tmp/listi-lha-test-XXXXXX";
    Require(mkdtemp(directory), "Cannot create test folder");
    path = directory;
  }
  ~Scratch() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};

void Write(const std::string &path, const std::string &bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(bytes.data(), bytes.size());
  output.close();
  Require(bool(output), "Cannot write LHA test file");
}

std::string Read(const std::string &url) {
  std::string bytes;
  ReadFile(url, [&](const char *data, size_t n) { bytes.append(data, n); });
  return bytes;
}

void Test() {
  Scratch scratch;
  const auto first = scratch.path + "/first";
  const auto second = scratch.path + "/second";
  const auto folder = first + "/directory";
  const auto file = folder + "/-binary";
  const auto text = second + "/føø bar";
  const auto archive = scratch.path + "/output.lzh";
  std::filesystem::create_directories(folder + "/empty-directory");
  std::filesystem::create_directories(second);
  const std::string binary = std::string(250000, 'L') +
                             std::string("\0\n\xff", 3) +
                             std::string(150000, 'H');
  Write(file, binary);
  Write(folder + "/empty", "");
  Write(folder + "/.hidden \x24(test); [x]", "literal path");
  Write(text, "second root");
  Require(chmod(file.c_str(), 0751) == 0, "Cannot set LHA test mode");

  for (int level : {0, 6}) {
    int output = open(archive.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    Require(output >= 0, "Cannot open LHA fixture output");
    try {
      LhaCreate(output, {folder, text}, level, {});
    } catch (...) {
      close(output);
      throw;
    }
    close(output);

    const auto root = OpenArchive(archive);
    Require(!root.empty() && ListFiles(root).size() == 2,
            "Cannot browse both LHA roots");
    const auto directory = ChildLocation(root, "directory");
    Require(ListFiles(directory).size() == 4,
            "Incorrect LHA directory listing");
    auto member = ChildLocation(directory, "-binary");
    Require(Read(member) == binary, "LHA did not preserve binary bytes");
    Require(Read(ChildLocation(directory, ".hidden \x24(test); [x]")) ==
                "literal path",
            "LHA did not preserve a literal hidden filename");
    Require(Read(ChildLocation(directory, "empty")).empty(),
            "LHA did not preserve an empty file");
    Require(ListFiles(ChildLocation(directory, "empty-directory")).empty(),
            "LHA did not preserve an empty folder");
    Require(Read(ChildLocation(root, "føø bar")) == "second root",
            "LHA did not preserve a Unicode filename");
    FileEntry entry;
    Require(StatFile(member, entry) && entry.permissions == 0751,
            "LHA did not preserve file permissions");
  }

  int output = open(archive.c_str(), O_WRONLY | O_TRUNC);
  Require(output >= 0, "Cannot open cancellation fixture output");
  bool cancelled = false;
  Hooks stop;
  stop.proceed = [] { return false; };
  try {
    LhaCreate(output, {folder}, 6, stop);
  } catch (const Cancelled &) {
    cancelled = true;
  }
  struct stat state;
  Require(cancelled && fstat(output, &state) == 0 && state.st_size == 0,
          "Cancelled LHA creation wrote output");
  Require(symlink(file.c_str(), (folder + "/external").c_str()) == 0,
          "Cannot create LHA link fixture");
  bool rejected = false;
  try {
    LhaCreate(output, {folder}, 6, {});
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  Require(rejected && fstat(output, &state) == 0 && state.st_size == 0,
          "LHA writer did not refuse a source link");
  close(output);
}
} // namespace

int main() {
  try {
    if (!LhaCanCreate()) {
      std::cout << "SKIP optional LHA writer or reader is not installed\n";
      return 0;
    }
    Test();
    std::cout << "PASS LHA stored and compressed round trips, cancellation and "
                 "link refusal\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL LHA: " << e.what() << '\n';
    return 1;
  }
}
