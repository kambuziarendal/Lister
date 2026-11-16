// SPDX-License-Identifier: MIT
#include "VirtualFile.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <pthread.h>
using namespace Listi;
std::string bytes(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) throw std::runtime_error("Missing expected fixture: " + path);
  return {std::istreambuf_iterator<char>(file), {}};
}
int Qualify(int argc, char **argv) {
  if (argc != 3) return 2;
  const std::string root = argv[1];
  int count = 0, failed = 0;
  auto check = [&](bool ok, const std::string &label) {
    ++count; failed += !ok;
    std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
  };
  auto rejected = [&](auto fn) {
    try { fn(); } catch (const std::exception &) { return true; }
    return false;
  };
  auto read = [](const std::string &path) {
    std::string out;
    ReadFile(path, [&](const char *p, size_t n) { out.append(p, n); });
    return out;
  };
  std::ifstream manifest(argv[2]);
  if (!manifest) return 2;
  std::string line;
  while (std::getline(manifest, line)) {
    std::istringstream fields(line); std::string file, entry, expected;
    std::getline(fields, file, '\t'); std::getline(fields, entry, '\t'); std::getline(fields, expected);
    try {
      auto url = OpenArchive(root + "/" + file);
      check(!url.empty(), file + " detected");
      auto folder = url;
      auto at = entry.rfind('/');
      if (at != std::string::npos) folder = url + EncodePath(entry.substr(0, at));
      auto rows = ListFiles(folder);
      bool found = false;
      for (auto &row : rows) if (row.name == LocationLeaf(entry) && !row.directory) found = true;
      check(found, file + " browsed");
      auto full = url + EncodePath(entry);
      check(read(full) == bytes(root + "/" + expected), file + " exact extracted bytes");
      TransferControl cancelled; cancelled.proceed = [] { return false; };
      bool stopped = false;
      try { ReadFile(full, [](const char *, size_t) {}, cancelled); } catch (const Cancelled &) { stopped = true; }
      check(stopped, file + " cancellable");
    } catch (const std::exception &e) { check(false, file + ": " + e.what()); }
  }
  check(OpenArchive(root + "/ordinary.txt").empty(), "ordinary text is not archive");
  check(OpenArchive(root + "/empty.txt").empty(), "empty ordinary file is not archive");
  check(rejected([&] { ListFiles(OpenArchive(root + "/broken.7z")); }), "truncated archive rejected");
  check(rejected([&] { ListFiles(OpenArchive(root + "/unsafe.zip")); }), "traversal listing refused");
  check(rejected([&] { read(OpenArchive(root + "/links.tar") + "link"); }), "symlink extraction refused");
  for (const char *name : {"test_read_format_rar5_encrypted.rar",
                            "test_read_format_rar5_multiarchive.part01.rar"}) {
    std::ifstream fixture(root + "/" + name);
    if (!fixture) continue;
    std::function<void(const std::string &, int)> walk;
    walk = [&](const std::string &folder, int depth) {
      if (depth > 32) throw std::runtime_error("Fixture too deep");
      for (const auto &entry : ListFiles(folder)) {
        if (entry.directory) walk(entry.path, depth + 1);
        else if (!entry.link) read(entry.path);
      }
    };
    check(rejected([&] { walk(OpenArchive(root + "/" + name), 0); }),
          std::string(name) + " unsupported encryption/missing volume fails explicitly");
  }
  std::cout << count << " checks, " << failed << " failures" << std::endl;
  return failed ? 1 : 0;
}

// Haiku worker stacks are much smaller than main's stack. Archive extraction
// must also be safe when called by preview and queued-copy worker threads.
struct Arguments { int argc; char **argv; int result = 2; };
int main(int argc, char **argv) {
  Arguments args{argc, argv};
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 256 * 1024);
  pthread_t worker;
  int error = pthread_create(&worker, &attr, [](void *raw) -> void * {
    auto &args = *static_cast<Arguments *>(raw);
    args.result = Qualify(args.argc, args.argv);
    return nullptr;
  }, &args);
  pthread_attr_destroy(&attr);
  if (error) return 2;
  pthread_join(worker, nullptr);
  return args.result;
}
