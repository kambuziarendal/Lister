// SPDX-License-Identifier: MIT
#include "BulkRename.h"
#include "BulkRenameWindow.h"
#include "Common.h"
#include "JobSnapshot.h"
#include <Application.h>
#include <Button.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <StringView.h>
#include <TextControl.h>
#include <Window.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <unistd.h>

namespace {
class Receiver : public BLooper {
public:
  const sem_id accepted = create_sem(0, "rename accepted");
  BMessage result;

  ~Receiver() override { delete_sem(accepted); }

  void MessageReceived(BMessage *message) override {
    if (message->what == kBulkRenameAccepted) {
      result = *message;
      release_sem(accepted);
    } else {
      BLooper::MessageReceived(message);
    }
  }
};

std::string Read(const std::string &path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), {}};
}

template <class F> bool WithWindow(BMessenger dialog, F action) {
  if (!dialog.LockTarget())
    return false;
  BLooper *looper = nullptr;
  dialog.Target(&looper);
  const bool ok = action(*static_cast<BWindow *>(looper));
  looper->Unlock();
  return ok;
}

template <class F> bool WaitFor(BMessenger dialog, F condition) {
  for (int i = 0; i < 150 && dialog.IsValid(); ++i) {
    if (WithWindow(dialog, condition))
      return true;
    snooze(20000);
  }
  return false;
}

std::string PreviewName(BColumnListView &rows, int index) {
  const auto row = rows.RowAt(index);
  const auto field = row ? dynamic_cast<BStringField *>(row->GetField(1))
                         : nullptr;
  return field ? field->String() : "";
}
} // namespace

int main() {
  BApplication app("application/x-vnd.Listi-BulkRenameWindowTests");
  char temp[] = "/tmp/Listi-rename-window-XXXXXX";
  if (!mkdtemp(temp))
    return 2;
  const std::string folder(temp), a = folder + "/alpha", b = folder + "/beta";
  std::ofstream(a) << "first";
  std::ofstream(b) << "second";
  auto receiver = new Receiver;
  receiver->Run();
  const auto dialog = Listi::OpenBulkRenameWindow(
      Listi::FreezeBulkRename(folder, {a, b}), BMessenger(receiver));

  auto rows = [](BWindow &window) {
    return dynamic_cast<BColumnListView *>(window.FindView("rename preview"));
  };
  auto button = [](BWindow &window, const char *name) {
    return dynamic_cast<BButton *>(window.FindView(name));
  };
  auto editor = [](BWindow &window) {
    return dynamic_cast<BTextControl *>(window.FindView("row-name"));
  };
  auto select = [&](int index, const char *expected) {
    return WithWindow(dialog, [&](BWindow &window) {
             auto list = rows(window);
             auto row = list ? list->RowAt(index) : nullptr;
             if (!row)
               return false;
             list->DeselectAll();
             list->AddToSelection(row);
             list->SelectionChanged();
             return true;
           }) &&
           WaitFor(dialog, [&](BWindow &window) {
             return editor(window) && editor(window)->IsEnabled() &&
                    std::string(editor(window)->Text()) == expected;
           });
  };
  auto edit = [&](const char *text, bool save) {
    return WithWindow(dialog, [&](BWindow &window) {
      auto field = editor(window);
      if (!field || !field->IsEnabled() || !field->ModificationMessage())
        return false;
      field->SetText(text);
      if (dialog.SendMessage(field->ModificationMessage()) != B_OK)
        return false;
      return !save || (button(window, "set-row-name") &&
                       button(window, "set-row-name")->Invoke() == B_OK);
    });
  };
  auto swapPreview = [&](BWindow &window) {
    auto list = rows(window);
    auto apply = button(window, "apply-rename");
    return list && apply && apply->IsEnabled() &&
           PreviewName(*list, 0) == "beta" && PreviewName(*list, 1) == "alpha";
  };
  auto check = [](bool ok, const char *label) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    return ok;
  };
  auto noSubmission = [&] {
    return acquire_sem_etc(receiver->accepted, 1, B_RELATIVE_TIMEOUT, 150000) ==
           B_TIMED_OUT;
  };
  bool ok = check(select(0, "alpha") && edit("beta", true) &&
                      WaitFor(dialog, [&](BWindow &window) {
                        auto list = rows(window);
                        auto apply = button(window, "apply-rename");
                        return list && apply && !apply->IsEnabled() &&
                               PreviewName(*list, 0) == "beta";
                      }) &&
                      select(1, "beta") && edit("alpha", true) &&
                      WaitFor(dialog, swapPreview) &&
                      Read(a) == "first" && Read(b) == "second" && noSubmission(),
                  "editing both native rows previews a swap without modifying files");

  ok &= check(edit("uncommitted", false) &&
                  WaitFor(dialog, [&](BWindow &window) {
                    auto apply = button(window, "apply-rename");
                    return apply && !apply->IsEnabled();
                  }) &&
                  WithWindow(dialog, [&](BWindow &window) {
                    // Even a stale Apply message cannot submit older names.
                    auto apply = button(window, "apply-rename");
                    return apply && dialog.SendMessage(apply->Message()) == B_OK;
                  }) &&
                  noSubmission() && Read(a) == "first" && Read(b) == "second",
              "an uncommitted row edit cannot apply a different preview");

  ok &= check(WithWindow(dialog, [&](BWindow &window) {
                auto prefix =
                    dynamic_cast<BTextControl *>(window.FindView("Prefix:"));
                auto apply = button(window, "apply-rename");
                if (!prefix || !prefix->ModificationMessage() || !apply)
                  return false;
                prefix->SetText("rule ");
                return dialog.SendMessage(prefix->ModificationMessage()) == B_OK &&
                       dialog.SendMessage(apply->Message()) == B_OK;
              }) &&
                  WaitFor(dialog, [&](BWindow &window) {
                    auto list = rows(window);
                    auto field = editor(window);
                    auto apply = button(window, "apply-rename");
                    return list && field && apply && list->CountRows() == 0 &&
                           !field->IsEnabled() && !apply->IsEnabled() &&
                           std::string(field->Text()).empty();
                  }) &&
                  noSubmission(),
              "editing a rule invalidates stale row text and the old Apply action");

  const bool confirmed = WithWindow(dialog, [&](BWindow &window) {
                           auto update = button(window, "update-preview");
                           return update && update->Invoke() == B_OK;
                         }) &&
                         WaitFor(dialog, swapPreview) &&
                         WithWindow(dialog, [&](BWindow &window) {
                           auto apply = button(window, "apply-rename");
                           return apply && apply->Invoke() == B_OK;
                         }) &&
                         acquire_sem_etc(receiver->accepted, 1, B_RELATIVE_TIMEOUT,
                                         3000000) == B_OK;
  const auto job = Listi::DecodeJob(receiver->result);
  const std::map<std::string, std::string> expected{{a, "beta"}, {b, "alpha"}};
  ok &= check(confirmed && job.action == Listi::Action::BulkRename &&
                  job.sources == std::vector<std::string>{a, b} &&
                  job.bulkNames == expected &&
                  Read(a) == "first" && Read(b) == "second",
              "Apply submits exactly the refreshed preview, without doing the worker's writes");
  if (confirmed) {
    const auto result = Listi::Run(job, {});
    ok &= check(result.completed == 2 && result.errors.empty() &&
                    Read(a) == "second" && Read(b) == "first",
                "the queued mapping swaps the real entries in the worker");
  }

  if (dialog.IsValid())
    dialog.SendMessage(B_QUIT_REQUESTED);
  receiver->Lock();
  receiver->Quit();
  unlink(a.c_str());
  unlink(b.c_str());
  rmdir(folder.c_str());
  return ok ? 0 : 1;
}
