// SPDX-License-Identifier: MIT
#include "InlineAction.h"
#include "ShelfCollections.h"
#include "ShelfWindow.h"
#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <Entry.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <Window.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Listi;
namespace fs = std::filesystem;
namespace {
struct Fixture {
  fs::path root;
  Fixture() {
    char path[] = "/boot/home/Listi-collections-window-XXXXXX";
    const char *created = mkdtemp(path);
    if (!created)
      throw std::runtime_error("Cannot create UI test fixture.");
    root = fs::canonical(created);
  }
  ~Fixture() {
    std::error_code error;
    fs::remove_all(root, error);
  }
};

void Check(bool success, const char *name) {
  if (!success)
    throw std::runtime_error(name);
  std::cout << "PASS " << name << std::endl;
}

template <typename Condition> bool Until(Condition check) {
  for (int i = 0; i < 125; ++i) {
    if (check())
      return true;
    snooze(20000);
  }
  return false;
}

template <typename CheckView>
bool InWindow(BWindow *window, CheckView check) {
  if (!window || window->LockWithTimeout(20000) != B_OK)
    return false;
  const bool result = check(*window);
  window->Unlock();
  return result;
}

template <class View, typename Accept>
View *Find(BView *view, Accept accept) {
  if (auto *match = dynamic_cast<View *>(view); match && accept(*match))
    return match;
  for (int32 i = 0; i < view->CountChildren(); ++i)
    if (auto *found = Find<View>(view->ChildAt(i), accept))
      return found;
  return nullptr;
}

BButton *Button(BView &root, const char *label) {
  return Find<BButton>(&root, [&](BButton &button) {
    return button.Label() && std::strcmp(button.Label(), label) == 0;
  });
}

bool Click(BWindow *window, const char *label) {
  return InWindow(window, [&](BWindow &locked) {
    for (int32 i = 0; i < locked.CountChildren(); ++i)
      if (auto *button = Button(*locked.ChildAt(i), label))
        return button->IsEnabled() && button->Invoke() == B_OK;
    return false;
  });
}

BMenu *ShelfMenu(BWindow &window, const char *name) {
  auto *bar = dynamic_cast<BMenuBar *>(window.FindView("shelf menu"));
  auto *item = bar ? bar->FindItem(name) : nullptr;
  return item ? item->Submenu() : nullptr;
}

bool Menu(BWindow *window, const char *label, const char *named = nullptr,
          const char *menuName = "Collections") {
  return InWindow(window, [&](BWindow &locked) {
    auto *menu = ShelfMenu(locked, menuName);
    auto *item = menu ? menu->FindItem(label) : nullptr;
    if (named) {
      menu = item ? item->Submenu() : nullptr;
      item = menu ? menu->FindItem(named) : nullptr;
    }
    return item && item->IsEnabled() && static_cast<BInvoker *>(item)->Invoke() == B_OK;
  });
}

bool NarrowShelf(BWindow *window) {
  return InWindow(window, [](BWindow &locked) {
    if (locked.Bounds().Width() > 330)
      return false;
    for (const auto *label : {"Refresh", "Relink...", "Remove from Shelf"}) {
      BButton *button = nullptr;
      for (int32 i = 0; i < locked.CountChildren() && !button; ++i)
        button = Button(*locked.ChildAt(i), label);
      if (!button || button->IsHidden() || !button->Bounds().IsValid() ||
          !locked.Frame().Contains(button->ConvertToScreen(button->Bounds())))
        return false;
    }
    auto *actions = ShelfMenu(locked, "Actions");
    if (!actions)
      return false;
    for (const auto *label : {"Copy all...", "Move all...", "Create archive...",
                              "Clear Shelf"}) {
      auto *item = actions->FindItem(label);
      if (!item || !item->IsEnabled())
        return false;
    }
    return true;
  });
}

bool NamePrompt(BWindow *window, const char *name, const char *buttonLabel) {
  return InWindow(window, [&](BWindow &locked) {
    auto *prompt =
        dynamic_cast<InlineAction *>(locked.FindView("shelf collection prompt"));
    if (!prompt || !prompt->Pending())
      return false;
    auto *field = Find<BTextControl>(prompt, [](const auto &) { return true; });
    auto *button = Button(*prompt, buttonLabel);
    if (!field || !button)
      return false;
    field->SetText(name);
    return button->Invoke() == B_OK;
  });
}

bool PromptClosed(BWindow *window) {
  return InWindow(window, [](BWindow &locked) {
    auto *prompt =
        dynamic_cast<InlineAction *>(locked.FindView("shelf collection prompt"));
    return prompt && !prompt->Pending();
  });
}

bool Alert(BApplication &app, int32 choice) {
  for (int32 i = 0; i < app.CountWindows(); ++i) {
    auto *alert = dynamic_cast<BAlert *>(app.WindowAt(i));
    if (InWindow(alert, [&](BWindow &) {
          return !alert->IsHidden() && alert->ButtonAt(choice) &&
                 alert->ButtonAt(choice)->Invoke() == B_OK;
        }))
      return true;
  }
  return false;
}

bool PickerVisible(BApplication &app) {
  for (int32 i = 0; i < app.CountWindows(); ++i)
    if (InWindow(app.WindowAt(i), [](BWindow &window) {
          const char *title = window.Title();
          return !window.IsHidden() && title &&
                 std::strncmp(title, "Listi: Relink ", 14) == 0;
        }))
      return true;
  return false;
}

bool NoAlerts(BApplication &app) {
  for (int32 i = 0; i < app.CountWindows(); ++i)
    if (dynamic_cast<BAlert *>(app.WindowAt(i)))
      return false;
  return true;
}

bool Status(BWindow *window, const char *prefix) {
  return InWindow(window, [&](BWindow &locked) {
    auto *view = dynamic_cast<BStringView *>(locked.FindView("shelf status"));
    return view && std::strncmp(view->Text(), prefix, std::strlen(prefix)) == 0;
  });
}

bool SelectMissing(BWindow *window) {
  return InWindow(window, [](BWindow &locked) {
    auto *list =
        dynamic_cast<BColumnListView *>(locked.FindView("shelf entries"));
    auto *row = list ? list->RowAt(0) : nullptr;
    auto *field = row ? dynamic_cast<BStringField *>(row->GetField(2)) : nullptr;
    if (!field || std::strcmp(field->String(), "Unavailable") != 0)
      return false;
    list->AddToSelection(row);
    list->SelectionChanged();
    return true;
  });
}

bool SubmitPicker(const BMessenger &window, int64 serial,
                  const fs::path &candidate) {
  entry_ref ref;
  if (BEntry(candidate.c_str(), false).GetRef(&ref) != B_OK)
    return false;
  // Deliver the same callback a confirmed Tracker file panel sends. The UI
  // invokes the real picker first; this avoids depending on Tracker icon order.
  BMessage chosen('sfrc');
  chosen.AddInt64("shelf picker", serial);
  chosen.AddRef("refs", &ref);
  return window.SendMessage(&chosen) == B_OK;
}
}

int main() {
  try {
    BApplication app("application/x-vnd.Listi-ShelfCollectionsWindowTest");
    Fixture fixture;
    const auto original = fixture.root / "original";
    const auto selected = fixture.root / "chosen";
    const auto movedFolder = fixture.root / "moved";
    const auto store = (fixture.root / "isolated-settings").string();
    std::ofstream(original) << "original";
    std::ofstream(selected) << "selected";
    auto shelf = std::make_shared<Shelf>();
    shelf->Add({original.string()});
    BMessenger messenger;
    OpenShelfWindow({}, shelf, messenger, store);
    auto *window = app.WindowAt(0);
    Check(window && messenger.IsValid(), "open actual Shelf window");

    Check(InWindow(window, [](BWindow &locked) {
            locked.ResizeTo(280, locked.Bounds().Height());
            return true;
          }) && Until([&] { return NarrowShelf(window); }),
          "narrow Shelf fits direct controls and keeps every action available");

    Check(Until([&] { return Menu(window, "Save current Shelf..."); }),
          "invoke Save current Shelf after native window resize");
    Check(Until([&] { return NamePrompt(window, "Cancelled", "Cancel"); }),
          "invoke Cancel in the visible naming prompt");
    Check(Until([&] { return PromptClosed(window); }) && !fs::exists(store),
          "cancel name prompt without creating settings");
    Check(Menu(window, "Save current Shelf...") &&
              Until([&] { return NamePrompt(window, "Dinner", "Save"); }) &&
              Until([&] { return Status(window, "Saved collection"); }) &&
              ShelfCollections::Load(store).Find("Dinner"),
          "submit actual save name prompt into isolated settings");

    shelf->Clear();
    shelf->Add({selected.string()});
    Check(Menu(window, "Save current Shelf...") &&
              Until([&] { return NamePrompt(window, "Dinner", "Save"); }) &&
              Until([&] { return Alert(app, 0); }) &&
              Until([&] { return NoAlerts(app) && PromptClosed(window); }),
          "duplicate name opens a cancellable replacement alert");
    const auto stored = ShelfCollections::Load(store);
    Check(stored.Find("Dinner")->entries[0].path == original.string(),
          "cancel does not silently overwrite a saved collection");

    Check(Menu(window, "Load", "Dinner") &&
              Until([&] { return Alert(app, 0); }) &&
              Until([&] { return NoAlerts(app); }) &&
              shelf->Snapshot().entries[0].path == selected.string(),
          "declining load leaves the working Shelf alone");
    Check(Menu(window, "Clear Shelf", nullptr, "Actions") &&
              Until([&] { return shelf->Snapshot().entries.empty(); }),
          "clear current Shelf through its Actions menu");
    fs::create_directory(movedFolder);
    const auto moved = movedFolder / "renamed";
    fs::rename(original, moved);
    Check(Menu(window, "Load", "Dinner") &&
              Until([&] { return Status(window, "Loaded"); }) &&
              Until([&] { return SelectMissing(window); }),
          "load named collection and render its missing member");

    Check(Until([&] { return Click(window, "Relink..."); }) &&
              Until([&] { return PickerVisible(app); }),
          "select missing row and open its relink picker");
    const auto extra = fixture.root / "extra";
    std::ofstream(extra) << "extra";
    shelf->Add({extra.string()});
    Check(SubmitPicker(messenger, 1, selected) &&
              Until([&] { return Status(window, "The Shelf changed."); }) &&
              shelf->Snapshot().entries[0].path == original.string(),
          "refuse a picker callback for a stale Shelf revision");

    Check(Until([&] { return Click(window, "Relink..."); }) &&
              Until([&] { return PickerVisible(app); }) &&
              SubmitPicker(messenger, 2, selected) &&
              Until([&] { return Alert(app, 0); }) &&
              Until([&] { return NoAlerts(app); }) &&
              shelf->Snapshot().entries[0].path == original.string(),
          "cancel explicit relink without replacing the reference");
    Check(Until([&] { return Click(window, "Relink..."); }) &&
              Until([&] { return PickerVisible(app); }) &&
              SubmitPicker(messenger, 3, selected) &&
              Until([&] { return Alert(app, 1); }) &&
              Until([&] { return Status(window, "Reference relinked."); }) &&
              shelf->Snapshot().entries[0].path == selected.string() &&
              fs::exists(moved) && fs::exists(selected),
          "confirm relink explicitly without modifying either file");
    Check(ShelfCollections::Load(store).Find("Dinner")->entries[0].path ==
              original.string(),
          "relink does not silently overwrite the saved collection");
    messenger.SendMessage(B_QUIT_REQUESTED);
    Check(Until([&] { return !messenger.IsValid(); }), "close Shelf window");

    // The model outlives the Shelf window. Reopening must resolve saved
    // references again rather than displaying the last cached "Ready" state.
    fs::rename(moved, original);
    const auto collection = ShelfCollections::Load(store);
    shelf->Load(collection.Find("Dinner")->entries, shelf->Snapshot().revision);
    const auto renamed = fixture.root / "renamed-while-closed";
    fs::rename(original, renamed);
    OpenShelfWindow({}, shelf, messenger, store);
    Check(Until([&] {
            const auto restored = shelf->Snapshot();
            return restored.entries[0].path == renamed.string() &&
                   restored.entries[0].State() == ShelfState::Ready;
          }), "reopening Shelf resolves a rename while its window was closed");
    messenger.SendMessage(B_QUIT_REQUESTED);
    Check(Until([&] { return !messenger.IsValid(); }), "close reopened Shelf");
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
  return 0;
}
