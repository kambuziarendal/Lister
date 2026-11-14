// SPDX-License-Identifier: MIT
#include "FileOperation.h"
#include <File.h>
#include <Node.h>
#include <TypeConstants.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
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
  r = Run({Action::Delete, {root}, ""}, hooks);
  check(r.errors.empty() && !exists(root),
        "recursive delete of owned test tree completes");
  std::cout << checks << " checks, " << failures << " failures" << std::endl;
  return failures ? 1 : 0;
}
} // namespace Lister
