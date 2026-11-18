// SPDX-License-Identifier: MIT
#include "ClipboardTextFile.h"
#include <Application.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using namespace Listi;

namespace {
int failWrite = 0;
struct Fixture {
  char folder[64] = "/boot/home/Listi-clipboard-text-test-XXXXXX";
  Fixture() {
    if (!mkdtemp(folder))
      throw std::runtime_error("Cannot create fixture.");
  }
  ~Fixture() {
    std::string root(folder);
    for (const char *name : {"Clipboard.txt", "note.txt", "link", "partial.txt"})
      unlink((root + "/" + name).c_str());
    rmdir((root + "/destination").c_str());
    rmdir((root + "/old-destination").c_str());
    rmdir(folder);
  }
};
void Check(bool passed, const char *description) {
  if (!passed)
    throw std::runtime_error(description);
  printf("PASS %s\n", description);
}
template <class F> bool Rejected(F action) {
  try {
    action();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
std::string Read(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
void Set(BClipboard &clipboard, const BMessage &contents) {
  if (!clipboard.Lock())
    throw std::runtime_error("Cannot lock test clipboard.");
  *clipboard.Data() = contents;
  auto status = clipboard.Commit(true);
  clipboard.Unlock();
  if (status != B_OK)
    throw std::runtime_error("Cannot populate test clipboard.");
}
BMessage Text(const std::string &bytes) {
  BMessage message;
  if (message.AddData("text/plain", B_MIME_TYPE, bytes.data(), bytes.size()) != B_OK)
    throw std::runtime_error("Cannot add test clipboard text.");
  return message;
}
}

extern "C" ssize_t __real_write(int, const void *, size_t);
extern "C" ssize_t __wrap_write(int fd, const void *buffer, size_t size) {
  if (failWrite == 0)
    return __real_write(fd, buffer, size);
  if (failWrite++ == 1)
    return __real_write(fd, buffer, 1);
  errno = ENOSPC;
  return -1;
}

int main(int argc, char **argv) {
  BApplication app("application/x-vnd.Kambuzi-ListiClipboardTextFileTest");
  try {
    // Seed real GUI clipboard without exposing it in the default audit.
    if (argc == 3 && std::strcmp(argv[1], "--seed-text") == 0) {
      Set(*be_clipboard, Text(argv[2]));
      return 0;
    }
    Fixture fixture;
    const std::string folder(fixture.folder);
    const char payload[] = "echo do not run\nUTF-8: æøå\n\0trailing bytes";
    const std::string original(payload, sizeof(payload) - 1);
    auto text = Text(original);
    BClipboard clipboard("listi-clipboard-text-file-test");
    Set(clipboard, text);
    Check(CanSaveClipboardTextIn(folder) && !CanSaveClipboardTextIn("@disks") &&
              !CanSaveClipboardTextIn(folder + "/missing"),
          "only available local destinations are allowed");
    auto first = PrepareClipboardTextFile(folder, &clipboard);
    Check(first.suggestedName == "Clipboard.txt", "default text filename");
    std::ofstream(folder + "/Clipboard.txt") << "existing";
    auto frozen = PrepareClipboardTextFile(folder, &clipboard);
    Check(frozen.suggestedName == "Clipboard 2.txt", "suggest an unused filename");
    Set(clipboard, Text("newer clipboard data"));
    auto clipboardCount = clipboard.SystemCount();
    SaveClipboardTextFile(frozen, "note.txt");
    Check(Read(folder + "/note.txt") == original &&
              clipboard.SystemCount() == clipboardCount,
          "save literal frozen bytes without changing clipboard");
    Check(Rejected([&] { SaveClipboardTextFile(first, "Clipboard.txt"); }) &&
              Read(folder + "/Clipboard.txt") == "existing",
          "a later collision never overwrites a file");
    Check(symlink("missing-target", (folder + "/link").c_str()) == 0 &&
              Rejected([&] { SaveClipboardTextFile(frozen, "link"); }),
          "a dangling symlink is never followed or replaced");
    Check(Rejected([&] { SaveClipboardTextFile(frozen, "../escape"); }) &&
              Rejected([&] { SaveClipboardTextFile(frozen, "."); }),
          "the name must be a single leaf");
    const auto destination = folder + "/destination";
    const auto old = folder + "/old-destination";
    Check(mkdir(destination.c_str(), 0700) == 0, "create destination fixture");
    auto stale = PrepareClipboardTextFile(destination, &clipboard);
    Check(rename(destination.c_str(), old.c_str()) == 0 &&
              mkdir(destination.c_str(), 0700) == 0 &&
              Rejected([&] { SaveClipboardTextFile(stale, "unexpected.txt"); }),
          "a replaced destination is rejected");
    failWrite = 1;
    bool rejected = Rejected([&] { SaveClipboardTextFile(frozen, "partial.txt"); });
    failWrite = 0;
    struct stat partial;
    Check(rejected && lstat((folder + "/partial.txt").c_str(), &partial) < 0 &&
              errno == ENOENT,
          "failed partial write leaves no output");
    BMessage refs = MakeFileSelection({folder + "/Clipboard.txt"});
    Set(clipboard, refs);
    Check(Rejected([&] { PrepareClipboardTextFile(folder, &clipboard); }),
          "native file references are not text");
    refs.AddData("text/plain", B_MIME_TYPE, original.data(), original.size());
    Set(clipboard, refs);
    Check(Rejected([&] { PrepareClipboardTextFile(folder, &clipboard); }),
          "mixed file and text clipboard is not converted");
    BMessage archive;
    text.AddMessage("listi:archive-copy", &archive);
    Set(clipboard, text);
    Check(Rejected([&] { PrepareClipboardTextFile(folder, &clipboard); }),
          "archive selection is not converted");
    Set(clipboard, BMessage());
    Check(Rejected([&] { PrepareClipboardTextFile(folder, &clipboard); }),
          "empty clipboard is rejected");
  } catch (const std::exception &e) {
    fprintf(stderr, "FAIL %s\n", e.what());
    return 1;
  }
  return 0;
}
