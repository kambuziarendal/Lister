// SPDX-License-Identifier: MIT
#include "InspectorEditor.h"
#include "ExplicitSubmit.h"
#include "InspectorDocument.h"
#include "InspectorSyntax.h"

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <Clipboard.h>
#include <GroupLayout.h>
#include <GroupView.h>
#include <Input.h>
#include <LayoutBuilder.h>
#include <Layout.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace {
enum : uint32 {
  kSave = 'ieSv',
  kUndo = 'ieUn',
  kRedo = 'ieRe',
  kFind = 'ieFn',
  kNext = 'ieNx',
  kReplace = 'ieRp',
  kReplaceAll = 'ieRa',
  kLine = 'ieLn',
  kJump = 'ieJp',
  kHideBar = 'ieHb',
  kClose = 'ieCl',
  kWrap = 'ieWr',
  kTick = 'ieTk',
  kFocus = 'ieFc'
};
struct EditorShortcut {
  char key;
  uint32 action;
  uint32 modifiers = 0;
};

constexpr EditorShortcut editorShortcuts[] = {
    {'S', kSave}, {'W', kClose}, {'Z', kUndo},   {'Z', kRedo, B_SHIFT_KEY},
    {'X', B_CUT}, {'C', B_COPY}, {'V', B_PASTE}, {'A', B_SELECT_ALL},
    {'F', kFind}, {'G', kNext},  {'L', kLine}};

std::mutex editorsLock;
class InspectorEditor;
class EditorWindow;

struct OpenEditor {
  InspectorEditor *view;
  BMessenger target;
};
std::map<std::string, OpenEditor> editors;

// The embedded menu must not become the file window's keyboard menu.
class EditorMenuBar : public BMenuBar {
public:
  bool embedded = false;

  EditorMenuBar() : BMenuBar("editor menus") {}

  void AttachedToWindow() override {
    auto previous = Window()->KeyMenuBar();
    BMenuBar::AttachedToWindow();
    if (embedded)
      Window()->SetKeyMenuBar(previous);
  }
};

class BeforeEditorDispatch : public BMessageFilter {
public:
  explicit BeforeEditorDispatch(InspectorEditor *editor)
      : BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE), editor(editor) {}

  filter_result Filter(BMessage *message, BHandler **) override;

private:
  InspectorEditor *editor;
};

struct Change {
  int32 offset;
  bool insert;
  std::string bytes;
};
using Edit = std::vector<Change>;

// Keep the changes made by one native input event together. This includes
// selection replacement, paste, drag edits and our own replace-all action.
class EditorText : public BTextView {
public:
  EditorText() : BTextView("editor text") {}
  bool recording = false;
  bool composing = false;
  void MessageReceived(BMessage *message) override {
    if (message->what == B_INPUT_METHOD_EVENT) {
      const int32 op = message->GetInt32("be:opcode", -1);
      if (op == B_INPUT_METHOD_STARTED)
        composing = true;
      else if (op == B_INPUT_METHOD_STOPPED ||
               (op == B_INPUT_METHOD_CHANGED &&
                message->GetBool("be:confirmed", false)))
        composing = false;
    }
    BTextView::MessageReceived(message);
  }
  Edit pending;
  uint64 revision = 0;

  std::string_view Content() const {
    return {Text(), static_cast<size_t>(TextLength())};
  }

protected:
  void InsertText(const char *text, int32 size, int32 at,
                  const text_run_array *) override {
    if (recording && size > 0)
      pending.push_back({at, true, std::string(text, size)});
    // Files are plain text. Never import font runs from a rich clipboard.
    BTextView::InsertText(text, size, at, nullptr);
    ++revision;
  }

  void DeleteText(int32 from, int32 to) override {
    if (recording && to > from)
      pending.push_back({from, false, std::string(Text() + from, to - from)});
    BTextView::DeleteText(from, to);
    ++revision;
  }
};

BButton *Button(const char *label, uint32 action) {
  return new BButton(label, new BMessage(action));
}

BStringView *FlexibleLabel(const char *name, const char *text) {
  auto label = new BStringView(name, text);
  label->SetTruncation(B_TRUNCATE_MIDDLE);
  label->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
  label->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
  return label;
}

class InspectorEditor : public BGroupView {
  const std::string path, leaf;
  std::unique_ptr<Inspector::Document> document;
  Inspector::Syntax::Family language;
  EditorText *body;
  BGroupView *findBar, *lineBar;
  BTextControl *find, *replacement, *line;
  BStringView *position, *feedback, *location;
  BButton *saveButton;
  EditorMenuBar *menu;
  BHandler *closeTarget;
  BeforeEditorDispatch *dispatch = nullptr;
  Listi::ExplicitSubmit *submit[3] = {};
  BMenuItem *undoItem, *redoItem, *wrapItem, *saveItem;
  std::deque<Edit> undo, redo;
  size_t undoBytes = 0;
  bool dirty = false, needsColour = true;
  uint64 lastRevision = 0, feedbackRevision = 0;
  int32 lastPosition = -1;
  std::unique_ptr<BMessageRunner> timer;

  bool Changed() const { return body->Content() != document->Text(); }

  void Status(const std::string &message) {
    feedbackRevision = body->revision;
    feedback->SetText(message.c_str());
    feedback->SetToolTip(message.c_str());
  }

  void UpdateDirty() {
    dirty = Changed();
    if (IsEmbedded())
      location->SetText((path + (dirty ? " *" : "")).c_str());
    else if (Window())
      Window()->SetTitle(("Listi: " + leaf + (dirty ? " *" : "")).c_str());
    saveButton->SetEnabled(dirty);
    saveItem->SetEnabled(dirty);
  }

  static size_t Cost(const Edit &edit) {
    size_t bytes = 0;
    for (const auto &change : edit)
      bytes += sizeof(Change) + change.bytes.size();
    return bytes;
  }

  void FinishEdit() {
    if (body->pending.empty() || body->composing)
      return;
    if (feedbackRevision != body->revision)
      Status("");
    undoBytes += Cost(body->pending);
    undo.push_back(std::move(body->pending));
    body->pending.clear();
    redo.clear();
    while (undo.size() > 1 &&
           (undo.size() > 512 || undoBytes > 16 * 1024 * 1024)) {
      undoBytes -= Cost(undo.front());
      undo.pop_front();
    }
    UpdateDirty();
  }

  void ApplyHistory(bool forward) {
    auto &from = forward ? redo : undo;
    auto &to = forward ? undo : redo;
    if (from.empty())
      return;
    auto &edit = from.back();
    const size_t bytes = Cost(edit);
    int32 caret = 0;
    body->recording = false;
    auto apply = [&](const Change &change) {
      caret = change.offset;
      const int32 size = static_cast<int32>(change.bytes.size());
      if (forward == change.insert) {
        body->Insert(caret, change.bytes.data(), size);
        caret += size;
      } else {
        body->Delete(caret, caret + size);
      }
    };
    if (forward) {
      for (const auto &change : edit)
        apply(change);
      undoBytes += bytes;
    } else {
      for (auto it = edit.rbegin(); it != edit.rend(); ++it)
        apply(*it);
      undoBytes -= bytes;
    }
    body->recording = true;
    to.push_back(std::move(edit));
    from.pop_back();
    Select(caret, caret);
    UpdateDirty();
    Status(forward ? "Redone" : "Undone");
  }

  void Select(int32 from, int32 to) {
    body->Select(from, to);
    body->ScrollToSelection();
  }

  bool Save() {
    try {
      if (Changed()) {
        document->Save(std::string(body->Content()));
        BMessage changed('isav');
        changed.AddString("path", path.c_str());
        be_app->PostMessage(&changed);
      }
      UpdateDirty();
      Status("Saved");
      return true;
    } catch (const std::exception &e) {
      const std::string message = "Could not save " + leaf + ".\n\n" + e.what();
      (new BAlert("Save failed", message.c_str(), "OK", nullptr, nullptr,
                  B_WIDTH_AS_USUAL, B_WARNING_ALERT))
          ->Go(nullptr);
      Status("Not saved. Your edits are still open.");
      return false;
    }
  }

  void Colour() {
    using namespace Inspector::Syntax;
    const auto content = body->Content();
    const rgb_color normal = ui_color(B_DOCUMENT_TEXT_COLOR);
    const rgb_color background = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
    // Do not feed thousands of BFont objects into the native layout on large
    // files. Plain formatting never limits editing, navigation or saving.
    auto runs = ColourRuns(content, content.size() > 128 * 1024 ? Family::Plain
                                                                : language);
    if (runs.size() > 4096)
      runs = {{0, Ink::Text}};
    const bool dark = background.Brightness() < 128;
    const rgb_color palette[] = {
        normal,
        dark ? rgb_color{143, 193, 151, 255} : rgb_color{52, 105, 61, 255},
        dark ? rgb_color{243, 177, 135, 255} : rgb_color{146, 65, 23, 255},
        dark ? rgb_color{159, 192, 255, 255} : rgb_color{45, 75, 154, 255},
        dark ? rgb_color{217, 168, 240, 255} : rgb_color{120, 52, 142, 255}};
    text_run_array *styles = BTextView::AllocRunArray(runs.size());
    if (!styles)
      return;
    for (size_t i = 0; i < runs.size(); ++i) {
      styles->runs[i].offset = runs[i].offset;
      styles->runs[i].font = *be_fixed_font;
      styles->runs[i].color = palette[static_cast<size_t>(runs[i].ink)];
    }
    body->SetRunArray(0, body->TextLength(), styles);
    BTextView::FreeRunArray(styles);
    needsColour = false;
  }

  void Tick() {
    const bool changed = lastRevision != body->revision;
    // SetRunArray cancels active input methods in BTextView.
    if (changed)
      needsColour = true;
    if (needsColour && !body->composing)
      Colour();
    int32 from, to;
    body->GetSelection(&from, &to);
    if (changed || lastPosition != from) {
      const auto prefix = body->Content().substr(0, from);
      const size_t number = 1 + std::count(prefix.begin(), prefix.end(), '\n');
      const auto onLine = prefix.substr(prefix.rfind('\n') + 1);
      const size_t column = 1 + std::count_if(onLine.begin(), onLine.end(),
                                              [](unsigned char byte) {
                                                return (byte & 0xc0) != 0x80;
                                              });
      const std::string label =
          "Ln " + std::to_string(number) + ", Col " + std::to_string(column);
      position->SetText(label.c_str());
      lastPosition = from;
    }
    lastRevision = body->revision;
  }

  void ShowBar(BGroupView *active) {
    for (auto bar : {findBar, lineBar}) {
      if (bar == active && bar->IsHidden())
        bar->Show();
      if (bar != active && !bar->IsHidden())
        bar->Hide();
    }
    if (!active) {
      body->MakeFocus();
      return;
    }
    BTextControl *field = active == findBar ? find : line;
    field->MakeFocus();
    field->TextView()->SelectAll();
  }

  bool FindNext() {
    const std::string_view needle = find->Text(), source = body->Content();
    if (needle.empty()) {
      ShowBar(findBar);
      Status("Enter text to find.");
      return false;
    }
    int32 from, to;
    body->GetSelection(&from, &to);
    size_t match = source.find(needle, to);
    const bool wrapped = match == source.npos;
    if (wrapped)
      match = source.find(needle);
    if (match == source.npos) {
      Status("No match");
      return false;
    }
    body->MakeFocus();
    Select(match, match + needle.size());
    Status(wrapped ? "Found from start" : "Match found");
    return true;
  }

  void Replace(bool all) {
    const std::string_view needle = find->Text(), value = replacement->Text();
    const auto source = body->Content();
    if (needle.empty()) {
      ShowBar(findBar);
      Status("Enter text to find.");
      return;
    }
    if (all) {
      std::string result;
      size_t count = 0, start = 0, match;
      while ((match = source.find(needle, start)) != source.npos) {
        if (result.size() + match - start + value.size() >
            Inspector::kDocumentLimit) {
          Status(
              "Replacement exceeds the document size limit. Nothing changed.");
          return;
        }
        result.append(source.substr(start, match - start));
        result.append(value);
        ++count;
        start = match + needle.size();
      }
      if (count == 0) {
        Status("No match");
        return;
      }
      if (result.size() + source.size() - start > Inspector::kDocumentLimit) {
        Status("Replacement exceeds the document size limit. Nothing changed.");
        return;
      }
      result.append(source.substr(start));
      if (source != result)
        body->SetText(result.data(), result.size());
      body->MakeFocus();
      Status("Replaced " + std::to_string(count) +
             (count == 1 ? " match" : " matches"));
      return;
    }
    int32 from, to;
    body->GetSelection(&from, &to);
    if (source.substr(from, to - from) != needle) {
      if (!FindNext())
        return;
      body->GetSelection(&from, &to);
    }
    if (source.size() - needle.size() + value.size() >
        Inspector::kDocumentLimit) {
      Status("Replacement exceeds the document size limit. Nothing changed.");
      return;
    }
    body->Delete(from, to);
    body->Insert(from, value.data(), value.size());
    Select(from + value.size(), from + value.size());
    if (!FindNext())
      Status("Replaced. No more matches.");
  }

  void GoToLine() {
    const std::string_view input = line->Text(), source = body->Content();
    unsigned wanted = 0;
    auto parsed =
        std::from_chars(input.data(), input.data() + input.size(), wanted);
    if (parsed.ec != std::errc() || parsed.ptr != input.data() + input.size() ||
        !wanted) {
      Status("Enter a line number starting at 1.");
      return;
    }
    size_t offset = 0;
    for (unsigned number = 1; number < wanted; ++number) {
      offset = source.find('\n', offset);
      if (offset == source.npos) {
        Status("The document has only " + std::to_string(number) +
               (number == 1 ? " line." : " lines."));
        return;
      }
      ++offset;
    }
    ShowBar(nullptr);
    Select(offset, offset);
    Status("");
  }

  EditorMenuBar *BuildMenu() {
    auto bar = new EditorMenuBar;
    bar->embedded = closeTarget != nullptr;
    auto file = new BMenu("File");
    saveItem = new BMenuItem("Save", new BMessage(kSave));
    file->AddItem(saveItem);
    file->AddItem(new BMenuItem("Close", new BMessage(kClose)));
    auto edit = new BMenu("Edit");
    undoItem = new BMenuItem("Undo", new BMessage(kUndo));
    redoItem = new BMenuItem("Redo", new BMessage(kRedo));
    edit->AddItem(undoItem);
    edit->AddItem(redoItem);
    edit->AddSeparatorItem();
    edit->AddItem(new BMenuItem("Cut", new BMessage(B_CUT)));
    edit->AddItem(new BMenuItem("Copy", new BMessage(B_COPY)));
    edit->AddItem(new BMenuItem("Paste", new BMessage(B_PASTE)));
    edit->AddItem(new BMenuItem("Select all", new BMessage(B_SELECT_ALL)));
    auto search = new BMenu("Search");
    search->AddItem(new BMenuItem("Find and replace…", new BMessage(kFind)));
    search->AddItem(new BMenuItem("Find next", new BMessage(kNext)));
    search->AddItem(new BMenuItem("Go to line…", new BMessage(kLine)));
    auto view = new BMenu("View");
    wrapItem = new BMenuItem("Wrap lines", new BMessage(kWrap));
    view->AddItem(wrapItem);
    for (auto menu : {file, edit, search, view})
      bar->AddItem(menu);
    return bar;
  }

public:
  InspectorEditor(const std::string &file,
                  std::unique_ptr<Inspector::Document> source, BHandler *owner)
      : BGroupView("inspector editor", B_VERTICAL, 0), path(file),
        leaf(file.substr(file.rfind('/') + 1)), document(std::move(source)),
        language(Inspector::Syntax::Language(path, document->Text())),
        closeTarget(owner) {
    menu = BuildMenu();
    body = new EditorText;
    body->SetFontAndColor(be_fixed_font);
    body->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    body->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
    body->SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
    body->SetStylable(true);
    body->SetDoesUndo(false);
    body->SetWordWrap(language == Inspector::Syntax::Family::Plain ||
                      language == Inspector::Syntax::Family::Markdown);
    wrapItem->SetMarked(body->DoesWordWrap());
    body->SetInsets(8, 6, 8, 6);
    body->SetTabWidth(be_fixed_font->StringWidth("    "));
    body->SetText(document->Text().c_str());
    auto scroll =
        new BScrollView("editor scroll", body, 0, true, true, B_NO_BORDER);
    scroll->SetExplicitMinSize(BSize(360, 140));
    scroll->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));

    find = new BTextControl("editor find", "Find:", "", nullptr);
    replacement =
        new BTextControl("editor replacement", "Replace with:", "", nullptr);
    line = new BTextControl("editor line", "Line:", "", nullptr);
    for (auto field : {find, replacement, line})
      field->SetDivider(be_plain_font->StringWidth(field->Label()) + 8);
    findBar = new BGroupView("editor find bar", B_VERTICAL, 4);
    BLayoutBuilder::Group<>(findBar, B_VERTICAL, 4)
        .SetInsets(8, 3, 8, 3)
        .AddGroup(B_HORIZONTAL, 6)
        .Add(find)
        .Add(Button("Find next", kNext))
        .Add(Button("Done", kHideBar))
        .End()
        .AddGroup(B_HORIZONTAL, 6)
        .Add(replacement)
        .Add(Button("Replace", kReplace))
        .Add(Button("Replace all", kReplaceAll))
        .End();
    lineBar = new BGroupView("editor line bar", B_HORIZONTAL);
    BLayoutBuilder::Group<>(lineBar, B_HORIZONTAL, 6)
        .SetInsets(8, 3, 8, 3)
        .Add(line)
        .Add(Button("Go", kJump))
        .Add(Button("Done", kHideBar));

    feedback = FlexibleLabel("editor status", "");
    position = new BStringView("editor position", "Ln 1, Col 1");
    saveButton = Button("Save", kSave);
    location = FlexibleLabel("editor path", path.c_str());
    location->SetToolTip(path.c_str());
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu)
        .AddGroup(B_HORIZONTAL)
        .SetInsets(8, 4, 8, 4)
        .Add(location)
        .End()
        .Add(scroll, 1)
        .Add(findBar)
        .Add(lineBar)
        .AddGroup(B_HORIZONTAL, 8)
        .SetInsets(8, 4, 8, 4)
        .Add(position)
        .Add(feedback)
        .Add(saveButton)
        .End();
    findBar->Hide();
    lineBar->Hide();
    UpdateDirty();
    body->recording = true;
  }

  void AddStandaloneShortcuts() {
    for (int32 i = 0; i < menu->CountItems(); ++i) {
      auto submenu = menu->SubmenuAt(i);
      for (int32 j = 0; j < submenu->CountItems(); ++j) {
        auto item = submenu->ItemAt(j);
        auto message = item->Message();
        if (message)
          for (const auto &shortcut : editorShortcuts)
            if (shortcut.action == message->what)
              item->SetShortcut(shortcut.key, shortcut.modifiers);
      }
    }
  }

  ~InspectorEditor() override {
    std::lock_guard<std::mutex> lock(editorsLock);
    auto found = editors.find(path);
    if (found != editors.end() && found->second.view == this)
      editors.erase(found);
  }

  void AllAttached() override {
    BGroupView::AllAttached();
    BMessenger target(this);
    auto fields = {find, replacement, line};
    const uint32 actions[] = {kNext, kReplace, kJump};
    int index = 0;
    for (auto field : fields) {
      submit[index] =
          new Listi::ExplicitSubmit(target, BMessage(actions[index]));
      field->TextView()->AddFilter(submit[index]);
      ++index;
    }
    for (auto group : {findBar, lineBar})
      for (int32 i = 0; i < group->CountChildren(); ++i)
        if (auto button = dynamic_cast<BButton *>(group->ChildAt(i)))
          button->SetTarget(target);
    saveButton->SetTarget(target);
    for (int32 i = 0; i < menu->CountItems(); ++i)
      menu->SubmenuAt(i)->SetTargetForItems(target);
    dispatch = new BeforeEditorDispatch(this);
    Window()->AddCommonFilter(dispatch);
    BMessage tick(kTick);
    timer = std::make_unique<BMessageRunner>(target, &tick, 250000);
    UpdateDirty();
    body->MakeFocus();
  }

  void DetachedFromWindow() override {
    timer.reset();
    Window()->RemoveCommonFilter(dispatch);
    delete dispatch;
    dispatch = nullptr;
    int index = 0;
    for (auto field : {find, replacement, line}) {
      field->TextView()->RemoveFilter(submit[index]);
      delete submit[index];
      submit[index++] = nullptr;
    }
    BGroupView::DetachedFromWindow();
  }

  const std::string &Path() const { return path; }

  bool IsEmbedded() const { return closeTarget != nullptr; }

  void MakeStandalone() {
    closeTarget = nullptr;
    menu->embedded = false;
    location->SetText(path.c_str());
    AddStandaloneShortcuts();
  }

  void BeforeDispatch() {
    FinishEdit();
    bool canUndo = !undo.empty(), canRedo = !redo.empty();
    auto text = FocusedText();
    if (text && text != body) {
      bool isRedo = false;
      bool available = text->UndoState(&isRedo) != B_UNDO_UNAVAILABLE;
      canUndo = available && !isRedo;
      canRedo = available && isRedo;
    }
    undoItem->SetEnabled(canUndo);
    redoItem->SetEnabled(canRedo);
  }

  BTextView *FocusedText() const {
    auto text = dynamic_cast<BTextView *>(Window()->CurrentFocus());
    for (BView *view = text; view; view = view->Parent())
      if (view == this)
        return text;
    return nullptr;
  }

  void Raise() {
    if (Window()->IsMinimized())
      Window()->Minimize(false);
    Window()->Activate();
    body->MakeFocus();
  }

  bool AllowQuit() {
    if (!Changed())
      return true;
    Raise();
    Status("Save your changes or close this editor before quitting Listi.");
    return false;
  }

  bool AllowClose() {
    if (!Changed())
      return true;
    const std::string prompt = "Save changes to " + leaf + " before closing?";
    auto alert =
        new BAlert("Unsaved changes", prompt.c_str(), "Discard", "Cancel",
                   "Save", B_WIDTH_AS_USUAL, B_WARNING_ALERT);
    alert->SetShortcut(1, B_ESCAPE);
    const int32 choice = alert->Go();
    return choice == 0 || (choice == 2 && Save());
  }

  bool HandleKey(const BMessage &key) {
    const char *bytes = key.GetString("bytes", "");
    const int32 modifiers = key.GetInt32("modifiers", 0);
    uint32 action = 0;
    if (bytes[0] == B_ESCAPE && !(modifiers & B_COMMAND_KEY)) {
      action = !IsEmbedded() || !findBar->IsHidden() || !lineBar->IsHidden()
                   ? kHideBar
                   : kClose;
    } else if ((modifiers & (B_COMMAND_KEY | B_CONTROL_KEY | B_OPTION_KEY)) ==
               B_COMMAND_KEY) {
      const int32 raw = key.GetInt32("raw_char", bytes[0]);
      const int command = std::toupper(static_cast<unsigned char>(raw));
      const uint32 shift = modifiers & B_SHIFT_KEY;
      if (command == 'Q' || (IsEmbedded() && command == 'P' && shift))
        return false;
      for (const auto &shortcut : editorShortcuts)
        if (shortcut.key == command && shortcut.modifiers == shift)
          action = shortcut.action;
      if (!action)
        return IsEmbedded();
    }
    if (action == 0)
      return false;
    Window()->PostMessage(action, this);
    return true;
  }

  void MessageReceived(BMessage *m) override {
    if (!HandleMessage(m))
      BGroupView::MessageReceived(m);
  }

  bool HandleMessage(BMessage *m) {
    switch (m->what) {
    case kClose:
      if (closeTarget)
        closeTarget->Looper()->PostMessage(kInspectorEmbeddedClose,
                                           closeTarget);
      else
        Window()->PostMessage(B_QUIT_REQUESTED);
      return true;
    case kSave:
      Save();
      return true;
    case kUndo:
    case kRedo: {
      auto text = FocusedText();
      if (text && text != body) {
        bool isRedo = false;
        if (text->UndoState(&isRedo) != B_UNDO_UNAVAILABLE &&
            isRedo == (m->what == kRedo))
          text->Undo(be_clipboard);
      } else
        ApplyHistory(m->what == kRedo);
      return true;
    }
    case B_CUT:
    case B_COPY:
    case B_PASTE:
    case B_SELECT_ALL:
      if (auto text = FocusedText())
        Window()->PostMessage(m, text);
      return true;
    case kFind:
      ShowBar(findBar);
      return true;
    case kNext:
      FindNext();
      return true;
    case kReplace:
      Replace(false);
      return true;
    case kReplaceAll:
      Replace(true);
      return true;
    case kLine:
      ShowBar(lineBar);
      return true;
    case kJump:
      GoToLine();
      return true;
    case kHideBar:
      ShowBar(nullptr);
      return true;
    case kWrap:
      body->SetWordWrap(!body->DoesWordWrap());
      wrapItem->SetMarked(body->DoesWordWrap());
      return true;
    case kTick:
      Tick();
      return true;
    case kFocus:
      Raise();
      return true;
    case B_COLORS_UPDATED:
      needsColour = true;
      break;
    }
    return false;
  }
};

filter_result BeforeEditorDispatch::Filter(BMessage *message, BHandler **) {
  editor->BeforeDispatch();
  if (message->what == B_KEY_DOWN && !editor->IsEmbedded() &&
      message->GetString("bytes", "")[0] == B_ESCAPE &&
      editor->HandleKey(*message))
    return B_SKIP_MESSAGE;
  return B_DISPATCH_MESSAGE;
}

class EditorWindow : public BWindow {
public:
  EditorWindow(BWindow *owner)
      : BWindow(BRect(110, 110, 870, 620), "Listi: Editor", B_TITLED_WINDOW,
                B_AUTO_UPDATE_SIZE_LIMITS) {
    SetLayout(new BGroupLayout(B_VERTICAL));
    if (owner)
      CenterIn(owner->Frame());
    MoveOnScreen();
  }

  void Mount(InspectorEditor *view) {
    editor = view;
    GetLayout()->AddView(editor);
  }

  bool QuitRequested() override { return editor->AllowClose(); }

  void MessageReceived(BMessage *message) override {
    if (!editor->HandleMessage(message))
      BWindow::MessageReceived(message);
  }

private:
  InspectorEditor *editor = nullptr;
};

InspectorEditor *CreateEditor(const std::string &path, BWindow *owner,
                              BView *parent, BHandler *closeTarget) {
  try {
    std::unique_lock<std::mutex> lock(editorsLock);
    auto found = editors.find(path);
    if (found != editors.end()) {
      BMessenger existing = found->second.target;
      lock.unlock();
      existing.SendMessage(kFocus);
      return nullptr;
    }
    auto source = std::make_unique<Inspector::Document>(path);
    auto editor = new InspectorEditor(path, std::move(source), closeTarget);
    EditorWindow *window = nullptr;
    if (parent) {
      if (auto layout = parent->GetLayout())
        layout->AddView(editor);
      else
        parent->AddChild(editor);
    } else {
      window = new EditorWindow(owner);
      editor->AddStandaloneShortcuts();
      window->Mount(editor);
    }
    editors.emplace(path, OpenEditor{editor, BMessenger(editor)});
    lock.unlock();
    if (window)
      window->Show();
    return editor;
  } catch (const std::exception &e) {
    const std::string message =
        "Could not open " + path + " for editing.\n\n" + e.what();
    (new BAlert("Open editor", message.c_str(), "OK", nullptr, nullptr,
                B_WIDTH_AS_USUAL, B_WARNING_ALERT))
        ->Go(nullptr);
    return nullptr;
  }
}
} // namespace

void OpenInspectorEditor(const std::string &path, BWindow *owner) {
  CreateEditor(path, owner, nullptr, nullptr);
}

BView *OpenEmbeddedInspectorEditor(const std::string &path, BView *parent,
                                   BHandler *closeTarget) {
  return CreateEditor(path, parent->Window(), parent, closeTarget);
}

bool InspectorEditorAllowClose(BView *editor) {
  return static_cast<InspectorEditor *>(editor)->AllowClose();
}

bool DetachInspectorEditor(BView *view) {
  auto editor = static_cast<InspectorEditor *>(view);
  if (!editor->IsEmbedded())
    return false;
  auto window = new EditorWindow(editor->Window());
  {
    std::lock_guard<std::mutex> lock(editorsLock);
    editor->RemoveSelf();
    editor->MakeStandalone();
    window->Mount(editor);
    editors.at(editor->Path()).target = BMessenger(editor);
  }
  window->Show();
  return true;
}

bool HandleInspectorEditorKey(BView *focus, const BMessage &key) {
  for (; focus; focus = focus->Parent())
    if (auto editor = dynamic_cast<InspectorEditor *>(focus))
      return editor->HandleKey(key);
  return false;
}

bool InspectorEditorsAllowQuit() {
  std::vector<BMessenger> targets;
  {
    std::lock_guard<std::mutex> lock(editorsLock);
    for (const auto &entry : editors)
      targets.push_back(entry.second.target);
  }
  for (auto &target : targets) {
    if (!target.IsValid())
      continue;
    // Quit can originate in the same window that owns an embedded editor.
    // Lock the target (recursive for that window) rather than send and wait.
    if (target.LockTargetWithTimeout(500000) != B_OK)
      return false;
    BLooper *looper = nullptr;
    auto editor = dynamic_cast<InspectorEditor *>(target.Target(&looper));
    const bool allowed =
        editor && editor->Looper() == looper && editor->AllowQuit();
    looper->Unlock();
    if (!allowed)
      return false;
  }
  return true;
}
