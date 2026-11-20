// SPDX-License-Identifier: MIT
#pragma once

#include <Messenger.h>
#include <functional>

class InlineAction;
class OperationQueue;

namespace Listi {

constexpr uint32 kDropAccepted = 'dacc', kImageDropConvert = 'dcvi';

// Owns the request generation shared by a drop, its inline choices and the
// asynchronous translator probe. One instance per file window.
class FileDropFlow {
public:
  FileDropFlow(InlineAction &, OperationQueue &, BMessenger owner,
               BMessenger service, std::function<void()> hideBank);
  void Handle(const BMessage &message);

private:
  InlineAction &actionArea;
  OperationQueue &queue;
  BMessenger owner, service;
  std::function<void()> hideBank;
  int64 serial = 0;
};

} // namespace Listi
