// SPDX-License-Identifier: MIT
#pragma once
#include <dlfcn.h>
#include <initializer_list>
#include <stdexcept>
#include <string>

// Optional system libraries stay optional at app startup. Function types come
// from the corresponding headers; plugin adapters must verify their native ABI.
class DynamicLibrary {
  void *handle = nullptr;

public:
  DynamicLibrary(const DynamicLibrary &) = delete;
  DynamicLibrary &operator=(const DynamicLibrary &) = delete;
  explicit DynamicLibrary(std::initializer_list<const char *> names) {
    for (auto name : names)
      if ((handle = dlopen(name, RTLD_NOW | RTLD_LOCAL)))
        break;
  }
  ~DynamicLibrary() {
    if (handle)
      dlclose(handle);
  }
  bool Available() const { return handle != nullptr; }
  template <typename T> T Function(const char *name) const {
    auto fn = handle ? dlsym(handle, name) : nullptr;
    if (!fn)
      throw std::runtime_error(
          std::string("System library function unavailable: ") + name);
    return reinterpret_cast<T>(fn);
  }
};
#define LIBCALL(lib, name) (lib.Function<decltype(&name)>(#name))
