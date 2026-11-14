// SPDX-License-Identifier: MIT
#include "OpenWith.h"
#include <AppFileInfo.h>
#include <MimeType.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Roster.h>
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
namespace Lister {
namespace {
void Require(status_t status, const char *context) {
  if (status != B_OK)
    throw std::runtime_error(std::string(context) + ": " + strerror(status));
}
bool Same(const struct stat &a, const struct stat &b) {
  return a.st_dev == b.st_dev && a.st_ino == b.st_ino &&
    a.st_mode == b.st_mode && a.st_size == b.st_size &&
    a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec &&
    a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}
void LocalFile(const std::string &path, struct stat &info) {
  if (path.empty() || path[0] != '/' || lstat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode))
    throw std::runtime_error("Choose one local regular file. Copy remote entries locally; open link targets directly.");
}
}
OpenApplication InspectApplication(const std::string &path) {
  OpenApplication app;
  // Resolve an application symlink, but record the actual executable identity.
  BEntry entry(path.c_str(), true);
  BPath resolved;
  Require(entry.GetPath(&resolved), "Find application");
  app.path = resolved.Path();
  LocalFile(app.path, app.identity);
  if (!(app.identity.st_mode & 0111))
    throw std::runtime_error("The selected file is not an executable application.");
  BFile executable(app.path.c_str(), B_READ_ONLY);
  Require(executable.InitCheck(), "Read application");
  char signature[B_MIME_TYPE_LENGTH] = {};
  BAppFileInfo info(&executable);
  Require(info.GetSignature(signature), "Read native application signature");
  if (!BMimeType::IsValid(signature) || !strcmp(signature, "application/x-vnd.Kambuzi-Lister"))
    throw std::runtime_error("Choose another native application, not Listi itself.");
  uint32 flags = 0;
  Require(info.GetAppFlags(&flags), "Read application flags");
  if (flags & B_ARGV_ONLY)
    throw std::runtime_error("This application does not accept native file references.");
  app.signature = signature;
  app.name = resolved.Leaf();
  struct stat actual{};
  Require(executable.GetStat(&actual), "Check application");
  if (!Same(app.identity, actual)) throw std::runtime_error("Application changed while reading it. Choose it again.");
  return app;
}
OpenWithSession::OpenWithSession(const std::string &p) : path(p) {
  LocalFile(path, identity);
  Require(file.SetTo(path.c_str(), B_READ_ONLY), "Read selected file");
  Require(get_ref_for_path(path.c_str(), &ref), "Find selected file");
  Validate();
}
void OpenWithSession::Validate() {
  struct stat current{}, pinned{};
  LocalFile(path, current);
  Require(file.GetStat(&pinned), "Check selected file");
  if (!Same(identity, current) || !Same(identity, pinned))
    throw std::runtime_error("The selected file changed or moved. Close this dialog and select it again.");
  entry_ref currentRef;
  Require(get_ref_for_path(path.c_str(), &currentRef), "Check selected location");
  if (currentRef != ref)
    throw std::runtime_error("The selected location changed. Select the file again.");
}
OpenTypeInfo OpenWithSession::Inspect() {
  Validate();
  OpenTypeInfo result;
  char type[B_MIME_TYPE_LENGTH] = {}, preferred[B_MIME_TYPE_LENGTH] = {};
  BNodeInfo info(&file);
  if (info.GetType(type) == B_OK && BMimeType::IsValid(type)) result.mime = type;
  else {
    BMimeType guess;
    if (BMimeType::GuessMimeType(&ref, &guess) == B_OK && guess.IsValid()) result.mime = guess.Type();
    else result.mime = "application/octet-stream";
    result.inferred = true;
  }
  BMimeType mime(result.mime.c_str());
  char description[B_MIME_TYPE_LENGTH] = {};
  if (mime.GetShortDescription(description) == B_OK) result.description = description;
  if (info.GetPreferredApp(preferred) == B_OK) result.preferred = preferred;
  else if (mime.GetPreferredApp(preferred) == B_OK) result.preferred = preferred;
  BMessage supporting;
  mime.GetSupportingApps(&supporting);
  std::set<std::string> paths;
  auto add = [&](const char *signature) {
    entry_ref application;
    if (!signature || !*signature || be_roster->FindApp(signature, &application) != B_OK) return;
    BPath location(&application);
    if (location.InitCheck() != B_OK) return;
    try {
      auto candidate = InspectApplication(location.Path());
      if (candidate.signature == signature && paths.insert(candidate.path).second)
        result.applications.push_back(std::move(candidate));
    } catch (const std::exception &) { /* Stale database entries are not launchable choices. */ }
  };
  add(result.preferred.c_str());
  const char *signature = nullptr;
  for (int32 i = 0; i < 512 && supporting.FindString("applications", i, &signature) == B_OK; ++i) add(signature);
  std::stable_sort(result.applications.begin(), result.applications.end(), [&](const auto &a, const auto &b) {
    bool ap = a.signature == result.preferred, bp = b.signature == result.preferred;
    return ap != bp ? ap : a.name < b.name;
  });
  Validate();
  return result;
}
void OpenWithSession::Launch(const OpenApplication &application) {
  auto current = InspectApplication(application.path);
  if (!Same(application.identity, current.identity) || application.signature != current.signature)
    throw std::runtime_error("The application changed. Close this dialog and choose it again.");
  entry_ref appRef;
  Require(get_ref_for_path(current.path.c_str(), &appRef), "Find application");
  BMessage refs(B_REFS_RECEIVED);
  Require(refs.AddRef("refs", &ref), "Prepare file reference");
  Validate();
  status_t status = be_roster->Launch(&appRef, &refs);
  if (status != B_OK && status != B_ALREADY_RUNNING) Require(status, "Open file with application");
}
}
