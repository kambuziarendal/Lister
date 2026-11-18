// SPDX-License-Identifier: MIT
#pragma once
#include "ArchiveJobs.h"
#include <Messenger.h>
#include <functional>
constexpr uint32 kArchiveAccepted = 'arac';
struct ArchiveWindowOptions {
  std::string suggestedName;
  bool appendExtension = false;
  std::function<void()> validate;
};
BMessenger OpenArchiveWindow(const Listi::Job &, const std::string &,
                             BMessenger, ArchiveWindowOptions = {});
