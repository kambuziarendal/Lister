// SPDX-License-Identifier: MIT
#pragma once
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace Listi {
inline bool ReadableSshFile(const std::string &path) {
  struct stat info;
  return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode) &&
         access(path.c_str(), R_OK) == 0;
}

// Haiku's OpenSSH uses config/settings/ssh; .ssh may be a symlink, a
// separate directory, or absent. Resolve each file independently.
inline std::string DefaultSshPath(bool identity) {
  const char *home = std::getenv("HOME");
  std::string base = home && *home ? home : "/boot/home";
  const std::string native = base + "/config/settings/ssh/";
  for (const auto &directory : {native, base + "/.ssh/"}) {
    if (!identity) {
      if (ReadableSshFile(directory + "known_hosts"))
        return directory + "known_hosts";
    } else {
      for (auto name : {"id_ed25519", "id_ecdsa", "id_rsa"})
        if (ReadableSshFile(directory + name))
          return directory + name;
    }
  }
  return identity ? std::string() : native + "known_hosts";
}
}
