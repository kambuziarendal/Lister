// SPDX-License-Identifier: MIT
#include "ArchiveEditor.h"
#include "ArchiveJobs.h"
#include "VirtualFile.h"
#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using namespace Listi;
namespace {
enum Fault { None, Publication, Journal } fault = None;
std::string archivePath, journalPath;
bool injected = false, published = false;
std::string bytes(const std::string &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
void writeFile(const std::string &p, const std::string &b) {
  std::ofstream f(p, std::ios::binary);
  f << b;
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
bool corrupt(const std::string &root) {
  DIR *dir = opendir(root.c_str());
  if (!dir)
    return false;
  bool changed = false;
  while (auto e = readdir(dir)) {
    if (strncmp(e->d_name, ".Listi-archive-stage-", 21))
      continue;
    auto path = Join(Join(root, e->d_name), "updated");
    auto data = bytes(path);
    auto at = data.find("editor corruption probe");
    if (at != std::string::npos) {
      int fd = open(path.c_str(), O_WRONLY);
      if (fd >= 0) {
        changed = pwrite(fd, "X", 1, at) == 1;
        fsync(fd);
        close(fd);
      }
      break;
    }
  }
  closedir(dir);
  return changed;
}
} // namespace
extern "C" int __real_rename(const char *, const char *);
extern "C" int __wrap_rename(const char *from, const char *to) {
  if (!injected && ((fault == Publication && to == archivePath) ||
                    (fault == Journal && published && to == journalPath))) {
    injected = true;
    errno = EIO;
    return -1;
  }
  int status = __real_rename(from, to);
  if (!status && to == archivePath)
    published = true;
  return status;
}
int main() {
  BApplication app("application/x-vnd.Listi-editor-fault-test");
  char pattern[] = "/tmp/Listi-edit-fault-XXXXXX";
  auto made = mkdtemp(pattern);
  if (!made)
    return 2;
  std::string root = made;
  writeFile(root + "/item", "original");
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const std::string &label) {
    ++checks;
    failures += !ok;
    std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
  };
  for (int scenario = 0; scenario < 3; ++scenario) {
    fault = None;
    injected = published = false;
    auto job = PrepareArchiveJob(false, {root + "/item"});
    job.archiveFormat = "tar";
    SetArchiveDestination(job, root, "archive" + std::to_string(scenario));
    if (Run(job, {}).completed != 1)
      return 2;
    archivePath = job.destination;
    auto url = OpenArchive(archivePath);
    auto session = ArchiveEditSession::Open(ChildLocation(url, "item"),
                                            root + "/sessions");
    journalPath = Join(session->Directory(), "session");
    auto original = bytes(archivePath);
    writeFile(session->WorkingFile(), "editor corruption probe");
    Hooks hooks;
    if (scenario == 0)
      hooks.proceed = [&] {
        if (!injected)
          injected = corrupt(root);
        return true;
      };
    else
      fault = scenario == 1 ? Publication : Journal;
    std::string error;
    try {
      session->Save(hooks);
    } catch (const std::exception &e) {
      error = e.what();
    }
    fault = None;
    check(injected && !error.empty(),
          "fault " + std::to_string(scenario) + " is reported");
    check(bytes(session->WorkingFile()) == "editor corruption probe",
          "fault " + std::to_string(scenario) +
              " retains edited working bytes");
    if (scenario < 2)
      check(bytes(archivePath) == original,
            "fault " + std::to_string(scenario) +
                " preserves original archive before commit");
    else {
      check(published && bytes(archivePath) != original &&
                error.find("Archive was saved") != std::string::npos,
            "journal failure after publication reports saved archive and "
            "retained working copy");
      bool refused = false;
      try {
        ArchiveEditSession::Resume(session->Directory())->Save();
      } catch (const std::exception &) {
        refused = true;
      }
      check(refused,
            "ambiguous recovery does not silently rebase source snapshot");
    }
  }
  clean(root);
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
