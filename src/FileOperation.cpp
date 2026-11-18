// SPDX-License-Identifier: MIT
#include "FileOperation.h"
#include "ArchiveVolumes.h"
#include "ArchiveJobs.h"
#include "NamedEntry.h"
#include "Trash.h"
#include <Directory.h>
#include <Entry.h>
#include <Node.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fs_attr.h>
#include <limits.h>
#include <map>
#include <memory>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace Listi {
bool IsVirtual(const std::string &);
Result RunVirtual(const Job &, const Hooks &);

std::string Join(const std::string &a, const std::string &b) {
  return a + (a.empty() || a.back() == '/' ? "" : "/") + b;
}
std::string Leaf(const std::string &p) {
  auto n = p.find_last_of('/');
  return n == std::string::npos ? p : p.substr(n + 1);
}
namespace {
struct Cancelled {};
struct FD {
  int n;
  explicit FD(int n) : n(n) {}
  ~FD() {
    if (n >= 0)
      close(n);
  }
};
using Dir = std::unique_ptr<DIR, decltype(&closedir)>;
std::string Canonical(const std::string &p) {
  char out[PATH_MAX];
  if (!realpath(p.c_str(), out))
    throw std::runtime_error(p + ": " + strerror(errno));
  return out;
}
void Fail(const std::string &p) {
  throw std::runtime_error(p + ": " + strerror(errno));
}
dirent *NextEntry(DIR *directory, const std::string &path) {
  errno = 0;
  auto entry = readdir(directory);
  if (!entry && errno)
    Fail(path);
  return entry;
}
bool Exists(const std::string &p, struct stat *st = nullptr) {
  struct stat local;
  if (lstat(p.c_str(), st ? st : &local) == 0)
    return true;
  if (errno == ENOENT)
    return false;
  Fail(p);
  return false;
}
struct Runner {
  const Hooks &h;
  bool replaceAll = false, skipAll = false;
  Result result;
  const Job *guardedJob = nullptr;
  std::string guardedSource;
  void Guard() {
    if (!guardedJob)
      return;
    struct stat st;
    if (!guardedJob->expectedSources.empty()) {
      auto expected = guardedJob->expectedSources.find(guardedSource);
      if (expected == guardedJob->expectedSources.end() ||
          lstat(guardedSource.c_str(), &st) ||
          st.st_dev != expected->second.device ||
          st.st_ino != expected->second.inode)
        throw std::runtime_error(guardedSource +
                                 ": selected source changed; left untouched");
    }
    const auto &destination = guardedJob->expectedDestination;
    const auto &path = guardedJob->destinationFolder.empty()
                           ? guardedJob->destination
                           : guardedJob->destinationFolder;
    if (destination.device >= 0 &&
        (stat(path.c_str(), &st) || !S_ISDIR(st.st_mode) ||
         st.st_dev != destination.device || st.st_ino != destination.inode))
      throw std::runtime_error(
          "Selected destination changed; operation stopped.");
  }

  bool deferCompletion = false;
  std::vector<std::pair<std::string, std::string>> copied;
  void Done(const std::string &src, const std::string &dst) {
    if (deferCompletion)
      copied.emplace_back(src, dst);
    else if (h.fileDone)
      h.fileDone(src, dst);
  }
  void Check() {
    if (h.proceed && !h.proceed())
      throw Cancelled();
  }
  void Tick(const std::string &p, uint64_t n = 0) {
    Check();
    if (h.progress)
      h.progress(p, n);
  }
  void Attributes(const std::string &src, const std::string &dst) {
    BNode from(src.c_str()), to(dst.c_str());
    char name[B_ATTR_NAME_LENGTH];
    if (from.InitCheck() != B_OK || to.InitCheck() != B_OK)
      throw std::runtime_error("Could not open copy metadata");
    status_t status;
    std::vector<char> buf(65536);
    while ((status = from.GetNextAttrName(name)) == B_OK) {
      Check();
      attr_info info;
      if (from.GetAttrInfo(name, &info) != B_OK)
        throw std::runtime_error(src + ": attribute changed during copy");
      if (info.size == 0)
        throw std::runtime_error(
            src + ": zero-length attribute cannot be preserved: " + name);
      off_t pos = 0;
      while (pos < info.size) {
        ssize_t n = from.ReadAttr(name, info.type, pos, buf.data(),
                                  std::min<off_t>(buf.size(), info.size - pos));
        if (n <= 0)
          throw std::runtime_error(src + ": could not read attribute " + name);
        if (to.WriteAttr(name, info.type, pos, buf.data(), n) != n)
          throw std::runtime_error(dst + ": could not preserve attribute " +
                                   name);
        pos += n;
      }
    }
    if (status != B_ENTRY_NOT_FOUND)
      throw std::runtime_error(src + ": could not enumerate attributes");
  }
  bool Allow(const std::string &dst) {
    if (!Exists(dst))
      return true;
    if (guardedJob && guardedJob->action == Action::Duplicate)
      throw std::runtime_error(
          dst + ": already exists; duplicate does not overwrite");
    if (skipAll) {
      result.skipped++;
      return false;
    }
    if (replaceAll)
      return true;
    Decision d = h.conflict ? h.conflict(dst) : Decision::Abort;
    if (d == Decision::Abort)
      throw Cancelled();
    if (d == Decision::SkipAll)
      skipAll = true;
    if (d == Decision::Skip || d == Decision::SkipAll) {
      result.skipped++;
      return false;
    }
    Guard();
    if (d == Decision::ReplaceAll)
      replaceAll = true;
    return true;
  }
  bool Remove(const std::string &p) {
    Check();
    struct stat st;
    if (lstat(p.c_str(), &st) != 0)
      Fail(p);
    if (S_ISDIR(st.st_mode)) {
      Dir dir(opendir(p.c_str()), closedir);
      if (!dir)
        Fail(p);
      while (auto e = NextEntry(dir.get(), p)) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
          continue;
        Remove(Join(p, e->d_name));
      }
      if (rmdir(p.c_str()) != 0)
        Fail(p);
    } else if (unlink(p.c_str()) != 0)
      Fail(p);
    Tick(p);
    return true;
  }
  using Snapshot = std::map<std::string, struct stat>;
  void SnapshotTree(const std::string &path, Snapshot &entries, dev_t volume,
                    unsigned depth = 0) {
    Check();
    if (depth > 64 || entries.size() >= 200000)
      throw std::runtime_error("Folder tree exceeds the safe operation limit");
    struct stat st;
    if (lstat(path.c_str(), &st))
      Fail(path);
    if (st.st_dev != volume)
      throw std::runtime_error(path +
                               ": refusing to traverse another mounted volume");
    entries.emplace(path, st);
    if (!S_ISDIR(st.st_mode))
      return;
    Dir dir(opendir(path.c_str()), closedir);
    if (!dir)
      Fail(path);
    while (auto e = NextEntry(dir.get(), path)) {
      if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
        continue;
      SnapshotTree(Join(path, e->d_name), entries, volume, depth + 1);
    }
  }
  bool SameEntry(const struct stat &a, const struct stat &b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino &&
           a.st_mode == b.st_mode && a.st_size == b.st_size &&
           a.st_mtim.tv_sec == b.st_mtim.tv_sec &&
           a.st_mtim.tv_nsec == b.st_mtim.tv_nsec &&
           a.st_ctim.tv_sec == b.st_ctim.tv_sec &&
           a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
  }
  void RemoveCopied(const std::string &root, const Snapshot &before) {
    Snapshot after;
    SnapshotTree(root, after, before.at(root).st_dev);
    if (before.size() != after.size())
      throw std::runtime_error(
          root + ": source changed; copied data kept and source retained");
    for (auto &entry : before) {
      auto found = after.find(entry.first);
      if (found == after.end() || !SameEntry(entry.second, found->second))
        throw std::runtime_error(
            root + ": source changed; copied data kept and source retained");
    }
    // Reverse path order visits children before parents. Never enumerate new
    // entries during deletion: rmdir fails safely if a new child appears after
    // the snapshot.
    for (auto it = before.rbegin(); it != before.rend(); ++it) {
      Check();
      struct stat now;
      if (lstat(it->first.c_str(), &now))
        Fail(it->first);
      bool directory = S_ISDIR(it->second.st_mode);
      if (now.st_ino != it->second.st_ino || now.st_dev != it->second.st_dev ||
          (!directory && !SameEntry(it->second, now)))
        throw std::runtime_error(
            it->first + ": source changed during cleanup; left untouched");
      if (directory ? rmdir(it->first.c_str()) : unlink(it->first.c_str()))
        Fail(it->first);
      Tick(it->first);
    }
  }
  void Commit(const std::string &tmp, const std::string &dst, bool replace,
              const struct stat &original) {
    Guard();
    struct stat now;
    bool exists = Exists(dst, &now);
    if (exists != replace || (exists && !SameEntry(now, original)))
      throw std::runtime_error(
          dst + ": destination changed during operation; left untouched");
    BEntry e(tmp.c_str());
    std::string parent = dst.substr(0, dst.find_last_of('/'));
    if (parent.empty())
      parent = "/";
    BDirectory directory(parent.c_str());
    status_t status = e.MoveTo(&directory, Leaf(dst).c_str(), replace);
    if (status != B_OK)
      throw std::runtime_error(dst + ": " + strerror(status));
  }
  bool Copy(const std::string &src, const std::string &dst,
            unsigned depth = 0) {
    Check();
    if (depth > 64)
      throw std::runtime_error("Folder tree exceeds the safe copy depth");
    Guard();
    struct stat st;
    if (lstat(src.c_str(), &st) != 0)
      Fail(src);
    struct stat before {};
    bool existed = Exists(dst, &before);
    if (existed && guardedJob && guardedJob->action == Action::Duplicate)
      throw std::runtime_error(
          dst + ": already exists; duplicate does not overwrite");
    if (existed && st.st_dev == before.st_dev && st.st_ino == before.st_ino)
      throw std::runtime_error(src +
                               ": source and destination are the same entry");
    if (S_ISDIR(st.st_mode)) {
      if (existed && !S_ISDIR(before.st_mode))
        throw std::runtime_error(
            dst + ": destination is not a directory; not replaced");
      // Merge directories, but never traverse destination symbolic links.
      if (!existed && mkdir(dst.c_str(), 0700) != 0)
        Fail(dst);
      bool complete = true;
      Dir dir(opendir(src.c_str()), closedir);
      if (!dir)
        Fail(src);
      while (auto e = NextEntry(dir.get(), src)) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
          continue;
        if (!Copy(Join(src, e->d_name), Join(dst, e->d_name), depth + 1))
          complete = false;
      }
      if (!existed) {
        Attributes(src, dst);
        if (chmod(dst.c_str(), st.st_mode & 07777))
          Fail(dst);
        struct timespec times[2] = {st.st_atim, st.st_mtim};
        if (utimensat(AT_FDCWD, dst.c_str(), times, 0))
          Fail(dst);
      }
      Tick(src);
      if (complete)
        Done(src, dst);
      return complete;
    }
    if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode))
      throw std::runtime_error(src + ": special files are not copied");
    if (existed && S_ISDIR(before.st_mode))
      throw std::runtime_error(dst +
                               ": a directory cannot be replaced by a file");
    if (!Allow(dst))
      return false;
    Guard();
    std::string pattern = dst + ".listi-XXXXXX";
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back(0);
    FD out(mkstemp(name.data()));
    if (out.n < 0)
      Fail(dst);
    std::string tmp = name.data();
    try {
      if (S_ISLNK(st.st_mode)) {
        char link[PATH_MAX];
        ssize_t n = readlink(src.c_str(), link, sizeof(link) - 1);
        if (n < 0)
          Fail(src);
        link[n] = 0;
        if (unlink(tmp.c_str()) || symlink(link, tmp.c_str()))
          Fail(tmp);
      } else {
        FD in(open(src.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK));
        if (in.n < 0)
          Fail(src);
        struct stat opened;
        if (fstat(in.n, &opened) || !S_ISREG(opened.st_mode) ||
            !SameEntry(opened, st))
          throw std::runtime_error(src + ": source changed before copying");
        std::vector<char> buf(131072);
        for (;;) {
          Check();
          ssize_t n = read(in.n, buf.data(), buf.size());
          if (n == 0)
            break;
          if (n < 0) {
            if (errno == EINTR)
              continue;
            Fail(src);
          }
          ssize_t at = 0;
          while (at < n) {
            ssize_t w = write(out.n, buf.data() + at, n - at);
            if (w < 0 && errno == EINTR)
              continue;
            if (w <= 0)
              Fail(dst);
            at += w;
          }
          Tick(src, n);
        }
        struct stat after;
        if (fstat(in.n, &after) || !SameEntry(after, st))
          throw std::runtime_error(src + ": source changed while copying");
        Attributes(src, tmp);
        if (fchmod(out.n, st.st_mode & 07777))
          Fail(tmp);
        struct timespec times[2] = {st.st_atim, st.st_mtim};
        if (futimens(out.n, times) || fsync(out.n))
          Fail(tmp);
      }
      Check();
      struct stat current;
      if (lstat(src.c_str(), &current) || !SameEntry(current, st))
        throw std::runtime_error(src + ": source changed before commit");
      Commit(tmp, dst, existed, before);
      Done(src, dst);
    } catch (...) {
      unlink(tmp.c_str());
      throw;
    }
    return true;
  }
  void Execute(const Job &job) {
    guardedJob = &job;
    const bool named =
        job.action == Action::Duplicate || job.action == Action::Link;
    if (named) {
      if (job.sources.size() != 1 || job.expectedSources.size() != 1 ||
          job.expectedDestination.device < 0)
        throw std::runtime_error(
            "Named entry operations require one guarded local source and destination.");
      ValidateNewEntryName(job.destination, job.entryName);
    }
    if (job.action == Action::MakeDirectory) {
      Check();
      Guard();
      if (mkdir(job.destination.c_str(), 0755))
        Fail(job.destination);
      result.completed++;
      return;
    }
    for (auto &source : job.sources) {
      Check();
      copied.clear();
      deferCompletion = job.action == Action::Move;
      try {
        guardedSource = source;
        Guard();
        // Do not allow volume roots, parent pseudoentries or empty paths to be
        // removed.
        std::string leaf = Leaf(source);
        if (source.empty() || source == "/" || leaf.empty() || leaf == "." ||
            leaf == "..")
          throw std::runtime_error("Refusing unsafe source path: " + source);
        struct stat sourceStat;
        if (lstat(source.c_str(), &sourceStat))
          Fail(source);
        if (job.action == Action::Link) {
          Check();
          Guard();
          BDirectory directory(job.destination.c_str());
          node_ref node;
          if (directory.GetNodeRef(&node) != B_OK ||
              node.device != job.expectedDestination.device ||
              node.node != job.expectedDestination.inode)
            throw std::runtime_error(
                "The link destination changed; nothing was created.");
          // One native exclusive create: existing files, folders and broken
          // symlinks are never replaced. No shell or source dereferencing.
          const auto text = LinkText(job);
          status_t status = directory.CreateSymLink(job.entryName.c_str(),
                                                    text.c_str(), nullptr);
          if (status != B_OK)
            throw std::runtime_error("Could not create link: " +
                                     std::string(strerror(status)));
          const auto destination = Join(job.destination, job.entryName);
          result.completedRoots.emplace_back(source, destination);
          ++result.completed;
          if (h.fileDone)
            h.fileDone(source, destination);
          continue;
        }
        if (S_ISDIR(sourceStat.st_mode) && job.action != Action::Copy &&
            job.action != Action::Duplicate) {
          std::string parent = source.substr(0, source.find_last_of('/'));
          if (parent.empty())
            parent = "/";
          struct stat parentStat;
          if (stat(parent.c_str(), &parentStat))
            Fail(parent);
          if (sourceStat.st_dev != parentStat.st_dev)
            throw std::runtime_error(
                source +
                ": mounted volumes cannot be deleted, moved or renamed");
        }
        if (job.action == Action::Trash || job.action == Action::Restore) {
          const std::string destination = job.action == Action::Trash
                                              ? MoveToTrash(source)
                                              : RestoreFromTrash(source);
          result.completed++;
          if (h.fileDone)
            h.fileDone(source, destination);
          continue;
        }
        if (job.action == Action::Delete) {
          Snapshot deletion;
          SnapshotTree(source, deletion, sourceStat.st_dev);
          RemoveCopied(source, deletion);
          result.completed++;
          continue;
        }
        std::string destination = job.action == Action::Rename
                                      ? job.destination
                                      : Join(Canonical(job.destination),
                                             named ? job.entryName : leaf);
        if (S_ISDIR(sourceStat.st_mode)) {
          std::string from = Canonical(source),
                      parent = Canonical(
                          destination.substr(0, destination.find_last_of('/')));
          if (parent == from ||
              parent.compare(0, from.size() + 1, from + "/") == 0)
            throw std::runtime_error(
                "Cannot copy or move a directory inside itself");
        }
        if (job.action == Action::Rename) {
          if (Exists(destination))
            throw std::runtime_error(
                destination + ": already exists; rename does not overwrite");
          struct stat empty {};
          Commit(source, destination, false, empty);
          if (h.fileDone)
            h.fileDone(source, destination);
          result.completed++;
          continue;
        }
        if (job.action == Action::Move && !Exists(destination)) {
          Guard();
          BEntry e(source.c_str());
          BDirectory dir(job.destination.c_str());
          status_t s = e.MoveTo(&dir, leaf.c_str(), false);
          if (s == B_OK) {
            result.completedRoots.emplace_back(source, destination);
            ++result.completed;
            if (h.fileDone)
              h.fileDone(source, destination);
            if (h.progress)
              h.progress(source, 0);
            continue;
          }
          if (s != B_CROSS_DEVICE_LINK)
            throw std::runtime_error(source + ": " + strerror(s));
        }
        Snapshot beforeMove;
        if (job.action == Action::Move)
          SnapshotTree(source, beforeMove, sourceStat.st_dev);
        if (Copy(source, destination)) {
          if (job.action == Action::Move) {
            Guard();
            RemoveCopied(source, beforeMove);
          }
          result.completedRoots.emplace_back(source, destination);
          result.completed++;
          if (h.fileDone)
            for (auto &entry : copied)
              h.fileDone(entry.first, entry.second);
        }
      } catch (const std::runtime_error &e) {
        result.errors.push_back(e.what());
        if (h.fileFailed)
          h.fileFailed(source, e.what());
      }
    }
  }
};
} // namespace
Result Run(const Job &j, const Hooks &h) {
  if (j.action == Action::CreateArchive || j.action == Action::ExtractArchive ||
      j.action == Action::EditArchive || j.action == Action::MoveArchive)
    return RunArchiveJob(j, h);
  // Source-only actions must not depend on the location in the other pane.
  const bool usesDestination =
      j.action == Action::Copy || j.action == Action::Move ||
      j.action == Action::Rename || j.action == Action::MakeDirectory ||
      j.action == Action::Duplicate || j.action == Action::Link;
  bool virtualJob = usesDestination && IsVirtual(j.destination);
  for (auto &path : j.sources)
    virtualJob = virtualJob || IsVirtual(path);
  if (virtualJob) {
    if (j.action == Action::Duplicate || j.action == Action::Link) {
      Result result;
      result.errors.push_back(
          "Guarded file clipboard transfers require local entries.");
      return result;
    }
    try {
      for (const auto &entry : j.expectedArchiveSets)
        if (FindArchiveVolumes(entry.first).Fingerprint() != entry.second)
          throw std::runtime_error(
              "Archive changed after selection. Select its entries again.");
    } catch (const std::exception &e) {
      Result result;
      result.errors.push_back(e.what());
      return result;
    }
    return RunVirtual(j, h);
  }

  Runner r{h};
  try {
    r.Execute(j);
  } catch (const Cancelled &) {
    r.result.cancelled = true;
  } catch (const std::exception &e) {
    r.result.errors.push_back(e.what());
  }
  return r.result;
}
} // namespace Listi
