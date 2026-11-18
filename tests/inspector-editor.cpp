// SPDX-License-Identifier: MIT
#include "InspectorEditor.h"
#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <Font.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace {
void Check(bool ok, const char *claim) {
  std::cout << (ok ? "PASS " : "FAIL ") << claim << std::endl;
  if (!ok)
    std::exit(1);
}
template <class T> T *View(BWindow *window, const char *name) {
  auto *view = dynamic_cast<T *>(window->FindView(name));
  Check(view != nullptr, name);
  return view;
}
void Send(BMessenger target, uint32 what) {
  BMessage action(what), reply;
  Check(target.SendMessage(&action, &reply, 2000000, 2000000) == B_OK,
        "native editor command processed");
}
std::string Read(const std::string &path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
}
}

int main() {
  BApplication app("application/x-vnd.Listi-InspectorEditorTest");
  char tmp[] = "/tmp/Listi-editor-XXXXXX";
  Check(mkdtemp(tmp) != nullptr, "private test directory");
  const std::string path = std::string(tmp) + "/example.py";
  const std::string disk = "#!/bin/python\r\nreturn café\r\nreturn two\r\n";
  const std::string original = "#!/bin/python\nreturn café\nreturn two\n";
  const std::string modified = "#!/bin/python\nyield café\nyield two\n";
  std::ofstream(path) << disk;
  OpenInspectorEditor(path, nullptr);
  auto window = app.WindowAt(0);
  Check(window && window->Lock(), "editor opened for complete document");
  BMessenger editor(window);
  auto body = View<BTextView>(window, "editor text");
  auto find = View<BTextControl>(window, "editor find");
  auto replacement = View<BTextControl>(window, "editor replacement");
  auto line = View<BTextControl>(window, "editor line");
  auto position = View<BStringView>(window, "editor position");
  Check(original == body->Text(),
        "file opened without CR characters in editing buffer");
  window->Unlock();
  Send(editor, 'ieTk');
  Check(window->Lock(), "inspect syntax colours");
  BFont font;
  rgb_color comment, keyword;
  body->GetFontAndColor(0, &font, &comment);
  body->GetFontAndColor(original.find("return"), &font, &keyword);
  Check(comment != keyword,
        "script comments and keywords have distinct colours");
  Check(!body->DoesWordWrap(), "source opens without line wrapping");
  window->Unlock();
  Send(editor, 'ieWr');
  Send(editor, 'ieFn');
  Check(window->Lock(), "find and replace panel");
  Check(body->DoesWordWrap() && !find->IsHidden(),
        "wrap toggled and find bar is visible");
  find->SetText("return");
  replacement->SetText("yield");
  window->Unlock();
  Send(editor, 'ieNx');
  Check(window->Lock(), "find result");
  int32 from, to;
  body->GetSelection(&from, &to);
  Check(original.substr(from, to - from) == "return", "find selects the match");
  window->Unlock();
  Send(editor, 'ieRa');
  Check(window->Lock(), "replacement in memory");
  Check(modified == body->Text() && Read(path) == disk,
        "replace all changes only the draft");
  body->MakeFocus();
  window->Unlock();
  Check(!InspectorEditorsAllowQuit(),
        "unsaved document prevents application quit");
  OpenInspectorEditor(path, nullptr);
  Check(app.CountWindows() == 1,
        "opening the same file preserves its existing draft");

  editor.SendMessage(B_QUIT_REQUESTED);
  BAlert *confirm = nullptr;
  for (int i = 0; i < 100 && !confirm; ++i) {
    for (int32 j = 0; j < app.CountWindows(); ++j)
      if (auto alert = dynamic_cast<BAlert *>(app.WindowAt(j)))
        confirm = alert;
    if (!confirm)
      snooze(20000);
  }
  Check(confirm && confirm->Lock(), "closing a dirty editor asks what to do");
  Check(confirm->ButtonAt(1)->Invoke() == B_OK, "cancel close");
  confirm->Unlock();

  Send(editor, 'ieUn');
  Check(window->Lock(), "undo result");
  Check(original == body->Text(), "one undo restores every replaced match");
  window->Unlock();
  Check(InspectorEditorsAllowQuit(), "undo to original clears dirty state");
  Send(editor, 'ieRe');
  Send(editor, 'ieLn');
  Check(window->Lock(), "line navigation");
  line->SetText("2");
  window->Unlock();
  Send(editor, 'ieJp');
  Send(editor, 'ieTk');
  Check(window->Lock(), "inspect logical line");
  body->GetSelection(&from, &to);
  Check(modified == body->Text() && from == int32(modified.find("yield")) &&
            std::string(position->Text()) == "Ln 2, Col 1",
        "redo and go to logical line work with wrapping enabled");
  const int32 nextLine = modified.find('\n', from);
  body->Select(nextLine, nextLine);
  window->Unlock();
  Send(editor, 'ieTk');
  Check(window->Lock(), "inspect UTF-8 column");
  Check(std::string(position->Text()) == "Ln 2, Col 11",
        "column counts UTF-8 characters, not bytes");
  window->Unlock();
  Send(editor, 'ieSv');
  Check(Read(path) == "#!/bin/python\r\nyield café\r\nyield two\r\n" &&
            InspectorEditorsAllowQuit(),
        "save persists edits, preserves CRLF and permits quit");
  editor.SendMessage(B_QUIT_REQUESTED);
  for (int i = 0; i < 100 && editor.IsValid(); ++i)
    snooze(20000);
  Check(!editor.IsValid(), "clean editor closes");
  unlink(path.c_str());
  rmdir(tmp);
}
