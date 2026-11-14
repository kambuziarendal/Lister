// SPDX-License-Identifier: MIT
// Only this renderer depends on Haiku's private ColumnListView API.
#include "FileDetails.h"
#include "Pane.h"
#include "PaneModel.h"
#include "VirtualFile.h"
#include <Bitmap.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <Entry.h>
#include <LayoutBuilder.h>
#include <Window.h>
#include <algorithm>
#include <map>
#include <set>
#include <strings.h>
namespace {
struct NameField : BStringField {
  PaneEntry *entry;
  explicit NameField(PaneEntry *e) : BStringField(e->name.c_str()), entry(e) {}
};
struct FileRow : BRow {
  PaneEntry *entry;
  FileRow(PaneEntry *e, float height) : BRow(height), entry(e) {}
};
class NameColumn : public BStringColumn {
public:
  Preferences prefs;
  NameColumn(const Preferences &p)
      : BStringColumn("Name", 260, 80, 1200, B_TRUNCATE_MIDDLE), prefs(p) {}
  void DrawField(BField *f, BRect r, BView *v) override {
    auto n = static_cast<NameField *>(f);
    rgb_color old = v->HighColor();
    if (n->entry->parent)
      v->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
    else
      v->SetHighColor(prefs.Color(n->entry->kind == 1   ? prefs.folders
                                  : n->entry->kind == 2 ? prefs.links
                                                 : prefs.files));
    if (prefs.icons) {
      if (n->entry->icon) {
        v->SetDrawingMode(B_OP_ALPHA);
        float top = r.top + (r.Height() - 16) / 2;
        v->DrawBitmap(n->entry->icon.get(), n->entry->icon->Bounds(),
                      BRect(r.left + 3, top, r.left + 18, top + 15));
        v->SetDrawingMode(B_OP_COPY);
      }
      r.left += 23;
    }
    BStringColumn::DrawField(f, r, v);
    if (n->entry->transferState) {
      rgb_color color = ui_color(n->entry->transferState == 2   ? B_SUCCESS_COLOR
                                 : n->entry->transferState == 3 ? B_FAILURE_COLOR
                                                         : B_STATUS_BAR_COLOR);
      v->SetHighColor(tint_color(color, n->entry->transferState == 1 ? 1.2f : 1.f));
      float width = std::max(
          5.f, r.Width() * (n->entry->transferState == 1 ? n->entry->fraction : 1.f));
      v->FillRect(BRect(r.left, r.bottom - 2, r.left + width, r.bottom));
    }
    v->SetHighColor(old);
  }
  int CompareFields(BField *a, BField *b) override {
    auto x = static_cast<NameField *>(a), y = static_cast<NameField *>(b);
    if (x->entry->parent != y->entry->parent)
      return x->entry->parent ? -1 : 1;
    if ((x->entry->kind == 1) != (y->entry->kind == 1))
      return x->entry->kind == 1 ? -1 : 1;
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

class DetailsList : public BColumnListView {
public:
  Pane *pane;
  PaneModel *model;
  NameColumn *name;
  bool syncing = false;
  DetailsList(Pane *p, PaneModel *state, const Preferences &prefs)
      : BColumnListView("files", B_WILL_DRAW | B_NAVIGABLE, B_FANCY_BORDER),
        pane(p), model(state) {
    name = new NameColumn(prefs);
    AddColumn(name, 0);
    AddColumn(new SizeColumn(), 1);
    AddColumn(new BDateColumn("Modified", 160, 80, 280), 2);
    SetSelectionMode(B_MULTIPLE_SELECTION_LIST);
    SetSortColumn(name, false, true);
    SetInvocationMessage(new BMessage(kInvoke));
  }
  void AttachedToWindow() override {
    BColumnListView::AttachedToWindow();
    SetTarget(this);
  }
  void MessageReceived(BMessage *message) override {
    if (message->what == kInvoke) {
      auto row = static_cast<FileRow *>(FocusRow());
      model->SetSelection(model->Selected(), row ? row->entry->path : "");
      pane->MessageReceived(message);
      return;
    }
    BColumnListView::MessageReceived(message);
  }
  void MakeFocus(bool focus = true) override {
    BColumnListView::MakeFocus(focus);
    if (focus)
      pane->Activate();
  }
  void SelectionChanged() override {
    BColumnListView::SelectionChanged();
    if (syncing)
      return;
    std::vector<std::string> selected;
    for (auto r = CurrentSelection(); r; r = CurrentSelection(r)) {
      auto row = static_cast<FileRow *>(r);
      if (!row->entry->parent)
        selected.push_back(row->entry->path);
    }
    auto focus = static_cast<FileRow *>(FocusRow());
    model->SetSelection(selected, focus ? focus->entry->path : "");
    if (!pane->IsScanning()) {
      pane->Activate();
      pane->SendSelection();
    }
  }
  void KeyDown(const char *bytes, int32 length) override {
    if (length == 1 && bytes[0] == B_TAB) {
      Window()->PostMessage(kSwitch);
      return;
    }
    if (length == 1 && bytes[0] == B_BACKSPACE) {
      BMessage parent(kParent);
      pane->MessageReceived(&parent);
      return;
    }
    if (length == 1 && bytes[0] == ' ') {
      auto row = static_cast<FileRow *>(FocusRow());
      if (row) {
        if (model->IsSelected(row->entry->path))
          Deselect(row);
        else
          AddToSelection(row);
      }
      return;
    }
    BColumnListView::KeyDown(bytes, length);
  }
  bool InitiateDrag(BPoint point, bool) override {
    BMessage drag(B_SIMPLE_DATA);
    for (const auto &path : model->Selected()) {
      entry_ref ref;
      if (!Lister::IsVirtual(path) && get_ref_for_path(path.c_str(), &ref) == B_OK)
        drag.AddRef("refs", &ref);
      else
        drag.AddString("listi:path", path.c_str());
    }
    if (!drag.HasRef("refs") && !drag.HasString("listi:path"))
      return false;
    DragMessage(&drag, BRect(point.x, point.y, point.x + 90, point.y + 22));
    return true;
  }
  void MessageDropped(BMessage *message, BPoint) override {
    BMessage drop(*message);
    drop.what = kDrop;
    drop.AddString("destination", pane->Path());
    Window()->PostMessage(&drop);
  }
};
class DetailsView : public FileDetails {
  PaneModel *model;
  DetailsList *list;
  std::map<std::string, FileRow *> rows;
  Preferences prefs;
public:
  DetailsView(Pane *pane, PaneModel *state, const Preferences &p)
      : FileDetails("file details", B_WILL_DRAW | B_SUPPORTS_LAYOUT),
        model(state), prefs(p) {
    list = new DetailsList(pane, state, p);
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0).Add(list);
  }
  ~DetailsView() { list->syncing = true; }
  void MakeFocus(bool focus = true) override { list->MakeFocus(focus); }
  void Redraw() override { list->Invalidate(); }
  void Apply(const Preferences &p) override {
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
    Redraw();
  }
  void Refresh() override {
    list->syncing = true;
    std::set<std::string> visible;
    for (auto entry : model->Visible())
      visible.insert(entry->path);
    for (auto it = rows.begin(); it != rows.end();) {
      if (!visible.count(it->first)) {
        list->RemoveRow(it->second);
        delete it->second;
        it = rows.erase(it);
      } else
        ++it;
    }
    for (auto entry : model->Visible()) {
      if (rows.count(entry->path))
        continue;
      auto row = new FileRow(entry, std::max(22.f, prefs.font.Size() + 9));
      row->SetField(new NameField(entry), 0);
      row->SetField(new BSizeField(entry->directory ? -1 : entry->size), 1);
      time_t date = entry->modified;
      row->SetField(new BDateField(&date), 2);
      rows.emplace(entry->path, row);
      list->AddRow(row);
    }
    list->syncing = false;
    SyncSelection();
  }
  void SyncSelection() override {
    if (list->syncing)
      return;
    list->syncing = true;
    auto focused = model->Focused();
    std::set<BRow *> selected;
    for (auto row = list->CurrentSelection(); row; row = list->CurrentSelection(row))
      selected.insert(row);
    for (auto &item : rows) {
      bool want = model->IsSelected(item.first);
      if (want && !selected.count(item.second))
        list->AddToSelection(item.second);
      else if (!want && selected.count(item.second))
        list->Deselect(item.second);
      if (focused && focused->path == item.first)
        list->SetFocusRow(item.second);
    }
    list->syncing = false;
    Redraw();
  }
};
} // namespace
FileDetails *CreateFileDetails(Pane *pane, PaneModel *model,
                               const Preferences &prefs) {
  return new DetailsView(pane, model, prefs);
}
