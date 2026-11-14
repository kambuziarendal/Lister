// SPDX-License-Identifier: MIT
#include "Common.h"
#include "OperationQueue.h"
#include "Pane.h"
#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <GridView.h>
#include <GroupView.h>
#include <Invoker.h>
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageFilter.h>
#include <Path.h>
#include <PropertyInfo.h>
#include <Roster.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
constexpr uint32 kConfirm = 'cfir', kViewLoaded = 'vloa',
                 kServiceError = 'serr', kSavePreferences = 'save';
static void Notice(const char *text) {
  (new BAlert("Lister", text, "OK", nullptr, nullptr, B_WIDTH_AS_USUAL,
              B_WARNING_ALERT))
      ->Go(nullptr);
}
static std::string Quote(const std::string &s) {
  std::string out = "'";
  for (char c : s)
    out += c == '\'' ? "'\\''" : std::string(1, c);
  return out + "'";
}
static BString SettingsPath() {
  BPath p;
  find_directory(B_USER_SETTINGS_DIRECTORY, &p);
  p.Append("Lister_settings");
  return p.Path();
}
class Services : public BLooper {
  BMessenger app;

public:
  Services(BMessenger a) : BLooper("file viewer and launcher"), app(a) {}
  void MessageReceived(BMessage *m) override {
    if (m->what == kSavePreferences) {
      BString path = SettingsPath(), tmp = path;
      tmp << ".tmp";
      BFile file(tmp, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
      status_t s = m->Flatten(&file);
      if (s == B_OK)
        s = file.Sync();
      if (s == B_OK) {
        BEntry e(tmp);
        s = e.Rename(path, true);
      }
      if (s != B_OK) {
        BMessage error(kServiceError);
        error.AddString("error", "Could not save preferences.");
        app.SendMessage(&error);
      }
      return;
    }
    if (m->what != kOpenPaths) {
      BLooper::MessageReceived(m);
      return;
    }
    BString path = m->GetString("path", ""),
            mode = m->GetString("mode", "open");
    if (mode == "view") {
      std::ifstream in(path.String(), std::ios::binary);
      if (!in) {
        BMessage error(kServiceError);
        error.AddString("error", "Could not read the selected file.");
        app.SendMessage(&error);
        return;
      }
      std::string data(1024 * 1024, '\0');
      in.read(data.data(), data.size());
      data.resize(in.gcount());
      if (data.find('\0') != std::string::npos) {
        BMessage error(kServiceError);
        error.AddString(
            "error",
            "This is a binary file. Open it with its associated application.");
        app.SendMessage(&error);
        return;
      }
      BMessage done(kViewLoaded);
      done.AddString("path", path);
      done.AddString("text", data.c_str());
      done.AddBool("truncated", !in.eof());
      app.SendMessage(&done);
      return;
    }
    status_t s = B_OK;
    if (mode == "terminal" || mode == "command") {
      BString application =
          m->GetString("application", "/boot/system/apps/Terminal");
      entry_ref ref;
      get_ref_for_path(application, &ref);
      std::string script = "cd -- " + Quote(path.String()) + " && ";
      script += mode == "command" ? std::string(m->GetString("command", "")) +
                                        "; exec /bin/bash"
                                  : "exec /bin/bash";
      const char *args[] = {"-t", "Lister", "/bin/bash", "-c", script.c_str()};
      s = be_roster->Launch(&ref, 5, args);
    } else {
      entry_ref ref;
      s = get_ref_for_path(path, &ref);
      if (s == B_OK) {
        BMessage refs(B_REFS_RECEIVED);
        refs.AddRef("refs", &ref);
        if (mode == "edit") {
          entry_ref editor;
          s = get_ref_for_path(
              m->GetString("application", "/boot/system/apps/StyledEdit"),
              &editor);
          if (s == B_OK)
            s = be_roster->Launch(&editor, &refs);
        } else
          s = be_roster->Launch(&ref);
      }
    }
    if (s != B_OK && s != B_ALREADY_RUNNING) {
      BMessage error(kServiceError);
      BString text("Could not open application: ");
      text << strerror(s);
      error.AddString("error", text);
      app.SendMessage(&error);
    }
  }
};
class NameWindow : public BWindow {
  BTextControl *name;
  BMessenger target;
  BMessage result;

public:
  NameWindow(const char *title, const char *initial, BMessenger t,
             const BMessage &m)
      : BWindow(BRect(0, 0, 440, 130), title, B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS),
        target(t), result(m) {
    name = new BTextControl("Name:", initial, new BMessage(kNameAccepted));
    auto ok = new BButton("Continue", new BMessage(kNameAccepted));
    BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
        .SetInsets(12)
        .Add(name)
        .AddGroup(B_HORIZONTAL)
        .AddGlue()
        .Add(new BButton("Cancel", new BMessage(B_QUIT_REQUESTED)))
        .Add(ok)
        .End();
    SetDefaultButton(ok);
    CenterOnScreen();
    name->MakeFocus(true);
    name->TextView()->SelectAll();
  }
  void MessageReceived(BMessage *m) override {
    if (m->what == kNameAccepted) {
      BString s = name->Text();
      if (s.IsEmpty() || s == "." || s == ".." || s.FindFirst('/') >= 0) {
        Notice("Enter a single file or folder name, without a slash.");
        return;
      }
      result.AddString("name", s);
      target.SendMessage(&result);
      Quit();
    } else
      BWindow::MessageReceived(m);
  }
};
class Keys : public BMessageFilter {
public:
  Keys() : BMessageFilter(B_KEY_DOWN) {}
  filter_result Filter(BMessage *m, BHandler **target) override {
    const char *bytes = m->GetString("bytes", "");
    if (bytes[0] == B_TAB) {
      auto view = dynamic_cast<BView *>(*target);
      while (view) {
        if (!strcmp(view->Name(), "files")) {
          Looper()->PostMessage(kSwitch);
          return B_SKIP_MESSAGE;
        }
        view = view->Parent();
      }
    }
    if (bytes[0] != B_FUNCTION_KEY)
      return B_DISPATCH_MESSAGE;
    uint32 action = 0;
    switch (m->GetInt32("key", 0)) {
    case B_F3_KEY:
      action = kView;
      break;
    case B_F4_KEY:
      action = kEdit;
      break;
    case B_F5_KEY:
      action = kCopy;
      break;
    case B_F6_KEY:
      action = kMove;
      break;
    case B_F7_KEY:
      action = kMkdir;
      break;
    case B_F8_KEY:
      action = kDelete;
      break;
    case B_F9_KEY:
      action = kPreferences;
      break;
    default:
      return B_DISPATCH_MESSAGE;
    }
    Looper()->PostMessage(action);
    return B_SKIP_MESSAGE;
  }
};
class MainWindow : public BWindow {
  Preferences prefs;
  Pane *panes[2];
  int source = 0;
  OperationQueue *queue;
  BGroupView *bank;
  BTextControl *command;
  BMessenger service;
  bool bankVisible = true, commandVisible = true;
  void Apply() {
    for (auto p : panes) {
      p->Apply(prefs);
      p->Reload();
    }
    if (prefs.buttons != bankVisible) {
      bankVisible = prefs.buttons;
      if (prefs.buttons)
        bank->Show();
      else
        bank->Hide();
    }
    if (prefs.command != commandVisible) {
      commandVisible = prefs.command;
      if (prefs.command)
        command->Show();
      else
        command->Hide();
    }
  }
  Lister::Job Snapshot(Lister::Action a) {
    Lister::Job j;
    j.action = a;
    j.sources = panes[source]->Selected();
    j.destination = panes[1 - source]->Path();
    return j;
  }
  BMessage Encode(const Lister::Job &j, uint32 what) {
    BMessage m(what);
    m.AddInt32("action", int(j.action));
    m.AddString("destination", j.destination.c_str());
    for (auto &s : j.sources)
      m.AddString("source", s.c_str());
    return m;
  }
  Lister::Job Decode(const BMessage &m) {
    Lister::Job j;
    j.action = static_cast<Lister::Action>(m.GetInt32("action", 0));
    j.destination = m.GetString("destination", "");
    const char *s;
    for (int i = 0; m.FindString("source", i, &s) == B_OK; i++)
      j.sources.push_back(s);
    return j;
  }
  void Operation(Lister::Action a) {
    auto job = Snapshot(a);
    if (a != Lister::Action::MakeDirectory && job.sources.empty()) {
      Notice("Select one or more files in the SOURCE panel first.");
      return;
    }
    if ((a == Lister::Action::Copy || a == Lister::Action::Move) &&
        job.destination == "@disks") {
      Notice("Open a destination folder first.");
      return;
    }
    if (a == Lister::Action::Rename || a == Lister::Action::MakeDirectory) {
      if (a == Lister::Action::Rename && job.sources.size() != 1) {
        Notice("Select one entry to rename.");
        return;
      }
      job.destination = panes[source]->Path();
      if (job.destination == "@disks") {
        Notice("Open a folder first.");
        return;
      }
      auto message = Encode(job, kNameAccepted);
      (new NameWindow(a == Lister::Action::Rename ? "Rename" : "Create folder",
                      a == Lister::Action::Rename
                          ? Lister::Leaf(job.sources[0]).c_str()
                          : "New folder",
                      BMessenger(this), message))
          ->Show();
      return;
    }
    auto m = Encode(job, kConfirm);
    BString text;
    if (a == Lister::Action::Trash)
      text << "Move " << int32(job.sources.size())
           << " selected entries to this volume's Trash?\n\nYou can restore "
              "them from Trash.\n"
           << job.sources[0].c_str();
    else if (a == Lister::Action::Restore)
      text << "Restore " << int32(job.sources.size())
           << " selected entries to their original folders?\n\nExisting files "
              "will not be overwritten.";
    else if (a == Lister::Action::Delete)
      text << "Permanently delete " << int32(job.sources.size())
           << " selected entries?\n\nThis cannot be undone.\n"
           << job.sources[0].c_str();
    else
      text << (a == Lister::Action::Copy ? "Copy " : "Move ")
           << int32(job.sources.size()) << " selected entries from:\n"
           << panes[source]->Path() << "\n\nto:\n"
           << job.destination.c_str();
    auto alert = new BAlert("Confirm file operation", text, "Cancel",
                            a == Lister::Action::Trash     ? "Move to Trash"
                            : a == Lister::Action::Restore ? "Restore"
                            : a == Lister::Action::Delete  ? "Delete"
                            : a == Lister::Action::Copy    ? "Copy"
                                                           : "Move",
                            nullptr, B_WIDTH_AS_USUAL,
                            a == Lister::Action::Delete ? B_WARNING_ALERT
                                                        : B_INFO_ALERT);
    alert->SetShortcut(0, B_ESCAPE);
    alert->Go(new BInvoker(new BMessage(m), BMessenger(this)));
  }
  void Open(uint32 action) {
    auto selected = panes[source]->Selected();
    if (selected.size() != 1) {
      Notice("Select exactly one file.");
      return;
    }
    BMessage m(kOpenPaths);
    m.AddString("path", selected[0].c_str());
    m.AddString("mode", action == kView ? "view" : "edit");
    m.AddString("application", prefs.editor);
    service.SendMessage(&m);
  }

public:
  MainWindow(const Preferences &p, BMessenger svc)
      : BWindow(BRect(70, 70, 1170, 710), "Lister 0.2.0", B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS),
        prefs(p), service(svc) {
    auto menu = new BMenuBar("menu");
    auto files = new BMenu("Files");
    struct Item {
      const char *label;
      uint32 action;
      char key;
    };
    Item commands[] = {{"View [F3]", kView, 'V'},
                       {"Edit [F4]", kEdit, 'E'},
                       {"Copy [F5]", kCopy, 'C'},
                       {"Move [F6]", kMove, 'M'},
                       {"Rename", kRename, 'R'},
                       {"New folder [F7]", kMkdir, 'N'},
                       {"Move to Trash [F8]", kDelete, 'D'},
                       {"Restore from Trash", kRestore, 0},
                       {"Delete permanently...", kPermanentDelete, 0}};
    for (auto c : commands)
      files->AddItem(new BMenuItem(c.label, new BMessage(c.action), c.key));
    files->AddSeparatorItem();
    files->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
    menu->AddItem(files);
    auto select = new BMenu("Select");
    select->AddItem(new BMenuItem("All", new BMessage(kAll), 'A'));
    select->AddItem(
        new BMenuItem("None", new BMessage(kNone), 'A', B_SHIFT_KEY));
    select->AddItem(new BMenuItem("Invert", new BMessage(kInvert), 'I'));
    menu->AddItem(select);
    auto view = new BMenu("Lister");
    view->AddItem(
        new BMenuItem("Swap source / destination", new BMessage(kSwitch), 'S'));
    view->AddItem(new BMenuItem("Same folder in destination",
                                new BMessage(kSame), 'S', B_SHIFT_KEY));
    view->AddItem(new BMenuItem("Refresh", new BMessage(kRefresh), 'L'));
    view->AddItem(new BMenuItem("Terminal here", new BMessage(kTerminal), 'T'));
    view->AddItem(new BMenuItem("Operations", new BMessage(kShowJobs), 'O'));
    view->AddSeparatorItem();
    view->AddItem(
        new BMenuItem("Preferences [F9]", new BMessage(kPreferences), ','));
    menu->AddItem(view);
    panes[0] = new Pane(0, prefs);
    panes[1] = new Pane(1, prefs);
    auto split = new BSplitView(B_HORIZONTAL, 6);
    split->AddChild(panes[0]);
    split->AddChild(panes[1]);
    split->SetCollapsible(0, false);
    split->SetCollapsible(1, false);
    bank = new BGroupView(B_VERTICAL, 5);
    auto grid = new BGridView(5, 5);
    Item buttons[] = {{"View", kView, 0},         {"Edit", kEdit, 0},
                      {"Copy", kCopy, 0},         {"Move", kMove, 0},
                      {"Rename", kRename, 0},     {"Trash", kDelete, 0},
                      {"All", kAll, 0},           {"None", kNone, 0},
                      {"Invert", kInvert, 0},     {"New folder", kMkdir, 0},
                      {"Terminal", kTerminal, 0}, {"Operations", kShowJobs, 0}};
    for (int i = 0; i < 12; i++) {
      auto button =
          new BButton(buttons[i].label, new BMessage(buttons[i].action));
      button->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
      grid->GridLayout()->AddView(button, i % 6, i / 6);
    }
    bank->AddChild(grid);
    bank->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    command = new BTextControl("Command:", "", new BMessage(kCommand));
    command->SetToolTip(
        "Runs a shell command in the SOURCE directory using Terminal.");
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu)
        .Add(split)
        .AddGroup(B_VERTICAL, 5)
        .SetInsets(8, 4, 8, 8)
        .Add(bank)
        .Add(command)
        .End();
    queue = new OperationQueue(BMessenger(this));
    AddCommonFilter(new Keys());
    Apply();
    panes[0]->SetSource(true);
    panes[1]->SetSource(false);
    panes[0]->FocusList();
    ResizeTo(1140, 650);
  }
  status_t GetSupportedSuites(BMessage *m) override {
    static property_info properties[] = {{"SourcePath",
                                          {B_GET_PROPERTY, B_SET_PROPERTY, 0},
                                          {B_DIRECT_SPECIFIER, 0},
                                          "Source directory",
                                          0},
                                         {"DestinationPath",
                                          {B_GET_PROPERTY, B_SET_PROPERTY, 0},
                                          {B_DIRECT_SPECIFIER, 0},
                                          "Destination directory",
                                          0},
                                         {"PathReady",
                                          {B_GET_PROPERTY, 0},
                                          {B_DIRECT_SPECIFIER, 0},
                                          "Both directory scans complete",
                                          0},
                                         {"Busy",
                                          {B_GET_PROPERTY, 0},
                                          {B_DIRECT_SPECIFIER, 0},
                                          "Operations are queued or running",
                                          0},
                                         {nullptr}};
    BPropertyInfo info(properties);
    m->AddString("suites", "suite/vnd.Kambuzi-Lister");
    m->AddFlat("messages", &info);
    return BWindow::GetSupportedSuites(m);
  }
  BHandler *ResolveSpecifier(BMessage *msg, int32 index, BMessage *spec,
                             int32 what, const char *property) override {
    if (!strcmp(property, "SourcePath") ||
        !strcmp(property, "DestinationPath") || !strcmp(property, "Busy") ||
        !strcmp(property, "PathReady"))
      return this;
    return BWindow::ResolveSpecifier(msg, index, spec, what, property);
  }
  ~MainWindow() { delete queue; }
  bool QuitRequested() override {
    if (queue->Busy()) {
      queue->Show();
      Notice("File operations are still running. Let them finish, or use "
             "Cancel all before quitting.");
      return false;
    }
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
  }
  void MessageReceived(BMessage *m) override {
    if (m->what == B_GET_PROPERTY || m->what == B_SET_PROPERTY) {
      int32 index, what;
      BMessage spec;
      const char *property;
      if (m->GetCurrentSpecifier(&index, &spec, &what, &property) == B_OK) {
        BMessage reply(B_REPLY);
        if (!strcmp(property, "SourcePath") ||
            !strcmp(property, "DestinationPath")) {
          Pane *pane =
              panes[!strcmp(property, "SourcePath") ? source : 1 - source];
          if (m->what == B_SET_PROPERTY) {
            const char *value;
            if (m->FindString("data", &value) == B_OK)
              pane->Navigate(value);
            else {
              reply.AddInt32("error", B_BAD_VALUE);
              m->SendReply(&reply);
              return;
            }
          }
          reply.AddString("result", pane->Path());
        } else if (!strcmp(property, "PathReady"))
          reply.AddBool("result",
                        !panes[0]->IsScanning() && !panes[1]->IsScanning());
        else if (!strcmp(property, "Busy"))
          reply.AddBool("result", queue->Busy());
        else {
          BWindow::MessageReceived(m);
          return;
        }
        reply.AddInt32("error", B_OK);
        m->SendReply(&reply);
        return;
      }
    }
    switch (m->what) {
    case kActivate:
      source = m->GetInt32("pane", source);
      for (int i = 0; i < 2; i++)
        panes[i]->SetSource(i == source);
      break;
    case kSwitch:
      source = 1 - source;
      for (int i = 0; i < 2; i++)
        panes[i]->SetSource(i == source);
      panes[source]->FocusList();
      break;
    case kSame:
      panes[1 - source]->Navigate(panes[source]->Path());
      break;
    case kRefresh:
      for (auto p : panes)
        p->Reload();
      break;
    case kCopy:
      Operation(Lister::Action::Copy);
      break;
    case kMove:
      Operation(Lister::Action::Move);
      break;
    case kDelete:
      Operation(Lister::Action::Trash);
      break;
    case kRestore:
      Operation(Lister::Action::Restore);
      break;
    case kPermanentDelete:
      Operation(Lister::Action::Delete);
      break;
    case kRename:
      Operation(Lister::Action::Rename);
      break;
    case kMkdir:
      Operation(Lister::Action::MakeDirectory);
      break;
    case kConfirm:
      if (m->GetInt32("which", 0) == 1)
        queue->Enqueue(Decode(*m));
      break;
    case kNameAccepted: {
      auto j = Decode(*m);
      j.destination = Lister::Join(j.destination, m->GetString("name", ""));
      queue->Enqueue(j);
      break;
    }
    case kDrop: {
      Lister::Job j;
      j.destination = m->GetString("destination", "");
      if (j.destination == "@disks")
        break;
      entry_ref r;
      for (int i = 0; m->FindRef("refs", i, &r) == B_OK; i++) {
        BEntry e(&r);
        BPath p;
        if (e.GetPath(&p) == B_OK)
          j.sources.push_back(p.Path());
      }
      if (j.sources.empty())
        break;
      auto confirm = Encode(j, kConfirm);
      BString text("Copy dropped entries to:\n");
      text << j.destination.c_str();
      (new BAlert("Copy", text, "Cancel", "Copy"))
          ->Go(new BInvoker(new BMessage(confirm), BMessenger(this)));
      break;
    }
    case kJobDone:
      for (auto p : panes)
        p->Reload();
      break;
    case kShowJobs:
      queue->Show();
      break;
    case kAll:
      panes[source]->Select(1);
      break;
    case kNone:
      panes[source]->Select(0);
      break;
    case kInvert:
      panes[source]->Select(2);
      break;
    case kView:
    case kEdit:
      Open(m->what);
      break;
    case kOpenPaths:
      service.SendMessage(m);
      break;
    case kTerminal:
    case kCommand: {
      if (!strcmp(panes[source]->Path(), "@disks")) {
        Notice("Open a folder first.");
        break;
      }
      BMessage launch(kOpenPaths);
      launch.AddString("path", panes[source]->Path());
      launch.AddString("mode", m->what == kTerminal ? "terminal" : "command");
      launch.AddString("command", command->Text());
      launch.AddString("application", prefs.terminal);
      service.SendMessage(&launch);
      break;
    }
    case kPreferences:
      OpenPreferences(BMessenger(this), prefs);
      break;
    case kApplyPreferences: {
      prefs.Load(*m);
      Apply();
      BMessage save(prefs.Save());
      save.what = kSavePreferences;
      service.SendMessage(&save);
      break;
    }
    default:
      BWindow::MessageReceived(m);
      break;
    }
  }
};
class App : public BApplication {
  Services *services = nullptr;

public:
  App() : BApplication(kSignature) {}
  ~App() {
    if (services && services->Lock())
      services->Quit();
  }
  void ReadyToRun() override {
    Preferences prefs;
    BMessage settings;
    BFile file(SettingsPath(), B_READ_ONLY);
    if (settings.Unflatten(&file) == B_OK)
      prefs.Load(settings);
    services = new Services(BMessenger(this));
    services->Run();
    (new MainWindow(prefs, BMessenger(services)))->Show();
  }
  void MessageReceived(BMessage *m) override {
    if (m->what == kServiceError) {
      Notice(m->GetString("error", "Operation failed"));
      return;
    }
    if (m->what == kViewLoaded) {
      BString title = m->GetString("path", "");
      if (m->GetBool("truncated", false))
        title << " (first 1 MiB)";
      auto window = new BWindow(BRect(120, 100, 900, 650), title,
                                B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS);
      auto text = new BTextView("document");
      text->SetText(m->GetString("text", ""));
      text->MakeEditable(false);
      text->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
      text->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
      text->SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
      text->SetFontAndColor(be_fixed_font, B_FONT_ALL);
      auto scroll = new BScrollView("scroll", text, 0, false, true);
      scroll->SetExplicitMinSize(BSize(300, 200));
      BLayoutBuilder::Group<>(window, B_VERTICAL, 0).Add(scroll);
      window->Show();
      return;
    }
    BApplication::MessageReceived(m);
  }
};
int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "--self-test"))
    return Lister::SelfTest();
  App app;
  app.Run();
  return 0;
}
