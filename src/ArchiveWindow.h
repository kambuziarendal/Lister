// SPDX-License-Identifier: MIT
#pragma once
#include "ArchiveJobs.h"
#include <Messenger.h>
constexpr uint32 kArchiveAccepted = 'arac';
BMessenger OpenArchiveWindow(const Listi::Job &, const std::string &,
                             BMessenger);
