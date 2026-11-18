// SPDX-License-Identifier: MIT
#include "Common.h"
#include "WorkspaceEditor.h"
#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <ListView.h>
#include <TextControl.h>
#include <TabView.h>
#include <Window.h>
#include <cstdlib>
#include <iostream>

namespace {
class Receiver : public BLooper {
public:
  sem_id saved = create_sem(0, "workspace saved");
  BMessage result;
  ~Receiver() { delete_sem(saved); }
  void MessageReceived(BMessage *message) override {
    if (message->what != kApplyWorkspaceEditor) {
      BLooper::MessageReceived(message);
      return;
    }
    result = *message;
    release_sem(saved);
  }
};

int failures = 0;
void Check(bool ok, const char *label) {
  std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
  failures += !ok;
}

template<class T>
T *Control(BWindow *window, const char *name) {
  auto *view = dynamic_cast<T *>(window->FindView(name));
  if (!view) {
    std::cerr << "Missing control: " << name << std::endl;
    std::exit(2);
  }
  return view;
}
}

int main() {
  BApplication app("application/x-vnd.Listi-WorkspaceEditorTest");
  auto *receiver = new Receiver;
  receiver->Run();
  Preferences prefs;
  for (bool save : {false, true}) {
    OpenWorkspaceEditor(BMessenger(receiver), prefs);
    auto *window = app.WindowAt(0);
    if (!window || !window->Lock())
      return 2;
    BMessenger dialog(window);
    auto *actions = Control<BListView>(window, "visible actions");
    Check(actions->CountItems() == WorkspaceLayout::ActionButtonCount,
          "all standard actions begin in order");
    actions->Select(1);
    BMessage move(*Control<BButton>(window, "move action up")->Message());
    window->Unlock();

    BMessage reply;
    dialog.SendMessage(&move, &reply);
    if (!window->Lock())
      return 2;
    auto *tabs = Control<BTabView>(window, "workspace tabs");
    tabs->Select(1);
    auto *adaptive = Control<BCheckBox>(window, "adaptive workspace");
    Check(adaptive->Value() == B_CONTROL_OFF, "adaptation defaults to off");
    adaptive->SetValue(B_CONTROL_ON);
    tabs->Select(2);
    Control<BTextControl>(window, "profile name")->SetText("Small screen");
    BMessage add(*Control<BButton>(window, "create profile")->Message());
    window->Unlock();
    dialog.SendMessage(&add, &reply);
    if (!window->Lock())
      return 2;
    BMessage close(*Control<BButton>(window, save ? "save workspace"
                                                   : "cancel workspace")->Message());
    window->Unlock();
    dialog.SendMessage(&close);
    Check((acquire_sem_etc(receiver->saved, 1, B_RELATIVE_TIMEOUT,
                           save ? 3000000 : 250000) == B_OK) == save,
          save ? "Save submits changes" : "Cancel discards profile and layout");
    for (int i = 0; i < 100 && dialog.IsValid(); ++i)
      snooze(20000);
    Check(!dialog.IsValid(), "workspace editor closed");
  }
  receiver->Lock();
  BMessage layoutMessage, profilesMessage;
  Check(receiver->result.FindMessage("layout", &layoutMessage) == B_OK &&
            receiver->result.FindMessage("profiles", &profilesMessage) == B_OK,
        "layout and profile collection are submitted together");
  receiver->Quit();
  WorkspaceLayout layout;
  WorkspaceProfiles profiles;
  layout.Load(layoutMessage);
  profiles.Load(profilesMessage);
  Check(layout.adaptive && layout.actionButtons.size() >= 2 &&
            layout.actionButtons[0] == 1 && layout.actionButtons[1] == 0,
        "saved layout preserves reordered actions and explicit adaptation");
  const auto *profile = profiles.Find("Small screen");
  Check(profile && profile->layout.adaptive &&
            profile->layout.actionButtons == layout.actionButtons,
        "named snapshot captures controls edited before creation");
  return failures ? 1 : 0;
}
