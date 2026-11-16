// SPDX-License-Identifier: MIT
#include "Common.h"
#include <Alert.h>
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>
#include <Window.h>
#include <algorithm>
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
    workspace = stored >= 0 && stored <= 3 ? static_cast<Workspace>(stored) : Workspace::tviskipt;
  } else if (m.HasString("workspaceId")) {
    workspace = Workspace::tviskipt;
    for (auto value : {Workspace::tviskipt, Workspace::einn, Workspace::askr, Workspace::smidja})
      if (!strcmp(m.GetString("workspaceId", ""), WorkspaceId(value))) workspace = value;
  } else {
    workspace = m.GetBool("simpleWorkspace", false) ? Workspace::einn : Workspace::tviskipt;
  }
  singlePane = m.GetInt32("singlePane", 0) == 1 ? 1 : 0;
  paneMode[0] = std::max(0, std::min(3, m.GetInt32("leftMode", 0)));
  paneMode[1] = std::max(0, std::min(3, m.GetInt32("rightMode", 0)));
  paneTree[0] = m.GetBool("leftTree", false);
  paneTree[1] = m.GetBool("rightTree", false);
  smidjaTree[0] = m.GetBool("workbenchLeftTree", m.GetBool("smidjaLeftTree", true));
  smidjaTree[1] = m.GetBool("workbenchRightTree", m.GetBool("smidjaRightTree", true));
  preview = m.GetBool("preview", false);
  smidjaPreview = m.GetBool("workbenchPreview", m.GetBool("smidjaPreview", true));
  folderModes.clear();
  BMessage folder;
  for (int32 i = 0; i < 4096 && m.FindMessage("folderView", i, &folder) == B_OK;
       ++i) {
    const char *path = folder.GetString("path", "");
    int32 mode = folder.GetInt32("mode", -1);
    if (*path && mode >= 0 && mode <= 3)
      folderModes[path] = mode;
  }
  icons = m.GetBool("icons", true);
  size = m.GetBool("size", true);
  date = m.GetBool("date", true);
  buttons = m.GetBool("buttons", true);
  command = m.GetBool("command", false);
  float fontSize = m.GetFloat("fontSize", be_plain_font->Size());
  font.SetSize(std::isfinite(fontSize) && fontSize >= 8 && fontSize <= 40
                   ? fontSize : 12);
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
    updated.paneMode[i] = paneMode[i];
    updated.paneTree[i] = paneTree[i];
    updated.smidjaTree[i] = smidjaTree[i];
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
  m.AddInt32("leftMode", paneMode[0]);
  m.AddInt32("rightMode", paneMode[1]);
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
class PreferencesWindow : public BWindow {
  BMessenger target;
  Preferences prefs;
  BCheckBox *icons, *size, *date, *buttons, *command;
  BTextControl *family, *fontSize, *folders, *files, *links, *editor, *terminal;

public:
  PreferencesWindow(BMessenger t, const Preferences &p)
      : BWindow(BRect(0, 0, 550, 500), "Listi preferences", B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS),
        target(t), prefs(p) {
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
    snprintf(number, sizeof(number), "%.0f", p.font.Size());
    family = new BTextControl("Font family:", f, nullptr);
    fontSize = new BTextControl("Size:", number, nullptr);
    folders = new BTextControl("Folders:", p.folders, nullptr);
    files = new BTextControl("Files:", p.files, nullptr);
    links = new BTextControl("Symbolic links:", p.links, nullptr);
    editor = new BTextControl("Editor application:", p.editor, nullptr);
    terminal = new BTextControl("Terminal application:", p.terminal, nullptr);
    BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
        .SetInsets(12)
        .Add(new BStringView("title", "Display and workflow"))
        .AddGroup(B_HORIZONTAL)
        .Add(icons)
        .Add(size)
        .Add(date)
        .End()
        .Add(buttons)
        .Add(command)
        .AddGrid(8, 8)
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
        .Add(editor->CreateLabelLayoutItem(), 0, 5)
        .Add(editor->CreateTextViewLayoutItem(), 1, 5)
        .Add(terminal->CreateLabelLayoutItem(), 0, 6)
        .Add(terminal->CreateTextViewLayoutItem(), 1, 6)
        .End()
        .Add(new BStringView("colors",
                             "Colors: system (follows the theme) or #RRGGBB"))
        .AddGroup(B_HORIZONTAL)
        .AddGlue()
        .Add(new BButton("Cancel", new BMessage(B_QUIT_REQUESTED)))
        .Add(new BButton("Save", new BMessage(kApplyPreferences)))
        .End();
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
      (new BAlert("size", "Font size must be 8 to 40.", "OK"))->Go(nullptr);
      return;
    }
    for (auto c : {folders, files, links}) {
      BString s = c->Text();
      bool valid = s == "system";
      if (s.Length() == 7 && s.ByteAt(0) == '#') {
        valid = true;
        for (int i = 1; i < 7; i++)
          if (!isxdigit(s.ByteAt(i)))
            valid = false;
      }
      if (!valid) {
        (new BAlert("color", "Use system or a six-digit color such as #B8C4D0.",
                    "OK"))
            ->Go(nullptr);
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
    BMessage result = prefs.Save();
    result.what = kApplyPreferences;
    target.SendMessage(&result);
    Quit();
  }
};
} // namespace
void OpenPreferences(BMessenger target, const Preferences &prefs) {
  (new PreferencesWindow(target, prefs))->Show();
}
