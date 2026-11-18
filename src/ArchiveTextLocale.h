// SPDX-License-Identifier: MIT
#pragma once

#include <locale.h>

namespace Listi {
// Haiku paths are UTF-8, even when the worker inherited the POSIX C locale.
// Libarchive converts several legacy formats through LC_CTYPE. Limit this
// normalization to the calling worker and restore its locale on return.
class ArchiveTextLocale {
public:
  ArchiveTextLocale() {
    locale = newlocale(LC_CTYPE_MASK, "en_US.UTF-8", nullptr);
    if (locale)
      previous = uselocale(locale);
  }
  ~ArchiveTextLocale() {
    if (previous)
      uselocale(previous);
    if (locale)
      freelocale(locale);
  }
  ArchiveTextLocale(const ArchiveTextLocale &) = delete;
  ArchiveTextLocale &operator=(const ArchiveTextLocale &) = delete;

private:
  locale_t locale = nullptr;
  locale_t previous = nullptr;
};
} // namespace Listi
