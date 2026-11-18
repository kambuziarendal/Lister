// SPDX-License-Identifier: MIT
#include "ClipboardTextFile.h"
#include "NamedEntry.h"
#include "ClipboardImage.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace Listi {
namespace {
struct FD {
  int value;
  explicit FD(int fd) : value(fd) {}
  ~FD() {
    if (value >= 0)
      close(value);
  }
  FD(const FD &) = delete;
  FD &operator=(const FD &) = delete;
};

bool IsFullPath(const std::string &folder) {
  return !folder.empty() && folder.front() == '/' &&
         folder.find('\0') == std::string::npos;
}

bool IsWritable(int directory) {
  struct statvfs volume;
  return directory >= 0 && fstatvfs(directory, &volume) == 0 &&
         !(volume.f_flag & ST_RDONLY) &&
         faccessat(directory, ".", W_OK | X_OK, 0) == 0;
}

void Fail(const char *operation) {
  throw std::runtime_error(std::string(operation) + ": " + strerror(errno));
}
}

bool CanSaveClipboardIn(const std::string &folder) {
  if (!IsFullPath(folder))
    return false;
  FD directory(open(folder.c_str(), O_RDONLY | O_DIRECTORY));
  return IsWritable(directory.value);
}

ClipboardFile PrepareClipboardFile(const std::string &folder,
                                   ClipboardContent content,
                                   BClipboard *clipboard) {
  FD directory(IsFullPath(folder) ? open(folder.c_str(), O_RDONLY | O_DIRECTORY)
                                  : -1);
  struct stat st;
  if (!IsWritable(directory.value) || fstat(directory.value, &st))
    throw std::runtime_error("Open a writable local folder first.");
  const bool image = content == ClipboardContent::Image;
  std::string extension = image ? ".png" : ".txt";
  ClipboardFile file{folder,
                     image ? ReadClipboardPNG(clipboard)
                           : ReadClipboardText(clipboard),
                     "Clipboard" + extension,
                     {int32_t(st.st_dev), int64_t(st.st_ino)}};
  for (int i = 1; i <= 1000; ++i) {
    if (i > 1)
      file.suggestedName = "Clipboard " + std::to_string(i) + extension;
    if (fstatat(directory.value, file.suggestedName.c_str(), &st,
                AT_SYMLINK_NOFOLLOW) == 0)
      continue;
    if (errno == ENOENT)
      return file;
    Fail("Check suggested filename");
  }
  // Leave the field editable even if all default names are in use.
  return file;
}

void SaveClipboardFile(const ClipboardFile &file, const std::string &name) {
  ValidateEntryLeaf(name);
  FD directory(open(file.folder.c_str(), O_RDONLY | O_DIRECTORY));
  struct stat folder;
  if (directory.value < 0 || fstat(directory.value, &folder) ||
      folder.st_dev != file.destination.device ||
      folder.st_ino != file.destination.inode)
    throw std::runtime_error(
        "The destination folder changed. Start the save again.");

  const int dir = directory.value;
  const char *leaf = name.c_str();
  // BFS has no hard links or atomic rename without replacement. Exclusive
  // create is the publication guard; clean up incomplete writes on failure.
  FD output(openat(dir, leaf, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600));
  if (output.value < 0) {
    if (errno == EEXIST)
      throw std::runtime_error(
          "That name already exists. No file was replaced.");
    Fail("Create clipboard file");
  }
  struct stat identity;
  if (fstat(output.value, &identity))
    Fail("Inspect new clipboard file");
  try {
    for (size_t offset = 0; offset < file.bytes.size();) {
      ssize_t n = write(output.value, file.bytes.data() + offset,
                        file.bytes.size() - offset);
      if (n < 0 && errno == EINTR)
        continue;
      if (n < 0)
        Fail("Write clipboard file");
      if (n == 0)
        throw std::runtime_error(
            "Writing the clipboard file made no progress.");
      offset += n;
    }
    if (fsync(output.value))
      Fail("Sync clipboard file");
  } catch (...) {
    struct stat current;
    if (fstatat(dir, leaf, &current, AT_SYMLINK_NOFOLLOW) == 0 &&
        current.st_dev == identity.st_dev &&
        current.st_ino == identity.st_ino && unlinkat(dir, leaf, 0))
      Fail("Could not remove incomplete clipboard file");
    throw;
  }
  // A descriptor remains valid after a rename. Keep completed bytes, but do
  // not report success at a path that no longer names the saved file.
  struct stat namedFolder, namedFile;
  if (stat(file.folder.c_str(), &namedFolder) ||
      namedFolder.st_dev != folder.st_dev || namedFolder.st_ino != folder.st_ino ||
      fstatat(dir, leaf, &namedFile, AT_SYMLINK_NOFOLLOW) ||
      namedFile.st_dev != identity.st_dev || namedFile.st_ino != identity.st_ino)
    throw std::runtime_error("The clipboard file or its folder moved during the save.");
}
}
