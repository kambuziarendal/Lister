// SPDX-License-Identifier: MIT
// Run with ftp-home.py, which owns the loopback server and both sample files.
#include "VirtualFile.h"
#include <iostream>

int main(int argc, char **argv) try {
  using namespace Listi;
  if (argc != 2)
    return 2;
  const std::string root = argv[1];
  Connection auth;
  auth.username = auth.password = "fixture";
  SetConnection(root, auth);

  const auto old = ChildLocation(root, "visible");
  const auto renamed = ChildLocation(root, "renamed");
  const auto folder = ChildLocation(root, "new folder");
  FileEntry entry;
  if (!StatNetwork(ParseLocation(old), entry, {}))
    return 1;
  RenameFile(old, renamed, {});
  if (!StatNetwork(ParseLocation(renamed), entry, {}))
    return 1;
  MakeFolder(folder, {});
  RemoveFile(renamed, false, {});
  RemoveFile(folder, true, {});
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
