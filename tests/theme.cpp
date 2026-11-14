// SPDX-License-Identifier: MIT
#include <Application.h>
#include <InterfaceDefs.h>
#include <cstring>
#include <initializer_list>
int main(int argc, char **argv) {
  BApplication app("application/x-vnd.Kambuzi-Lister-ThemeTest");
  bool dark = argc > 1 && !strcmp(argv[1], "dark");
  rgb_color bg =
      dark ? rgb_color{40, 43, 47, 255} : rgb_color{216, 216, 216, 255};
  rgb_color fg =
      dark ? rgb_color{235, 235, 235, 255} : rgb_color{20, 20, 20, 255};
  rgb_color doc =
      dark ? rgb_color{30, 33, 37, 255} : rgb_color{255, 255, 255, 255};
  for (auto key : {B_PANEL_BACKGROUND_COLOR, B_CONTROL_BACKGROUND_COLOR,
                   B_MENU_BACKGROUND_COLOR})
    set_ui_color(key, bg);
  for (auto key :
       {B_PANEL_TEXT_COLOR, B_CONTROL_TEXT_COLOR, B_MENU_ITEM_TEXT_COLOR,
        B_DOCUMENT_TEXT_COLOR, B_LIST_ITEM_TEXT_COLOR})
    set_ui_color(key, fg);
  set_ui_color(B_DOCUMENT_BACKGROUND_COLOR, doc);
  set_ui_color(B_LIST_BACKGROUND_COLOR, doc);
  set_ui_color(B_LIST_SELECTED_BACKGROUND_COLOR, {65, 90, 120, 255});
  set_ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR, {255, 255, 255, 255});
  set_ui_color(B_MENU_SELECTED_BACKGROUND_COLOR, {65, 90, 120, 255});
  set_ui_color(B_MENU_SELECTED_ITEM_TEXT_COLOR, {255, 255, 255, 255});
  set_ui_color(B_STATUS_BAR_COLOR, {90, 155, 215, 255});
  return 0;
}
