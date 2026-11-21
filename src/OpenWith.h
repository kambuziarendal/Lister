// SPDX-License-Identifier: MIT
#pragma once
#include <Entry.h>
#include <File.h>
#include <string>
#include <vector>
#include <sys/stat.h>
namespace Listi {
struct OpenApplication {
  std::string path, name, signature;
  struct stat identity {};
};
struct OpenTypeInfo {
  std::string mime, description, preferred;
  std::string filePreferred, typePreferred;
  bool inferred = false;
  bool CanSetTypeDefault() const;
  std::vector<OpenApplication> applications;
};
bool SameMimeName(const std::string &, const std::string &);
OpenApplication InspectApplication(const std::string &path);
// One local regular file. Neither discovery nor opening writes associations.
class OpenWithSession {
public:
  explicit OpenWithSession(const std::string &path);
  OpenTypeInfo Inspect();
  void Validate();
  void Launch(const OpenApplication &application);
  void SetTypeDefault(const OpenApplication &application,
                      const std::string &displayedType);

private:
  std::string path;
  BFile file;
  struct stat identity {};
  entry_ref ref;
};
}
void OpenWithFile(const std::string &path);
bool OpenWithAllowQuit();
