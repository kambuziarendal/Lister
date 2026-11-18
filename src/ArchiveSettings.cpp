// SPDX-License-Identifier: MIT
#include "ArchiveSettings.h"
#include "MessageStore.h"
#include <stdexcept>
namespace Listi {
BMessage LoadArchiveSettings(const std::string &path) {
  auto settings = LoadMessageStore(path);
  if (settings.what == 'none') {
    settings.what = 'arpf';
    settings.AddInt32("version", 1);
  }
  if (settings.what != 'arpf' || settings.GetInt32("version", 0) != 1)
    throw std::runtime_error(
        "Unsupported archive settings. The existing file was not changed.");
  return settings;
}
void SaveArchiveSettings(const std::string &path, const BMessage &settings) {
  if (settings.what != 'arpf' || settings.GetInt32("version", 0) != 1)
    throw std::runtime_error("Cannot save unsupported archive settings.");
  SaveMessageStore(path, settings);
}
}
