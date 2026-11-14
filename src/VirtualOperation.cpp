// SPDX-License-Identifier: MIT
#include "FileOperation.h"
#include "VirtualFile.h"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Lister {
namespace {
std::atomic<unsigned long> sequence{0};
std::string Token() {
  return ".listi-" + std::to_string(getpid()) + "-" +
         std::to_string(++sequence) + "-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count());
}
struct TempFile {
  std::string folder, path;
  int fd = -1;
  explicit TempFile(const std::string &name) {
    char pattern[] = "/tmp/Listi-transfer-XXXXXX";
    char *p = mkdtemp(pattern);
    if (!p)
      throw std::runtime_error(strerror(errno));
    folder = p;
    path = ChildLocation(folder, name);
    fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) {
      rmdir(folder.c_str());
      throw std::runtime_error(strerror(errno));
    }
  }
  ~TempFile() {
    if (fd >= 0)
      close(fd);
    unlink(path.c_str());
    rmdir(folder.c_str());
  }
  void Write(const char *p, size_t n) {
    while (n) {
      auto r = write(fd, p, n);
      if (r < 0 && errno == EINTR)
        continue;
      if (r <= 0)
        throw std::runtime_error(strerror(errno));
      p += r;
      n -= r;
    }
  }
  void Finish(const FileEntry &source) {
    // Preserve available portable metadata, never import setuid/setgid bits.
    if (fchmod(fd, source.permissions & 0777))
      throw std::runtime_error(strerror(errno));
    if (source.modified > 0) {
      struct timespec times[2] = {{0, UTIME_OMIT}, {source.modified, 0}};
      if (futimens(fd, times))
        throw std::runtime_error(strerror(errno));
    }
    if (fsync(fd))
      throw std::runtime_error(strerror(errno));
    if (close(fd)) {
      fd = -1;
      throw std::runtime_error(strerror(errno));
    }
    fd = -1;
  }
};
bool Same(const FileEntry &a, const FileEntry &b) {
  return a.directory == b.directory && a.link == b.link && a.size == b.size &&
         a.sizeKnown == b.sizeKnown && a.modified == b.modified;
}
struct Runner {
  const Hooks &hooks;
  Result result;
  bool replaceAll = false, skipAll = false;
  size_t visited = 0;
  TransferControl Control(const std::string &p, bool bytes = true) {
    return {hooks.proceed, [&, p, bytes](uint64_t n) {
              if (hooks.progress)
                hooks.progress(p, bytes ? n : 0);
            }};
  }
  void Stage(const std::string &src, const FileEntry &expected, TempFile &temp,
             const TransferControl &control) {
    uint64_t bytes = 0;
    ReadFile(
        src,
        [&](const char *data, size_t size) {
          temp.Write(data, size);
          bytes += size;
        },
        control);
    FileEntry after;
    // Remote metadata is not a transactional snapshot. Reject observable
    // changes; never claim this detects same-size edits on servers without
    // timestamps.
    if (!StatFile(src, after, control) || !Same(expected, after) ||
        (expected.sizeKnown && bytes != uint64_t(expected.size)))
      throw std::runtime_error(
          "Source changed during transfer; destination was not committed");
    temp.Finish(expected);
  }
  bool Replace(const std::string &p) {
    if (skipAll) {
      ++result.skipped;
      return false;
    }
    if (replaceAll)
      return true;
    Decision d = hooks.conflict ? hooks.conflict(p) : Decision::Skip;
    if (d == Decision::Abort)
      throw Cancelled();
    if (d == Decision::ReplaceAll)
      replaceAll = true;
    if (d == Decision::SkipAll)
      skipAll = true;
    if (d == Decision::Skip || d == Decision::SkipAll) {
      ++result.skipped;
      return false;
    }
    return true;
  }
  void Copy(const std::string &src, const std::string &dst, int depth = 0) {
    if (depth > 128 || ++visited > 200000)
      throw std::runtime_error("Folder tree exceeds the transfer limit");
    auto control = Control(src);
    control.Check();
    if (hooks.progress)
      hooks.progress(src, 0);
    FileEntry source, target;
    if (!StatFile(src, source, control))
      throw std::runtime_error("Source no longer exists");
    if (source.link)
      throw std::runtime_error(
          "Links are not followed during archive/network transfers");
    bool exists = StatFile(dst, target, control);
    if (exists && target.link)
      throw std::runtime_error(
          "Destination is a link; choose a real folder or file");
    if (source.directory) {
      if (exists && !target.directory)
        throw std::runtime_error("Destination is not a folder");
      if (!exists)
        MakeFolder(dst, control);
      int skippedBefore = result.skipped;
      for (auto &e : ListFiles(src, control))
        Copy(e.path, ChildLocation(dst, e.name), depth + 1);
      if (!exists && !IsVirtual(dst)) {
        if (chmod(dst.c_str(), source.permissions & 0777))
          throw std::runtime_error(strerror(errno));
        if (source.modified > 0) {
          struct timespec times[2] = {{0, UTIME_OMIT}, {source.modified, 0}};
          if (utimensat(AT_FDCWD, dst.c_str(), times, AT_SYMLINK_NOFOLLOW))
            throw std::runtime_error(strerror(errno));
        }
      }
      if (result.skipped == skippedBefore && hooks.fileDone)
        hooks.fileDone(src, dst);
      return;
    }
    if (exists && target.directory)
      throw std::runtime_error("Destination is a folder");
    if (!IsVirtual(dst)) {
      TempFile temp(LocationLeaf(dst));
      Stage(src, source, temp, control);
      Hooks local = hooks;
      local.fileDone = {};
      local.fileFailed = {};
      local.progress = [&](const std::string &, uint64_t) { control.Check(); };
      local.conflict = [&](const std::string &p) {
        return Replace(p) ? Decision::Replace : Decision::Skip;
      };
      auto done = Run({Action::Copy, {temp.path}, ParentLocation(dst)}, local);
      if (done.cancelled)
        throw Cancelled();
      if (!done.errors.empty())
        throw std::runtime_error(done.errors.front());
      if (done.completed && hooks.fileDone)
        hooks.fileDone(src, dst);
      return;
    }
    if (ParseLocation(dst).Archive())
      throw std::runtime_error("Archives are read-only. Copy entries to a "
                               "regular folder to edit them.");
    if (exists && !Replace(dst))
      return;
    TempFile temp(LocationLeaf(dst));
    Stage(src, source, temp, Control(src, false));
    auto staged = ChildLocation(ParentLocation(dst), Token() + ".part"),
         backup = ChildLocation(ParentLocation(dst), Token() + ".previous");
    bool stagedExists = false, backupExists = false, committed = false;
    try {
      FileEntry check;
      if (StatFile(staged, check, control) || StatFile(backup, check, control))
        throw std::runtime_error("Temporary transfer name collision");
      stagedExists = true;
      UploadFile(temp.path, staged, control);
      control.Check();
      bool now = StatFile(dst, check, control);
      if (now != exists || (now && !Same(target, check)))
        throw std::runtime_error(
            "Destination changed during transfer; it was left untouched");
      if (exists) {
        RenameFile(dst, backup, control);
        backupExists = true;
      }
      RenameFile(staged, dst, control);
      stagedExists = false;
      committed = true;
      if (backupExists) {
        RemoveFile(backup, false, {});
        backupExists = false;
      }
      if (hooks.fileDone)
        hooks.fileDone(src, dst);
    } catch (...) {
      auto failure = std::current_exception();
      if (stagedExists)
        try {
          RemoveFile(staged, false, {});
          stagedExists = false;
        } catch (...) {
        }
      if (backupExists)
        try {
          FileEntry check;
          if (!StatFile(dst, check, {})) {
            RenameFile(backup, dst, {});
            backupExists = false;
          }
        } catch (...) {
        }
      if (backupExists || stagedExists) {
        std::string message = committed ? "New file committed; cleanup failed. "
                                        : "Transfer interrupted. ";
        if (backupExists)
          message += "Original file is preserved at " + backup + ". ";
        if (stagedExists)
          message += "Temporary transfer may remain at " + staged + ". ";
        try {
          std::rethrow_exception(failure);
        } catch (const Cancelled &) {
          message += "Operation was cancelled.";
        } catch (const std::exception &e) {
          message += e.what();
        }
        throw std::runtime_error(
            message);
      }
      std::rethrow_exception(failure);
    }
  }
  void DeleteTree(const std::string &p, int depth = 0) {
    if (depth > 128 || ++visited > 200000)
      throw std::runtime_error("Folder tree exceeds deletion limit");
    auto c = Control(p);
    c.Check();
    FileEntry e;
    if (!StatFile(p, e, c))
      throw std::runtime_error("Entry no longer exists");
    if (e.directory && !e.link)
      for (auto &child : ListFiles(p, c))
        DeleteTree(child.path, depth + 1);
    RemoveFile(p, e.directory && !e.link, c);
  }
};
} // namespace
Result RunVirtual(const Job &job, const Hooks &hooks) {
  Runner runner{hooks, {}};
  try {
    if (job.action == Action::Trash || job.action == Action::Restore)
      throw std::runtime_error("Trash and Restore are available only on local "
                               "supported volumes. Nothing was deleted.");
    if (ParseLocation(job.destination).Archive() &&
        (job.action == Action::Copy || job.action == Action::Move ||
         job.action == Action::MakeDirectory || job.action == Action::Rename))
      throw std::runtime_error(
          "Archives are read-only. Choose a regular destination folder.");
    if (job.action == Action::MakeDirectory) {
      MakeFolder(job.destination, runner.Control(job.destination));
      runner.result.completed = 1;
      return runner.result;
    }
    for (auto &source : job.sources) {
      try {
        auto control = runner.Control(source);
        int skippedBefore = runner.result.skipped;
        if (LocationLeaf(source).empty())
          throw std::runtime_error(
              "A location root cannot be transferred or deleted");
        control.Check();
        auto dst = (job.action == Action::Rename)
                       ? job.destination
                       : ChildLocation(job.destination, LocationLeaf(source));
        if (job.action == Action::Delete) {
          if (ParseLocation(source).Archive())
            throw std::runtime_error("Archives are read-only");
          runner.DeleteTree(source);
        } else if (job.action == Action::Move || job.action == Action::Rename) {
          auto from = ParseLocation(source), to = ParseLocation(dst);
          if (from.Archive() || from.Local() ||
              from.Authority() != to.Authority())
            throw std::runtime_error(
                "Use Copy for transfers between local, archive and network "
                "locations. The source is retained.");
          FileEntry target;
          if (StatFile(dst, target, control))
            throw std::runtime_error(
                "Rename/move target already exists; it was not replaced");
          if (dst == source ||
              dst.compare(0, source.size() + 1, source + "/") == 0)
            throw std::runtime_error("Cannot move a folder into itself");
          RenameFile(source, dst, control);
          if (hooks.fileDone)
            hooks.fileDone(source, dst);
        } else {
          if (dst == source ||
              dst.compare(0, source.size() + 1, source + "/") == 0)
            throw std::runtime_error(
                "Cannot copy an entry onto itself or its descendants");
          runner.Copy(source, dst);
        }
        if (runner.result.skipped == skippedBefore)
          ++runner.result.completed;
      } catch (const Cancelled &) {
        throw;
      } catch (const std::exception &e) {
        runner.result.errors.push_back(source + ": " + e.what());
        if (hooks.fileFailed)
          hooks.fileFailed(source, e.what());
      }
    }
  } catch (const Cancelled &) {
    runner.result.cancelled = true;
  } catch (const std::exception &e) {
    runner.result.errors.push_back(e.what());
  }
  return runner.result;
}
} // namespace Lister
