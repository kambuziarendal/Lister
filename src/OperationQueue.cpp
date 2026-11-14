// SPDX-License-Identifier: MIT
#include "OperationQueue.h"
#include <Button.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StatusBar.h>
#include <StringView.h>
#include <TextView.h>
#include <Window.h>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
namespace {
uint64_t Size(const std::string &p, const std::function<bool()> &proceed) {
  if (!proceed())
    return 0;
  struct stat st;
  if (lstat(p.c_str(), &st))
    return 0;
  if (S_ISREG(st.st_mode))
    return st.st_size;
  if (!S_ISDIR(st.st_mode))
    return 0;
  uint64_t total = 0;
  DIR *dir = opendir(p.c_str());
  if (!dir)
    return 0;
  while (auto e = readdir(dir)) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
      continue;
    total += Size(Lister::Join(p, e->d_name), proceed);
    if (!proceed())
      break;
  }
  closedir(dir);
  return total;
}
const char *Verb(Lister::Action a) {
  switch (a) {
  case Lister::Action::Copy:
    return "Copy";
  case Lister::Action::Move:
    return "Move";
  case Lister::Action::Trash:
    return "Move to Trash";
  case Lister::Action::Restore:
    return "Restore";
  case Lister::Action::Delete:
    return "Delete";
  case Lister::Action::Rename:
    return "Rename";
  default:
    return "Create directory";
  }
}
} // namespace
class ProgressWindow : public BWindow {
  OperationQueue *owner;
  BStringView *title, *file, *summary;
  BStatusBar *bar;
  BButton *pause;
  BGroupView *choices;
  bool choicesVisible = false;
  void ShowChoices(bool show) {
    if (show == choicesVisible)
      return;
    choicesVisible = show;
    if (show)
      choices->Show();
    else
      choices->Hide();
  }
  BTextView *history;

public:
  ProgressWindow(OperationQueue *o)
      : BWindow(BRect(0, 0, 640, 340), "File operations", B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS),
        owner(o) {
    title = new BStringView("operation", "No queued operations");
    title->SetFont(be_bold_font);
    file = new BStringView("current-file", "");
    file->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    file->SetExplicitMinSize(BSize(200, B_SIZE_UNSET));
    file->SetTruncation(B_TRUNCATE_MIDDLE);
    summary = new BStringView("summary", "");
    bar = new BStatusBar("progress");
    bar->SetBarColor(ui_color(B_STATUS_BAR_COLOR));
    pause = new BButton("pause", "Pause", new BMessage(kPause));
    choices = new BGroupView(B_HORIZONTAL, 4);
    const char *labels[] = {"Replace", "Replace all", "Skip", "Skip all",
                            "Abort"};
    for (int i = 0; i < 5; i++) {
      auto m = new BMessage(kAnswer);
      m->AddInt32("decision", i);
      choices->AddChild(new BButton(labels[i], m));
    }
    choices->Hide();
    history = new BTextView("history");
    history->MakeEditable(false);
    history->SetWordWrap(true);
    history->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    history->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    history->SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
    auto scroll = new BScrollView("history-scroll", history, 0, false, true);
    scroll->SetExplicitMinSize(BSize(440, 110));
    BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
        .SetInsets(12)
        .Add(title)
        .Add(file)
        .Add(bar)
        .Add(summary)
        .Add(choices)
        .Add(scroll)
        .AddGroup(B_HORIZONTAL, 8)
        .AddGlue()
        .Add(pause)
        .Add(new BButton("cancel", "Cancel all", new BMessage(kAbort)))
        .End();
    CenterOnScreen();
  }
  bool QuitRequested() override {
    Hide();
    return false;
  }
  void MessageReceived(BMessage *m) override {
    switch (m->what) {
    case kPause:
      owner->Pause();
      pause->SetLabel(!strcmp(pause->Label(), "Pause") ? "Resume" : "Pause");
      break;
    case kAbort:
      owner->Cancel();
      pause->SetLabel("Pause");
      ShowChoices(false);
      summary->SetText("Cancelling. Finishing the current safe step...");
      break;
    case kAnswer:
      owner->Answer(static_cast<Lister::Decision>(m->GetInt32("decision", 4)));
      ShowChoices(false);
      break;
    case kConflict:
      if (IsHidden())
        Show();
      Activate();
      summary->SetText("Destination exists. Choose how to continue:");
      file->SetText(m->GetString("path", ""));
      ShowChoices(true);
      break;
    case kJobUpdate: {
      if (m->GetBool("start", false)) {
        bar->Reset();
        bar->SetMaxValue(float(std::max<int64>(1, m->GetInt64("total", 1))));
        pause->SetLabel("Pause");
        summary->SetText("Working. You can keep using both panels.");
      }
      title->SetText(m->GetString("title", title->Text()));
      file->SetText(m->GetString("path", file->Text()));
      float bytes = float(m->GetInt64("bytes", 0));
      bar->Update(bytes - bar->CurrentValue());
      BString s;
      s << m->GetInt64("bytes", 0) << " bytes processed";
      bar->SetTrailingText(s);
      break;
    }
    case kJobDone: {
      bar->Update(bar->MaxValue() - bar->CurrentValue());
      BString s = m->GetString("result", "");
      summary->SetText(s);
      s << "\n";
      history->Insert(s.String());
      const char *error;
      for (int i = 0; m->FindString("error", i, &error) == B_OK; i++) {
        history->Insert(error);
        history->Insert("\n");
      }
      history->Select(history->TextLength(), history->TextLength());
      history->ScrollToSelection();
      if (m->GetBool("idle", false))
        title->SetText("Queue finished");
      ShowChoices(false);
      break;
    }
    case B_COLORS_UPDATED:
      bar->SetBarColor(ui_color(B_STATUS_BAR_COLOR));
      BWindow::MessageReceived(m);
      break;
    default:
      BWindow::MessageReceived(m);
      break;
    }
  }
};
OperationQueue::OperationQueue(BMessenger m) : main(m) {
  window = new ProgressWindow(this);
  window->Show();
  window->Lock();
  window->Hide();
  window->Unlock();
  display = BMessenger(window);
  thread = std::thread([this] { Loop(); });
}
OperationQueue::~OperationQueue() {
  {
    std::lock_guard<std::mutex> l(mutex);
    stopping = true;
    cancel = true;
    paused = false;
    answer = int(Lister::Decision::Abort);
  }
  changed.notify_all();
  thread.join();
  if (window->Lock())
    window->Quit();
}
void OperationQueue::Show() {
  if (window->Lock()) {
    if (window->IsHidden())
      window->Show();
    window->Activate();
    window->Unlock();
  }
}
void OperationQueue::Enqueue(const Lister::Job &j) {
  {
    std::lock_guard<std::mutex> l(mutex);
    jobs.push_back(j);
    count++;
  }
  changed.notify_all();
  Show();
}
void OperationQueue::Pause() {
  {
    std::lock_guard<std::mutex> l(mutex);
    paused = !paused;
  }
  changed.notify_all();
}
void OperationQueue::Cancel() {
  {
    std::lock_guard<std::mutex> l(mutex);
    cancel = true;
    paused = false;
    count -= jobs.size();
    jobs.clear();
    answer = int(Lister::Decision::Abort);
  }
  changed.notify_all();
}
void OperationQueue::Answer(Lister::Decision d) {
  {
    std::lock_guard<std::mutex> l(mutex);
    answer = int(d);
  }
  changed.notify_all();
}
bool OperationQueue::Proceed() {
  std::unique_lock<std::mutex> l(mutex);
  changed.wait(l, [this] { return !paused || cancel || stopping; });
  return !cancel && !stopping;
}
Lister::Decision OperationQueue::Ask(const std::string &path) {
  {
    std::lock_guard<std::mutex> l(mutex);
    answer = -1;
  }
  BMessage m(kConflict);
  m.AddString("path", path.c_str());
  display.SendMessage(&m, (BHandler *)nullptr, 500000);
  std::unique_lock<std::mutex> l(mutex);
  changed.wait(l, [this] { return answer >= 0 || cancel || stopping; });
  return cancel || stopping ? Lister::Decision::Abort
                            : static_cast<Lister::Decision>(answer);
}
void OperationQueue::Loop() {
  for (;;) {
    Lister::Job job;
    {
      std::unique_lock<std::mutex> l(mutex);
      changed.wait(l, [this] { return stopping || !jobs.empty(); });
      if (stopping)
        return;
      job = jobs.front();
      jobs.pop_front();
      cancel = false;
      paused = false;
    }
    BString title;
    title << Verb(job.action) << ": " << int32(job.sources.size())
          << " selected entries";
    BMessage start(kJobUpdate);
    start.AddString("title", title);
    start.AddString("path", job.sources.empty() ? job.destination.c_str()
                                                : job.sources[0].c_str());
    start.AddBool("start", true);
    display.SendMessage(&start);
    uint64_t total = 0;
    if (job.action == Lister::Action::Copy ||
        job.action == Lister::Action::Move)
      for (auto &s : job.sources)
        total += Size(s, [this] { return Proceed(); });
    start.AddInt64("total", std::max<uint64_t>(1, total));
    display.SendMessage(&start);
    uint64_t bytes = 0;
    bigtime_t last = 0;
    Lister::Hooks hooks;
    hooks.proceed = [this] { return Proceed(); };
    hooks.conflict = [this](auto &p) { return Ask(p); };
    hooks.progress = [&](auto &p, uint64_t n) {
      bytes += n;
      if (system_time() - last < 80000)
        return;
      last = system_time();
      BMessage m(kJobUpdate);
      m.AddString("path", p.c_str());
      m.AddInt64("bytes", bytes);
      display.SendMessage(&m, (BHandler *)nullptr, 500000);
    };
    auto result = Lister::Run(job, hooks);
    count--;
    BMessage done(kJobDone);
    BString summary;
    summary << Verb(job.action) << ": " << result.completed << " completed, "
            << result.skipped << " skipped, " << int32(result.errors.size())
            << " errors";
    if (result.cancelled)
      summary << ". Cancelled; completed entries kept.";
    done.AddString("result", summary);
    for (auto &e : result.errors)
      done.AddString("error", e.c_str());
    done.AddBool("idle", count.load() == 0);
    display.SendMessage(&done);
    main.SendMessage(&done);
  }
}
