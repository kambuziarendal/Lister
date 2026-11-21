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
  std::string expectedHosts = std::string(home) + "/config/settings/ssh/known_hosts";
  auto check = [&](const std::string &expected) {
    std::unique_ptr<BView> view(CreateConnectionView({}, {}, BMessage()));
    auto key = dynamic_cast<BTextControl *>(view->FindView("ssh key"));
    auto hosts = dynamic_cast<BTextControl *>(view->FindView("known hosts"));
    return key && key->Text() == expected && hosts &&
           hosts->Text() == expectedHosts;
  };
  const auto rsa = directory + "/id_rsa", ed = directory + "/id_ed25519";
  std::ofstream(ed + ".pub") << "public key only";
  bool ok = check("");
  std::ofstream(rsa) << "test";
  ok &= check(rsa);
  std::ofstream(ed) << "test";
  ok &= check(ed);
  const auto native = std::string(home) + "/config/settings/ssh";
  mkdir((std::string(home) + "/config").c_str(), 0700);
  mkdir((std::string(home) + "/config/settings").c_str(), 0700);
  mkdir(native.c_str(), 0700);
  std::ofstream(directory + "/known_hosts") << "test";
  expectedHosts = directory + "/known_hosts";
  ok &= check(ed);
  std::ofstream(native + "/id_ed25519") << "test";
  ok &= check(native + "/id_ed25519");
  std::ofstream(native + "/known_hosts") << "test";
  expectedHosts = native + "/known_hosts";
  ok &= check(native + "/id_ed25519");
  unlink((directory + "/known_hosts").c_str());
  unlink((native + "/known_hosts").c_str());
  unlink((native + "/id_ed25519").c_str());
  rmdir(native.c_str());
  rmdir((std::string(home) + "/config/settings").c_str());
  rmdir((std::string(home) + "/config").c_str());
  unlink(ed.c_str());
  unlink(rsa.c_str());
  unlink((ed + ".pub").c_str());
  rmdir(directory.c_str());
  rmdir(home);
  puts(ok ? "PASS editable SSH path defaults" : "FAIL SSH path defaults");
  return ok ? 0 : 1;
}
