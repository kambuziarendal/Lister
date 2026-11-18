// SPDX-License-Identifier: MIT
#include "FileProperties.h"
#include "Pane.h"
#include "PropertiesWindow.h"
#include <Application.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Node.h>
#include <StringView.h>
#include <TextView.h>
#include <Window.h>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <unistd.h>

namespace {
constexpr const char *kLabel = "Listi:ui-label", *kNumber = "Listi:ui-number";

void Check(bool condition, const char *name) {
  if (!condition)
    throw std::runtime_error(name);
  std::cout << "PASS " << name << std::endl;
}

template <class F> bool Await(F condition) {
  const auto deadline = system_time() + 5000000;
  do {
    if (condition())
      return true;
    snooze(10000);
  } while (system_time() < deadline);
  return false;
}

struct Fixture {
  std::string root, first, second, third;
  Fixture() {
    char pattern[] = "/boot/home/listi-column-ui-XXXXXX";
    if (!mkdtemp(pattern))
      throw std::runtime_error("mkdtemp failed");
    root = pattern;
    first = root + "/first";
    second = root + "/second";
    third = root + "/third";
  }
  ~Fixture() {
    unlink(first.c_str());
    unlink(second.c_str());
    unlink(third.c_str());
    rmdir(root.c_str());
  }
};

struct WindowLock {
  BWindow *window;
  explicit WindowLock(BWindow *owner) : window(owner) {
    if (!window->Lock())
      throw std::runtime_error("could not lock test window");
  }
  ~WindowLock() { window->Unlock(); }
};

class Windows {
  std::vector<BMessenger> windows;

public:
  void Add(BWindow *window) { windows.emplace_back(window); }
  ~Windows() {
    for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
      if (!it->LockTarget())
        continue;
      BLooper *looper = nullptr;
      it->Target(&looper);
      looper->Quit();
    }
  }
};

BButton *FindButton(BView *view, const char *label) {
  if (auto button = dynamic_cast<BButton *>(view))
    if (BString(button->Label()) == label)
      return button;
  for (int32 i = 0; i < view->CountChildren(); ++i)
    if (auto result = FindButton(view->ChildAt(i), label))
      return result;
  return nullptr;
}

bool Click(BWindow &window, const char *label) {
  for (int32 i = 0; i < window.CountChildren(); ++i)
    if (auto button = FindButton(window.ChildAt(i), label))
      return button->IsEnabled() && button->Invoke() == B_OK;
  return false;
}

BColumnListView &List(BView &scope, const char *name) {
  auto list = dynamic_cast<BColumnListView *>(scope.FindView(name));
  if (!list)
    throw std::runtime_error("missing native list");
  return *list;
}

std::string Cell(BRow &row, int32 column) {
  auto field = dynamic_cast<BStringField *>(row.GetField(column));
  return field ? field->String() : "";
}

void SelectAttribute(BColumnListView &list, const char *name) {
  for (int32 i = 0; i < list.CountRows(); ++i) {
    auto row = list.RowAt(i);
    if (Cell(*row, 0) == name) {
      list.DeselectAll();
      list.AddToSelection(row);
      return;
    }
  }
  throw std::runtime_error("attribute not found in Properties");
}

std::set<std::pair<std::string, std::string>> Values(BColumnListView &list) {
  std::set<std::pair<std::string, std::string>> values;
  for (int32 i = 0; i < list.CountRows(); ++i) {
    auto row = list.RowAt(i);
    const auto label = Cell(*row, 3);
    if (!label.empty())
      values.emplace(label, Cell(*row, 4));
  }
  return values;
}

bool ContainsStatus(BWindow &window, const char *text) {
  auto status = dynamic_cast<BStringView *>(window.FindView("property status"));
  return status && BString(status->Text()).FindFirst(text) >= 0;
}
}

int main() {
  BApplication app("application/x-vnd.Listi-AttributeColumnsUITest");
  try {
    Fixture f;
    Windows windows;
    std::ofstream(f.first) << "first";
    std::ofstream(f.second) << "second";
    std::ofstream(f.third) << "third";
    Listi::PropertySession first(f.first), second(f.second);
    first.Save(nullptr, kLabel, B_STRING_TYPE, "alpha");
    first.Save(nullptr, kNumber, B_INT32_TYPE, "-14");
    second.Save(nullptr, kLabel, B_STRING_TYPE, "");

    Preferences prefs;
    prefs.folderModes[f.root] = 1; // Same remembered Icons view in both panes.
    auto left = new Pane(0, prefs);
    auto right = new Pane(1, prefs);
    auto browser = new BWindow(BRect(20, 40, 1000, 520), "Columns UI fixture",
                               B_TITLED_WINDOW, 0);
    BLayoutBuilder::Group<>(browser, B_HORIZONTAL).Add(left).Add(right);
    windows.Add(browser);
    browser->Show();
    {
      WindowLock lock(browser);
      left->Navigate(f.root.c_str());
      right->Navigate(f.root.c_str());
    }
    Check(Await([&] {
            WindowLock lock(browser);
            return !left->IsScanning() && !right->IsScanning() &&
                   left->CurrentView() == 1 && right->CurrentView() == 1 &&
                   f.root == left->Path() && f.root == right->Path();
          }),
          "both panes start in their shared remembered Icons view");

    OpenFileProperties(f.first, BMessenger(left));
    BWindow *properties = nullptr;
    for (int32 i = 0; i < app.CountWindows(); ++i) {
      auto candidate = app.WindowAt(i);
      if (candidate && BString(candidate->Title()) == "Listi: Properties") {
        properties = candidate;
        break;
      }
    }
    Check(properties != nullptr, "actual native Properties window opened");
    windows.Add(properties);
    Check(Await([&] {
            WindowLock lock(properties);
            return ContainsStatus(*properties, "Properties loaded.");
          }),
          "Properties read both stored synthetic attributes");
    {
      WindowLock lock(properties);
      auto attributes = dynamic_cast<BColumnListView *>(
          properties->FindView("property attributes"));
      Check(attributes != nullptr, "Properties exposes its attribute list");
      SelectAttribute(*attributes, kLabel);
      Check(Click(*properties, "Show in Details"),
            "selected string attribute is sent to the originating pane");
    }
    Check(
        Await([&] {
          WindowLock lock(browser);
          auto &list = List(*left, "files");
          return !left->IsScanning() && left->CurrentView() == 0 &&
                 left->PaneOverride() == -1 && right->CurrentView() == 1 &&
                 List(*right, "files").CountColumns() == 3 &&
                 list.CountColumns() == 4 &&
                 Values(list) ==
                     std::set<std::pair<std::string, std::string>>{
                         {"alpha", ""}, {"(empty)", ""}, {"(missing)", ""}};
        }),
        "Details shows stored, empty and absent values without changing the other pane");

    {
      WindowLock lock(properties);
      Check(Click(*properties, "Edit selected"),
            "editable attribute can still be opened");
    }
    Check(
        Await([&] {
          WindowLock lock(properties);
          auto editor = dynamic_cast<BTextView *>(
              properties->FindView("attribute value"));
          if (!editor || !editor->IsEditable() ||
              BString(editor->Text()) != "alpha")
            return false;
          editor->SetText("edited");
          auto attributes = static_cast<BColumnListView *>(
              properties->FindView("property attributes"));
          SelectAttribute(*attributes, kNumber);
          return Click(*properties, "Show in Details");
        }),
        "a second column can be requested without discarding an open edit draft");
    Check(
        Await([&] {
          WindowLock lock(browser);
          auto &list = List(*left, "files");
          if (left->IsScanning() || list.CountColumns() != 5)
            return false;
          return Values(list) == std::set<std::pair<std::string, std::string>>{
                                     {"alpha", "-14"},
                                     {"(empty)", "(missing)"},
                                     {"(missing)", "(missing)"}};
        }),
        "both columns show the matching typed attributes for each native row");
    {
      WindowLock lock(browser);
      auto &list = List(*left, "files");
      auto attribute = list.ColumnAt(4);
      list.SetSortColumn(attribute, false, false);
      BMessage state;
      list.SaveState(&state);
      Check(state.GetInt32("sortID", -1) == 4,
            "the native Details list is sorted by an attribute column");
      auto display = dynamic_cast<BMenuField *>(left->FindView("display mode"));
      auto clear = display
                       ? display->Menu()->FindItem("Clear attribute columns")
                       : nullptr;
      Check(clear && clear->Invoke() == B_OK,
            "clear via the actual pane menu item");
    }
    Check(
        Await([&] {
          WindowLock lock(browser);
          auto &list = List(*left, "files");
          BMessage state;
          list.SaveState(&state);
          return !left->IsScanning() && left->CurrentView() == 1 &&
                 right->CurrentView() == 1 && list.CountColumns() == 3 &&
                 state.GetInt32("sortID", -1) == 0 &&
                 List(*right, "files").CountColumns() == 3;
        }),
        "clearing sorted columns restores the remembered view and a valid native sort");

    BNode node(f.first.c_str());
    Check(Listi::AttributeCell(node, kLabel) == "alpha",
          "showing, sorting and clearing columns did not write the draft");
    {
      WindowLock lock(properties);
      auto editor =
          dynamic_cast<BTextView *>(properties->FindView("attribute value"));
      Check(editor && editor->IsEditable() &&
                BString(editor->Text()) == "edited",
            "editable Properties draft remains intact");
      Check(Click(*properties, "Save attribute"),
            "draft saves through Properties");
    }
    Check(Await([&] {
            WindowLock lock(properties);
            return ContainsStatus(*properties, "Properties loaded.") &&
                   Listi::AttributeCell(node, kLabel) == "edited" &&
                   Listi::AttributeCell(node, kNumber) == "-14";
          }),
          "saved draft edits only its original attribute");
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << std::endl;
    return 1;
  }
}
