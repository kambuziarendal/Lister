// SPDX-License-Identifier: MIT
#include "ArchivePasswordWindow.h"
#include "ArchivePasswords.h"
#include "TextFieldFocus.h"
#include <Button.h>
#include <LayoutBuilder.h>
#include <Messenger.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
namespace {
struct Reply {
  std::mutex mutex;
  std::condition_variable changed;
  bool done = false;
  Listi::ArchiveSecretPtr secret;
};
class PasswordWindow : public BWindow {
  std::shared_ptr<Reply> reply;
  BTextControl *password;
  BStringView *status;
  void Finish() {
    password->SetText("");
    std::lock_guard<std::mutex> lock(reply->mutex);
    reply->done = true;
    reply->changed.notify_all();
  }

public:
  PasswordWindow(const std::string &path, bool retry,
                 std::shared_ptr<Reply> result)
      : BWindow(BRect(290, 230, 930, 455), "Listi: Archive password",
                B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
        reply(std::move(result)) {
    auto file =
        new BStringView("password archive", Listi::LocationLeaf(path).c_str());
    file->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
    file->SetTruncation(B_TRUNCATE_MIDDLE);
    file->SetToolTip(path.c_str());
    password = new BTextControl("archive password", "Password:", "", nullptr);
    password->TextView()->HideTyping(true);
    password->TextView()->SetMaxBytes(1024);
    status = new BStringView(
        "password status", retry ? "Password not accepted. Try again or cancel."
                                 : "Enter the password to read this archive.");
    auto unlock = new BButton("Unlock", new BMessage('apok'));
    BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
        .SetInsets(12)
        .Add(file)
        .Add(status)
        .Add(password)
        .Add(new BStringView(
            "password lifetime",
            "Kept only in memory until Listi closes or you choose Forget passwords."))
        .AddGlue()
        .AddGroup(B_HORIZONTAL, 8)
        .AddGlue()
        .Add(new BButton("Cancel", new BMessage(B_QUIT_REQUESTED)))
        .Add(unlock)
        .End();
    GetLayout()->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
    AddShortcut(B_ESCAPE, B_NO_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
    SetDefaultButton(unlock);
    Listi::FocusTextAtEnd(*password);
    Show();
  }
  ~PasswordWindow() { Finish(); }
  bool QuitRequested() override {
    Finish();
    return true;
  }
  void MessageReceived(BMessage *m) override {
    if (m->what != 'apok') {
      BWindow::MessageReceived(m);
      return;
    }
    try {
      auto secret = std::make_shared<Listi::ArchiveSecret>(password->Text());
      {
        std::lock_guard<std::mutex> lock(reply->mutex);
        if (reply->done)
          return;
        reply->secret = std::move(secret);
      }
      Finish();
      PostMessage(B_QUIT_REQUESTED);
    } catch (const std::exception &e) {
      status->SetText(e.what());
    }
  }
};
Listi::ArchiveSecretPtr Ask(const std::string &path, bool retry,
                            const Listi::TransferControl &control) {
  auto reply = std::make_shared<Reply>();
  BMessenger window(new PasswordWindow(path, retry, reply));
  try {
    for (;;) {
      control.Check();
      std::unique_lock<std::mutex> lock(reply->mutex);
      if (reply->done)
        return reply->secret;
      reply->changed.wait_for(lock, std::chrono::milliseconds(100));
    }
  } catch (...) {
    {
      std::lock_guard<std::mutex> lock(reply->mutex);
      reply->done = true;
      reply->secret.reset();
    }
    window.SendMessage(B_QUIT_REQUESTED);
    throw;
  }
}
}
void InstallArchivePasswordWindow() { Listi::SetArchivePasswordPrompt(Ask); }
