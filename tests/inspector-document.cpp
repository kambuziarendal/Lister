// SPDX-License-Identifier: MIT
#include "InspectorDocument.h"
#include <Node.h>
#include <TypeConstants.h>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using Inspector::Document;
namespace {
struct stat sourceNode;
int failSync = 0, concurrentWriter = -1;
std::string moveSource;

void Check(bool ok, const char *label) {
  if (!ok)
    throw std::runtime_error(label);
  std::cout << "PASS " << label << std::endl;
}

void Write(const std::string &path, const std::string &bytes) {
  std::ofstream file(path, std::ios::binary);
  file.write(bytes.data(), bytes.size());
  file.close();
  if (!file)
    throw std::runtime_error("Cannot write fixture");
}

std::string Read(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("Cannot read fixture: " + path);
  return {(std::istreambuf_iterator<char>(file)), {}};
}

template <class Action> std::string Error(Action action) {
  try {
    action();
  } catch (const std::exception &error) {
    return error.what();
  }
  throw std::runtime_error("Expected rejection");
}

std::string RecoveryPath(const std::string &error) {
  const std::string label = "Original bytes: ";
  auto pos = error.find(label);
  if (pos == std::string::npos)
    throw std::runtime_error("No recovery path in: " + error);
  return error.substr(pos + label.size());
}
}

extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat st;
  if (!fstat(fd, &st) && st.st_dev == sourceNode.st_dev &&
      st.st_ino == sourceNode.st_ino) {
    if (!moveSource.empty()) {
      auto path = moveSource;
      moveSource.clear();
      if (rename(path.c_str(), (path + ".moved").c_str()) != 0)
        abort();
      Write(path, "replacement");
    }
    if (concurrentWriter >= 0) {
      if (pwrite(concurrentWriter, "third", 5, 0) != 5)
        abort();
      close(concurrentWriter);
      concurrentWriter = -1;
    }
    if (failSync) {
      --failSync;
      errno = EIO;
      return -1;
    }
  }
  return __real_fsync(fd);
}

int main() {
  char root[] = "/boot/home/Listi-document-XXXXXX";
  if (!mkdtemp(root))
    return 1;
  const std::string path = std::string(root) + "/note";
  const std::string bom = "\xef\xbb\xbf";
  try {
    Write(path, bom + "æ\r\nline\r\n");
    Check(chmod(path.c_str(), 0750) == 0 && !stat(path.c_str(), &sourceNode),
          "editable mode fixture");
    int32 number = 42;
    {
      BNode node(path.c_str());
      Check(node.WriteAttr("Listi:number", B_INT32_TYPE, 0, &number,
                           sizeof(number)) == sizeof(number),
            "typed BFS fixture");
    }
    {
      Document doc(path);
      Check(doc.Text() == "æ\nline\n", "editor sees LF and no encoding marker");
      doc.Save("π\n🎵");
      Check(Read(path) == bom + "π\r\n🎵",
            "preserves BOM, CRLF and final newline choice");
      doc.Save("\nx\n");
      Check(doc.Text() == "\nx\n" && Read(path) == bom + "\r\nx\r\n",
            "second save uses committed baseline and format");
      struct stat after;
      BNode result(path.c_str());
      int32 value = 0;
      Check(!stat(path.c_str(), &after) && after.st_ino == sourceNode.st_ino &&
                after.st_mode == sourceNode.st_mode &&
                result.ReadAttr("Listi:number", B_INT32_TYPE, 0, &value,
                                sizeof(value)) == sizeof(value) &&
                value == number,
            "preserves node, permissions and typed BFS attribute");
      auto before = Read(path);
      Error([&] { doc.Save(std::string(Inspector::kDocumentLimit, '\n')); });
      Error([&] { doc.Save("a\rb"); });
      Error([&] { doc.Save(std::string("a\0b", 3)); });
      Error([&] { doc.Save("\xf0\x80\x80\x80"); });
      Check(Read(path) == before,
            "invalid edits and oversized encoded bytes leave source intact");
    }

    for (const auto &bad :
         {std::string("a\r\nb\n"), std::string("a\rb"), std::string("a\0b", 3),
          std::string(8192, 'a') + "\xc3",
          std::string(Inspector::kDocumentLimit + 1, 'x')}) {
      Write(path, bad);
      Error([&] { Document doc(path); });
      Check(Read(path) == bad,
            "unsupported or incomplete source remains untouched");
    }
    Write(path, "");
    {
      Document doc(path);
      doc.Save("LF\n");
      Check(Read(path) == "LF\n", "empty document uses LF without a BOM");
    }

    Check(!chmod(path.c_str(), 0444), "read-only fixture");
    Error([&] { Document doc(path); });
    Check(!chmod(path.c_str(), 0644), "restore writable fixture");
    const std::string link = path + ".link", fifo = path + ".fifo";
    Check(!symlink(path.c_str(), link.c_str()) && !mkfifo(fifo.c_str(), 0600),
          "special entry fixtures");
    for (auto &other :
         {link, fifo, std::string("archive:///tmp/source.zip!/note"),
          std::string(root)})
      Error([&] { Document doc(other); });
    Check(!unlink(link.c_str()) && !unlink(fifo.c_str()),
          "non-file selections refused");

    Write(path, "first");
    {
      Document doc(path);
      Write(path, "other");
      Error([&] { doc.Save("local"); });
      Check(Read(path) == "other",
            "same-size external edit is not overwritten");
      Check(!rename(path.c_str(), (path + ".other").c_str()), "move source");
      Write(path, "first");
      Error([&] { doc.Save("local"); });
      Check(
          Read(path) == "first" && Read(path + ".other") == "other",
          "same-byte replacement has a different identity and is not touched");
      unlink((path + ".other").c_str());
    }

    {
      Document doc(path);
      Check(!stat(path.c_str(), &sourceNode), "save fault fixture");
      failSync = 1;
      Error([&] { doc.Save("x"); });
      Check(!failSync && Read(path) == "first" && doc.Text() == "first",
            "failed sync rolls shortened data back to the exact original");
      failSync = 2;
      auto backup = RecoveryPath(Error([&] { doc.Save("longer original"); }));
      Check(!failSync && Read(backup) == "first",
            "failed rollback retains readable original");
      unlink(backup.c_str());
      doc.Save("again");
      Check(Read(path) == "again", "safe retry still uses original baseline");
      concurrentWriter = open(path.c_str(), O_RDWR);
      Check(concurrentWriter >= 0,
            "independent writer opened before save lock");
      backup = RecoveryPath(Error([&] { doc.Save("local"); }));
      Check(concurrentWriter == -1 && Read(path) == "third" &&
                Read(backup) == "again",
            "late concurrent edit is not clobbered by rollback");
      unlink(backup.c_str());
    }
    {
      Document doc(path);
      moveSource = path;
      auto backup = RecoveryPath(Error([&] { doc.Save("local"); }));
      Check(Read(path) == "replacement" && Read(path + ".moved") == "third" &&
                Read(backup) == "third",
            "late name collision leaves replacement and original intact");
      unlink(backup.c_str());
      unlink((path + ".moved").c_str());
    }
    Check(!unlink(path.c_str()) && !rmdir(root),
          "successful saves and rollbacks leave no debris");
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL " << error.what() << " (" << root << ")" << std::endl;
    return 1;
  }
}
