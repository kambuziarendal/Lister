// SPDX-License-Identifier: MIT
#include "InlineAction.h"
#include "NamedEntry.h"
#include <StringView.h>
#include "ConnectionView.h"
#include "ExplicitSubmit.h"
#include <Button.h>
#include <LayoutBuilder.h>
#include <TextControl.h>
#include <TextView.h>
#include "ThemeTextView.h"
#include <string>
namespace {
constexpr uint32 kAccept = 'iacc', kAlternate = 'ialt', kCancel = 'iccl',
                 kClosed = 'ican';
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
  nameHint = nullptr;
  result.MakeEmpty();
  // Destroy connection password controls immediately, not on the next prompt.
  while (auto child = ChildAt(0)) {
    RemoveChild(child);
    delete child;
  }
}
bool InlineAction::Ask(const char *text, const char *button,
                       const BMessage &accepted, const char *initial,
                       const char *alternative) {
  if (pending)
    return false;
  ++serial;
  result = accepted;
  auto explanation = new ThemeTextView("question", B_PANEL_BACKGROUND_COLOR,
                                       B_PANEL_TEXT_COLOR);
  explanation->SetText(text);
  explanation->SetToolTip(text);
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
        new Listi::ExplicitSubmit(BMessenger(this), Event(kAccept)));
  }
  if (name && result.HasString("newEntryIn")) {
    nameHint = new BStringView(
        "name hint",
        "Choose an unused name. Existing entries are never replaced.");
    nameHint->SetExplicitMinSize(BSize(200, B_SIZE_UNSET));
    nameHint->SetTruncation(B_TRUNCATE_END);
    AddChild(nameHint);
  }
  auto cancel = new BButton("Cancel", new BMessage(Event(kCancel)));
  auto accept = new BButton(button, new BMessage(Event(kAccept)));
  auto buttons = new BGroupView(B_HORIZONTAL, 8);
  BLayoutBuilder::Group<>(buttons, B_HORIZONTAL, 8)
      .AddGlue()
      .Add(cancel)
      .Add(accept);
  if (alternative) {
    auto alternate = new BButton(alternative, new BMessage(Event(kAlternate)));
    buttons->AddChild(alternate);
    alternate->SetTarget(this);
  }
  AddChild(buttons);
  cancel->SetTarget(this);
  accept->SetTarget(this);
  pending = true;
  Show();
  if (name) {
    name->MakeFocus(true);
    name->TextView()->SelectAll();
  } else if (alternative) {
    cancel->MakeFocus(true);
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
  if (message->what != kAccept && message->what != kAlternate &&
      message->what != kCancel) {
    BGroupView::MessageReceived(message);
    return;
  }
  if (!pending || message->GetInt64("question", -1) != serial)
    return;
  BMessage accepted(result);
  if (message->what == kAccept || message->what == kAlternate) {
    if (name) {
      std::string value = name->Text();
      if (nameHint) {
        try {
          Listi::ValidateNewEntryName(result.GetString("newEntryIn", ""),
                                      value);
        } catch (const std::exception &e) {
          nameHint->SetText(e.what());
          nameHint->SetToolTip(e.what());
          name->MakeFocus(true);
          return;
        }
      }
      if (value.empty() || (!result.GetBool("placeLabel", false) &&
                            (value == "." || value == ".." ||
                             value.find('/') != std::string::npos))) {
        name->SetToolTip("Enter one file or folder name, without a slash");
        return;
      }
      accepted.AddString("name", value.c_str());
    }
    accepted.AddInt32("which", message->what == kAlternate ? 2 : 1);
  }
  Close();
  // No queued close event may cancel an unrelated later question.
  if ((message->what == kAccept || message->what == kAlternate) &&
      accepted.what != kClosed)
    target.SendMessage(&accepted);
  target.SendMessage(kInlineClosed);
}
