// SPDX-License-Identifier: MIT
#include "ThemeTextView.h"
#include "WrappedLabel.h"
#include <Application.h>
#include <LayoutBuilder.h>
#include <Window.h>
#include <cstdio>
#include <initializer_list>

static bool CheckWrappedLabel() {
  auto *window = new BWindow(BRect(10, 10, 310, 210), "Wrapped label test",
                             B_TITLED_WINDOW, 0);
  auto *label = new WrappedLabel("status", "Ready");
  BLayoutBuilder::Group<>(window, B_VERTICAL).AddGlue().Add(label, 0);
  window->Layout(true);
  const float singleLine = label->Bounds().Height();
  const char *message =
      "Cannot open: /boot/home/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
      "abcdefghijklmnopqrstuvwxyz. Check the server address and try again.";
  label->SetMessage(message);
  window->Layout(false);
  const float narrowHeight = label->Bounds().Height();
  const bool grew = narrowHeight > singleLine && label->CountLines() > 1 &&
                    label->TextRect().bottom <= narrowHeight + 1 &&
                    std::strcmp(label->Text(), message) == 0;
  window->ResizeTo(600, 200);
  window->Layout(true);
  const bool reflowed = label->Bounds().Height() < narrowHeight;
  label->SetMessage("Ready");
  window->Layout(false);
  const bool shrank = label->Bounds().Height() == singleLine &&
                      label->CountLines() == 1;
  window->Quit();
  const bool ok = grew && reflowed && shrank;
  printf("%s status wraps without clipping and returns to one line\n",
         ok ? "PASS" : "FAIL");
  return ok;
}

int main() {
  BApplication app("application/x-vnd.Kambuzi-ThemeTextTests");
  auto window = new BWindow(BRect(10,10,410,310), "Theme text test", B_TITLED_WINDOW, 0);
  auto text = new ThemeTextView("text", B_DOCUMENT_BACKGROUND_COLOR, B_DOCUMENT_TEXT_COLOR);
  window->AddChild(text);
  const rgb_color correct = ui_color(B_DOCUMENT_TEXT_COLOR);
  rgb_color wrong = {uint8(255-correct.red), uint8(255-correct.green), uint8(255-correct.blue),255};
  text->SetText("Earlier operation\nLater operation\n");
  text->SetFontAndColor(0, text->TextLength(), nullptr, B_FONT_ALL, &wrong);
  text->Select(0,0);
  BMessage changed(B_COLORS_UPDATED);
  text->MessageReceived(&changed);
  BFont font; rgb_color actual;
  int failures = 0;
  for (int32 at : {int32(0), int32(19)}) {
    text->GetFontAndColor(at, &font, &actual);
    bool ok = actual == correct;
    printf("%s existing text run at %ld follows current system palette\n", ok?"PASS":"FAIL", long(at));
    failures += !ok;
  }
  int32 appended = text->TextLength();
  text->Insert("New operation\n");
  text->GetFontAndColor(appended, &font, &actual);
  bool ok = actual == correct;
  printf("%s appended text follows current system palette\n", ok?"PASS":"FAIL");
  failures += !ok;
  window->Quit();
  failures += !CheckWrappedLabel();
  return failures ? 1 : 0;
}
