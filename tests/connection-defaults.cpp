// SPDX-License-Identifier: MIT
#include "ConnectionView.h"
#include <Application.h>
#include <TextControl.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  BApplication app("application/x-vnd.Listi-ConnectionDefaultsTest");
  char home[] = "/tmp/listi-ssh-XXXXXX";
  if (!mkdtemp(home) || setenv("HOME", home, 1) != 0)
    return 1;
  const std::string directory = std::string(home) + "/.ssh";
  if (mkdir(directory.c_str(), 0700) != 0)
    return 1;
  auto check = [&](const std::string &expected) {
    std::unique_ptr<BView> view(CreateConnectionView({}, {}, BMessage()));
    auto key = dynamic_cast<BTextControl *>(view->FindView("ssh key"));
    auto hosts = dynamic_cast<BTextControl *>(view->FindView("known hosts"));
    return key && key->Text() == expected && hosts &&
           hosts->Text() == directory + "/known_hosts";
  };
  const auto rsa = directory + "/id_rsa", ed = directory + "/id_ed25519";
  std::ofstream(ed + ".pub") << "public key only";
  bool ok = check("");
  std::ofstream(rsa) << "test";
  ok &= check(rsa);
  std::ofstream(ed) << "test";
  ok &= check(ed);
  unlink(ed.c_str());
  unlink(rsa.c_str());
  unlink((ed + ".pub").c_str());
  rmdir(directory.c_str());
  rmdir(home);
  puts(ok ? "PASS editable SSH path defaults" : "FAIL SSH path defaults");
  return ok ? 0 : 1;
}
