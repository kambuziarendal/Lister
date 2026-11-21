// SPDX-License-Identifier: MIT
#include "ConnectionView.h"
#include "Common.h"
#include "ExplicitSubmit.h"
#include "VirtualFile.h"
#include "WrappedLabel.h"
#include <Button.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <TextControl.h>
#include <TextView.h>
#include "SshPaths.h"
namespace {
constexpr uint32 kConnect = 'conn', kAdvanced = 'nadv', kCancelConnect = 'nccl';
class ConnectionView : public BGroupView {
  BMessenger pane, closeTarget;
  BMessage closeMessage;
  BTextControl *url, *user, *password, *domain, *key, *hosts;
  WrappedLabel *error;
  BGroupView *advanced;
  bool expanded = false;
  void Close() {
    password->SetText("");
    BMessage done(closeMessage);
    closeTarget.SendMessage(&done);
  }

public:
  ConnectionView(BMessenger target, BMessenger close, const BMessage &message)
      : BGroupView(B_VERTICAL, 6), pane(target), closeTarget(close),
        closeMessage(message) {
    // BTextControl invokes its message on focus loss as well as Enter. Never
    // submit partially entered credentials when the user tabs between fields.
    url = new BTextControl("Address:", "smb://", nullptr);
    user = new BTextControl("Username:", "", nullptr);
    password = new BTextControl("Password:", "", nullptr);
    password->TextView()->HideTyping(true);
    domain = new BTextControl("SMB domain:", "", nullptr);
    key = new BTextControl("ssh key", "SSH private key:",
                           Listi::DefaultSshPath(Listi::SshFile::Identity).c_str(), nullptr);
    hosts = new BTextControl("known hosts", "SSH known hosts:",
                             Listi::DefaultSshPath(Listi::SshFile::KnownHosts).c_str(), nullptr);
    advanced = new BGroupView(B_VERTICAL, 4);
    BLayoutBuilder::Group<>(advanced, B_VERTICAL, 4)
        .AddGroup(B_HORIZONTAL, 12)
        .Add(domain)
        .Add(key)
        .End()
        .Add(hosts);
    advanced->Hide();
    error = new WrappedLabel("network hint",
                             "SMB, SFTP, FTP or FTPS. Credentials are kept only "
                             "for this session. FTP is unencrypted.");
    BLayoutBuilder::Group<>(this, B_VERTICAL, 6)
        .Add(url)
        .AddGroup(B_HORIZONTAL, 12)
        .Add(user)
        .Add(password)
        .End()
        .Add(advanced)
        .Add(error, 0)
        .AddGroup(B_HORIZONTAL, 8)
        .Add(new BButton("Advanced", new BMessage(kAdvanced)))
        .AddGlue()
        .Add(new BButton("Cancel", new BMessage(kCancelConnect)))
        .Add(new BButton("Connect", new BMessage(kConnect)))
        .End();
  }
  void AttachedToWindow() override {
    BGroupView::AttachedToWindow();
    auto target = [&](auto &&self, BView *view) -> void {
      for (int32 i = 0; i < view->CountChildren(); ++i) {
        auto child = view->ChildAt(i);
        if (auto button = dynamic_cast<BButton *>(child))
          button->SetTarget(this);
        if (auto control = dynamic_cast<BTextControl *>(child)) {
          control->SetTarget(this);
          control->TextView()->AddFilter(
              new Listi::ExplicitSubmit(BMessenger(this), BMessage(kConnect)));
        }
        self(self, child);
      }
    };
    target(target, this);
    url->MakeFocus(true);
  }
  void MessageReceived(BMessage *m) override {
    if (m->what == kCancelConnect) {
      Close();
      return;
    }
    if (m->what == kAdvanced) {
      expanded = !expanded;
      if (expanded)
        advanced->Show();
      else
        advanced->Hide();
      return;
    }
    if (m->what != kConnect) {
      BGroupView::MessageReceived(m);
      return;
    }
    try {
      auto location = Listi::ParseLocation(url->Text());
      if (location.Local() || location.Archive())
        throw std::runtime_error("Enter an SMB, SFTP, FTP or FTPS address.");
      Listi::Connection auth;
      auth.username = user->Text();
      if (location.scheme == "sftp" && auth.username.empty()) {
        error->SetMessage("Enter the SSH username for this server.");
        user->MakeFocus(true);
        return;
      }
      auth.password = password->Text();
      auth.domain = domain->Text();
      auth.privateKey = key->Text();
      auth.knownHosts = hosts->Text();
      Listi::SetConnection(location.URL(), auth);
      BMessage navigate(kNavigate);
      navigate.AddString("path", location.URL().c_str());
      pane.SendMessage(&navigate);
      Close();
    } catch (const std::exception &e) {
      error->SetMessage(e.what());
    }
  }
};
} // namespace
BView *CreateConnectionView(BMessenger pane, BMessenger close,
                            const BMessage &message) {
  return new ConnectionView(pane, close, message);
}
