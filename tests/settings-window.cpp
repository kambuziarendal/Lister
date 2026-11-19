// SPDX-License-Identifier: MIT
#include "Common.h"
#include "WorkspaceEditor.h"
#include <Button.h>
#include <Application.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <TabView.h>
#include <Window.h>
#include <iostream>

class Receiver : public BLooper {
public:
  sem_id saved = create_sem(0, "settings saved");
  sem_id layout = create_sem(0, "layout shortcut");
  BMessage result;
  ~Receiver() {
    delete_sem(layout);
    delete_sem(saved);
  }
  void MessageReceived(BMessage *message) override {
    if (message->what == kApplyPreferences) {
      result = *message;
      release_sem(saved);
    } else if (message->what == kOpenWorkspaceEditor && message->GetBool("modules", false)) {
      release_sem(layout);
    } else
      BLooper::MessageReceived(message);
  }
};

int main() {
  BApplication app("application/x-vnd.Listi-SettingsWindowTest");
  auto receiver = new Receiver;
  receiver->Run();
  Preferences prefs;
  int failures = 0;
  auto check = [&](bool ok, const char *label) {
    std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
    failures += !ok;
  };
  for (bool save : {false, true}) {
    OpenPreferences(BMessenger(receiver), prefs);
    auto window = app.WindowAt(0);
    if (!window || !window->Lock())
      return 2;
    BMessenger dialog(window);
    auto tabs = dynamic_cast<BTabView *>(window->FindView("settings tabs"));
    auto field = dynamic_cast<BMenuField *>(window->FindView("einn"));
    auto choice = field ? field->Menu()->ItemAt(1) : nullptr;
    check(tabs && tabs->CountTabs() >= 2, "settings split into named pages");
    check(field && field->Menu()->CountItems() == ViewModeCount &&
              field->Menu()->ItemAt(0)->IsMarked(),
          "Einn starts in Details and all file views remain available");
    auto customize = dynamic_cast<BButton *>(window->FindView("workspace layout"));
    check(customize && customize->Invoke() == B_OK &&
              acquire_sem_etc(receiver->layout, 1, B_RELATIVE_TIMEOUT, 1000000) == B_OK,
          "Workspaces shortcut requests layout modules without saving settings");
    if (choice) {
      choice->SetMarked(true);
    }
    window->Unlock();
    OpenPreferences(BMessenger(receiver), prefs);
    check(app.CountWindows() == 1, "reopening activates the existing dialog");
    dialog.SendMessage(save ? uint32(kApplyPreferences)
                            : uint32(B_QUIT_REQUESTED));
    check((acquire_sem_etc(receiver->saved, 1, B_RELATIVE_TIMEOUT,
                           save ? 3000000 : 250000) == B_OK) == save,
          save ? "Save submits settings" : "Cancel does not submit settings");
    for (int i = 0; i < 100 && dialog.IsValid(); ++i)
      snooze(20000);
    check(!dialog.IsValid(), "dialog closed");
  }
  receiver->Lock();
  prefs.ApplyDialog(receiver->result);
  receiver->Quit();
  check(prefs.DefaultMode(Workspace::einn) == 1 &&
            prefs.DefaultMode(Workspace::askr) == 0,
        "only chosen default changed");
  return failures ? 1 : 0;
}
