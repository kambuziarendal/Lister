// SPDX-License-Identifier: MIT
#pragma once
#include <InterfaceDefs.h>
#include <MessageFilter.h>
#include <Messenger.h>

namespace Listi {
// BTextControl invokes its action on focus loss too. For forms that must never
// accept a partial answer on blur, use a null control action and this filter.
class ExplicitSubmit : public BMessageFilter {
  BMessenger target;
  BMessage action;
public:
  ExplicitSubmit(BMessenger receiver, const BMessage &message)
      : BMessageFilter(B_KEY_DOWN), target(receiver), action(message) {}
  filter_result Filter(BMessage *message, BHandler **) override {
    if (message->GetString("bytes", "")[0] != B_ENTER)
      return B_DISPATCH_MESSAGE;
    target.SendMessage(&action);
    return B_SKIP_MESSAGE;
  }
};
} // namespace Listi
