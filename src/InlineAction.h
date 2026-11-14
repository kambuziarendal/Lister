// SPDX-License-Identifier: MIT
#pragma once
#include <GroupView.h>
#include <Message.h>
#include <Messenger.h>
class BTextControl;
constexpr uint32 kInlineClosed = 'icls';
// Owns one frozen question. New requests cannot replace a pending decision.
class InlineAction : public BGroupView {
public:
  explicit InlineAction(BMessenger target);
  bool Pending() const { return pending; }
  bool Ask(const char *text, const char *button, const BMessage &result,
           const char *initial = nullptr);
  bool Connect(BMessenger pane);
  void Close();
  void MessageReceived(BMessage *) override;
private:
  BMessenger target;
  BMessage result;
  BTextControl *name = nullptr;
  bool pending = false;
  int64 serial = 0;
  BMessage Event(uint32 what) const;
};
