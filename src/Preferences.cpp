// SPDX-License-Identifier: MIT
#include "Common.h"
#include <Alert.h>
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>
#include <Window.h>
#include <cstdio>
#include <cstdlib>
rgb_color Preferences::Color(const BString &text) const {
  unsigned int r, g, b;
  if (text.Length() == 7 &&
      sscanf(text.String(), "#%2x%2x%2x", &r, &g, &b) == 3)
    return {(uint8)r, (uint8)g, (uint8)b, 255};
  return ui_color(B_PANEL_TEXT_COLOR);
}
void Preferences::Load(const BMessage &m) {
  icons = m.GetBool("icons", true);
  size = m.GetBool("size", true);
  date = m.GetBool("date", true);
  buttons = m.GetBool("buttons", true);
  command = m.GetBool("command", false);
  font.SetSize(m.GetFloat("fontSize", be_plain_font->Size()));
  const char *family = m.GetString("fontFamily", "");
  if (*family)
    font.SetFamilyAndStyle(family, nullptr);
  editor = m.GetString("editor", editor);
  terminal = m.GetString("terminal", terminal);
  folders = m.GetString("folders", "system");
  files = m.GetString("files", "system");
  links = m.GetString("links", "system");
}
BMessage Preferences::Save() const {
  BMessage m;
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
    CenterOnScreen();
  }
  void MessageReceived(BMessage *m) override {
    if (m->what != kApplyPreferences) {
      BWindow::MessageReceived(m);
      return;
    }
    char *end = nullptr;
    float n = strtof(fontSize->Text(), &end);
    if (*end || n < 8 || n > 40) {
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
