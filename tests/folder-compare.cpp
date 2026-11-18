// SPDX-License-Identifier: MIT
#include "ArchiveJobs.h"
#include "FolderCompare.h"
#include "Sha256.h"
#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <cerrno>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using namespace Listi;
static std::string deniedFolder;
extern "C" DIR *__real_opendir(const char *);
extern "C" DIR *__wrap_opendir(const char *path) {
  if (path == deniedFolder) {
    errno = EACCES;
    return nullptr;
  }
  return __real_opendir(path);
}

namespace {
int checks = 0, failures = 0;
void check(bool ok, const std::string &s) {
  ++checks;
  failures += !ok;
  std::cout << (ok ? "PASS " : "FAIL ") << s << std::endl;
}
void put(const std::string &p, const std::string &s) {
  std::ofstream f(p, std::ios::binary);
  f << s;
}
std::string hash(const std::string &s, size_t block = 1) {
  Sha256 h;
  for (size_t i = 0; i < s.size(); i += block)
    h.Update(s.data() + i, std::min(block, s.size() - i));
  return h.Finish();
}
bool state(const CompareResult &r, const std::string &name, CompareState s) {
  for (auto &e : r.entries)
    if (e.relative == name)
      return e.state == s;
  return false;
}
void clean(const std::string &p) {
  BDirectory d(p.c_str());
  BEntry e;
  while (d.GetNextEntry(&e, false) == B_OK) {
    struct stat s;
    e.GetStat(&s);
    if (S_ISDIR(s.st_mode)) {
      BPath path;
      e.GetPath(&path);
      clean(path.Path());
    }
    e.Remove();
  }
  rmdir(p.c_str());
}
} // namespace
int main() {
  BApplication app("application/x-vnd.Listi-folder-compare-test");
  check(hash("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA-256 empty FIPS vector");
  check(hash("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256 abc streamed bytewise");
  check(hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 7) ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        "SHA-256 two-block padding vector");
  check(hash(std::string(1000000, 'a'), 65536) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
        "SHA-256 million-byte vector");
  char pattern[] = "/tmp/Listi-compare-XXXXXX";
  auto made = mkdtemp(pattern);
  if (!made)
    return 2;
  std::string root = made, l = root + "/left", r = root + "/right";
  mkdir(l.c_str(), 0700);
  mkdir(r.c_str(), 0700);
  try {
    for (auto &p : {l, r}) {
      mkdir((p + "/nested").c_str(), 0700);
      mkdir((p + "/empty").c_str(), 0700);
      put(p + "/nested/equal.txt", "same bytes");
      put(p + "/.hidden", "hidden");
      put(p + "/zero", "");
      put(p + "/same-size", "AAAA");
      put(p + "/size", "short");
    }
    put(r + "/same-size", "BBBB");
    put(r + "/size", "longer bytes");
    put(l + "/missing", "left only");
    put(r + "/extra", "right only");
    put(l + "/type", "file");
    mkdir((r + "/type").c_str(), 0700);
    auto quick = CompareFolders({l, r, false});
    check(quick.complete &&
              state(quick, "same-size", CompareState::SizeMatch) &&
              quick.verified == 0,
          "quick mode never claims byte equality from sizes");
    check(state(quick, "missing", CompareState::OnlyLeft) &&
              state(quick, "extra", CompareState::OnlyRight),
          "recursive relative names show missing and extra entries");
    check(state(quick, "size", CompareState::SizeDifferent) &&
              state(quick, "type", CompareState::TypeDifferent),
          "size and file-versus-directory mismatches are distinct");
    check(state(quick, "empty", CompareState::DirectoryMatch) &&
              state(quick, ".hidden", CompareState::SizeMatch),
          "empty directories and dotfiles included");
    auto deep = CompareFolders({l, r, true});
    check(deep.complete &&
              state(deep, "same-size", CompareState::ContentDifferent),
          "SHA-256 finds corruption with unchanged byte count");
    check(state(deep, "nested/equal.txt", CompareState::Verified) &&
              state(deep, "zero", CompareState::Verified) &&
              state(deep, ".hidden", CompareState::Verified),
          "recursive exact copies and empty files verified");
    check(deep.different == 5 && deep.verified == 3 && deep.unverified == 0,
          "summary counts differences without including directories as "
          "verified files");
    symlink("missing-target", (l + "/link").c_str());
    symlink("missing-target", (r + "/link").c_str());
    check(state(CompareFolders({l, r, true}), "link", CompareState::LinkMatch),
          "matching dangling symlinks compared without following");
    unlink((r + "/link").c_str());
    symlink("other-target", (r + "/link").c_str());
    check(state(CompareFolders({l, r, true}), "link",
                CompareState::ContentDifferent),
          "different symlink target detected");
    mkfifo((l + "/pipe").c_str(), 0600);
    mkfifo((r + "/pipe").c_str(), 0600);
    auto special = CompareFolders({l, r, true});
    check(!special.complete && state(special, "pipe", CompareState::Unverified),
          "FIFO never blocks or claims successful comparison");
    unlink((l + "/pipe").c_str());
    unlink((r + "/pipe").c_str());
    check(!CompareFolders({l, root + "/absent", true}).complete,
          "missing root cannot look like empty matching folder");
    deniedFolder = r + "/nested";
    check(
        !CompareFolders({l, r, true}).complete,
        "injected unreadable-folder error cannot produce complete comparison");
    deniedFolder.clear();
    check(!CompareFolders({l, r, true, 3}).complete,
          "entry limit reports incomplete rather than truncated success");
    TransferControl cancel;
    cancel.proceed = [] { return false; };
    auto stopped = CompareFolders({l, r, true}, cancel);
    check(stopped.cancelled && !stopped.complete,
          "cancelled comparison is explicitly incomplete");
    bool changed = false;
    TransferControl race;
    race.progress = [&](uint64_t) {
      if (!changed) {
        changed = true;
        put(l + "/new-during-read", "new");
      }
    };
    auto raced = CompareFolders({l, r, true}, race);
    check(changed && !raced.complete && !raced.error.empty(),
          "re-list catches new file created while checksums are read");
    unlink((l + "/new-during-read").c_str());
    changed = false;
    TransferControl overwrite;
    overwrite.progress = [&](uint64_t) {
      if (!changed) {
        changed = true;
        put(l + "/.hidden", "xxxxxx");
      }
    };
    auto overwritten = CompareFolders({l, r, true}, overwrite);
    check(changed && !overwritten.complete,
          "same-size source change during reading never passes verification");
    put(l + "/.hidden", "hidden");
    for (const auto &format : WritableArchiveFormats()) {
      const auto native = root + "/native-" + format.id;
      mkdir(native.c_str(), 0700);
      std::vector<std::string> sources;
      // ar stores flat files; CAB represents directories through member paths.
      const bool flat = format.id == "ar";
      const auto member = flat ? "file" : "sub/file";
      if (!flat)
        mkdir((native + "/sub").c_str(), 0700);
      put(native + "/" + member, "archive comparison");
      sources.push_back(native + (flat ? "/file" : "/sub"));
      if (!flat && format.id != "cab") {
        mkdir((native + "/empty").c_str(), 0700);
        sources.push_back(native + "/empty");
      }

      auto job = PrepareArchiveJob(false, sources);
      job.archiveFormat = format.id;
      SetArchiveDestination(job, root, "fixture-" + format.id);
      auto result = Run(job, {});
      if (result.completed != 1 || !result.errors.empty()) {
        check(false, format.id + " fixture: " +
              (result.errors.empty() ? "not completed" : result.errors.front()));
        continue;
      }
      auto diff = CompareFolders({native, OpenArchive(job.destination), true});
      check(diff.complete && diff.different == 0 && diff.verified == 1 &&
                state(diff, member, CompareState::Verified),
            format.id + " compares local folder against archive contents by SHA-256" +
                (diff.error.empty() ? "" : ": " + diff.error));
    }
    check(CompareFolders({l, l, true}).complete,
          "same folder comparison is read-only and valid");
  } catch (const std::exception &e) {
    check(false, e.what());
  }
  clean(root);
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
