// SPDX-License-Identifier: MIT
#include "OperationQueue.h"
#include "ArchivePasswords.h"
#include "ArchiveBackend.h"
#include "VirtualFile.h"
#include <Button.h>
#include <MessageRunner.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StatusBar.h>
#include <StringView.h>
#include <TextView.h>
#include "ThemeTextView.h"
#include <Window.h>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
namespace {
uint64_t Size(const std::string &p, const std::function<bool()> &proceed, unsigned depth = 0) {
  if (depth > 64 || !proceed())
    return 0;
  if (Listi::IsVirtual(p)) {
    try {
      Listi::TransferControl control;
      control.proceed = proceed;
      Listi::FileEntry e;
      if (!Listi::StatFile(p, e, control) || e.link)
        return 0;
      if (!e.directory)
        return std::max<int64_t>(0, e.size);
      uint64_t total = 0;
      for (auto &child : Listi::ListFiles(p, control))
        total += Size(child.path, proceed, depth + 1);
      return total;
    } catch (...) {
      return 0;
    }
  }
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
    total += Size(Listi::Join(p, e->d_name), proceed, depth + 1);
    if (!proceed())
      break;
  }
  closedir(dir);
  return total;
}
const char *Verb(Listi::Action a) {
  switch (a) {
  case Listi::Action::CreateArchive: return "Create archive";
  case Listi::Action::ExtractArchive: return "Extract archive";
  case Listi::Action::EditArchive: return "Update archive";
  case Listi::Action::MoveArchive: return "Move archive entries";
  case Listi::Action::Copy:
    return "Copy";
  case Listi::Action::Move:
    return "Move";
  case Listi::Action::Duplicate:
    return "Duplicate";
  case Listi::Action::Link:
    return "Create link";
  case Listi::Action::Trash:
    return "Move to Trash";
  case Listi::Action::Restore:
    return "Restore";
  case Listi::Action::Delete:
    return "Delete";
  case Listi::Action::Rename:
    return "Rename";
  default:
    return "Create directory";
  }
}
} // namespace
class ProgressView : public BView {
  OperationQueue *owner;
  BStringView *title, *file, *summary;
  BStatusBar *bar;
  BButton *pause, *cancelButton;
  BString lastError;
  BGroupView *choices;
  BView *details;
  BButton *detailsButton;
  bool expanded = false;
  bool choicesVisible = false;
  int64 conflictRequest = 0, activeOperation = -1;
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
  ProgressView(OperationQueue *o)
      : BView("operations", B_WILL_DRAW | B_SUPPORTS_LAYOUT), owner(o) {
    SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
    title = new BStringView("operation", "No queued operations");
    title->SetFont(be_bold_font);
    file = new BStringView("current-file", "");
    file->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    file->SetExplicitMinSize(BSize(200, B_SIZE_UNSET));
    file->SetTruncation(B_TRUNCATE_MIDDLE);
    summary = new BStringView("summary", "Ready");
    summary->SetExplicitMinSize(BSize(180, B_SIZE_UNSET));
    summary->SetTruncation(B_TRUNCATE_END);
    title->SetExplicitMinSize(BSize(140, B_SIZE_UNSET));
    title->SetTruncation(B_TRUNCATE_END);
    bar = new BStatusBar("progress");
    bar->SetBarColor(tint_color(ui_color(B_STATUS_BAR_COLOR), 1.2));
    bar->SetBarHeight(4);
    pause = new BButton("pause", "Pause", new BMessage(kPause));
    cancelButton = new BButton("Cancel", new BMessage(kAbort));
    pause->SetEnabled(false);
    cancelButton->SetEnabled(false);
    file->Hide();
    bar->Hide();
    choices = new BGroupView(B_HORIZONTAL, 4);
    const char *labels[] = {"Replace", "Replace all", "Skip", "Skip all",
                            "Abort"};
    for (int i = 0; i < 5; i++) {
      auto m = new BMessage(kAnswer);
      m->AddInt32("decision", i);
      choices->AddChild(new BButton(labels[i], m));
    }
    choices->Hide();
    history = new ThemeTextView("history", B_DOCUMENT_BACKGROUND_COLOR, B_DOCUMENT_TEXT_COLOR);
    history->MakeEditable(false);
    history->SetWordWrap(true);
    history->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    history->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    history->SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
    auto scroll = new BScrollView("history-scroll", history, 0, false, true);
    scroll->SetExplicitMinSize(BSize(440, 55));
    scroll->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 55));
    details = new BGroupView(B_VERTICAL, 5);
    BLayoutBuilder::Group<>(details, B_VERTICAL, 5).Add(choices).Add(scroll);
    details->Hide();
    detailsButton = new BButton("Details", new BMessage(kShowJobs));
    BLayoutBuilder::Group<>(this, B_VERTICAL, 4)
        .SetInsets(8, 4, 8, 6)
        .AddGroup(B_HORIZONTAL, 8)
        .Add(title)
        .AddGlue()
        .Add(summary)
        .Add(pause)
        .Add(cancelButton)
        .Add(detailsButton)
        .End()
        .Add(file)
        .Add(bar)
        .Add(details);
    SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
  }
  void AttachedToWindow() override {
    pause->SetTarget(this);
    detailsButton->SetTarget(this);
    cancelButton->SetTarget(this);
    for (int32 i = 0; i < choices->CountChildren(); ++i)
      if (auto button = dynamic_cast<BButton *>(choices->ChildAt(i)))
        button->SetTarget(this);
  }
  bool compact = false, visible = true;
  void UpdateVisibility() {
    const bool show = !compact || expanded || choicesVisible || owner->Busy();
    if (visible == show) return;
    visible = show;
    if (show) BView::Show(); else BView::Hide();
  }
  void SetCompact(bool value) { compact = value; UpdateVisibility(); }
  void Expand(bool show) {
    if (expanded == show) { UpdateVisibility(); return; }
    expanded = show;
    if (show)
      details->Show();
    else
      details->Hide();
    detailsButton->SetLabel(show ? "Less" : "Details");
    UpdateVisibility();
  }
  void Toggle() { Expand(!expanded); }
  void Dismiss() {
    if (choicesVisible) {
      owner->Answer(Listi::Decision::Abort, conflictRequest);
      ShowChoices(false);
    }
    Expand(false);
  }
  void MessageReceived(BMessage *m) override {
    switch (m->what) {
    case kShowJobs:
      Toggle();
      break;
    case kFileErrorSelected:
      if (lastError == m->GetString("error", ""))
        break;
      lastError = m->GetString("error", "");
      Expand(true);
      history->Insert(m->GetString("error", ""));
      history->Insert("\n");
      if (history->TextLength() > 65536)
        history->Delete(0, history->TextLength() - 65536);
      history->Select(history->TextLength(), history->TextLength());
      history->ScrollToSelection();
      break;
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
      if (owner->Answer(static_cast<Listi::Decision>(m->GetInt32("decision", 4)),
                        m->GetInt64("request", -1)))
        ShowChoices(false);
      break;
    case kConflict:
      conflictRequest = m->GetInt64("request", -1);
      for (int32 i = 0; i < choices->CountChildren(); ++i) {
        if (auto button = dynamic_cast<BButton *>(choices->ChildAt(i))) {
          auto answer = new BMessage(kAnswer);
          answer->AddInt32("decision", i);
          answer->AddInt64("request", conflictRequest);
          button->SetMessage(answer);
        }
      }
      Expand(true);
      summary->SetText("Destination exists. Choose how to continue:");
      file->SetText(m->GetString("path", ""));
      ShowChoices(true);
      break;
    case kJobUpdate: {
      if (m->GetBool("start", false)) {
        activeOperation = m->GetInt64("operation", -1);
        if (file->IsHidden())
          file->Show();
        if (bar->IsHidden())
          bar->Show();
        pause->SetEnabled(true);
        cancelButton->SetEnabled(true);
        bar->Reset();
        bar->SetMaxValue(float(std::max<int64>(1, m->GetInt64("total", 1))));
        pause->SetLabel("Pause");
        summary->SetText("Working. You can keep browsing.");
      }
      if (m->GetInt64("operation", -2) != activeOperation) break;
      title->SetText(m->GetString("title", title->Text()));
      file->SetText(m->GetString("path", file->Text()));
      float bytes = float(m->GetInt64("bytes", 0));
      bar->Update(bytes - bar->CurrentValue());
      BString s;
      s << m->GetInt64("bytes", 0) << " bytes processed";
      bar->SetToolTip(s);
      break;
    }
    case kJobDone: {
      if (m->GetBool("complete", false))
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
      if (history->TextLength() > 65536)
        history->Delete(0, history->TextLength() - 65536);
      history->Select(history->TextLength(), history->TextLength());
      history->ScrollToSelection();
      if (!owner->Busy()) {
        title->SetText("Queue finished");
        pause->SetEnabled(false);
        cancelButton->SetEnabled(false);
        if (!file->IsHidden())
          file->Hide();
        if (!bar->IsHidden())
          bar->Hide();
      }
      if (m->HasString("error"))
        Expand(true);
      ShowChoices(false);
      activeOperation = -1;
      break;
    }
    case B_COLORS_UPDATED:
      bar->SetBarColor(ui_color(B_STATUS_BAR_COLOR));
      BView::MessageReceived(m);
      break;
    default:
      BView::MessageReceived(m);
      break;
    }
    UpdateVisibility();
  }
};
OperationQueue::OperationQueue(BMessenger m) : main(m) {
  view = new ProgressView(this);
}
BView *OperationQueue::View() { return view; }
void OperationQueue::Start() {
  display = BMessenger(view);
  BMessage poll(kQueueWake);
  eventTimer.reset(new BMessageRunner(main, &poll, 250000));
  if (eventTimer->InitCheck() != B_OK)
    throw std::runtime_error("Cannot start operation result delivery.");
  thread = std::thread([this] { Loop(); });
}
OperationQueue::~OperationQueue() {
  eventTimer.reset();
  {
    std::lock_guard<std::mutex> l(mutex);
    stopping = true;
    cancel = true;
    paused = false;
    answer = int(Listi::Decision::Abort);
  }
  changed.notify_all();
  if (thread.joinable())
    thread.join();
}
void OperationQueue::SetCompact(bool value) { view->SetCompact(value); }
void OperationQueue::Show() { view->Expand(true); }
void OperationQueue::Toggle() { view->Toggle(); }
void OperationQueue::Collapse() { view->Dismiss(); }
void OperationQueue::ErrorInfo(const char *text) {
  BMessage error(kFileErrorSelected);
  error.AddString("error", text);
  display.SendMessage(&error);
}
void OperationQueue::Enqueue(const Listi::Job &j) {
  {
    std::lock_guard<std::mutex> l(mutex);
    jobs.push_back(j);
    count++;
  }
  changed.notify_all();
}
void OperationQueue::Pause() {
  {
    std::lock_guard<std::mutex> l(mutex);
    paused = !paused;
  }
  changed.notify_all();
}
void OperationQueue::Cancel() {
  std::vector<int64> discardedTags;
  std::vector<uint64_t> discardedPasswords;
  {
    std::lock_guard<std::mutex> l(mutex);
    cancel = true;
    paused = false;
    for (const auto &job : jobs) {
      if (job.tag) discardedTags.push_back(job.tag);
      if (job.archivePasswordToken) discardedPasswords.push_back(job.archivePasswordToken);
    }
    count -= jobs.size();
    jobs.clear();
    answer = int(Listi::Decision::Abort);
  }
  changed.notify_all();
  for (auto token : discardedPasswords)
    Listi::DiscardArchiveCreationPassword(token);
  // Queued jobs never reach Loop(), but their clipboard leases still need a
  // terminal result. Send outside the mutex and do not dismiss active choices.
  for (int64 tag : discardedTags) {
    BMessage done(kJobDone);
    done.AddInt64("tag", tag);
    done.AddBool("complete", false);
    done.AddBool("idle", count.load() == 0);
    done.AddString("result", "Cancelled before starting.");
    Publish(done);
  }
}
void OperationQueue::Publish(const BMessage &done) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    events.push_back(done);
  }
  BMessage wake(kQueueWake);
  main.SendMessage(&wake, (BHandler *)nullptr, 100000);
}
std::vector<BMessage> OperationQueue::TakeEvents() {
  std::lock_guard<std::mutex> lock(mutex);
  std::vector<BMessage> out;
  out.swap(events);
  return out;
}
void OperationQueue::PresentEvent(BMessage &done) {
  // Called on the view's owning window thread, never by the worker.
  view->MessageReceived(&done);
}
bool OperationQueue::Answer(Listi::Decision d, int64 request) {
  {
    std::lock_guard<std::mutex> l(mutex);
    if (!awaitingAnswer || request != conflictSerial || answer >= 0 ||
        int(d) < int(Listi::Decision::Replace) ||
        int(d) > int(Listi::Decision::Abort))
      return false;
    answer = int(d);
  }
  changed.notify_all();
  return true;
}
bool OperationQueue::Proceed() {
  std::unique_lock<std::mutex> l(mutex);
  changed.wait(l, [this] { return !paused || cancel || stopping; });
  return !cancel && !stopping;
}
Listi::Decision OperationQueue::Ask(const std::string &path) {
  int64 request;
  {
    std::lock_guard<std::mutex> l(mutex);
    if (cancel || stopping)
      return Listi::Decision::Abort;
    answer = -1;
    request = ++conflictSerial;
    awaitingAnswer = true;
  }
  BMessage m(kConflict);
  m.AddInt64("request", request);
  m.AddString("path", path.c_str());
  Publish(m);
  std::unique_lock<std::mutex> l(mutex);
  changed.wait(l, [this] { return answer >= 0 || cancel || stopping; });
  awaitingAnswer = false;
  return cancel || stopping ? Listi::Decision::Abort
                                           : static_cast<Listi::Decision>(answer);
}
void OperationQueue::Loop() {
  for (;;) {
    Listi::Job job;
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
    Listi::ArchiveListingScope listingScope;
    const int64 operation = ++operationSerial;
    BString title;
    title << Verb(job.action) << ": " << int32(job.sources.size())
          << " selected entries";
    BMessage start(kJobUpdate);
    start.AddInt64("operation", operation);
    start.AddString("title", title);
    start.AddString("path", job.sources.empty() ? job.destination.c_str()
                                                : job.sources[0].c_str());
    start.AddBool("start", true);
    Publish(start);
    uint64_t total = 0;
    if (job.action == Listi::Action::Copy ||
        job.action == Listi::Action::Move || job.action == Listi::Action::Duplicate || job.action == Listi::Action::CreateArchive ||
        job.action == Listi::Action::EditArchive || job.action == Listi::Action::MoveArchive)
      for (auto &s : job.sources)
        total += Size(s, [this] { return Proceed(); });
    start.AddInt64("total", std::max<uint64_t>(1, total));
    Publish(start);
    std::map<std::string, uint64_t> fileBytes, fileSizes;
    uint64_t bytes = 0;
    bigtime_t last = 0;
    Listi::Hooks hooks;
    hooks.proceed = [this] { return Proceed(); };
    hooks.conflict = [this](auto &p) { return Ask(p); };
    hooks.progress = [&](auto &p, uint64_t n) {
      bytes += n;
      fileBytes[p] += n;
      if (!fileSizes.count(p)) {
        try {
          Listi::FileEntry entry;
          Listi::TransferControl control;
          control.proceed = hooks.proceed;
          if (Listi::StatFile(p, entry, control))
            fileSizes[p] = std::max<int64_t>(0, entry.size);
        } catch (...) {
          fileSizes[p] = 0;
        }
      }
      if (system_time() - last < 80000)
        return;
      last = system_time();
      BMessage m(kJobUpdate);
      m.AddInt64("operation", operation);
      m.AddString("path", p.c_str());
      m.AddInt64("bytes", bytes);
      display.SendMessage(&m, (BHandler *)nullptr, 500000);
      BMessage file(kFileProgress);
      file.AddString("path", p.c_str());
      file.AddInt32("state", 1);
      file.AddFloat("fraction",
                    fileSizes[p]
                        ? std::min(0.99f, float(fileBytes[p]) / fileSizes[p])
                        : 0.f);
      main.SendMessage(&file, (BHandler *)nullptr, 500000);
    };
    hooks.fileDone = [&](const std::string &path,
                         const std::string &destination) {
      BMessage file(kFileProgress);
      file.AddString("path", path.c_str());
      file.AddString("destination", destination.c_str());
      file.AddInt32("state", 2);
      file.AddFloat("fraction", 1.f);
      main.SendMessage(&file, (BHandler *)nullptr, 500000);
    };
    hooks.fileFailed = [&](const std::string &path, const std::string &error) {
      BMessage file(kFileProgress);
      file.AddString("path", path.c_str());
      file.AddString("error", error.c_str());
      file.AddInt32("state", 3);
      main.SendMessage(&file, (BHandler *)nullptr, 500000);
    };
    auto result = Listi::Run(job, hooks);
    count--;
    BMessage done(kJobDone);
    done.AddInt64("operation", operation);
    done.AddInt64("tag", job.tag);
    if (job.action == Listi::Action::Move || job.action == Listi::Action::Duplicate || job.action == Listi::Action::CreateArchive ||
        job.action == Listi::Action::EditArchive || job.action == Listi::Action::MoveArchive)
      for (const auto &root : result.completedRoots)
        done.AddString("moved", root.first.c_str());
    BString summary;
    summary << Verb(job.action) << ": " << result.completed << " completed, "
            << result.skipped << " skipped, " << int32(result.errors.size())
            << " errors";
    if (result.cancelled)
      summary << ". Cancelled; completed entries kept.";
    done.AddString("result", summary);
    done.AddBool("complete", !result.cancelled && result.errors.empty() &&
                                 result.skipped == 0);
    for (auto &e : result.errors)
      done.AddString("error", e.c_str());
    done.AddBool("idle", count.load() == 0);
    Publish(done);
  }
}
