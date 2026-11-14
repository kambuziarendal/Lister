// SPDX-License-Identifier: MIT
#include "InlineAction.h"
#include "ConnectionView.h"
#include "ExplicitSubmit.h"
#include <Button.h>
#include <LayoutBuilder.h>
#include <TextControl.h>
#include <TextView.h>
#include <string>
namespace {
constexpr uint32 kAccept = 'iacc', kCancel = 'iccl', kClosed = 'ican';
}
InlineAction::InlineAction(BMessenger receiver)
    : BGroupView(B_VERTICAL, 5), target(receiver) {}
BMessage InlineAction::Event(uint32 what) const {
  BMessage message(what);
  message.AddInt64("question", serial);
  return message;
}
void InlineAction::Close() {
  if (!pending)
    return;
  pending = false;
  ++serial;
  Hide();
  name = nullptr;
  result.MakeEmpty();
  // Destroy connection password controls immediately, not on the next prompt.
  while (auto child = ChildAt(0)) {
    RemoveChild(child);
    delete child;
  }
}
bool InlineAction::Ask(const char *text, const char *button,
                       const BMessage &accepted, const char *initial) {
  if (pending)
    return false;
  ++serial;
  result = accepted;
  auto explanation = new BTextView("question");
  explanation->SetText(text);
  explanation->MakeEditable(false);
  explanation->MakeSelectable(false);
  explanation->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
  explanation->SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
  explanation->SetHighUIColor(B_PANEL_TEXT_COLOR);
  explanation->SetExplicitMinSize(BSize(300, 35));
  explanation->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 75));
  AddChild(explanation);
  if (initial) {
    name = new BTextControl("Name:", initial, nullptr);
    AddChild(name);
    name->SetTarget(this);
    name->TextView()->AddFilter(
        new Lister::ExplicitSubmit(BMessenger(this), Event(kAccept)));
  }
  auto cancel = new BButton("Cancel", new BMessage(Event(kCancel)));
  auto accept = new BButton(button, new BMessage(Event(kAccept)));
  auto buttons = new BGroupView(B_HORIZONTAL, 8);
  BLayoutBuilder::Group<>(buttons, B_HORIZONTAL, 8)
      .AddGlue().Add(cancel).Add(accept);
  AddChild(buttons);
  cancel->SetTarget(this);
  accept->SetTarget(this);
  pending = true;
  Show();
  if (name) {
    name->MakeFocus(true);
    name->TextView()->SelectAll();
  }
  return true;
}
bool InlineAction::Connect(BMessenger pane) {
  if (pending)
    return false;
  ++serial;
  AddChild(CreateConnectionView(pane, BMessenger(this), Event(kCancel)));
  pending = true;
  Show();
  return true;
}
void InlineAction::MessageReceived(BMessage *message) {
  if (message->what != kAccept && message->what != kCancel) {
    BGroupView::MessageReceived(message);
    return;
  }
  if (!pending || message->GetInt64("question", -1) != serial)
    return;
  BMessage accepted(result);
  if (message->what == kAccept) {
    if (name) {
      std::string value = name->Text();
      if (value.empty() || (!result.GetBool("placeLabel", false) &&
          (value == "." || value == ".." || value.find('/') != std::string::npos))) {
        name->SetToolTip("Enter one file or folder name, without a slash");
        return;
      }
      accepted.AddString("name", value.c_str());
    } else
      accepted.AddInt32("which", 1);
  }
  Close();
  // No queued close event may cancel an unrelated later question.
  if (message->what == kAccept && accepted.what != kClosed)
    target.SendMessage(&accepted);
  target.SendMessage(kInlineClosed);
}
