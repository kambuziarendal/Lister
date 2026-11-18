// SPDX-License-Identifier: MIT
#include "Common.h"
#include <Alert.h>
#include <Button.h>
#include <CheckBox.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <Window.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
rgb_color Preferences::Color(const BString &text) const {
  unsigned int r, g, b;
  if (text.Length() == 7 &&
      sscanf(text.String(), "#%2x%2x%2x", &r, &g, &b) == 3)
    return {(uint8)r, (uint8)g, (uint8)b, 255};
  return ui_color(B_PANEL_TEXT_COLOR);
}
void Preferences::Load(const BMessage &m) {
  // Legacy numeric values remain authoritative when present: an older app may
  // have changed them since our last save. New-only settings use ASCII IDs.
  int32 stored;
  if (m.FindInt32("workspace", &stored) == B_OK) {
    workspace = stored >= 0 && stored <= 3 ? static_cast<Workspace>(stored)
                                           : Workspace::tviskipt;
  } else if (m.HasString("workspaceId")) {
    workspace = Workspace::tviskipt;
    for (auto value : {Workspace::tviskipt, Workspace::einn, Workspace::askr,
                       Workspace::smidja})
      if (!strcmp(m.GetString("workspaceId", ""), WorkspaceId(value)))
        workspace = value;
  } else {
    workspace = m.GetBool("simpleWorkspace", false) ? Workspace::einn
                                                    : Workspace::tviskipt;
  }
  singlePane = m.GetInt32("singlePane", 0) == 1 ? 1 : 0;
  for (auto w : {Workspace::tviskipt, Workspace::einn, Workspace::askr,
                 Workspace::smidja}) {
    BMessage view;
    bool hasProfile = m.FindMessage(WorkspaceId(w), &view) == B_OK;
    int32 fallback = w == Workspace::einn ? MillerMode : 0;
    int32 mode = view.GetInt32("default", fallback);
    defaultModes[int(w)] = mode >= 0 && mode < ViewModeCount ? mode : fallback;
    for (int pane = 0; pane < 2; ++pane) {
      const char *key = pane ? "rightMode" : "leftMode";
      int32 saved = hasProfile ? view.GetInt32(key, -1) : m.GetInt32(key, -1);
      // Legacy Details was always saved, even when the user never chose a mode.
      if (!hasProfile && saved == 0)
        saved = -1;
      paneOverrides[int(w)][pane] =
          saved >= 0 && saved < ViewModeCount ? saved : -1;
    }
  }
  paneTree[0] = m.GetBool("leftTree", false);
  paneTree[1] = m.GetBool("rightTree", false);
  smidjaTree[0] =
      m.GetBool("workbenchLeftTree", m.GetBool("smidjaLeftTree", true));
  smidjaTree[1] =
      m.GetBool("workbenchRightTree", m.GetBool("smidjaRightTree", true));
  preview = m.GetBool("preview", false);
  smidjaPreview =
      m.GetBool("workbenchPreview", m.GetBool("smidjaPreview", true));
  folderModes.clear();
  BMessage folder;
  for (int32 i = 0; i < 4096 && m.FindMessage("folderView", i, &folder) == B_OK;
       ++i) {
    const char *path = folder.GetString("path", "");
    int32 mode = folder.GetInt32("mode", -1);
    if (*path && mode >= 0 && mode < ViewModeCount)
      folderModes[path] = mode;
  }
  icons = m.GetBool("icons", true);
  size = m.GetBool("size", true);
  date = m.GetBool("date", true);
  buttons = m.GetBool("buttons", true);
  command = m.GetBool("command", false);
  float fontSize = m.GetFloat("fontSize", be_plain_font->Size());
  font.SetSize(std::isfinite(fontSize) && fontSize >= 8 && fontSize <= 40
                   ? fontSize
                   : 12);
  const char *family = m.GetString("fontFamily", "");
  if (*family)
    font.SetFamilyAndStyle(family, nullptr);
  editor = m.GetString("editor", editor);
  terminal = m.GetString("terminal", terminal);
  folders = m.GetString("folders", "system");
  files = m.GetString("files", "system");
  links = m.GetString("links", "system");
}
void Preferences::ApplyDialog(const BMessage &message) {
  // The dialog edits appearance and launchers, not the live panel workspace.
  Preferences updated = *this;
  updated.Load(message);
  for (int i = 0; i < 2; ++i) {
    updated.paneTree[i] = paneTree[i];
    updated.smidjaTree[i] = smidjaTree[i];
  }
  for (auto w : {Workspace::tviskipt, Workspace::einn, Workspace::askr,
                 Workspace::smidja}) {
    if (updated.DefaultMode(w) == DefaultMode(w)) {
      for (int pane = 0; pane < 2; ++pane)
        updated.paneOverrides[int(w)][pane] = paneOverrides[int(w)][pane];
    } else {
      updated.paneOverrides[int(w)][0] = updated.paneOverrides[int(w)][1] = -1;
    }
  }
  updated.workspace = workspace;
  updated.singlePane = singlePane;
  updated.preview = preview;
  updated.smidjaPreview = smidjaPreview;
  updated.folderModes = folderModes;
  *this = updated;
}
BMessage Preferences::Save() const {
  BMessage m;
  // Compatibility aliases support upgrades and rollback without a migration.
  // The pre-enum single-pane fallback must never reveal a hidden target.
  m.AddString("workspaceId", WorkspaceId(workspace));
  m.AddBool("simpleWorkspace", SinglePane());
  m.AddInt32("workspace", static_cast<int32>(workspace));
  m.AddInt32("singlePane", singlePane);
  // Old versions have no column mode; use Details rather than a thumbnail alias.
  m.AddInt32("leftMode", PaneMode(0) == MillerMode ? 0 : PaneMode(0));
  m.AddInt32("rightMode", PaneMode(1) == MillerMode ? 0 : PaneMode(1));
  for (auto w : {Workspace::tviskipt, Workspace::einn, Workspace::askr,
                 Workspace::smidja}) {
    BMessage view;
    view.AddInt32("default", DefaultMode(w));
    view.AddInt32("leftMode", paneOverrides[int(w)][0]);
    view.AddInt32("rightMode", paneOverrides[int(w)][1]);
    m.AddMessage(WorkspaceId(w), &view);
  }
  m.AddBool("leftTree", paneTree[0]);
  m.AddBool("rightTree", paneTree[1]);
  m.AddBool("smidjaLeftTree", smidjaTree[0]);
  m.AddBool("smidjaRightTree", smidjaTree[1]);
  m.AddBool("smidjaPreview", smidjaPreview);
  m.AddBool("workbenchLeftTree", smidjaTree[0]);
  m.AddBool("workbenchRightTree", smidjaTree[1]);
  m.AddBool("preview", preview);
  m.AddBool("workbenchPreview", smidjaPreview);
  for (auto &entry : folderModes) {
    BMessage folder;
    folder.AddString("path", entry.first.c_str());
    folder.AddInt32("mode", entry.second);
    m.AddMessage("folderView", &folder);
  }
  m.AddBool("icons", icons);
  m.AddBool("size", size);
  m.AddBool("date", date);
  m.AddBool("buttons", buttons);
  m.AddBool("command", command);
  font_family family;
  font_style style;
  font.GetFamilyAndStyle(&family, &style);
  m.AddString("fontFamily", family);
  m.AddFloat("fontSize", font.Size());
  m.AddString("editor", editor);
  m.AddString("terminal", terminal);
  m.AddString("folders", folders);
  m.AddString("files", files);
  m.AddString("links", links);
  return m;
}
namespace {
constexpr Workspace kSettingsWorkspaces[] = {
    Workspace::einn, Workspace::tviskipt, Workspace::askr, Workspace::smidja};
constexpr int32 kAppearanceTab = 1;

class PreferencesWindow : public BWindow {
  BMessenger target;
  Preferences prefs;
  BTabView *tabs;
  BPopUpMenu *defaultModes[sizeof(kSettingsWorkspaces) /
                           sizeof(kSettingsWorkspaces[0])];
  BCheckBox *icons, *size, *date, *buttons, *command;
  BTextControl *family, *fontSize, *folders, *files, *links, *editor, *terminal;

  void ShowInputError(BTextControl *control, const char *error) {
    tabs->Select(kAppearanceTab);
    control->MakeFocus();
    (new BAlert("invalid setting", error, "OK"))->Go(nullptr);
  }

public:
  PreferencesWindow(BMessenger t, const Preferences &p)
      : BWindow(BRect(0, 0, 560, 300), "Listi settings", B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE),
        target(t), prefs(p) {
    tabs = new BTabView("settings tabs", B_WIDTH_FROM_LABEL);

    auto workspaces = new BGroupView("Workspaces", B_VERTICAL);
    auto workspaceLayout = BLayoutBuilder::Group<>(workspaces);
    auto views = workspaceLayout.SetInsets(B_USE_WINDOW_SPACING)
                     .Add(new BStringView("default views",
                                          "Default file view by workspace"))
                     .AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING);
    int32 index = 0;
    for (Workspace workspace : kSettingsWorkspaces) {
      auto menu = new BPopUpMenu("File views");
      for (int32 mode = 0; mode < ViewModeCount; ++mode)
        menu->AddItem(new BMenuItem(ViewModeLabel(mode), nullptr));
      menu->ItemAt(p.DefaultMode(workspace))->SetMarked(true);
      defaultModes[index] = menu;
      auto field = new BMenuField(WorkspaceId(workspace),
                                  WorkspaceMenuLabel(workspace), menu);
      views.Add(field->CreateLabelLayoutItem(), 0, index)
          .Add(field->CreateMenuBarLayoutItem(), 1, index);
      ++index;
    }
    views.End()
        .Add(new BStringView(
            "view precedence",
            "An explicit view saved for a folder takes priority."))
        .AddGlue();
    tabs->AddTab(workspaces);

    icons = new BCheckBox("Show file icons", nullptr);
    icons->SetValue(p.icons);
    size = new BCheckBox("Size column", nullptr);
    size->SetValue(p.size);
    date = new BCheckBox("Modified column", nullptr);
    date->SetValue(p.date);
    buttons = new BCheckBox("Command button bank", nullptr);
    buttons->SetValue(p.buttons);
    command = new BCheckBox("Command line (runs in Terminal)", nullptr);
    command->SetValue(p.command);
    font_family f;
    font_style s;
    p.font.GetFamilyAndStyle(&f, &s);
    char number[32];
    snprintf(number, sizeof(number), "%.6g", p.font.Size());
    family = new BTextControl("File list font:", f, nullptr);
    fontSize = new BTextControl("Font size:", number, nullptr);
    folders = new BTextControl("Folder color:", p.folders, nullptr);
    files = new BTextControl("File color:", p.files, nullptr);
    links = new BTextControl("Symbolic link color:", p.links, nullptr);
    editor = new BTextControl("Editor application:", p.editor, nullptr);
    terminal = new BTextControl("Terminal application:", p.terminal, nullptr);

    auto appearance = new BGroupView("Appearance", B_VERTICAL);
    BLayoutBuilder::Group<>(appearance)
        .SetInsets(B_USE_WINDOW_SPACING)
        .Add(icons)
        .Add(size)
        .Add(date)
        .AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
        .Add(family->CreateLabelLayoutItem(), 0, 0)
        .Add(family->CreateTextViewLayoutItem(), 1, 0)
        .Add(fontSize->CreateLabelLayoutItem(), 0, 1)
        .Add(fontSize->CreateTextViewLayoutItem(), 1, 1)
        .Add(folders->CreateLabelLayoutItem(), 0, 2)
        .Add(folders->CreateTextViewLayoutItem(), 1, 2)
        .Add(files->CreateLabelLayoutItem(), 0, 3)
        .Add(files->CreateTextViewLayoutItem(), 1, 3)
        .Add(links->CreateLabelLayoutItem(), 0, 4)
        .Add(links->CreateTextViewLayoutItem(), 1, 4)
        .End()
        .Add(new BStringView("color format",
                             "Use system or a color like #B8C4D0."))
        .AddGlue();
    tabs->AddTab(appearance);

    auto workflow = new BGroupView("Workflow", B_VERTICAL);
    BLayoutBuilder::Group<>(workflow)
        .SetInsets(B_USE_WINDOW_SPACING)
        .Add(buttons)
        .Add(command)
        .AddGlue();
    tabs->AddTab(workflow);

    auto applications = new BGroupView("Applications", B_VERTICAL);
    BLayoutBuilder::Group<>(applications)
        .SetInsets(B_USE_WINDOW_SPACING)
        .AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
        .Add(editor->CreateLabelLayoutItem(), 0, 0)
        .Add(editor->CreateTextViewLayoutItem(), 1, 0)
        .Add(terminal->CreateLabelLayoutItem(), 0, 1)
        .Add(terminal->CreateTextViewLayoutItem(), 1, 1)
        .End()
        .AddGlue();
    tabs->AddTab(applications);

    auto save = new BButton("Save", new BMessage(kApplyPreferences));
    BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .SetInsets(B_USE_WINDOW_SPACING)
        .Add(tabs)
        .AddGroup(B_HORIZONTAL)
        .AddGlue()
        .Add(new BButton("Cancel", new BMessage(B_QUIT_REQUESTED)))
        .Add(save)
        .End();
    SetDefaultButton(save);
    AddShortcut(B_ESCAPE, B_NO_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
    AddShortcut('W', B_COMMAND_KEY, new BMessage(B_QUIT_REQUESTED));
    CenterOnScreen();
  }
  void MessageReceived(BMessage *m) override {
    if (m->what != kApplyPreferences) {
      BWindow::MessageReceived(m);
      return;
    }
    char *end = nullptr;
    float n = strtof(fontSize->Text(), &end);
    if (*end || !std::isfinite(n) || n < 8 || n > 40) {
      ShowInputError(fontSize, "Font size must be 8 to 40.");
      return;
    }
    for (auto c : {folders, files, links}) {
      BString s = c->Text();
      bool valid = s == "system";
      if (s.Length() == 7 && s.ByteAt(0) == '#') {
        valid = true;
        for (int i = 1; i < 7; i++)
          if (!std::isxdigit(static_cast<unsigned char>(s.ByteAt(i))))
            valid = false;
      }
      if (!valid) {
        ShowInputError(c, "Use system or a six-digit color such as #B8C4D0.");
        return;
      }
    }
    prefs.font.SetFamilyAndStyle(family->Text(), nullptr);
    prefs.font.SetSize(n);
    prefs.icons = icons->Value();
    prefs.size = size->Value();
    prefs.date = date->Value();
    prefs.buttons = buttons->Value();
    prefs.command = command->Value();
    prefs.folders = folders->Text();
    prefs.files = files->Text();
    prefs.links = links->Text();
    prefs.editor = editor->Text();
    prefs.terminal = terminal->Text();
    int32 index = 0;
    for (Workspace workspace : kSettingsWorkspaces) {
      auto menu = defaultModes[index++];
      prefs.SetDefaultMode(workspace, menu->IndexOf(menu->FindMarked()));
    }
    BMessage result = prefs.Save();
    result.what = kApplyPreferences;
    target.SendMessage(&result);
    Quit();
  }
};
} // namespace
void OpenPreferences(BMessenger target, const Preferences &prefs) {
  // The main window is the only caller. A messenger remains safe if the
  // settings window has already closed.
  static BMessenger openWindow;
  if (openWindow.LockTarget()) {
    BLooper *looper = nullptr;
    openWindow.Target(&looper);
    static_cast<BWindow *>(looper)->Activate();
    looper->Unlock();
    return;
  }
  auto window = new PreferencesWindow(target, prefs);
  openWindow = BMessenger(window);
  window->Show();
}
