// SPDX-License-Identifier: MIT
#pragma once
#include <Looper.h>
#include <Messenger.h>
#include <atomic>
#include <memory>
namespace Lister {
constexpr uint32 kSearchStart = 'ssrt', kSearchStop = 'sstp',
    kSearchRows = 'shit', kSearchDone = 'sfin', kSearchBegin = 'sbeg';
// The caller invalidates generation before Stop or Quit, interrupting scans.
BLooper *NewSearchWorker(BMessenger target, std::shared_ptr<std::atomic<int64>> generation);
}
