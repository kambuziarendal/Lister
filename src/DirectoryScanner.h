// SPDX-License-Identifier: MIT
#pragma once
#include <Looper.h>
#include <Messenger.h>
#include <atomic>
#include <memory>
BLooper *CreateDirectoryScanner(BMessenger target,
    std::shared_ptr<std::atomic<int64>> generation);
