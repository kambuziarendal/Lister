// SPDX-License-Identifier: MIT
#include "ArchivePasswordWindow.h"
#include "ArchivePasswords.h"
#include "Common.h"
#include "BulkRename.h"
#include "BulkRenameWindow.h"
#include "ClipboardTextFile.h"
#include "FileClipboard.h"
#include "FileDrop.h"
#include "FileContextMenu.h"
#include "NamedEntry.h"
#include "JobSnapshot.h"
#include "ArchiveWindow.h"
#include "ArchiveEditor.h"
#include "FolderCompare.h"
#include "MessageStore.h"
#include "PropertiesWindow.h"
#include "OpenWith.h"
#include "ExplicitSubmit.h"
#include "SavedPlaces.h"
#include "SavedSearches.h"
#include "SearchWindow.h"
#include "Shelf.h"
#include "ShelfWindow.h"
#include "ImagePreview.h"
#include "ImageConvert.h"
#include "InlineAction.h"
#include "OperationQueue.h"
#include "Pane.h"
#include "VirtualFile.h"
#include <Alert.h>
#include <Application.h>
#include <algorithm>
#include <exception>
#include <Button.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
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
#include "ThemeTextView.h"
#include <Window.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
constexpr uint32 kResumeArchiveEdits = 'ared';
constexpr uint32 kImageDropProbe = 'dimg', kImageDropOutputs = 'dfmt',
                 kImageDropConvert = 'dcvi';
constexpr uint32 kClipboardTextAccepted = 'ctac';
constexpr uint32 kNamedEntryAccepted = 'nacc', kDropAccepted = 'dacc',
                 kConfirm = 'cfir', kViewLoaded = 'vloa',
                 kServiceError = 'serr', kSavePreferences = 'save',
                 kNetwork = 'netw', kOpenSystem = 'osys',
                 kInlineNotice = 'inot', kInlineCancel = 'ican',
                 kArchiveAddChosen = 'aadc', kArchiveEditAccepted = 'aeda';
static void Notice(const char *text) {
  BMessage error(kServiceError);
  error.AddString("error", text);
  be_app->PostMessage(&error);
}
static std::string Quote(const std::string &s) {
  std::string out = "'";
  for (char c : s)
    out += c == '\'' ? "'\\''" : std::string(1, c);
  return out + "'";
}
static BString SettingsPath(const char *name = "Lister_settings") {
  BPath p;
  find_directory(B_USER_SETTINGS_DIRECTORY, &p);
  p.Append(name);
  return p.Path();
}
class Services : public BLooper {
  BMessenger app;

public:
  Services(BMessenger a) : BLooper("file viewer and launcher"), app(a) {}
  void MessageReceived(BMessage *m) override {
    if (m->what == kImageDropProbe) {
      BMessenger target;
      if (m->FindMessenger("replyTo", &target) != B_OK)
        return;
      BMessage reply(*m);
      reply.what = kImageDropOutputs;
      try {
        auto outputs = Listi::ImageOutputs(Listi::DecodeJob(*m));
        for (const auto &output : outputs) {
          BMessage choice;
          choice.AddInt32("type", output.type);
          choice.AddString("mime", output.mime.c_str());
          choice.AddString("name", output.name.c_str());
          choice.AddString("extension", output.extension.c_str());
          reply.AddMessage("output", &choice);
        }
        if (outputs.empty())
          reply.AddString(
              "error",
              "No installed translator can encode another image format.");
      } catch (const std::exception &e) {
        reply.AddString("error", e.what());
      }
      target.SendMessage(&reply, (BHandler *)nullptr, 500000);
      return;
    }
    if (m->what == kSaveSearches) {
      BMessage reply(kSearchesSaved);
      reply.AddInt64("revision", m->GetInt64("revision", -1));
      try {
        Listi::SavedSearches::FromMessage(*m).Save(
            SettingsPath("Lister_searches").String());
      } catch (const std::exception &e) {
        std::string error =
            "Could not save search criteria. The previous list is unchanged. ";
        error += e.what();
        reply.AddString("error", error.c_str());
      }
      app.SendMessage(&reply);
      return;
    }
    if (m->what == kSavePlaces) {
      BMessage reply(kPlacesSaved);
      reply.AddInt64("serial", m->GetInt64("serial", -1));
      try {
        Listi::SavedPlaces::FromMessage(*m).Save(
            SettingsPath("Lister_places").String());
      } catch (const std::exception &e) {
        std::string error =
            "Could not save places. The previous list is unchanged. ";
        error += e.what();
        reply.AddString("error", error.c_str());
      }
      app.SendMessage(&reply);
      return;
    }
    if (m->what == kSavePreferences) {
      try {
        Listi::SaveMessageStore(SettingsPath().String(), *m);
      } catch (const std::exception &e) {
        BMessage error(kServiceError);
        error.AddString(
            "error",
            (std::string("Could not save preferences: ") + e.what()).c_str());
        app.SendMessage(&error);
      }
      return;
    }
    if (m->what != kOpenPaths) {
      BLooper::MessageReceived(m);
      return;
    }
    try {
      BString path = m->GetString("path", ""),
              mode = m->GetString("mode", "open");
      BMessenger pane;
      m->FindMessenger("pane", &pane);
      if (mode == "open") {
        auto archive = Listi::OpenArchive(path.String());
        if (!archive.empty() && pane.IsValid()) {
          BMessage navigate(kNavigate);
          navigate.AddString("path", archive.c_str());
          navigate.AddInt64("serial", m->GetInt64("serial", -1));
          pane.SendMessage(&navigate);
          return;
        }
      }
      if (mode == "view") {
        std::string text;
        struct PreviewFull {};
        bool truncated = false;
        try {
          Listi::ReadFile(path.String(), [&](const char *p, size_t n) {
            size_t available = 1024 * 1024 - text.size();
            text.append(p, std::min(n, available));
            if (n > available)
              throw PreviewFull();
          });
        } catch (const PreviewFull &) {
          truncated = true;
        }
        if (text.find('\0') != std::string::npos)
          throw std::runtime_error(
              "Binary file. Open it with its associated application.");
        BMessage done(kViewLoaded);
        done.AddString("path", path);
        done.AddString("text", text.c_str());
        done.AddBool("truncated", truncated);
        app.SendMessage(&done);
        return;
      }
      if (Listi::IsVirtual(path.String()))
        throw std::runtime_error(
            "Copy this entry locally before editing or opening it with an application. Remote edits are not uploaded automatically.");
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
        const char *args[] = {"-t", "Listi", "/bin/bash", "-c", script.c_str()};
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
    } catch (const Listi::Cancelled &) {
      return;
    } catch (const std::exception &e) {
      BMessage error(kServiceError);
      error.AddString("error", e.what());
      app.SendMessage(&error);
    }
  }
};
class Keys : public BMessageFilter {
public:
  Keys() : BMessageFilter(B_KEY_DOWN) {}
  filter_result Filter(BMessage *m, BHandler **target) override {
    const char *bytes = m->GetString("bytes", "");
    auto window = dynamic_cast<BWindow *>(Looper());
    auto text =
        window ? dynamic_cast<BTextView *>(window->CurrentFocus()) : nullptr;
    // Window menu accelerators otherwise claim Command+A/C/V before the text
    // control can select/copy/paste. Route editing commands to the focused
    // view, not to the file panel. Explicit menu and button clicks still act on
    // files.
    if (bytes[0] == B_ESCAPE) {
      const char *name =
          text && text->Parent() ? text->Parent()->Name() : nullptr;
      if (name && !strcmp(name, "filter")) {
        auto parent = text->Parent();
        while (parent && !dynamic_cast<Pane *>(parent))
          parent = parent->Parent();
        if (parent) {
          Looper()->PostMessage(kClearFilter, parent);
          return B_SKIP_MESSAGE;
        }
      }
      Looper()->PostMessage(kInlineCancel);
      return B_SKIP_MESSAGE;
    }
    if (text) {
      if (m->GetInt32("modifiers", 0) & B_COMMAND_KEY) {
        int32 key = m->GetInt32("raw_char", bytes[0]);
        uint32 edit = 0;
        switch (key) {
        case 'a':
        case 'A':
          edit = B_SELECT_ALL;
          break;
        case 'c':
        case 'C':
          edit = B_COPY;
          break;
        case 'v':
        case 'V':
          edit = B_PASTE;
          break;
        case 'x':
        case 'X':
          edit = B_CUT;
          break;
        }
        if (edit) {
          Looper()->PostMessage(edit, text);
          return B_SKIP_MESSAGE;
        }
        // Do not invoke file actions while editing a path, filter or command.
        // Command+F remains available to enter the active pane's name filter.
        if (key != 0 && strchr("eEmMrRnNdDiIsSlLtToO", key))
          return B_SKIP_MESSAGE;
        if ((key == 'y' || key == 'Y') &&
            (m->GetInt32("modifiers", 0) & B_SHIFT_KEY))
          return B_SKIP_MESSAGE;
      }
      if (bytes[0] == B_FUNCTION_KEY)
        return B_DISPATCH_MESSAGE;
    }
    // Browsing-only shortcuts: text fields and inline questions retain their
    // own editing keys. Resolve the actual focused pane, not a queued role.
    int32 modifiers = m->GetInt32("modifiers", 0);
    int32 raw = m->GetInt32("raw_char", bytes[0]);
    if (!text &&
        (modifiers & (B_COMMAND_KEY | B_CONTROL_KEY | B_OPTION_KEY |
                      B_SHIFT_KEY)) == B_COMMAND_KEY &&
        (raw == '[' || raw == ']')) {
      auto view = window ? window->CurrentFocus() : nullptr;
      while (view && !dynamic_cast<Pane *>(view))
        view = view->Parent();
      if (view) {
        Looper()->PostMessage(raw == '[' ? kBack : kForward, view);
        return B_SKIP_MESSAGE;
      }
    }
    if (!text && (modifiers & B_COMMAND_KEY) && !(modifiers & B_SHIFT_KEY) &&
        strchr("cCxXvV", raw) && raw != 0) {
      auto view = window ? window->CurrentFocus() : nullptr;
      while (view && !dynamic_cast<Pane *>(view))
        view = view->Parent();
      if (!view)
        return B_SKIP_MESSAGE;
    }
    if (bytes[0] == B_TAB) {
      auto view = dynamic_cast<BView *>(*target);
      while (view) {
        if (view->Name() && !strcmp(view->Name(), "files")) {
          Looper()->PostMessage(kSwitch);
          return B_SKIP_MESSAGE;
        }
        view = view->Parent();
      }
    }
    // Function keys are deliberately unassigned.
    return B_DISPATCH_MESSAGE;
  }
};
class MainWindow : public BWindow {
  Preferences prefs;
  BMessenger searchWindow;
  BMessenger bulkRenameWindow;
  std::shared_ptr<Listi::Shelf> shelf = std::make_shared<Listi::Shelf>();
  BMessenger shelfWindow;
  void RefreshShelf() {
    if (shelfWindow.IsValid()) {
      BMessage refresh(Listi::kRefreshShelf);
      shelfWindow.SendMessage(&refresh, (BHandler *)nullptr, 0);
    }
  }
  void AddToShelf() {
    try {
      const auto added = shelf->Add(panes[source]->Selected());
      BString status;
      if (added)
        status << int32(added)
               << " added to Shelf. Open it from the Listi menu.";
      else
        status = "Already on the Shelf.";
      panes[source]->ShowStatus(status);
      RefreshShelf();
    } catch (const std::exception &e) {
      Notice(e.what());
    }
  }
  Listi::SavedSearches searches, pendingSearches;
  bool searchesWritable = true, searchesSaving = false;
  int64 searchesRevision = 0;
  std::string pendingSearchName;
  BMessage SearchStore() const {
    auto message = searches.Message();
    message.what = kSearchStore;
    message.AddInt64("revision", searchesRevision);
    message.AddBool("writable", searchesWritable);
    message.AddBool("saving", searchesSaving);
    return message;
  }
  Listi::SavedPlaces places, pendingPlaces;
  bool placesWritable = true, placesSaving = false;
  int64 placesSerial = 0;
  void PublishPlaces() {
    for (auto pane : panes)
      pane->SetPlaces(places, placesWritable && !placesSaving);
  }
  Pane *panes[2];
  ImagePreview *preview;
  BMenuItem *previewMenu, *einnMenu, *askrMenu, *tviskiptMenu, *smidjaMenu;
  BMenuBar *menu;
  BMenu *fileView;
  BMenuItem *saveClipboardTextItem;
  BSplitView *split;
  BGroupView *footer;
  bool footerVisible = true;
  float splitterSize = 0;
  bool paneVisible[2] = {true, true};
  bool previewVisible = true;
  void Save() {
    auto saved = prefs.Save();
    saved.what = kSavePreferences;
    service.SendMessage(&saved);
  }
  void UpdatePreview() {
    if (!prefs.Preview() || prefs.SinglePane()) {
      preview->ShowFile("");
      return;
    }
    auto selected = panes[source]->Selected();
    preview->ShowFile(selected.size() == 1 ? selected[0] : "");
  }
  InlineAction *actionArea;
  void CloseAction() {
    actionArea->Close();
    pendingNamedEntry = {};
    pendingClipboardText.reset();
    pendingArchiveEdit = {};
    if (archiveClipboardPending) {
      try {
        Listi::FinishFilePaste(clipboardPaste, {});
      } catch (const std::exception &e) {
        Notice(e.what());
      }
      clipboardPaste = {};
      archiveClipboardPending = false;
    }
    ShowBank(prefs.buttons);
    panes[source]->FocusList();
  }
  void AskInline(const char *text, const char *button, const BMessage &result,
                 const char *initial = nullptr) {
    if (!actionArea->Pending())
      ShowBank(false);
    if (!actionArea->Ask(text, button, result, initial))
      queue->ErrorInfo("Finish or cancel the open question first.");
  }
  int source = 0;
  int64 clipboardSerial = 0;
  Listi::Job pendingNamedEntry;
  int64 dropSerial = 0;
  Listi::Job pendingArchiveEdit;
  std::unique_ptr<BFilePanel> archiveAddPicker;
  std::string archiveAddTarget, archiveAddFolder;
  BMessenger archiveWindow;
  void ArchiveAction(bool extract) {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    if (archiveWindow.IsValid()) {
      archiveWindow.SendMessage('arav');
      return;
    }
    try {
      auto job = Listi::PrepareArchiveJob(extract, panes[source]->Selected());
      std::string folder =
          panes[prefs.SinglePane() ? source : 1 - source]->Path();
      if (folder == "@disks" || Listi::IsVirtual(folder)) {
        auto loc = Listi::ParseLocation(job.sources[0]);
        folder =
            Listi::ParentLocation(loc.Archive() ? loc.archive : job.sources[0]);
      }
      archiveWindow = OpenArchiveWindow(job, folder, BMessenger(this));
    } catch (const std::exception &e) {
      Notice(e.what());
    }
  }
  void ConfirmArchiveAddition(const Listi::Job &job) {
    pendingArchiveEdit = job;
    BString text;
    if (job.action == Listi::Action::MoveArchive) {
      text << "Move " << int32(job.sources.size()) << " selected entries to:\n"
           << job.destination.c_str();
      const bool sameArchive =
          !job.sources.empty() &&
          Listi::ParseLocation(job.sources.front()).archive ==
              Listi::ParseLocation(job.destination).archive;
      text
          << (sameArchive
                  ? "\nEntries move within this archive. Existing names are never replaced."
                  : "\nSource entries are removed only after the saved destination is verified."
                    " If interrupted after copying, both copies may remain. Existing names require a choice.");
      if (!job.sources.empty() &&
          Listi::ParseLocation(job.sources.front()).Local())
        text
            << "\nFiles with links or filesystem attributes must be copied instead, so the original metadata is retained.";
      AskInline(text.String(), "Move entries", BMessage(kArchiveEditAccepted));
      return;
    }
    if (!job.archiveRenames.empty())
      text << "Move selected entries inside this archive:\n";
    else
      text << "Add " << int32(job.sources.size()) << " selected entries to:\n";
    text << job.destination.c_str();
    if (!job.archivePath.empty())
      text << "\nInside: /" << job.archivePath.c_str();
    text
        << (job.archiveRenames.empty()
                ? "\nOriginal files are kept. Existing names require a Replace or Skip choice."
                : "\nEntries move within this archive. Existing names are never replaced.");
    AskInline(text.String(), "Update archive", BMessage(kArchiveEditAccepted));
  }
  void AddToArchive() {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    try {
      const auto folder = panes[source]->Path();
      auto location = Listi::ParseLocation(folder);
      if (location.Archive()) {
        archiveAddTarget = location.archive;
        archiveAddFolder = location.path;
      } else {
        const auto selected = panes[source]->Selected();
        if (selected.size() != 1 || Listi::IsVirtual(selected[0]))
          throw std::runtime_error(
              "Select one local writable archive, or open a folder inside it.");
        archiveAddTarget = selected[0];
        archiveAddFolder = "/";
      }
      struct stat st;
      if (lstat(archiveAddTarget.c_str(), &st) || !S_ISREG(st.st_mode))
        throw std::runtime_error("Choose one local writable archive.");
      pendingArchiveEdit =
          Listi::PrepareArchiveEditTarget(archiveAddTarget, archiveAddFolder);
    } catch (const std::exception &e) {
      Notice(e.what());
      return;
    }
    BMessenger here(this);
    BMessage chosen(kArchiveAddChosen);
    archiveAddPicker.reset(new BFilePanel(
        B_OPEN_PANEL, &here, nullptr,
        node_flavor(B_FILE_NODE | B_DIRECTORY_NODE), true, &chosen));
    archiveAddPicker->SetPanelDirectory(
        Listi::ParentLocation(archiveAddTarget).c_str());
    archiveAddPicker->Show();
  }
  void RemoveFromArchive() {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    try {
      const auto selected = panes[source]->Selected();
      if (selected.empty())
        throw std::runtime_error("Select entries inside one writable archive.");
      auto location = Listi::ParseLocation(selected[0]);
      if (!location.Archive())
        throw std::runtime_error(
            "Open an archive and select the entries to remove.");
      pendingArchiveEdit = Listi::PrepareArchiveEditJob(
          location.archive, {}, selected, location.path);
      BMessage accepted(kArchiveEditAccepted);
      BString text;
      text
          << "Remove " << int32(selected.size())
          << " selected entries from this archive?\n\n"
          << location.archive.c_str()
          << "\n\nThe archive is rebuilt safely. The original remains unchanged if the job fails.";
      AskInline(text.String(), "Remove from archive", accepted);
    } catch (const std::exception &e) {
      Notice(e.what());
      pendingArchiveEdit = {};
    }
  }
  void NamedEntry(Listi::Action action) {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    try {
      auto job = Listi::PrepareNamedEntry(
          action, panes[source]->Selected(),
          panes[prefs.SinglePane() ? source : 1 - source]->Path());
      const bool duplicate = action == Listi::Action::Duplicate;
      std::string text = std::string(duplicate ? "Duplicate: " : "Link to: ") +
                         job.sources[0] + "\nIn: " + job.destination;
      if (!duplicate)
        text +=
            "\nRelative: path from this folder. Absolute: full target path.";
      BMessage accepted(kNamedEntryAccepted);
      accepted.AddString("newEntryIn", job.destination.c_str());
      ShowBank(false);
      if (actionArea->Ask(
              text.c_str(), duplicate ? "Duplicate" : "Relative link", accepted,
              job.entryName.c_str(), duplicate ? nullptr : "Absolute link"))
        pendingNamedEntry = job;
    } catch (const std::exception &e) {
      Notice(e.what());
    }
  }
  std::optional<Listi::ClipboardTextFile> pendingClipboardText;
  void AskToSaveClipboardText() {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    try {
      auto file = Listi::PrepareClipboardTextFile(panes[source]->Path());
      const auto prompt = "Save clipboard text in: " + file.folder;
      BMessage accepted(kClipboardTextAccepted);
      accepted.AddString("newEntryIn", file.folder.c_str());
      ShowBank(false);
      if (actionArea->Ask(prompt.c_str(), "Save", accepted,
                          file.suggestedName.c_str()))
        pendingClipboardText = std::move(file);
    } catch (const std::exception &e) {
      Notice(e.what());
    }
  }
  void SaveClipboardText(const std::string &name) {
    if (!pendingClipboardText)
      return;
    auto file = std::move(*pendingClipboardText);
    pendingClipboardText.reset();
    try {
      Listi::SaveClipboardTextFile(file, name);
      const auto status = "Saved " + name;
      for (auto pane : panes) {
        if (pane->Path() == file.folder) {
          pane->Reload();
          pane->ShowStatus(status.c_str());
        }
      }
    } catch (const std::exception &e) {
      Notice(e.what());
    }
  }
  Listi::ClipboardPaste clipboardPaste;
  bool archiveClipboardPending = false;
  std::set<int64> clipboardJobs;
  std::vector<std::string> clipboardMoved;
  void ClipboardAction(uint32 action) {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    try {
      if (action != kClipboardPaste) {
        auto selected = panes[source]->Selected();
        Listi::PutFileClipboard(selected, action == kClipboardCut);
        BString status;
        status
            << int32(selected.size())
            << (action == kClipboardCut
                    ? " marked for moving. Paste in a destination folder."
                    : " copied to clipboard. Paste in a destination folder.");
        panes[source]->ShowStatus(status);
        return;
      }
      if (!clipboardJobs.empty()) {
        queue->ErrorInfo(
            "A clipboard transfer is still running. Let it finish before pasting again.");
        return;
      }
      clipboardPaste = Listi::PrepareFilePaste(panes[source]->Path());
      if (clipboardPaste.jobs.size() == 1 &&
          (clipboardPaste.jobs[0].action == Listi::Action::EditArchive ||
           clipboardPaste.jobs[0].action == Listi::Action::MoveArchive)) {
        archiveClipboardPending = !clipboardPaste.lease.empty();
        ConfirmArchiveAddition(clipboardPaste.jobs[0]);
        if (!archiveClipboardPending)
          clipboardPaste = {};
        return;
      }
      clipboardMoved.clear();
      for (auto job : clipboardPaste.jobs) {
        job.tag = ++clipboardSerial;
        clipboardJobs.insert(job.tag);
        queue->Enqueue(job);
      }
    } catch (const std::exception &e) {
      Notice(e.what());
    }
  }
  OperationQueue *queue;
  BGroupView *bank;
  BTextControl *command;
  BMessenger service;
  bool bankVisible = true, commandVisible = true;
  void ShowBank(bool visible) {
    visible = visible && !prefs.SinglePane();
    if (bankVisible == visible)
      return;
    bankVisible = visible;
    if (visible)
      bank->Show();
    else
      bank->Hide();
  }
  void ApplyWorkspace() {
    for (int i = 0; i < 2; ++i) {
      const bool visible = !prefs.SinglePane() || i == source;
      if (visible != paneVisible[i]) {
        paneVisible[i] = visible;
        if (visible)
          panes[i]->Show();
        else
          panes[i]->Hide();
      }
      panes[i]->SetWorkspace(prefs.workspace);
      panes[i]->SetSource(i == source);
    }
    const bool showPreview = prefs.Preview() && !prefs.SinglePane();
    if (showPreview != previewVisible) {
      previewVisible = showPreview;
      if (showPreview)
        preview->Show();
      else
        preview->Hide();
    }
    previewMenu->SetMarked(showPreview);
    previewMenu->SetEnabled(!prefs.SinglePane());
    split->SetSpacing(prefs.SinglePane() ? 0 : 6);
    split->SetSplitterSize(prefs.SinglePane() ? 0 : splitterSize);
    if (footerVisible == prefs.SinglePane()) {
      footerVisible = !prefs.SinglePane();
      if (footerVisible)
        footer->Show();
      else
        footer->Hide();
    }
    for (auto action : {kCopy, kMove, kSame, kSwitch})
      if (auto item = menu->FindItem(action))
        item->SetEnabled(!prefs.SinglePane());
    if (auto item = menu->FindItem(kFilter))
      item->SetEnabled(!prefs.IsEinn());
    queue->SetCompact(prefs.SinglePane());
    einnMenu->SetMarked(prefs.IsEinn());
    askrMenu->SetMarked(prefs.workspace == Workspace::askr);
    tviskiptMenu->SetMarked(prefs.workspace == Workspace::tviskipt);
    smidjaMenu->SetMarked(prefs.IsSmidja());
    ShowBank(prefs.buttons && !actionArea->Pending());
    const bool showCommand = prefs.command && !prefs.SinglePane();
    if (commandVisible != showCommand) {
      commandVisible = showCommand;
      if (showCommand)
        command->Show();
      else
        command->Hide();
    }
    UpdatePreview();
  }
  void SetWorkspace(Workspace value) {
    if (prefs.workspace == value)
      return;
    prefs.workspace = value;
    prefs.singlePane = source;
    ApplyWorkspace();
    if (!actionArea->Pending())
      panes[source]->FocusList();
    Save();
  }
  void Apply() {
    for (auto p : panes) {
      p->Apply(prefs);
      p->Reload();
    }
    ApplyWorkspace();
  }
  Listi::Job Snapshot(Listi::Action a) {
    Listi::Job j;
    j.action = a;
    j.sources = panes[source]->Selected();
    j.destination = panes[1 - source]->Path();
    return j;
  }
  void BulkRename() {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    if (bulkRenameWindow.IsValid() &&
        bulkRenameWindow.SendMessage(kBulkRename) == B_OK)
      return;
    try {
      auto job = Listi::FreezeBulkRename(panes[source]->Path(),
                                         panes[source]->Selected());
      bulkRenameWindow =
          Listi::OpenBulkRenameWindow(std::move(job), BMessenger(this));
    } catch (const std::exception &e) {
      Notice(e.what());
    }
  }
  void Operation(Listi::Action a) {
    if (actionArea->Pending()) {
      queue->ErrorInfo("Finish or cancel the open question first.");
      return;
    }
    if (prefs.SinglePane() &&
        (a == Listi::Action::Copy || a == Listi::Action::Move)) {
      Notice(
          "Use Copy or Cut, then navigate and Paste. Panel transfers need Tvískipt or Smiðja.");
      return;
    }
    auto job = Snapshot(a);
    if ((a == Listi::Action::Trash || a == Listi::Action::Delete) &&
        !job.sources.empty() &&
        Listi::ParseLocation(job.sources.front()).Archive()) {
      RemoveFromArchive();
      return;
    }
    if (a != Listi::Action::MakeDirectory && job.sources.empty()) {
      Notice("Select one or more files in the active panel first.");
      return;
    }
    if ((a == Listi::Action::Copy || a == Listi::Action::Move) &&
        job.destination == "@disks") {
      Notice("Open a destination folder first.");
      return;
    }
    if (a == Listi::Action::Rename || a == Listi::Action::MakeDirectory) {
      if (a == Listi::Action::Rename && job.sources.size() > 1) {
        BulkRename();
        return;
      }
      job.destination = panes[source]->Path();
      if (a == Listi::Action::MakeDirectory)
        job.sources.clear();
      if (job.destination == "@disks") {
        Notice("Open a folder first.");
        return;
      }
      try {
        Listi::FreezeLocalJob(job, job.destination);
        Listi::FreezeArchiveTargets(job);
        if (Listi::ParseLocation(job.destination).Archive())
          Listi::PrepareArchiveEditTarget(
              Listi::ParseLocation(job.destination).archive);
      } catch (const std::exception &e) {
        Notice(e.what());
        return;
      }
      auto message = Listi::EncodeJob(job, kNameAccepted);
      AskInline(
          a == Listi::Action::Rename ? "Rename selected entry"
                                     : "Create a folder in the active panel",
          a == Listi::Action::Rename ? "Rename" : "Create folder", message,
          a == Listi::Action::Rename
              ? Listi::LocationLeaf(job.sources[0]).c_str()
              : "New folder");
      return;
    }
    try {
      Listi::FreezeLocalJob(
          job, (a == Listi::Action::Copy || a == Listi::Action::Move)
                   ? job.destination
                   : "");
      Listi::FreezeArchiveTargets(job);
    } catch (const std::exception &e) {
      Notice(e.what());
      return;
    }
    if (a == Listi::Action::Copy || a == Listi::Action::Move) {
      if (Listi::ParseLocation(job.destination).Archive()) {
        try {
          ConfirmArchiveAddition(a == Listi::Action::Move
                                     ? Listi::PrepareArchiveMutation(job)
                                     : Listi::PrepareArchiveCopy(job));
        } catch (const std::exception &e) {
          Notice(e.what());
        }
        return;
      }
      if (a == Listi::Action::Move && !job.sources.empty() &&
          Listi::ParseLocation(job.sources.front()).Archive()) {
        try {
          ConfirmArchiveAddition(Listi::PrepareArchiveMove(job));
        } catch (const std::exception &e) {
          Notice(e.what());
        }
        return;
      }
      queue->Enqueue(job);
      return;
    }
    auto m = Listi::EncodeJob(job, kConfirm);
    BString text;
    if (a == Listi::Action::Trash)
      text << "Move " << int32(job.sources.size())
           << " selected entries to this volume's Trash?\n\nYou can restore "
              "them from Trash.\n"
           << job.sources[0].c_str();
    else if (a == Listi::Action::Restore)
      text << "Restore " << int32(job.sources.size())
           << " selected entries to their original folders?\n\nExisting files "
              "will not be overwritten.";
    else if (a == Listi::Action::Delete)
      text << "Permanently delete " << int32(job.sources.size())
           << " selected entries?\n\nThis cannot be undone.\n"
           << job.sources[0].c_str();
    else
      text << (a == Listi::Action::Copy ? "Copy " : "Move ")
           << int32(job.sources.size()) << " selected entries from:\n"
           << panes[source]->Path() << "\n\nto:\n"
           << job.destination.c_str();
    AskInline(text,
              a == Listi::Action::Trash     ? "Move to Trash"
              : a == Listi::Action::Restore ? "Restore"
                                            : "Delete permanently",
              m);
  }
  void Open(uint32 action) {
    auto selected = panes[source]->Selected();
    if (selected.size() != 1) {
      Notice("Select exactly one file.");
      return;
    }
    if (action == kEdit && Listi::ParseLocation(selected[0]).Archive()) {
      OpenArchiveEditor(selected[0], prefs.editor.String());
      return;
    }
    BMessage m(kOpenPaths);
    m.AddString("path", selected[0].c_str());
    m.AddString("mode", action == kView ? "view" : "edit");
    m.AddString("application", prefs.editor);
    service.SendMessage(&m);
  }

public:
  MainWindow(const Preferences &p, BMessenger svc,
             const Listi::SavedPlaces &saved, bool writable,
             const Listi::SavedSearches &queries, bool queriesWritable)
      : BWindow(BRect(70, 70, 1170, 710), "Listi 0.31.1", B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS),
        prefs(p), searches(queries), searchesWritable(queriesWritable),
        places(saved), placesWritable(writable), service(svc) {
    source = prefs.SinglePane() ? prefs.singlePane : 0;
    menu = new BMenuBar("menu");
    auto files = new BMenu("Files");
    struct Item {
      const char *label;
      uint32 action;
      char key;
    };
    auto clipboard = new BMenu("Clipboard");
    clipboard->AddItem(
        new BMenuItem("Copy", new BMessage(kClipboardCopy), 'C'));
    clipboard->AddItem(new BMenuItem("Cut", new BMessage(kClipboardCut), 'X'));
    clipboard->AddItem(new BMenuItem("Paste into active folder",
                                     new BMessage(kClipboardPaste), 'V'));
    clipboard->AddSeparatorItem();
    saveClipboardTextItem = new BMenuItem("Save clipboard text as file...",
                                          new BMessage(kClipboardSaveText));
    clipboard->AddItem(saveClipboardTextItem);
    files->AddItem(clipboard);
    files->AddSeparatorItem();
    Item commands[] = {{"View", kView, 0},
                       {"Edit", kEdit, 'E'},
                       {"Copy to other panel", kCopy, 0},
                       {"Move to other panel", kMove, 'M'},
                       {"Rename", kRename, 'R'},
                       {"Bulk rename...", kBulkRename, 0},
                       {"New folder", kMkdir, 'N'},
                       {"Move to Trash", kDelete, 'D'},
                       {"Restore from Trash", kRestore, 0},
                       {"Delete permanently...", kPermanentDelete, 0}};
    for (auto c : commands) {
      if (c.action == kCopy || c.action == kView)
        files->AddItem(new BMenuItem(c.label, new BMessage(c.action),
                                     c.action == kCopy ? 'C' : 'V',
                                     B_SHIFT_KEY));
      else
        files->AddItem(new BMenuItem(c.label, new BMessage(c.action), c.key));
    }
    files->AddItem(new BMenuItem("Open with...", new BMessage(kOpenWith), 'O',
                                 B_SHIFT_KEY));
    files->AddItem(new BMenuItem("Open with system application",
                                 new BMessage(kOpenSystem)));
    files->AddItem(new BMenuItem(
        "Add to Shelf", new BMessage(Listi::kAddToShelf), 'Y', B_SHIFT_KEY));
    auto more = new BMenu("More file actions");
    more->AddItem(new BMenuItem("Duplicate...", new BMessage(kDuplicate), 'D',
                                B_SHIFT_KEY));
    more->AddItem(new BMenuItem("Create symbolic link...",
                                new BMessage(kCreateLink), 'L', B_SHIFT_KEY));
    files->AddItem(more);
    auto archives = new BMenu("Archives");
    archives->AddItem(new BMenuItem(
        "Create archive...", new BMessage(kCreateArchive), 'P', B_SHIFT_KEY));
    archives->AddItem(new BMenuItem("Extract to folder...",
                                    new BMessage(kExtractArchive), 'E',
                                    B_SHIFT_KEY));
    archives->AddItem(new BMenuItem("Resume archive edits...",
                                    new BMessage(kResumeArchiveEdits)));
    archives->AddItem(
        new BMenuItem("Add or replace files...", new BMessage(kAddArchive)));
    archives->AddItem(new BMenuItem("Remove selected entries...",
                                    new BMessage(kRemoveArchive)));
    archives->AddSeparatorItem();
    archives->AddItem(
        new BMenuItem("Forget archive passwords", new BMessage('apfg')));
    files->AddItem(archives);
    files->AddItem(new BMenuItem("Properties...", new BMessage(kProperties),
                                 'I', B_SHIFT_KEY));
    files->AddSeparatorItem();
    files->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
    menu->AddItem(files);
    auto select = new BMenu("Select");
    select->AddItem(new BMenuItem("All", new BMessage(kAll), 'A'));
    select->AddItem(
        new BMenuItem("None", new BMessage(kNone), 'A', B_SHIFT_KEY));
    select->AddItem(new BMenuItem("Invert", new BMessage(kInvert), 'I'));
    select->AddItem(new BMenuItem("Same file type", new BMessage(kSelectType)));
    menu->AddItem(select);
    auto viewMenu = new BMenu("View");
    auto workspace = new BMenu("Workspace");
    auto einnMessage = new BMessage(kWorkspace);
    einnMessage->AddInt32("workspace", int32(Workspace::einn));
    auto askrMessage = new BMessage(kWorkspace);
    askrMessage->AddInt32("workspace", int32(Workspace::askr));
    auto smidjaMessage = new BMessage(kWorkspace);
    smidjaMessage->AddInt32("workspace", int32(Workspace::smidja));
    askrMenu = new BMenuItem(WorkspaceMenuLabel(Workspace::askr), askrMessage);
    einnMenu = new BMenuItem(WorkspaceMenuLabel(Workspace::einn), einnMessage);
    tviskiptMenu = new BMenuItem(WorkspaceMenuLabel(Workspace::tviskipt),
                                 new BMessage(kWorkspace));
    smidjaMenu =
        new BMenuItem(WorkspaceMenuLabel(Workspace::smidja), smidjaMessage);
    workspace->AddItem(einnMenu);
    workspace->AddItem(askrMenu);
    workspace->AddItem(tviskiptMenu);
    workspace->AddItem(smidjaMenu);
    viewMenu->AddItem(workspace);
    fileView = new BMenu("File view");
    for (int32 i = 0; i < ViewModeCount; ++i) {
      auto choice = new BMessage(kGridMode);
      choice->AddInt32("mode", i);
      fileView->AddItem(new BMenuItem(ViewModeLabel(i), choice));
    }
    auto resetView = new BMessage(kGridMode);
    resetView->AddInt32("mode", -1);
    fileView->AddSeparatorItem();
    fileView->AddItem(new BMenuItem("Use workspace default", resetView));
    viewMenu->AddItem(fileView);
    menu->AddItem(viewMenu);
    auto view = new BMenu("Listi");
    view->AddItem(new BMenuItem("Switch panel", new BMessage(kSwitch), 'S'));
    view->AddItem(new BMenuItem("Same folder in other panel",
                                new BMessage(kSame), 'S', B_SHIFT_KEY));
    previewMenu =
        new BMenuItem("Image preview panel", new BMessage(kPreviewToggle), 'P');
    view->AddItem(previewMenu);
    view->AddItem(
        new BMenuItem("Connect to server...", new BMessage(kNetwork), 'K'));
    view->AddItem(new BMenuItem("Refresh", new BMessage(kRefresh), 'L'));
    view->AddItem(new BMenuItem("Filter names", new BMessage(kFilter), 'F'));
    view->AddItem(new BMenuItem("Find files...", new BMessage(kSearch), 'F',
                                B_SHIFT_KEY));
    view->AddItem(
        new BMenuItem("Compare folders...", new BMessage(kCompareFolders)));
    view->AddItem(new BMenuItem("Terminal here", new BMessage(kTerminal), 'T'));
    view->AddItem(new BMenuItem("Operations", new BMessage(kShowJobs), 'O'));
    view->AddItem(new BMenuItem("Show Shelf", new BMessage(Listi::kShowShelf)));
    view->AddSeparatorItem();
    view->AddItem(new BMenuItem("Settings…", new BMessage(kPreferences), ','));
    menu->AddItem(view);
    panes[0] = new Pane(0, prefs);
    panes[1] = new Pane(1, prefs);
    split = new BSplitView(B_HORIZONTAL, 6);
    splitterSize = split->SplitterSize();
    split->AddChild(panes[0]);
    split->AddChild(panes[1]);
    preview = new ImagePreview();
    split->AddChild(preview);
    split->SetItemWeight(0, 0.39, false);
    split->SetItemWeight(1, 0.39, false);
    split->SetItemWeight(2, 0.22, true);
    split->SetCollapsible(0, false);
    split->SetCollapsible(1, false);
    bank = new BGroupView(B_VERTICAL, 5);
    auto grid = new BGridView(5, 5);
    Item buttons[] = {
        {"View [Shift+V]", kView, 'V'},   {"Edit [E]", kEdit, 'E'},
        {"Copy [Shift+C]", kCopy, 'C'},   {"Move [M]", kMove, 'M'},
        {"Rename [R]", kRename, 'R'},     {"Trash [D]", kDelete, 'D'},
        {"All [A]", kAll, 'A'},           {"None", kNone, 0},
        {"Invert [I]", kInvert, 'I'},     {"New folder [N]", kMkdir, 'N'},
        {"Terminal [T]", kTerminal, 'T'}, {"Operations [O]", kShowJobs, 'O'}};
    for (int i = 0; i < 12; i++) {
      auto button =
          new BButton(buttons[i].label, new BMessage(buttons[i].action));
      BString hint("Command+");
      if (buttons[i].action == kCopy || buttons[i].action == kView)
        hint << "Shift+" << buttons[i].key;
      else if (buttons[i].key)
        hint << buttons[i].key;
      else
        hint << "Shift+A";
      hint << " (normally Alt). File shortcuts apply outside text fields.";
      button->SetToolTip(hint);
      button->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
      grid->GridLayout()->AddView(button, i % 6, i / 6);
    }
    bank->AddChild(grid);
    bank->AddChild(new BStringView(
        "shortcuts",
        "Command + letter (normally Alt). Tab: other panel. Command+F: filter. "
        "C/X/V: clipboard. Shift+C: panel copy. Escape: close dialog."));
    bank->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    command = new BTextControl("Command:", "", nullptr);
    command->TextView()->AddFilter(
        new Listi::ExplicitSubmit(BMessenger(this), BMessage(kCommand)));
    command->SetToolTip(
        "Runs a shell command in the active directory using Terminal.");
    queue = new OperationQueue(BMessenger(this));
    actionArea = new InlineAction(BMessenger(this));
    actionArea->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
    actionArea->GroupLayout()->SetInsets(8, 6, 8, 6);
    actionArea->Hide();
    footer = new BGroupView(B_VERTICAL, 5);
    footer->SetName("command footer");
    footer->GroupLayout()->SetInsets(8, 4, 8, 8);
    footer->AddChild(bank);
    footer->AddChild(command);
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu)
        .Add(split)
        .Add(actionArea, 0)
        .Add(queue->View(), 0)
        .Add(footer, 0);
    // Child visibility must not impose a transient maximum on the window.
    // Apply after attachment: Haiku replaces layout explicit-data proxies.
    GetLayout()->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
    queue->Start();
    AddCommonFilter(new Keys());

    Apply();
    PublishPlaces();
    panes[source]->FocusList();
    ResizeTo(1140, 650);
  }
  status_t GetSupportedSuites(BMessage *m) override {
    static property_info properties[] = {{"Workspace",
                                          {B_GET_PROPERTY, 0},
                                          {B_DIRECT_SPECIFIER, 0},
                                          "Current workspace",
                                          0},
                                         {"SourcePath",
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
    if (!strcmp(property, "Workspace") || !strcmp(property, "SourcePath") ||
        !strcmp(property, "DestinationPath") || !strcmp(property, "Busy") ||
        !strcmp(property, "PathReady"))
      return this;
    return BWindow::ResolveSpecifier(msg, index, spec, what, property);
  }
  ~MainWindow() { delete queue; }
  void MenusBeginning() override {
    BWindow::MenusBeginning();
    for (int32 i = 0; i < ViewModeCount; ++i)
      fileView->ItemAt(i)->SetMarked(i == panes[source]->CurrentView());
    saveClipboardTextItem->SetEnabled(
        !actionArea->Pending() &&
        Listi::CanSaveClipboardTextIn(panes[source]->Path()));
  }

  bool QuitRequested() override {
    if (!FilePropertiesAllowQuit() || !OpenWithAllowQuit() ||
        !ArchiveEditorsAllowQuit())
      return false;
    if (placesSaving || searchesSaving) {
      Notice(
          "Saved places or searches are still being written. Please try quitting again shortly.");
      return false;
    }
    if (queue->Busy() || !clipboardJobs.empty()) {
      queue->Show();
      Notice("File operations are still running. Let them finish, or use "
             "Cancel all before quitting.");
      return false;
    }
    if (archiveClipboardPending)
      CloseAction();
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
        } else if (!strcmp(property, "Workspace"))
          reply.AddString("result", WorkspaceName(prefs.workspace));
        else if (!strcmp(property, "PathReady"))
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
    case Listi::kShowShelf:
      Listi::OpenShelfWindow(BMessenger(this), shelf, shelfWindow);
      break;
    case Listi::kAddToShelf:
      AddToShelf();
      break;
    case Listi::kSubmitShelfMove:
    case Listi::kSubmitShelfZip: {
      if (m->IsSourceRemote())
        break;
      auto job = Listi::DecodeJob(*m);
      const auto expected = m->what == Listi::kSubmitShelfMove
                                ? Listi::Action::Move
                                : Listi::Action::CreateArchive;
      if (job.action == expected && !job.sources.empty() &&
          job.expectedSources.size() == job.sources.size() &&
          job.expectedDestination.device >= 0)
        queue->Enqueue(job);
      break;
    }
    case kPlaceAction: {
      if (!placesWritable || placesSaving) {
        Notice("Saved places are unavailable or still being saved.");
        break;
      }
      const char *path = m->GetString("path", "");
      int32 action = m->GetInt32("action", -1);
      auto place = places.Find(path);
      if (action < 0 || action > 2 || (action != 0 && !place))
        break;
      BMessage accepted(*m);
      accepted.what = kPlaceAccepted;
      accepted.AddBool("placeLabel", true);
      std::string initial = place ? place->name : Listi::LocationLeaf(path);
      if (initial.empty())
        initial = path;
      std::string question = action == 0   ? "Save this work location: "
                             : action == 1 ? "Rename saved place: "
                                           : "Forget saved place: ";
      question += path;
      AskInline(question.c_str(),
                action == 0   ? "Save place"
                : action == 1 ? "Rename place"
                              : "Forget place",
                accepted, action == 2 ? nullptr : initial.c_str());
      break;
    }
    case kPlaceAccepted: {
      if (!placesWritable || placesSaving) {
        Notice("Saved places are unavailable or still being saved.");
        break;
      }
      try {
        auto next = places;
        const char *path = m->GetString("path", "");
        const char *name = m->GetString("name", "");
        switch (m->GetInt32("action", -1)) {
        case 0:
          next.Add(name, path);
          break;
        case 1:
          next.Rename(path, name);
          break;
        case 2:
          next.Remove(path);
          break;
        default:
          break;
        }
        BMessage save = next.Message();
        save.what = kSavePlaces;
        save.AddInt64("serial", ++placesSerial);
        pendingPlaces = next;
        placesSaving = true;
        if (service.SendMessage(&save) != B_OK) {
          placesSaving = false;
          throw std::runtime_error("Could not queue saved places.");
        }
        PublishPlaces();
      } catch (const std::exception &e) {
        Notice(e.what());
      }
      break;
    }
    case kSearchEdit: {
      try {
        if (!searchesWritable)
          throw std::runtime_error(
              "Saved searches could not be loaded. Their file is preserved; changes are disabled.");
        if (searchesSaving || m->GetInt64("revision", -1) != searchesRevision)
          throw std::runtime_error(
              "The saved-search list changed or is being written. Please try again.");
        auto next = searches;
        Listi::SavedSearch item;
        item.name = m->GetString("name", "");
        item.request.root = m->GetString("root", "");
        item.request.expression = m->GetString("expression", "");
        item.request.filters = Listi::ReadSearchFilters(*m);
        item.request.predicate = m->GetBool("predicate", false);
        item.request.includeTrash = m->GetBool("trash", false);
        item.live = m->GetBool("live", false);
        if (m->GetBool("remove", false))
          next.Remove(item.name);
        else
          next.Put(item, m->GetBool("replace", false));
        auto request = next.Message();
        request.what = kSaveSearches;
        request.AddInt64("revision", searchesRevision);
        pendingSearches = next;
        pendingSearchName = m->GetBool("remove", false) ? "" : item.name;
        searchesSaving = true;
        if (service.SendMessage(&request) != B_OK) {
          searchesSaving = false;
          throw std::runtime_error("Could not queue saved search.");
        }
      } catch (const std::exception &e) {
        auto reply = SearchStore();
        reply.AddString("error", e.what());
        searchWindow.SendMessage(&reply);
      }
      break;
    }
    case kSearchesSaved:
      if (searchesSaving && m->GetInt64("revision", -1) == searchesRevision) {
        searchesSaving = false;
        if (!m->HasString("error")) {
          searches = pendingSearches;
          ++searchesRevision;
        }
        pendingSearches = {};
        auto reply = SearchStore();
        if (m->HasString("error"))
          reply.AddString("error", m->GetString("error", "Save failed."));
        else {
          reply.AddBool("committed", true);
          reply.AddString("chosen", pendingSearchName.c_str());
        }
        searchWindow.SendMessage(&reply);
      }
      break;
    case kPlacesSaved:
      if (placesSaving && m->GetInt64("serial", -1) == placesSerial) {
        placesSaving = false;
        if (m->HasString("error"))
          Notice(m->GetString("error", "Save failed."));
        else
          places = pendingPlaces;
        pendingPlaces = {};
        PublishPlaces();
      }
      break;
    case kInlineNotice:
      if (actionArea->Pending()) {
        queue->ErrorInfo(m->GetString("error", "Operation failed"));
        break;
      }
      AskInline(m->GetString("error", "Operation failed"), "OK",
                BMessage(kInlineCancel));
      break;
    case kInlineClosed:
      if (!actionArea->Pending())
        pendingClipboardText.reset();
      if (!actionArea->Pending() && archiveClipboardPending)
        CloseAction();
      ShowBank(prefs.buttons && !actionArea->Pending());
      if (!actionArea->Pending())
        panes[source]->FocusList();
      break;
    case kInlineCancel:
      if (panes[source]->CancelSelectionGesture())
        break;
      if (actionArea->Pending())
        CloseAction();
      else
        queue->Collapse();
      break;
    case kPreviewToggle:
      if (prefs.SinglePane())
        break;
      prefs.SetPreview(!prefs.Preview());
      if (prefs.Preview() != previewVisible) {
        previewVisible = prefs.Preview();
        if (previewVisible)
          preview->Show();
        else
          preview->Hide();
      }
      previewMenu->SetMarked(prefs.Preview());
      UpdatePreview();
      Save();
      break;
    case kPreviewSelection:
      if (m->GetInt32("pane", -1) == source) {
        UpdatePreview();
        if (m->HasString("operationError"))
          queue->ErrorInfo(m->GetString("operationError", ""));
      }
      break;
    case kPaneDisplay:
      prefs.SetTree(0, panes[0]->TreeVisible());
      prefs.SetTree(1, panes[1]->TreeVisible());
      for (int i = 0; i < 2; ++i)
        prefs.SetPaneMode(i, panes[i]->PaneOverride());
      if (m->HasString("folder")) {
        std::string folder = m->GetString("folder", "");
        if (m->GetBool("forget", false))
          prefs.folderModes.erase(folder);
        else
          prefs.folderModes[folder] = m->GetInt32("mode", 0);
      }
      panes[0]->FolderModes(prefs.folderModes);
      panes[1]->FolderModes(prefs.folderModes);
      Save();
      break;
    case kGridMode:
      panes[source]->MessageReceived(m);
      break;
    case kWorkspace: {
      int32 value =
          m->GetInt32("workspace", m->GetBool("simple", false) ? 1 : 0);
      if (value >= 0 && value <= 3)
        SetWorkspace(static_cast<Workspace>(value));
    } break;
    case kActivate:
      if (m->GetInt32("pane", source) < 0 || m->GetInt32("pane", source) > 1)
        break;
      if (prefs.SinglePane() && m->GetInt32("pane", source) != source)
        break;
      source = m->GetInt32("pane", source);
      for (int i = 0; i < 2; i++)
        panes[i]->SetSource(i == source);
      UpdatePreview();
      break;
    case kSwitch:
      if (prefs.workspace == Workspace::askr) {
        panes[source]->ToggleTreeFocus();
        break;
      }
      if (prefs.SinglePane())
        break;
      source = 1 - source;
      for (int i = 0; i < 2; i++)
        panes[i]->SetSource(i == source);
      panes[source]->FocusList();
      UpdatePreview();
      break;
    case kSame:
      if (prefs.SinglePane())
        break;
      panes[1 - source]->Navigate(panes[source]->Path());
      break;
    case kRefresh:
      for (auto p : panes)
        p->Reload();
      break;
    case kSearch:
      if (searchWindow.IsValid())
        searchWindow.SendMessage(kSearch);
      else
        searchWindow = OpenFileSearch(BMessenger(this), source,
                                      panes[source]->Path(), SearchStore());
      break;
    case kRevealSearch: {
      int32 which = m->GetInt32("pane", -1);
      if (which < 0 || which > 1)
        break;
      source = which;
      if (prefs.SinglePane()) {
        prefs.singlePane = source;
        ApplyWorkspace();
        Save();
      }
      for (int i = 0; i < 2; ++i)
        panes[i]->SetSource(i == source);
      panes[source]->MessageReceived(m);
      Activate();
      panes[source]->FocusList();
      break;
    }
    case kFilter:
      panes[source]->FocusFilter();
      break;
    case kContextMenu: {
      if (actionArea->Pending()) {
        queue->ErrorInfo("Finish or cancel the open question first.");
        break;
      }
      const int32 owner = m->GetInt32("pane", -1);
      if (owner < 0 || owner > 1 || (prefs.SinglePane() && owner != source))
        break;
      source = owner;
      for (int i = 0; i < 2; ++i)
        panes[i]->SetSource(i == source);
      const auto selected = panes[source]->Selected();
      const std::string folder = panes[source]->Path(),
                        destination = panes[1 - source]->Path();
      const bool background = m->GetBool("folderContext", false);
      if (!background && selected.empty())
        break;
      const bool folderLocal = folder != "@disks" && !Listi::IsVirtual(folder);
      bool local = true;
      for (const auto &path : selected)
        local &= !Listi::IsVirtual(path);
      try {
        BMessage snapshot;
        std::exception_ptr fileSnapshotError;
        if (!selected.empty() && local) {
          try {
            snapshot = Listi::MakeFileSelection(selected);
          } catch (...) {
            fileSnapshotError = std::current_exception();
          }
        }
        struct stat folderBefore {};
        if (folderLocal && (stat(folder.c_str(), &folderBefore) ||
                            !S_ISDIR(folderBefore.st_mode)))
          throw std::runtime_error(
              "The folder is unavailable. Open the menu again.");
        struct stat before {};
        const bool destinationKnown = !stat(destination.c_str(), &before);
        BPoint screen;
        if (m->FindPoint("screen", &screen) != B_OK)
          break;
        ContextMenuOptions options;
        options.folder = background;
        options.selected = selected.size();
        options.directory = m->GetBool("directory", false);
        options.local = local;
        options.dual = !prefs.SinglePane();
        options.folderLocal = folderLocal;
        const auto folderLocation = Listi::ParseLocation(folder);
        options.folderArchive = folderLocation.Archive();
        options.archiveAdd =
            folderLocation.Archive() ||
            (selected.size() == 1 && local && !options.directory);
        options.archiveRemove = !selected.empty();
        std::string selectedArchive;
        for (const auto &path : selected) {
          auto selectedLocation = Listi::ParseLocation(path);
          if (!selectedLocation.Archive() ||
              (!selectedArchive.empty() &&
               selectedArchive != selectedLocation.archive)) {
            options.archiveRemove = false;
            break;
          }
          selectedArchive = selectedLocation.archive;
        }
        options.fileType = panes[owner]->SelectedFileType();
        options.sort = panes[owner]->Sorting();
        options.view = panes[owner]->CurrentView();
        const auto choice = ShowContextMenu(screen, options);
        const uint32 action = choice.what;
        if (!action)
          break;
        if (source != owner || selected != panes[owner]->Selected() ||
            folder != panes[owner]->Path() ||
            destination != panes[1 - owner]->Path())
          throw std::runtime_error(
              "The panel changed. Open the file menu again.");
        if (choice.folder) {
          struct stat folderAfter {};
          if (folderLocal && (stat(folder.c_str(), &folderAfter) ||
                              folderBefore.st_dev != folderAfter.st_dev ||
                              folderBefore.st_ino != folderAfter.st_ino))
            throw std::runtime_error(
                "The folder changed. Open the menu again.");
          if (action == kSort) {
            panes[owner]->Sort({choice.value, choice.ascending});
            break;
          }
          if (action == kGridMode) {
            BMessage command(kGridMode);
            command.AddInt32("mode", choice.value);
            panes[owner]->MessageReceived(&command);
            break;
          }
          if (action == kProperties) {
            OpenFileProperties(folder);
            break;
          }
          if (action == kRefresh) {
            panes[owner]->Reload();
            break;
          }
        } else if (local) {
          if (fileSnapshotError)
            std::rethrow_exception(fileSnapshotError);
          Listi::ReadFileClipboard(snapshot);
        }
        if (action == kCopy || action == kMove || action == kCreateLink) {
          struct stat after {};
          if (destinationKnown &&
              (stat(destination.c_str(), &after) ||
               before.st_dev != after.st_dev || before.st_ino != after.st_ino))
            throw std::runtime_error(
                "The destination changed. Open the file menu again.");
        }
        BMessage command(action);
        // Dispatch before any queued activation/navigation message can redirect it.
        if (action == kInvoke)
          panes[owner]->MessageReceived(&command);
        else
          MessageReceived(&command);
      } catch (const std::exception &e) {
        Notice(e.what());
      }
      break;
    }
    case kCreateArchive:
      ArchiveAction(false);
      break;
    case kExtractArchive:
      ArchiveAction(true);
      break;
    case kAddArchive:
      AddToArchive();
      break;
    case kRemoveArchive:
      RemoveFromArchive();
      break;
    case kArchiveAddChosen: {
      try {
        std::vector<std::string> additions;
        entry_ref ref;
        BPath path;
        for (int32 i = 0; m->FindRef("refs", i, &ref) == B_OK; ++i) {
          if (path.SetTo(&ref) != B_OK)
            throw std::runtime_error("A selected file is unavailable.");
          additions.emplace_back(path.Path());
        }
        Listi::SetArchiveEditAdditions(pendingArchiveEdit, additions);
        BMessage accepted(kArchiveEditAccepted);
        BString text;
        text << "Add " << int32(additions.size())
             << " selected entries to this archive?\n\n"
             << archiveAddTarget.c_str();
        if (archiveAddFolder != "/")
          text << "\nInside: " << archiveAddFolder.c_str();
        text << "\n\nSelected:";
        const int32 shown = std::min<int32>(int32(additions.size()), 3);
        for (int32 i = 0; i < shown; ++i)
          text << "\n" << Listi::Leaf(additions[i]).c_str();
        if (int32(additions.size()) > shown)
          text << "\n…and " << int32(additions.size()) - shown << " more";
        text
            << "\n\nExisting names will require an explicit Replace or Skip choice.";
        AskInline(text.String(), "Update archive", accepted);
      } catch (const std::exception &e) {
        Notice(e.what());
        pendingArchiveEdit = {};
      }
      break;
    }
    case kArchiveEditAccepted:
      if (!pendingArchiveEdit.destination.empty()) {
        if (archiveClipboardPending) {
          pendingArchiveEdit.tag = ++clipboardSerial;
          clipboardJobs.insert(pendingArchiveEdit.tag);
          clipboardMoved.clear();
          archiveClipboardPending = false;
        }
        queue->Enqueue(pendingArchiveEdit);
        pendingArchiveEdit = {};
      }
      break;
    case 'apfg':
      Listi::ForgetArchivePasswords();
      break;
    case kArchiveAccepted:
      queue->Enqueue(Listi::DecodeJob(*m));
      break;
    case kResumeArchiveEdits:
      ResumeArchiveEditors(prefs.editor.String());
      break;
    case kOpenWith: {
      auto selected = panes[source]->Selected();
      if (selected.size() != 1)
        Notice("Select one local file for Open with.");
      else
        OpenWithFile(selected[0]);
      break;
    }
    case kProperties: {
      auto selected = panes[source]->Selected();
      if (selected.size() != 1)
        Notice("Select one local file or folder for Properties.");
      else
        OpenFileProperties(selected[0]);
      break;
    }
    case kClipboardSaveText:
      AskToSaveClipboardText();
      break;
    case kClipboardTextAccepted:
      SaveClipboardText(m->GetString("name", ""));
      break;
    case kClipboardCopy:
    case kClipboardCut:
    case kClipboardPaste:
      ClipboardAction(m->what);
      break;
    case kCopy:
      Operation(Listi::Action::Copy);
      break;
    case kMove:
      Operation(Listi::Action::Move);
      break;
    case kDelete:
      Operation(Listi::Action::Trash);
      break;
    case kRestore:
      Operation(Listi::Action::Restore);
      break;
    case kPermanentDelete:
      Operation(Listi::Action::Delete);
      break;
    case kDuplicate:
      NamedEntry(Listi::Action::Duplicate);
      break;
    case kCreateLink:
      NamedEntry(Listi::Action::Link);
      break;
    case kNamedEntryAccepted:
      if (!pendingNamedEntry.sources.empty()) {
        pendingNamedEntry.entryName = m->GetString("name", "");
        pendingNamedEntry.relativeLink = m->GetInt32("which", 1) != 2;
        queue->Enqueue(pendingNamedEntry);
        pendingNamedEntry = {};
      }
      break;
    case kRename:
      Operation(Listi::Action::Rename);
      break;
    case kBulkRename:
      BulkRename();
      break;
    case kBulkRenameAccepted: {
      const auto job = Listi::DecodeJob(*m);
      if (job.action == Listi::Action::BulkRename)
        queue->Enqueue(job);
      break;
    }
    case kMkdir:
      Operation(Listi::Action::MakeDirectory);
      break;
    case kConfirm:
      if (m->GetInt32("which", 0) == 1)
        queue->Enqueue(Listi::DecodeJob(*m));
      break;
    case kNameAccepted: {
      auto j = Listi::DecodeJob(*m);
      try {
        j.destination =
            Listi::ChildLocation(j.destination, m->GetString("name", ""));
        if (Listi::ParseLocation(j.destination).Archive())
          j = Listi::PrepareArchiveMutation(j);
      } catch (const std::exception &e) {
        Notice(e.what());
        break;
      }
      queue->Enqueue(j);
      break;
    }
    case kDrop: {
      if (actionArea->Pending()) {
        queue->ErrorInfo(
            "Finish or cancel the open question before dropping files.");
        break;
      }
      try {
        ++dropSerial;
        auto job =
            Listi::PrepareFileDrop(*m, m->GetString("destination", ""), true);
        if (job.action == Listi::Action::EditArchive) {
          ConfirmArchiveAddition(job);
          break;
        }
        const bool canConvert = Listi::CanProbeImageDrop(job);
        const bool sameFolder = std::any_of(
            job.sources.begin(), job.sources.end(), [&](const auto &path) {
              return Listi::Join(job.destination, Listi::Leaf(path)) == path;
            });
        auto accepted = Listi::EncodeJob(job, kDropAccepted);
        accepted.AddInt64("request", dropSerial);
        if (sameFolder) {
          if (!canConvert)
            throw std::runtime_error(
                "Drop into a different folder, not the source folder.");
          accepted.AddBool("convertOnly", true);
          AskInline(
              "Convert an image in its current folder. The original is kept.",
              "Convert here", accepted);
          break;
        }
        const bool canMove = !Listi::IsVirtual(job.destination) &&
                             job.expectedSources.size() == job.sources.size();
        std::string text =
            "Drop " + std::to_string(job.sources.size()) + " item(s) into:\n" +
            job.destination +
            (!canMove
                 ? "\nCopy only for network/archive transfers. Originals are kept."
                 : "\nCopy keeps originals. Move removes originals after transfer.");
        ShowBank(false);
        actionArea->Ask(text.c_str(), "Copy here", accepted, nullptr,
                        canMove ? "Move here" : nullptr,
                        canConvert ? "Convert here" : nullptr);
      } catch (const std::exception &e) {
        Notice(e.what());
      }
      break;
    }
    case kDropAccepted: {
      if (m->GetInt64("request", -1) != dropSerial)
        break;
      const auto which = m->GetInt32("which", 0);
      if (which == 3 || m->GetBool("convertOnly", false)) {
        BMessage probe(*m);
        probe.what = kImageDropProbe;
        probe.AddMessenger("replyTo", BMessenger(this));
        if (service.SendMessage(&probe, (BHandler *)nullptr, 500000) != B_OK)
          Notice("Could not check installed image translators.");
      } else if (which == 1 || which == 2) {
        auto job = Listi::DecodeJob(*m);
        const bool move = which == 2 && !Listi::IsVirtual(job.destination) &&
                          job.expectedSources.size() == job.sources.size();
        job.action = move ? Listi::Action::Move : Listi::Action::Copy;
        queue->Enqueue(job);
      }
      break;
    }
    case kImageDropOutputs: {
      if (m->GetInt64("request", -1) != dropSerial)
        break;
      if (actionArea->Pending()) {
        queue->ErrorInfo("Finish the open question and drop the image again.");
        break;
      }
      if (m->HasString("error")) {
        Notice(m->GetString("error", ""));
        break;
      }
      auto job = Listi::DecodeJob(*m);
      if (job.sources.size() != 1)
        break;
      std::vector<std::string> labels;
      BMessage output;
      for (int32 i = 0; m->FindMessage("output", i, &output) == B_OK; ++i) {
        labels.push_back(std::string(output.GetString("name", "")) + " (." +
                         output.GetString("extension", "") + ")");
      }
      if (labels.empty())
        break;
      BMessage accepted(*m);
      accepted.what = kImageDropConvert;
      accepted.RemoveName("replyTo");
      accepted.RemoveName("which");
      std::string text = "Convert " + Listi::Leaf(job.sources.front()) +
                         " into:\n" + job.destination +
                         "\nThe original and existing files are kept.";
      ShowBank(false);
      actionArea->Ask(text.c_str(), "Convert", accepted, nullptr, nullptr,
                      nullptr, labels);
      break;
    }
    case kImageDropConvert: {
      if (m->GetInt64("request", -1) != dropSerial)
        break;
      BMessage output;
      const auto option = m->GetInt32("option", -1);
      if (option < 0 || m->FindMessage("output", option, &output) != B_OK)
        break;
      auto job = Listi::DecodeJob(*m);
      job.action = Listi::Action::ConvertImage;
      job.imageFormat = output.GetInt32("type", 0);
      job.imageMime = output.GetString("mime", "");
      queue->Enqueue(job);
      break;
    }
    case kFileProgress:
      for (auto p : panes) {
        p->MessageReceived(m);
        if (m->HasString("destination") &&
            Listi::ParentLocation(m->GetString("destination", "")) == p->Path())
          p->Reload();
      }
      break;
    case kQueueWake:
      for (auto &done : queue->TakeEvents()) {
        queue->PresentEvent(done);
        if (done.what != kJobDone)
          continue;
        if (clipboardJobs.erase(done.GetInt64("tag", 0))) {
          const char *path;
          for (int32 i = 0; done.FindString("moved", i, &path) == B_OK; ++i)
            clipboardMoved.emplace_back(path);
          if (clipboardJobs.empty()) {
            try {
              Listi::FinishFilePaste(clipboardPaste, clipboardMoved);
            } catch (const std::exception &e) {
              Notice(e.what());
            }
            clipboardPaste = {};
            clipboardMoved.clear();
          }
        }
        for (auto p : panes)
          p->MessageReceived(&done);
        RefreshShelf();
      }
      break;
    case kShowJobs:
      queue->Toggle();
      break;
    case kCompareFolders:
      OpenFolderCompare(panes[source]->Path(), panes[1 - source]->Path());
      break;
    case kSelectType:
      if (!panes[source]->SelectSameFileType())
        Notice(
            "Choose a file with a filename extension. Only visible files of that type are selected; folders and links are excluded.");
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
    case kNetwork:
      if (!actionArea->Pending())
        ShowBank(false);
      if (!actionArea->Connect(BMessenger(panes[source])))
        queue->ErrorInfo("Finish or cancel the open question first.");
      break;
    case kOpenSystem: {
      auto selected = panes[source]->Selected();
      if (selected.size() != 1) {
        Notice("Select exactly one file.");
        break;
      }
      BMessage open(kOpenPaths);
      open.AddString("path", selected[0].c_str());
      open.AddString("mode", "system");
      service.SendMessage(&open);
      break;
    }
    case kPreferences:
      OpenPreferences(BMessenger(this), prefs);
      break;
    case kApplyPreferences: {
      prefs.ApplyDialog(*m);
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
  BMessenger mainWindow;

public:
  App() : BApplication(kSignature) {}
  ~App() {
    Listi::StopArchivePasswords();
    if (services && services->Lock())
      services->Quit();
  }
  void ReadyToRun() override {
    InstallArchivePasswordWindow();
    Preferences prefs;
    BMessage settings;
    std::string settingsError;
    try {
      settings = Listi::LoadMessageStore(SettingsPath().String());
      prefs.Load(settings);
    } catch (const std::exception &e) {
      settingsError = e.what();
    }
    services = new Services(BMessenger(this));
    services->Run();
    Listi::SavedPlaces places;
    std::string placesError;
    try {
      places = Listi::SavedPlaces::Load(SettingsPath("Lister_places").String());
    } catch (const std::exception &e) {
      placesError = e.what();
    }
    Listi::SavedSearches searches;
    std::string searchesError;
    try {
      searches =
          Listi::SavedSearches::Load(SettingsPath("Lister_searches").String());
    } catch (const std::exception &e) {
      searchesError = e.what();
    }
    auto window =
        new MainWindow(prefs, BMessenger(services), places, placesError.empty(),
                       searches, searchesError.empty());
    mainWindow = BMessenger(window);
    window->Show();
    if (!settingsError.empty())
      Notice((std::string("Could not load preferences. Defaults are shown: ") +
              settingsError)
                 .c_str());
    if (!searchesError.empty()) {
      searchesError =
          "Saved searches could not be loaded. The file has been preserved; changes are disabled. " +
          searchesError;
      Notice(searchesError.c_str());
    }
    if (!placesError.empty()) {
      placesError =
          "Saved places could not be loaded. The file has been preserved; changes are disabled. " +
          placesError;
      Notice(placesError.c_str());
    }
  }
  bool QuitRequested() override {
    return FilePropertiesAllowQuit() && OpenWithAllowQuit() &&
           ArchiveEditorsAllowQuit() && BApplication::QuitRequested();
  }
  void MessageReceived(BMessage *m) override {
    if (m->what == kPlacesSaved || m->what == kSearchesSaved) {
      mainWindow.SendMessage(m);
      return;
    }
    if (m->what == kServiceError) {
      BMessage error(*m);
      error.what = kInlineNotice;
      mainWindow.SendMessage(&error);
      return;
    }
    if (m->what == kViewLoaded) {
      BString title = m->GetString("path", "");
      if (m->GetBool("truncated", false))
        title << " (first 1 MiB)";
      auto window = new BWindow(BRect(120, 100, 900, 650), title,
                                B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS);
      auto text = new ThemeTextView("document", B_DOCUMENT_BACKGROUND_COLOR,
                                    B_DOCUMENT_TEXT_COLOR);
      text->SetText(m->GetString("text", ""));
      text->MakeEditable(false);
      text->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
      text->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
      text->SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
      text->SetFontAndColor(be_fixed_font, B_FONT_ALL);
      auto scroll = new BScrollView("scroll", text, 0, false, true);
      scroll->SetExplicitMinSize(BSize(300, 200));
      BLayoutBuilder::Group<>(window, B_VERTICAL, 0).Add(scroll);
      window->AddShortcut(B_ESCAPE, B_NO_COMMAND_KEY,
                          new BMessage(B_QUIT_REQUESTED));
      window->AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
      window->Show();
      return;
    }
    BApplication::MessageReceived(m);
  }
};
namespace Listi {
int VirtualTest(const std::string &, const std::string &);
}
int main(int argc, char **argv) {
  if (argc == 4 && !strcmp(argv[1], "--virtual-test"))
    return Listi::VirtualTest(argv[2], argv[3]);
  if (argc > 1 && !strcmp(argv[1], "--self-test"))
    return Listi::SelfTest();
  App app;
  app.Run();
  return 0;
}
