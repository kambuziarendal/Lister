// SPDX-License-Identifier: MIT
#include "FileOperation.h"
#include "Trash.h"
#include "NameFilter.h"
#include <File.h>
#include <Node.h>
#include <String.h>
#include <TypeConstants.h>
#include <cstdlib>
#include <cstring>
#include <fs_attr.h>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
namespace Lister {
int SelfTest() {
  char pattern[] = "/boot/home/Lister-test-XXXXXX";
  char *dir = mkdtemp(pattern);
  if (!dir)
    return 1;
  std::string root = dir;
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *name) {
    checks++;
    if (!ok)
      failures++;
    std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
  };
  check(MatchesNameFilter("notes.txt", ""), "empty filter shows all names");
  check(MatchesNameFilter("Holiday Notes.txt", "NOTES"),
        "name filter matches literal substring without ASCII case sensitivity");
  check(!MatchesNameFilter("notes.txt", "*.txt"),
        "filter does not interpret shell wildcards");
  check(!MatchesNameFilter("notes.txt", "picture"), "nonmatching name is hidden");
  check(MatchesNameFilter("..", "picture", true),
        "parent remains navigable with an active filter");
  check(MatchesNameFilter("\xc3\x85rsrapport.txt", "\xc3\x85r"),
        "UTF-8 literal filename is preserved and matched");
  check(MatchesNameFilter("report [final].txt", "[final]"),
        "filter treats punctuation literally");
  auto exists = [](const std::string &p) {
    struct stat s;
    return lstat(p.c_str(), &s) == 0;
  };
  auto text = [](const std::string &p) {
    std::ifstream f(p);
    return std::string(std::istreambuf_iterator<char>(f), {});
  };
  auto write = [](const std::string &p, const std::string &s) {
    std::ofstream f(p);
    f << s;
  };
  mkdir(Join(root, "src").c_str(), 0755);
  mkdir(Join(root, "dst").c_str(), 0755);
  mkdir(Join(root, "src/tree").c_str(), 0755);
  mkdir(Join(root, "src/tree/nested").c_str(), 0755);
  write(Join(root, "src/tree/nested/file"), "test data\n");
  symlink("nested/file", Join(root, "src/tree/link").c_str());
  BNode node(Join(root, "src/tree/nested/file").c_str());
  const char attribute[] = "Haiku metadata";
  node.WriteAttr("Lister:test", B_STRING_TYPE, 0, attribute, sizeof(attribute));
  Hooks hooks;
  hooks.proceed = [] { return true; };
  hooks.conflict = [](auto &) { return Decision::Replace; };
  Job copy{Action::Copy, {Join(root, "src/tree")}, Join(root, "dst")};
  Result r;
  std::thread copyThread([&] { r = Run(copy, hooks); });
  copyThread.join();
  check(r.errors.empty() && r.completed == 1, "recursive copy completes");
  check(text(Join(root, "dst/tree/nested/file")) == "test data\n",
        "file contents preserved");
  char link[80] = {};
  readlink(Join(root, "dst/tree/link").c_str(), link, 79);
  check(!strcmp(link, "nested/file"), "symbolic link is copied, not traversed");
  BNode target(Join(root, "dst/tree/nested/file").c_str());
  char attr[80] = {};
  target.ReadAttr("Lister:test", B_STRING_TYPE, 0, attr, sizeof(attr));
  check(!strcmp(attr, attribute), "BFS attributes preserved");
  write(Join(root, "dst/tree/nested/file"), "keep me");
  hooks.conflict = [](auto &) { return Decision::SkipAll; };
  r = Run(copy, hooks);
  check(r.skipped >= 1 && text(Join(root, "dst/tree/nested/file")) == "keep me",
        "skip-all preserves destination");
  hooks.conflict = [](auto &) { return Decision::ReplaceAll; };
  r = Run(copy, hooks);
  check(r.errors.empty() &&
            text(Join(root, "dst/tree/nested/file")) == "test data\n",
        "replace-all replaces only confirmed conflicts");
  hooks.conflict = [](auto &) { return Decision::Abort; };
  r = Run(copy, hooks);
  check(r.cancelled, "conflict abort is reported");
  r = Run(
      {Action::Copy, {Join(root, "src/tree")}, Join(root, "src/tree/nested")},
      hooks);
  check(!r.errors.empty(), "copy into own descendant rejected");
  r = Run({Action::Copy, {Join(root, "src/tree")}, Join(root, "src")}, hooks);
  check(!r.errors.empty(), "copy onto itself rejected");
  write(Join(root, "src/large"), std::string(512 * 1024, 'x'));
  bool cancel = false;
  hooks.proceed = [&] { return !cancel; };
  hooks.progress = [&](auto &, uint64_t n) {
    if (n)
      cancel = true;
  };
  hooks.conflict = [](auto &) { return Decision::Replace; };
  r = Run({Action::Copy, {Join(root, "src/large")}, Join(root, "dst")}, hooks);
  check(r.cancelled && !exists(Join(root, "dst/large")),
        "cancel does not expose partial file");
  hooks.proceed = [] { return true; };
  hooks.progress = {};
  r = Run({Action::Move, {Join(root, "src/large")}, Join(root, "dst")}, hooks);
  check(r.errors.empty() && !exists(Join(root, "src/large")) &&
            exists(Join(root, "dst/large")),
        "same-volume move completes");
  r = Run(
      {Action::Rename, {Join(root, "dst/large")}, Join(root, "dst/renamed")},
      hooks);
  check(r.errors.empty() && exists(Join(root, "dst/renamed")),
        "rename completes");
  r = Run({Action::MakeDirectory, {}, Join(root, "dst/new folder")}, hooks);
  check(r.errors.empty() && exists(Join(root, "dst/new folder")),
        "create folder completes");
  symlink(Join(root, "src/tree").c_str(),
          Join(root, "dst/linked-directory").c_str());
  r = Run({Action::Delete, {Join(root, "dst/linked-directory")}, ""}, hooks);
  check(r.errors.empty() && exists(Join(root, "src/tree/nested/file")),
        "delete symbolic link leaves target untouched");
  mkdir(Join(root, "merge").c_str(), 0755);
  mkdir(Join(root, "merge/tree").c_str(), 0755);
  r = Run({Action::Move, {Join(root, "src/tree")}, Join(root, "merge")}, hooks);
  check(r.errors.empty() && !exists(Join(root, "src/tree")) &&
            text(Join(root, "merge/tree/nested/file")) == "test data\n",
        "merged move removes only successfully copied snapshot");
  mkdir(Join(root, "change").c_str(), 0755);
  mkdir(Join(root, "change/tree").c_str(), 0755);
  bool mutated = false;
  hooks.progress = [&](const std::string &, uint64_t n) {
    if (n && !mutated) {
      mutated = true;
      write(Join(root, "merge/tree/new-file"), "created during copy");
    }
  };
  r = Run({Action::Move, {Join(root, "merge/tree")}, Join(root, "change")},
          hooks);
  check(!r.errors.empty() && exists(Join(root, "merge/tree/new-file")) &&
            exists(Join(root, "merge/tree/nested/file")),
        "changed source tree is retained during merged move");
  hooks.progress = {};
  r = Run({Action::Delete, {"/boot"}, ""}, hooks);
  check(!r.errors.empty() && exists("/boot/home"),
        "mounted volume deletion refused");
  // Only this run's uniquely named entries are moved to the system Trash.
  std::string owned = Join(root, "trash-test-" + Leaf(root));
  mkdir(owned.c_str(), 0755);
  write(Join(owned, "inside"), "recover me");
  std::string trashed, collision, linked;
  try {
    trashed = MoveToTrash(owned);
    BString original;
    BNode trashNode(trashed.c_str());
    check(trashNode.ReadAttrString("_trk/original_path", &original) == B_OK &&
              original == owned.c_str() && !exists(owned) &&
              text(Join(trashed, "inside")) == "recover me",
          "native Trash preserves tree and Tracker original-path metadata");
    mkdir(owned.c_str(), 0755);
    write(Join(owned, "inside"), "second tree");
    collision = MoveToTrash(owned);
    check(collision != trashed &&
              text(Join(trashed, "inside")) == "recover me" &&
              text(Join(collision, "inside")) == "second tree",
          "same-name trash entries never overwrite one another");
    write(owned, "existing replacement");
    r = Run({Action::Restore, {trashed}, ""}, hooks);
    check(!r.errors.empty() && exists(trashed) &&
              text(owned) == "existing replacement",
          "restore conflict retains both current file and trashed tree");
    unlink(owned.c_str());
    r = Run({Action::Restore, {trashed}, ""}, hooks);
    attr_info metadata;
    BNode restored(owned.c_str());
    check(
        r.errors.empty() && r.completed == 1 && !exists(trashed) &&
            text(Join(owned, "inside")) == "recover me" &&
            restored.GetAttrInfo("_trk/original_path", &metadata) != B_OK,
        "restore returns tree to original location and removes Trash metadata");
    r = Run({Action::Trash, {collision}, ""}, hooks);
    check(!r.errors.empty() && exists(collision),
          "already trashed entry is retained");
    r = Run({Action::Restore, {owned}, ""}, hooks);
    check(!r.errors.empty() && exists(owned),
          "restore outside Trash is rejected");
    std::string symlinkPath = Join(root, "link-" + Leaf(root));
    symlink(owned.c_str(), symlinkPath.c_str());
    linked = MoveToTrash(symlinkPath);
    char linkValue[PATH_MAX] = {};
    readlink(linked.c_str(), linkValue, sizeof(linkValue) - 1);
    check(std::string(linkValue) == owned && exists(Join(owned, "inside")),
          "trashing a symlink leaves its target untouched");
    r = Run({Action::Restore, {linked}, ""}, hooks);
    check(r.errors.empty() && exists(symlinkPath) && exists(owned),
          "trashed symlink restores without traversing target");
    hooks.proceed = [] { return false; };
    r = Run({Action::Trash, {owned}, ""}, hooks);
    check(r.cancelled && exists(owned),
          "cancelled trash leaves source untouched");
    hooks.proceed = [] { return true; };
    r = Run({Action::Trash, {"/boot"}, ""}, hooks);
    check(!r.errors.empty() && exists("/boot/home"),
          "trashing a volume root is refused");
    std::string missing = Join(root, "missing-parent");
    mkdir(missing.c_str(), 0755);
    write(Join(missing, "file"), "keep if parent missing");
    std::string missingTrashed = MoveToTrash(Join(missing, "file"));
    rmdir(missing.c_str());
    r = Run({Action::Restore, {missingTrashed}, ""}, hooks);
    check(!r.errors.empty() && exists(missingTrashed),
          "missing original parent leaves trashed file intact with error");
    Run({Action::Delete, {missingTrashed}, ""}, hooks);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    check(false, "native Trash round trip");
  }
  for (auto path : {trashed, collision, linked})
    if (!path.empty() && exists(path))
      Run({Action::Delete, {path}, ""}, hooks);
  r = Run({Action::Delete, {root}, ""}, hooks);
  check(r.errors.empty() && !exists(root),
        "recursive delete of owned test tree completes");
  std::cout << checks << " checks, " << failures << " failures" << std::endl;
  return failures ? 1 : 0;
}
} // namespace Lister
