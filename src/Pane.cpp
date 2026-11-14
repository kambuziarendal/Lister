// SPDX-License-Identifier: MIT
// The private ColumnListView API is isolated to this translation unit.
#include "Pane.h"
#include "FileOperation.h"
#include <Bitmap.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <Directory.h>
#include <Entry.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <NodeInfo.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <Window.h>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
struct NameField : BStringField {
  int kind;
  BBitmap *icon = nullptr;
  bool parent = false;
  NameField(const char *s, int kind) : BStringField(s), kind(kind) {}
  ~NameField() { delete icon; }
};
struct FileRow : BRow {
  std::string path;
  entry_ref ref;
  bool directory, parent;
  FileRow(float height) : BRow(height) {}
};
class NameColumn : public BStringColumn {
public:
  Preferences prefs;
  NameColumn(const Preferences &p)
      : BStringColumn("Name", 260, 80, 1200, B_TRUNCATE_MIDDLE), prefs(p) {}
  void DrawField(BField *f, BRect r, BView *v) override {
    auto n = static_cast<NameField *>(f);
    rgb_color old = v->HighColor();
    if (n->parent)
      v->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
    else
      v->SetHighColor(prefs.Color(n->kind == 1   ? prefs.folders
                                  : n->kind == 2 ? prefs.links
                                                 : prefs.files));
    if (prefs.icons) {
      if (n->icon) {
        v->SetDrawingMode(B_OP_ALPHA);
        v->DrawBitmap(n->icon,
                      BPoint(r.left + 3, r.top + (r.Height() - 16) / 2));
        v->SetDrawingMode(B_OP_COPY);
      }
      r.left += 23;
    }
    BStringColumn::DrawField(f, r, v);
    v->SetHighColor(old);
  }
  int CompareFields(BField *a, BField *b) override {
    auto x = static_cast<NameField *>(a), y = static_cast<NameField *>(b);
    if (x->parent != y->parent)
      return x->parent ? -1 : 1;
    if ((x->kind == 1) != (y->kind == 1))
      return x->kind == 1 ? -1 : 1;
    return strcasecmp(x->String(), y->String());
  }
};
class SizeColumn : public BSizeColumn {
public:
  SizeColumn() : BSizeColumn("Size", 85, 50, 200) {}
  void DrawField(BField *field, BRect rect, BView *view) override {
    if (static_cast<BSizeField *>(field)->Size() >= 0)
      BSizeColumn::DrawField(field, rect, view);
  }
};
class Scanner : public BLooper {
  BMessenger target;
  std::shared_ptr<std::atomic<int64>> generation;

public:
  Scanner(BMessenger t, std::shared_ptr<std::atomic<int64>> g)
      : BLooper("directory reader"), target(t), generation(g) {}
  void MessageReceived(BMessage *m) override {
    if (m->what != kScan) {
      BLooper::MessageReceived(m);
      return;
    }
    int64 serial = m->GetInt64("serial", 0);
    BString path = m->GetString("path", "");
    bool icons = m->GetBool("icons", true);
    BMessage batch(kRows);
    batch.AddInt64("serial", serial);
    int count = 0;
    auto send = [&]() {
      target.SendMessage(&batch, (BHandler *)nullptr, 500000);
      batch.MakeEmpty();
      batch.what = kRows;
      batch.AddInt64("serial", serial);
      count = 0;
    };
    auto row = [&](const char *full, const char *label, bool parent = false) {
      struct stat st;
      if (lstat(full, &st))
        return;
      BMessage item;
      item.AddString("name", label);
      item.AddString("path", full);
      item.AddBool("parent", parent);
      bool link = S_ISLNK(st.st_mode);
      bool dir = S_ISDIR(st.st_mode);
      struct stat resolved;
      if (link && stat(full, &resolved) == 0)
        dir = S_ISDIR(resolved.st_mode);
      item.AddInt32("kind", link ? 2 : dir ? 1 : 0);
      item.AddBool("directory", dir);
      item.AddInt64("size", st.st_size);
      item.AddInt64("date", st.st_mtime);
      entry_ref ref;
      BEntry e(full, false);
      if (e.GetRef(&ref) == B_OK) {
        item.AddRef("ref", &ref);
        if (icons) {
          BBitmap bitmap(BRect(0, 0, 15, 15), B_CMAP8);
          if (BNodeInfo::GetTrackerIcon(&ref, &bitmap, B_MINI_ICON) == B_OK)
            item.AddData("icon", B_RAW_TYPE, bitmap.Bits(),
                         bitmap.BitsLength());
        }
      }
      batch.AddMessage("entry", &item);
      if (++count == 80)
        send();
    };
    BMessage done(kScanDone);
    done.AddInt64("serial", serial);
    if (path == "@disks") {
      BVolumeRoster roster;
      BVolume volume;
      while (generation->load() == serial &&
             roster.GetNextVolume(&volume) == B_OK) {
        BDirectory root;
        BEntry e;
        BPath p;
        char name[B_FILE_NAME_LENGTH];
        if (volume.GetRootDirectory(&root) == B_OK &&
            root.GetEntry(&e) == B_OK && e.GetPath(&p) == B_OK &&
            volume.GetName(name) == B_OK)
          row(p.Path(), name);
      }
    } else {
      char resolved[PATH_MAX];
      if (!realpath(path.String(), resolved)) {
        done.AddString("error", strerror(errno));
      } else {
        path = resolved;
        DIR *dir = opendir(path.String());
        if (!dir)
          done.AddString("error", strerror(errno));
        else {
          if (path != "/") {
            std::string parent = path.String();
            parent = parent.substr(0, parent.find_last_of('/'));
            if (parent.empty())
              parent = "/";
            row(parent.c_str(), "..", true);
          }
          while (generation->load() == serial) {
            auto e = readdir(dir);
            if (!e)
              break;
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
              continue;
            row(Lister::Join(path.String(), e->d_name).c_str(), e->d_name);
          }
          closedir(dir);
          BNode node(path.String());
          node_ref ref;
          if (node.GetNodeRef(&ref) == B_OK)
            done.AddData("node", B_RAW_TYPE, &ref, sizeof(ref));
        }
      }
    }
    if (generation->load() == serial) {
      if (count)
        send();
      done.AddString("path", path);
      target.SendMessage(&done, (BHandler *)nullptr, 500000);
    }
  }
};
} // namespace
class FileList : public BColumnListView {
public:
  Pane *pane;
  NameColumn *name;
  FileList(Pane *p, const Preferences &prefs)
      : BColumnListView("files", B_WILL_DRAW | B_NAVIGABLE, B_FANCY_BORDER),
        pane(p) {
    name = new NameColumn(prefs);
    AddColumn(name, 0);
    AddColumn(new SizeColumn(), 1);
    AddColumn(new BDateColumn("Modified", 160, 80, 280), 2);
    SetSelectionMode(B_MULTIPLE_SELECTION_LIST);
    SetSortColumn(name, false, true);
    SetInvocationMessage(new BMessage(kInvoke));
    SetSelectionMessage(new BMessage(kSelection));
  }
  void MakeFocus(bool f = true) override {
    BColumnListView::MakeFocus(f);
    if (f)
      pane->Activate();
  }
  void SelectionChanged() override {
    BColumnListView::SelectionChanged();
    if (!pane->IsScanning())
      pane->Activate();
  }
  void KeyDown(const char *b, int32 n) override {
    if (n == 1 && b[0] == B_TAB) {
      Window()->PostMessage(kSwitch);
      return;
    }
    if (n == 1 && b[0] == B_BACKSPACE) {
      BMessage parent(kParent);
      pane->MessageReceived(&parent);
      return;
    }
    if (n == 1 && b[0] == ' ') {
      auto r = FocusRow();
      if (r) {
        bool selected = false;
        for (auto s = CurrentSelection(); s; s = CurrentSelection(s))
          if (r == s)
            selected = true;
        if (selected)
          Deselect(r);
        else
          AddToSelection(r);
      }
      return;
    }
    BColumnListView::KeyDown(b, n);
  }
  bool InitiateDrag(BPoint p, bool) override {
    BMessage m(B_SIMPLE_DATA);
    for (auto r = CurrentSelection(); r; r = CurrentSelection(r)) {
      auto f = static_cast<FileRow *>(r);
      if (!f->parent)
        m.AddRef("refs", &f->ref);
    }
    if (!m.HasRef("refs"))
      return false;
    DragMessage(&m, BRect(p.x, p.y, p.x + 90, p.y + 22));
    return true;
  }
  void MessageDropped(BMessage *m, BPoint) override {
    BMessage drop(*m);
    drop.what = kDrop;
    drop.AddString("destination", pane->Path());
    Window()->PostMessage(&drop);
  }
};
Pane::Pane(int32 number, const Preferences &p)
    : BView("pane", B_WILL_DRAW | B_SUPPORTS_LAYOUT), id(number), prefs(p),
      generation(std::make_shared<std::atomic<int64>>(0)) {
  SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
  SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
  role = new BButton("role", "Destination", new BMessage(kActivate));
  auto menu = new BPopUpMenu("Go");
  const char *names[] = {"Parent", "Root", "Home", "Desktop", "Disks"};
  uint32 codes[] = {kParent, kRoot, kHome, kDesktop, kDisks};
  for (int i = 0; i < 5; i++)
    menu->AddItem(new BMenuItem(names[i], new BMessage(codes[i])));
  auto field = new BMenuField("go", "", menu);
  menu->SetRadioMode(false);
  menu->SetLabelFromMarked(false);
  edit = new BTextControl("path", "", path, new BMessage(kNavigate));
  edit->SetExplicitMinSize(BSize(120, B_SIZE_UNSET));
  list = new FileList(this, p);
  list->SetExplicitMinSize(BSize(260, 180));
  status = new BStringView("status", "Ready");
  status->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
  BLayoutBuilder::Group<>(this, B_VERTICAL, 5)
      .SetInsets(6)
      .AddGroup(B_HORIZONTAL, 5)
      .Add(role)
      .Add(field)
      .End()
      .Add(edit)
      .Add(list)
      .Add(status);
}
Pane::~Pane() {
  generation->store(-1);
  stop_watching(this);
  if (scanner)
    scanner->PostMessage(B_QUIT_REQUESTED);
}
void Pane::AttachedToWindow() {
  role->SetTarget(this);
  edit->SetTarget(this);
  list->SetTarget(this);
  auto field = dynamic_cast<BMenuField *>(FindView("go"));
  field->Menu()->SetTargetForItems(this);
  scanner = new Scanner(BMessenger(this), generation);
  scanner->Run();
  Apply(prefs);
  Navigate(path);
}
void Pane::Activate() {
  if (Window()) {
    BMessage m(kActivate);
    m.AddInt32("pane", id);
    Window()->PostMessage(&m);
  }
}
void Pane::SetSource(bool source) {
  role->SetLabel(source ? "SOURCE" : "DESTINATION");
  role->SetValue(source ? B_CONTROL_ON : B_CONTROL_OFF);
  role->SetFont(source ? be_bold_font : be_plain_font);
}
void Pane::FocusList() { list->MakeFocus(true); }
void Pane::Apply(const Preferences &p) {
  prefs = p;
  list->name->prefs = p;
  list->SetFont(&p.font);
  list->SetColumnVisible(1, p.size);
  list->SetColumnVisible(2, p.date);
  list->SetColor(B_COLOR_BACKGROUND, ui_color(B_PANEL_BACKGROUND_COLOR));
  list->SetColor(B_COLOR_TEXT, ui_color(B_PANEL_TEXT_COLOR));
  list->SetColor(B_COLOR_SELECTION,
                 tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 0.80));
  list->SetColor(B_COLOR_SELECTION_TEXT, ui_color(B_PANEL_TEXT_COLOR));
  list->SetColor(B_COLOR_NON_FOCUS_SELECTION,
                 tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 0.80));
  list->Invalidate();
}
void Pane::Navigate(const char *p) {
  if (!scanner)
    return;
  std::string request = p;
  if (request.empty())
    return;
  if (request[0] != '/' && request != "@disks")
    request = Lister::Join(path.String(), request);
  stop_watching(this);
  savedSelection =
      (path == request.c_str()) ? Selected() : std::vector<std::string>{};
  path = request.c_str();
  edit->SetText(path);
  scanning = true;
  refreshPending = false;
  serial++;
  generation->store(serial);
  list->Clear();
  status->SetText("Reading directory...");
  BMessage m(kScan);
  m.AddString("path", path);
  m.AddInt64("serial", serial);
  m.AddBool("icons", prefs.icons);
  scanner->PostMessage(&m);
}
void Pane::Reload() {
  if (scanning) {
    refreshPending = true;
    return;
  }
  Navigate(path);
}
std::vector<std::string> Pane::Selected() const {
  std::vector<std::string> out;
  for (auto r = list->CurrentSelection(); r; r = list->CurrentSelection(r)) {
    auto f = static_cast<FileRow *>(r);
    if (!f->parent)
      out.push_back(f->path);
  }
  return out;
}
void Pane::Select(int mode) {
  if (mode == 0) {
    list->DeselectAll();
    return;
  }
  auto selected = Selected();
  for (int32 i = 0; i < list->CountRows(); i++) {
    auto row = static_cast<FileRow *>(list->RowAt(i));
    if (row->parent)
      continue;
    bool had = std::find(selected.begin(), selected.end(), row->path) !=
               selected.end();
    if (mode == 2 && had)
      list->Deselect(row);
    else
      list->AddToSelection(row);
  }
}
void Pane::SendSelection() {
  BString s;
  s << int32(Selected().size()) << " selected / " << list->CountRows()
    << " entries";
  status->SetText(s);
}
void Pane::MessageReceived(BMessage *m) {
  switch (m->what) {
  case kActivate:
    Activate();
    FocusList();
    break;
  case kNavigate:
    Navigate(edit->Text());
    break;
  case kParent: {
    std::string p = path.String();
    if (p == "@disks")
      p = "/";
    if (p.size() > 1 && p.back() == '/')
      p.pop_back();
    auto i = p.find_last_of('/');
    p = i == 0 || i == std::string::npos ? "/" : p.substr(0, i);
    Navigate(p.c_str());
    break;
  }
  case kRoot:
    Navigate("/");
    break;
  case kHome:
    Navigate("/boot/home");
    break;
  case kDesktop:
    Navigate("/boot/home/Desktop");
    break;
  case kDisks:
    Navigate("@disks");
    break;
  case kSelection:
    if (!scanning)
      SendSelection();
    break;
  case kInvoke: {
    auto r = static_cast<FileRow *>(list->FocusRow());
    if (!r)
      r = static_cast<FileRow *>(list->CurrentSelection());
    if (r) {
      if (r->directory)
        Navigate(r->path.c_str());
      else {
        BMessage open(kOpenPaths);
        open.AddString("path", r->path.c_str());
        Window()->PostMessage(&open);
      }
    }
    break;
  }
  case kRows: {
    if (m->GetInt64("serial", -1) != serial)
      break;
    BMessage e;
    for (int i = 0; m->FindMessage("entry", i, &e) == B_OK; i++) {
      auto row = new FileRow(std::max(22.f, prefs.font.Size() + 9));
      row->path = e.GetString("path", "");
      row->directory = e.GetBool("directory", false);
      row->parent = e.GetBool("parent", false);
      e.FindRef("ref", &row->ref);
      auto name = new NameField(e.GetString("name", ""), e.GetInt32("kind", 0));
      name->parent = row->parent;
      const void *bits;
      ssize_t length;
      if (e.FindData("icon", B_RAW_TYPE, &bits, &length) == B_OK &&
          length == 256) {
        name->icon = new BBitmap(BRect(0, 0, 15, 15), B_CMAP8);
        memcpy(name->icon->Bits(), bits, length);
      }
      row->SetField(name, 0);
      row->SetField(new BSizeField(row->directory ? -1 : e.GetInt64("size", 0)),
                    1);
      time_t date = e.GetInt64("date", 0);
      row->SetField(new BDateField(&date), 2);
      list->AddRow(row);
      if (std::find(savedSelection.begin(), savedSelection.end(), row->path) !=
          savedSelection.end())
        list->AddToSelection(row);
    }
    break;
  }
  case kScanDone: {
    if (m->GetInt64("serial", -1) != serial)
      break;
    scanning = false;
    path = m->GetString("path", path);
    edit->SetText(path);
    const char *error;
    if (m->FindString("error", &error) == B_OK) {
      BString s("Cannot open: ");
      s << error;
      status->SetText(s);
    } else {
      SendSelection();
      const void *p;
      ssize_t n;
      if (m->FindData("node", B_RAW_TYPE, &p, &n) == B_OK &&
          n == sizeof(node_ref))
        watch_node(static_cast<const node_ref *>(p), B_WATCH_ALL, this);
    }
    if (refreshPending)
      Reload();
    break;
  }
  case B_NODE_MONITOR:
    Reload();
    break;
  case B_COLORS_UPDATED:
    Apply(prefs);
    break;
  default:
    BView::MessageReceived(m);
    break;
  }
}
