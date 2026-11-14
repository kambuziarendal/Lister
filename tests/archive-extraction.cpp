// SPDX-License-Identifier: MIT
#include "FileOperation.h"
#include "VirtualFile.h"
#include <iostream>
#include <unistd.h>
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  char pattern[] = "/tmp/Listi-archive-extract-XXXXXX";
  char *destination = mkdtemp(pattern);
  if (!destination) return 2;
  auto source = Lister::OpenArchive(argv[1]) + "folder/data.txt";
  Lister::Hooks hooks;
  hooks.proceed = [] { return true; };
  hooks.conflict = [](const std::string &) { return Lister::Decision::Abort; };
  Lister::Job job{Lister::Action::Copy, {source}, destination};
  auto first = Lister::Run(job, hooks);
  if (first.completed != 1 || !first.errors.empty()) return 1;
  std::cout << "PASS actual archive queue operation extracts one file\n";
  auto second = Lister::Run(job, hooks);
  if (!second.cancelled || !second.errors.empty() || second.completed) return 1;
  std::cout << "PASS local archive conflict abort reports cancellation, not error\n";
  std::string output = std::string(destination) + "/data.txt";
  unlink(output.c_str()); rmdir(destination);
}
