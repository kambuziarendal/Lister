// SPDX-License-Identifier: MIT
#include "ArchiveEditor.h"
#include "ArchiveJobs.h"
#include "ArchivePasswords.h"
#include "ArchiveVolumes.h"
#include "VirtualFile.h"
#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
using namespace Listi;
namespace {
int checks = 0, failures = 0, serial = 0;
std::string root;
void check(bool ok, const std::string &label) {
  ++checks;
  failures += !ok;
  std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
}
void writeFile(const std::string &p, const std::string &s) {
  std::ofstream f(p, std::ios::binary);
  f << s;
  f.close();
}
std::string bytes(const std::string &p) {
  std::string out;
  ReadFile(p, [&](const char *b, size_t n) { out.append(b, n); });
  return out;
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
template <class F> bool rejects(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
std::string archive(const std::string &format, bool encrypted = false) {
  auto job = PrepareArchiveJob(false, {root + "/in/folder", root + "/in/keep"});
  job.archiveFormat = format;
  if (encrypted) {
    job.archiveEncryptNames = format == "7z";
    job.archivePasswordToken = HoldArchiveCreationPassword(
        std::make_shared<ArchiveSecret>("Listi-edit-test"));
  }
  SetArchiveDestination(job, root, "archive" + std::to_string(++serial));
  auto r = Run(job, {});
  if (r.completed != 1 || !r.errors.empty())
    throw std::runtime_error("Cannot create editor fixture");
  return OpenArchive(job.destination);
}
std::string member(const std::string &a) {
  return ChildLocation(ChildLocation(a, "folder"), "edit.txt");
}
std::string physical(const std::string &a) { return ParseLocation(a).archive; }
std::string fingerprint(const std::string &a) {
  return FindArchiveVolumes(physical(a)).Fingerprint();
}
} // namespace
int main() {
  BApplication app("application/x-vnd.Listi-archive-editor-test");
  char pattern[] = "/tmp/Listi-editor-XXXXXX";
  auto made = mkdtemp(pattern);
  if (!made)
    return 2;
  root = made;
  mkdir((root + "/in").c_str(), 0700);
  mkdir((root + "/in/folder").c_str(), 0700);
  writeFile(root + "/in/folder/edit.txt", "original");
  chmod((root + "/in/folder/edit.txt").c_str(), 0640);
  writeFile(root + "/in/keep", "keep");
  const auto sessions = root + "/sessions";
  try {
    for (const auto &format : WritableArchiveFormats()) {
      auto a = archive(format.id), m = member(a);
      auto before = fingerprint(a);
      auto session = ArchiveEditSession::Open(m, sessions);
      struct stat dir, file;
      stat(session->Directory().c_str(), &dir);
      stat(session->WorkingFile().c_str(), &file);
      check(!(dir.st_mode & 0077) && !(file.st_mode & 0077) &&
                bytes(session->WorkingFile()) == "original",
            format.id + " creates private exact working copy");
      check(!session->Save() && fingerprint(a) == before,
            format.id + " unchanged working copy never rewrites archive");
      writeFile(session->WorkingFile(), "Edited snapshot " + format.id);
      check(session->Observe() == ArchiveEditSession::Waiting &&
                session->Observe() == ArchiveEditSession::Waiting &&
                session->Observe() == ArchiveEditSession::Changed,
            format.id + " auto-save waits for stable observations");
      bool saved = session->Save();
      FileEntry info;
      StatFile(m, info);
      check(saved && bytes(m) == "Edited snapshot " + format.id &&
                bytes(ChildLocation(a, "keep")) == "keep" &&
                (info.permissions & 0777) == 0640,
            format.id + " verified save preserves unrelated member and "
                        "original permissions");
      check(PrepareArchiveEditTarget(physical(a)).archiveFormat == format.id,
            format.id + " preserves archive container and compression");
      auto path = session->Directory();
      session.reset();
      session = ArchiveEditSession::Resume(path);
      before = fingerprint(a);
      check(!session->Save() && fingerprint(a) == before,
            format.id + " recovery recognizes already saved baseline");
      auto replacement = session->WorkingFile() + ".replacement";
      writeFile(replacement, "atomic editor save");
      rename(replacement.c_str(), session->WorkingFile().c_str());
      check(session->Save() && bytes(m) == "atomic editor save",
            format.id + " supports atomic editor file replacement");
      check(ArchiveEditSession::Open(m, sessions)->Directory() == path,
            format.id + " reopening member resumes retained session");
    }
    for (const auto &format : WritableArchiveFormats())
      if (format.encryption) {
        SetArchivePasswordPrompt(
            [](const std::string &, bool, const TransferControl &) {
              return std::make_shared<ArchiveSecret>("Listi-edit-test");
            });
        auto a = archive(format.id, true), m = member(a);
        auto session = ArchiveEditSession::Open(m, sessions);
        writeFile(session->WorkingFile(), "encrypted edit");
        ForgetArchivePasswords();
        check(session->Save(),
              format.id +
                  " encrypted edit saves with memory-only password prompt");
        ForgetArchivePasswords();
        int prompts = 0;
        SetArchivePasswordPrompt(
            [&](const std::string &, bool, const TransferControl &) {
              ++prompts;
              return std::make_shared<ArchiveSecret>("Listi-edit-test");
            });
        check(bytes(m) == "encrypted edit" && prompts == 1,
              format.id + " updated archive remains encrypted");
        check(bytes(Join(session->Directory(), "session"))
                      .find("Listi-edit-test") == std::string::npos,
              format.id + " recovery record contains no password");
        ForgetArchivePasswords();
        SetArchivePasswordPrompt({});
      }
    auto a = archive("zip"), m = member(a);
    auto session = ArchiveEditSession::Open(m, sessions);
    writeFile(session->WorkingFile(), "local changes");
    auto before = bytes(physical(a));
    Hooks cancel;
    cancel.proceed = [] { return false; };
    bool cancelled = false;
    try {
      session->Save(cancel);
    } catch (const Cancelled &) {
      cancelled = true;
    }
    check(cancelled && bytes(physical(a)) == before &&
              bytes(session->WorkingFile()) == "local changes",
          "cancel retains original archive and edited working file");
    auto newFile = root + "/outside";
    writeFile(newFile, "external");
    Run(PrepareArchiveCopy(Job{Action::Copy, {newFile}, a}), {});
    before = bytes(physical(a));
    check(rejects([&] { session->Save(); }) && bytes(physical(a)) == before &&
              bytes(session->WorkingFile()) == "local changes",
          "external archive change blocks save without losing local changes");
    session = ArchiveEditSession::Resume(session->Directory());
    check(rejects([&] { session->Save(); }),
          "recovery never silently rebases a stale archive snapshot");
    auto oldWorking = session->WorkingFile();
    session = session->StartFresh();
    check(session->WorkingFile() != oldWorking &&
              bytes(oldWorking) == "local changes" &&
              bytes(session->WorkingFile()) == "original",
          "explicit fresh edit retains old working copy and loads current "
          "archive member");
    check(ArchiveEditSession::Open(m, sessions)->Directory() ==
              session->Directory(),
          "reopen selects fresh session, not retired conflicting edit");
    a = archive("tar");
    m = member(a);
    session = ArchiveEditSession::Open(m, sessions);
    writeFile(session->WorkingFile(), "first saved version");
    bool changed = false;
    Hooks newer;
    newer.progress = [&](const std::string &, uint64_t) {
      if (!changed) {
        changed = true;
        writeFile(session->WorkingFile(), "later editor version");
      }
    };
    check(session->Save(newer) && changed &&
              bytes(m) == "first saved version" &&
              bytes(session->WorkingFile()) == "later editor version",
          "editor changes during packing remain local without changing "
          "in-flight snapshot");
    check(session->Save() && bytes(m) == "later editor version",
          "later editor save is applied by the following transaction");
    // Replace a working file with a link: neither target nor archive is
    // touched.
    a = archive("zip");
    m = member(a);
    session = ArchiveEditSession::Open(m, sessions);
    unlink(session->WorkingFile().c_str());
    symlink(newFile.c_str(), session->WorkingFile().c_str());
    before = bytes(physical(a));
    check(rejects([&] { session->Save(); }) && bytes(physical(a)) == before &&
              bytes(newFile) == "external",
          "symlink working file rejected");
    unlink(session->WorkingFile().c_str());
    writeFile(session->WorkingFile(), "restored");
    auto old = session->Directory() + "-old";
    rename(session->Directory().c_str(), old.c_str());
    mkdir(session->Directory().c_str(), 0700);
    mkdir((session->Directory() + "/work").c_str(), 0700);
    writeFile(session->WorkingFile(), "replacement");
    check(rejects([&] { session->Save(); }) && bytes(physical(a)) == before,
          "replaced private working directory is rejected");
    a = archive("zip");
    m = member(a);
    session = ArchiveEditSession::Open(m, sessions);
    writeFile(session->WorkingFile(), "retained after process restart");
    auto directory = session->Directory();
    session.reset();
    session = ArchiveEditSession::Resume(directory);
    check(session->Save() && bytes(m) == "retained after process restart",
          "unsaved disk copy survives session recreation and saves on explicit "
          "resume");
    // Two independently prepared operations on one archive must not both
    // publish.
    auto one = root + "/one", two = root + "/two";
    writeFile(one, "one");
    writeFile(two, "two");
    auto j1 = PrepareArchiveCopy(Job{Action::Copy, {one}, a}),
         j2 = PrepareArchiveCopy(Job{Action::Copy, {two}, a});
    Result r1, r2;
    std::thread t1([&] { r1 = Run(j1, {}); }), t2([&] { r2 = Run(j2, {}); });
    t1.join();
    t2.join();
    check(r1.completed + r2.completed == 1 &&
              r1.errors.size() + r2.errors.size() == 1,
          "competing editor/queue archive transactions serialize and reject "
          "stale second job");
  } catch (const std::exception &e) {
    check(false, std::string("unexpected exception: ") + e.what());
  }
  clean(root);
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
