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
  bool inferred = false;
  std::vector<OpenApplication> applications;
};
OpenApplication InspectApplication(const std::string &path);
// One local regular file. Neither discovery nor opening writes associations.
class OpenWithSession {
public:
  explicit OpenWithSession(const std::string &path);
  OpenTypeInfo Inspect();
  void Validate();
  void Launch(const OpenApplication &application);

private:
  std::string path;
  BFile file;
  struct stat identity {};
  entry_ref ref;
};
}
void OpenWithFile(const std::string &path);
bool OpenWithAllowQuit();
