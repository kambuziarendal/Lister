// SPDX-License-Identifier: MIT
#pragma once

#include <Messenger.h>

class InlineAction;
class OperationQueue;

namespace Listi {

constexpr uint32 kDropAccepted = 'dacc', kImageDropConvert = 'dcvi';

// Owns the request generation shared by a drop, its inline choices and the
// asynchronous translator probe. One instance per file window.
class FileDropFlow {
public:
  FileDropFlow(InlineAction &, OperationQueue &, BMessenger owner,
               BMessenger service);
  bool Handle(const BMessage &message);

private:
  InlineAction &actionArea;
  OperationQueue &queue;
  BMessenger owner, service;
  int64 serial = 0;
};

} // namespace Listi
