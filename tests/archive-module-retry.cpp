// SPDX-License-Identifier: MIT
// Native fault injection: link with -Wl,--wrap=dlopen and the archive objects.
#include "VirtualFile.h"
#include <cstring>
#include <iostream>
static bool deny = true;
extern "C" void *__real_dlopen(const char *, int);
extern "C" void *__wrap_dlopen(const char *path, int flags) {
  if (deny && path && strstr(path, "libarchive.so")) return nullptr;
  return __real_dlopen(path, flags);
}
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  if (Lister::ArchiveAvailable()) return 1;
  bool message = false;
  try { Lister::OpenArchive(argv[1]); }
  catch (const std::exception &e) { message = std::string(e.what()).find("HaikuDepot") != std::string::npos; }
  if (!message) return 1;
  std::cout << "PASS absent optional engine gives package guidance\n";
  deny = false;
  if (!Lister::ArchiveAvailable()) return 1;
  auto url = Lister::OpenArchive(argv[1]);
  if (url.empty() || Lister::ListFiles(url).empty()) return 1;
  std::cout << "PASS engine becomes available in the same worker without restart\n";
}
