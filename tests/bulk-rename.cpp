// SPDX-License-Identifier: MIT
#include "BulkRename.h"
#include "JobSnapshot.h"
#include <Application.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using namespace Listi;
namespace {
void Check(bool ok, const char *label) {
  printf("%s %s\n", ok ? "PASS" : "FAIL", label);
  if (!ok)
    throw std::runtime_error(label);
}
void Write(const std::string &path, const std::string &text) {
  std::ofstream file(path);
  if (!(file << text))
    throw std::runtime_error("Cannot write fixture: " + path);
}
std::string Read(const std::string &path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), {}};
}
bool Exists(const std::string &path) {
  struct stat st;
  return lstat(path.c_str(), &st) == 0;
}
template <class F> bool Reject(F action) {
  try {
    action();
    return false;
  } catch (const std::runtime_error &) {
    return true;
  }
}
} // namespace

int main() {
  BApplication app("application/x-vnd.Kambuzi-BulkRenameTests");
  char temp[] = "/boot/home/listi-bulk-tests-XXXXXX";
  if (!mkdtemp(temp))
    return 2;
  const std::string root(temp), folder = root + "/entries";
  try {
    if (mkdir(folder.c_str(), 0700))
      throw std::runtime_error("Cannot create fixture directory");
    const auto a = folder + "/find find.txt", b = folder + "/find folder.v1",
               c = folder + "/.link";
    Write(a, "asset");
    if (mkdir(b.c_str(), 0700) || symlink("missing", c.c_str()))
      throw std::runtime_error("Cannot create folder or symbolic link fixture");
    const auto batch = FreezeBulkRename(folder, {b, a, c});
    Check(Reject([&] { FreezeBulkRename(folder, {a}); }) &&
              Reject([&] { FreezeBulkRename(folder, {a, a}); }) &&
              Reject([&] { FreezeBulkRename(folder, {a, root + "/other"}); }) &&
              Reject([&] { FreezeBulkRename("sftp://example.invalid", {a, b}); }),
          "bulk selection requires distinct sibling entries in a local folder");
    const auto unchanged = PreviewBulkRename(batch, {});
    Check(!unchanged.CanApply() && unchanged.changed == 0 &&
              unchanged.rows.size() == 3,
          "a preview without changes cannot be applied");

    BulkRenameOptions options;
    options.search = "find";
    options.replacement = "æ";
    options.prefix = "Pre ";
    options.suffix = " end";
    options.number = true;
    options.start = 4;
    options.digits = 2;
    const auto preview = PreviewBulkRename(batch, options);
    Check(preview.CanApply() && preview.rows[0].after == "Pre æ folder.v1 end 04" &&
              preview.rows[1].after == "Pre æ æ end 05.txt" &&
              preview.rows[2].after == "Pre .link end 06",
          "all rules compose in frozen order without changing file extensions");
    Check(Exists(a) && Exists(b) && Exists(c),
          "preview does not modify files, folders or symlinks");

    options.prefix = "/";
    Check(!PreviewBulkRename(batch, options).CanApply(),
          "invalid new name blocks the full batch");
    options.prefix = std::string(260, 'x');
    Check(!PreviewBulkRename(batch, options).CanApply(),
          "overlong name blocks the full batch");

    const auto x = folder + "/filex", y = folder + "/filexx";
    Write(x, "x");
    Write(y, "y");
    BulkRenameOptions collapse;
    collapse.search = "x";
    auto collision = PreviewBulkRename(FreezeBulkRename(folder, {x, y}),
                                       collapse);
    Check(!collision.CanApply() && collision.blocked == 2,
          "duplicate generated name blocks both rows");
    collapse.replacement = "xx";
    collision = PreviewBulkRename(FreezeBulkRename(folder, {x, y}), collapse);
    Check(!collision.CanApply() && collision.blocked == 1,
          "a name occupied by another selected entry is not interchangeable");

    auto job = DecodeJob(EncodeJob(preview.job, 'brrt'));
    Check(job.action == Action::BulkRename && job.bulkNames == preview.job.bulkNames,
          "accepted message carries the confirmed names");
    const auto occupied = Join(folder, preview.rows[1].after);
    if (symlink("other-missing", occupied.c_str()))
      throw std::runtime_error("Cannot add collision fixture");
    bool prompted = false;
    Hooks hooks;
    hooks.conflict = [&](const auto &) {
      prompted = true;
      return Decision::Replace;
    };
    auto result = Run(job, hooks);
    Check(!prompted && result.completed == 0 && !result.errors.empty() &&
              Exists(a) && Exists(b) && Exists(c) && Exists(occupied),
          "broken symlink added after preview blocks all changes without replacement");
    if (unlink(occupied.c_str()))
      throw std::runtime_error("Cannot remove collision fixture");

    const auto oldA = folder + "/old-a";
    if (rename(a.c_str(), oldA.c_str()))
      throw std::runtime_error("Cannot move original source aside");
    Write(a, "different entry");
    const auto stale = PreviewBulkRename(batch, {});
    result = Run(job, {});
    Check(stale.blocked == 1 && result.completed == 0 &&
              !result.errors.empty() && Read(a) == "different entry" && Exists(b),
          "one replaced source blocks the full batch, including unchanged output");
    if (unlink(a.c_str()) || rename(oldA.c_str(), a.c_str()))
      throw std::runtime_error("Cannot restore original source");

    const auto moved = folder + "-moved";
    if (rename(folder.c_str(), moved.c_str()) || mkdir(folder.c_str(), 0700))
      throw std::runtime_error("Cannot replace original folder");
    result = Run(job, {});
    Check(result.completed == 0 && !result.errors.empty(),
          "replacement of the selected parent stops the entire job");
    if (rmdir(folder.c_str()) || rename(moved.c_str(), folder.c_str()))
      throw std::runtime_error("Cannot restore fixture folder");

    // Simulate a later collision after the first committed move.
    hooks.fileDone = [&](const auto &, const auto &) { Write(occupied, "other"); };
    result = Run(job, hooks);
    const auto renamedFolder = Join(folder, preview.rows[0].after);
    Check(result.completed == 1 && result.skipped == 1 &&
              result.errors.size() == 1 && result.completedRoots.size() == 1 &&
              !prompted && !Exists(b) && Exists(renamedFolder) &&
              Read(occupied) == "other" && Read(a) == "asset" && Exists(c),
          "late collision keeps completed moves and accurately reports partial success");
    if (unlink(occupied.c_str()) || rename(renamedFolder.c_str(), b.c_str()))
      throw std::runtime_error("Cannot restore partial batch fixture");

    bool keepGoing = true;
    hooks.fileDone = [&](const auto &, const auto &) { keepGoing = false; };
    hooks.proceed = [&] { return keepGoing; };
    result = Run(job, hooks);
    Check(result.cancelled && result.completed == 1 && result.skipped == 2 &&
              result.errors.empty() && Exists(renamedFolder) && Exists(a) &&
              Exists(c),
          "cancellation between rows keeps and reports the committed change");
    if (rename(renamedFolder.c_str(), b.c_str()))
      throw std::runtime_error("Cannot restore cancelled batch fixture");

    result = Run(job, {});
    struct stat st;
    const auto renamedLink = Join(folder, preview.rows[2].after);
    Check(result.completed == 3 && result.errors.empty() && !result.cancelled &&
              Read(occupied) == "asset" && Exists(renamedFolder) &&
              !lstat(renamedLink.c_str(), &st) && S_ISLNK(st.st_mode) &&
              !Exists(a) && !Exists(b) && !Exists(c),
          "ready batch moves entries in place, preserving payload and link type");
    unlink(x.c_str());
    unlink(y.c_str());
    unlink(occupied.c_str());
    unlink(renamedLink.c_str());
    rmdir(renamedFolder.c_str());
    rmdir(folder.c_str());
    rmdir(root.c_str());
  } catch (const std::exception &error) {
    fprintf(stderr, "%s\nFixtures kept at %s\n", error.what(), temp);
    return 1;
  }
  return 0;
}
