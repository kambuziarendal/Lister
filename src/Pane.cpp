// SPDX-License-Identifier: MIT
#include "Pane.h"
#include "DirectoryScanner.h"
#include "FileDetails.h"
#include "FileGrid.h"
#include "FileOperation.h"
#include "FolderTree.h"
#include "ImagePreview.h"
#include "NameFilter.h"
#include "Trash.h"
#include "VirtualFile.h"
#include <Bitmap.h>
#include <CardLayout.h>
#include <CardView.h>
#include <Directory.h>
#include <Entry.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <MessageFilter.h>
#include <MessageRunner.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <TextView.h>
#include <Window.h>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <map>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

namespace {
class PaneInput : public BMessageFilter {
  Pane *pane;
  bool isFilter;

public:
  PaneInput(Pane *p, bool filter)
      : BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE), pane(p),
        isFilter(filter) {}
  filter_result Filter(BMessage *message, BHandler **) override {
    if (message->what == B_MOUSE_DOWN || message->what == B_KEY_DOWN)
      pane->Activate();
    if (message->what == B_KEY_DOWN) {
      const char *bytes = message->GetString("bytes", "");
      if (isFilter && bytes[0] == B_ESCAPE) {
        BMessage clear(kClearFilter);
        pane->MessageReceived(&clear);
        return B_SKIP_MESSAGE;
      }
    }
    return B_DISPATCH_MESSAGE;
  }
};
constexpr uint32 kRememberView = 'grem',
                 kForgetView = 'gfor', kExpireProgress = 'expr',
                 kToggleTree = 'ttre';
} // namespace
Pane::Pane(int32 number, const Preferences &p)
    : BView("pane", B_WILL_DRAW | B_SUPPORTS_LAYOUT), id(number), prefs(p),
      generation(std::make_shared<std::atomic<int64>>(0)) {
  SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
  role = new BStringView("role", id == 0 ? "Left panel" : "Right panel");
  role->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
  auto menu = new BPopUpMenu("Go");
  const char *names[] = {"Parent",  "Root",  "Home",
                         "Desktop", "Disks", "Trash (this volume)"};
  uint32 codes[] = {kParent, kRoot, kHome, kDesktop, kDisks, kTrashFolder};
  for (int i = 0; i < 6; i++)
    menu->AddItem(new BMenuItem(names[i], new BMessage(codes[i])));
  menu->AddSeparatorItem();
  placesMenu = new BMenu("Places");
  menu->AddItem(placesMenu);
  UpdatePlaces();
  auto field = new BMenuField("go", "", menu);
  menu->SetRadioMode(false);
  menu->SetLabelFromMarked(false);
  edit = new BTextControl("path", "", path, new BMessage(kNavigate));
  back = new BButton("back", "<", new BMessage(kBack));
  forward = new BButton("forward", ">", new BMessage(kForward));
  for (auto button : {back, forward}) {
    button->SetExplicitMinSize(BSize(32, B_SIZE_UNSET));
    button->SetExplicitMaxSize(BSize(32, B_SIZE_UNSET));
  }
  back->SetToolTip("Back in this panel (Command+[ while browsing)");
  forward->SetToolTip("Forward in this panel (Command+] while browsing)");
  UpdateNavigation();
  edit->SetExplicitMinSize(BSize(120, B_SIZE_UNSET));
  filter = new BTextControl("filter", "Filter:", "", new BMessage(kFilterDone));
  filter->SetModificationMessage(new BMessage(kFilterChanged));
  filter->SetExplicitMinSize(BSize(120, B_SIZE_UNSET));
  filter->SetToolTip("Show names containing this text. Enter returns to the "
                     "list; Escape clears.");
  clearFilter =
      new BButton("clear filter", "Clear", new BMessage(kClearFilter));
  clearFilter->SetEnabled(false);
  list = CreateFileDetails(this, &entries, p);
  list->SetExplicitMinSize(BSize(220, 96));
  grid = CreateFileGrid(this, &entries);
  auto gridScroll = new BScrollView("icons scroll", grid, 0, false, true);
  cards = new BCardView("panel views");
  cards->AddChild(list);
  cards->AddChild(gridScroll);
  auto modes = new BPopUpMenu("View");
  const char *modeNames[] = {"Details", "Icons", "Large icons", "Thumbnails"};
  for (int32 i = 0; i < 4; ++i) {
    auto message = new BMessage(kGridMode);
    message->AddInt32("mode", i);
    modes->AddItem(new BMenuItem(modeNames[i], message));
  }
  modes->AddSeparatorItem();
  modes->AddItem(new BMenuItem("Remember view for this folder",
                               new BMessage(kRememberView)));
  modes->AddItem(
      new BMenuItem("Use default for this folder", new BMessage(kForgetView)));
  modes->SetRadioMode(false);
  modes->SetLabelFromMarked(false);
  display = new BMenuField("display mode", "", modes);
  modes->AddSeparatorItem();
  modes->AddItem(new BMenuItem("Folder tree", new BMessage(kToggleTree)));
  tree = new FolderTree();
  content = new BSplitView(B_HORIZONTAL, 4);
  contentSplitterSize = content->SplitterSize();
  content->AddChild(tree);
  content->AddChild(cards);
  content->SetCollapsible(0, false);
  content->SetCollapsible(1, false);
  status = new BStringView("status", "Ready");
  status->SetTruncation(B_TRUNCATE_END);
  status->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
  status->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
  header = new BGroupView(B_HORIZONTAL, 5);
  header->SetName("panel toolbar");
  header->AddChild(role);
  header->AddChild(field);
  header->AddChild(display);
  filterRow = new BGroupView(B_HORIZONTAL, 5);
  filterRow->SetName("filter toolbar");
  filterRow->AddChild(filter);
  filterRow->AddChild(clearFilter);
  BLayoutBuilder::Group<>(this, B_VERTICAL, 5)
      .SetInsets(6)
      .Add(header, 0)
      .AddGroup(B_HORIZONTAL, 3, 0)
      .Add(back)
      .Add(forward)
      .Add(edit)
      .End()
      .Add(filterRow, 0)
      .Add(content)
      .Add(status, 0);
  SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
}
Pane::~Pane() {
  generation->store(-1);
  stop_watching(this);
  if (scanner)
    scanner->PostMessage(B_QUIT_REQUESTED);
}
void Pane::AttachedToWindow() {
  display->Menu()->SetTargetForItems(this);
  edit->SetTarget(this);
  back->SetTarget(this);
  forward->SetTarget(this);
  filter->SetTarget(this);
  clearFilter->SetTarget(this);
  filter->TextView()->AddFilter(new PaneInput(this, true));
  edit->TextView()->AddFilter(new PaneInput(this, false));
  auto field = dynamic_cast<BMenuField *>(FindView("go"));
  field->Menu()->SetTargetForItems(this);
  placesMenu->SetTargetForItems(this);
  scanner = CreateDirectoryScanner(BMessenger(this), generation);
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
  active = source;
  BString label(simple ? "Files" : id == 0 ? "Left panel" : "Right panel");
  if (source && !simple)
    label << " (active)";
  role->SetText(label);
  role->SetHighUIColor(B_PANEL_TEXT_COLOR);
  role->SetFont(source ? be_bold_font : be_plain_font);
}
void Pane::SetDisplayMode(int32 value) {
  mode = std::max(0, std::min(3, value));
  cards->CardLayout()->SetVisibleItem(mode == 0 ? 0 : 1);
  grid->SetMode(mode);
  auto menu = display->Menu();
  for (int i = 0; i < 4; ++i)
    menu->ItemAt(i)->SetMarked(mode == i);
  menu->Superitem()->SetLabel(menu->ItemAt(mode)->Label());
  menu->ItemAt(5)->SetMarked(prefs.folderModes.count(path.String()));
  menu->ItemAt(6)->SetEnabled(prefs.folderModes.count(path.String()));
}
void Pane::PublishDisplay(bool folder, bool forget) {
  BMessage changed(kPaneDisplay);
  if (folder) {
    changed.AddString("folder", path);
    changed.AddInt32("mode", mode);
    changed.AddBool("forget", forget);
  }
  Window()->PostMessage(&changed);
}
void Pane::FocusList() {
  if (mode == 0)
    list->MakeFocus(true);
  else
    grid->MakeFocus(true);
}
void Pane::FocusFilter() {
  if (simple) return;
  Activate();
  filter->MakeFocus(true);
  filter->TextView()->SelectAll();
}
void Pane::ClearRows() {
  entries.Clear();
  list->Refresh();
  grid->Reset();
}
void Pane::ApplyFilter() {
  entries.Filter(simple ? "" : filter->Text());
  list->Refresh();
  clearFilter->SetEnabled(filter->Text()[0] != 0);
  if (!scanning)
    SendSelection();
}
void Pane::SetSimple(bool value) {
  const bool changed = simple != value;
  simple = value;
  prefs.simple = value;
  if (chromeVisible == simple) {
    chromeVisible = !simple;
    for (BView* control : {static_cast<BView*>(header), static_cast<BView*>(filterRow),
                          static_cast<BView*>(back), static_cast<BView*>(forward)}) {
      if (chromeVisible) control->Show(); else control->Hide();
    }
  }
  auto layout = static_cast<BGroupLayout*>(GetLayout());
  layout->SetInsets(simple ? 2 : 6);
  layout->SetSpacing(simple ? 2 : 5);
  content->SetSpacing(simple ? 0 : 4);
  content->SetSplitterSize(simple ? 0 : contentSplitterSize);
  // A hidden filter must not silently hide entries in the minimal workspace.
  // Keep its text for Commander, but suspend its effect while it is hidden.
  if (changed) { ApplyFilter(); grid->Update(); }
  const bool visible = prefs.paneTree[id] && !simple;
  if (treeVisible != visible) {
    treeVisible = visible;
    if (visible) tree->Show(); else tree->Hide();
  }
  if (visible) tree->ShowPath(path.String());
  display->Menu()->ItemAt(8)->SetMarked(treeVisible);
  display->Menu()->ItemAt(8)->SetEnabled(!simple);
  SetSource(active);
}
void Pane::Apply(const Preferences &p) {
  prefs = p;
  SetSimple(p.simple);
  tree->ShowPath(path.String());
  auto view = prefs.folderModes.find(path.String());
  SetDisplayMode(view == prefs.folderModes.end() ? prefs.paneMode[id]
                                                 : view->second);
  list->Apply(p);
  grid->SetFont(&p.font);
  grid->Update();
}
void Pane::Navigate(const char *p) {
  if (!scanner)
    return;
  std::string request = p;
  if (request.empty())
    return;
  try {
    if (request[0] != '/' && request != "@disks" && !Lister::IsVirtual(request))
      request = Lister::IsVirtual(path.String())
                  ? Lister::ChildLocation(path.String(), request)
                  : Lister::Join(path.String(), request);
    if (Lister::IsVirtual(request))
      request = Lister::ParseLocation(request).URL();
  } catch (const std::exception &error) {
    SetStatus(error.what());
    return;
  }
  StartNavigation(request);
}
void Pane::SetStatus(const char *text) {
  status->SetText(text);
  status->SetToolTip(text);
}
void Pane::SetPlaces(const Lister::SavedPlaces &value, bool writable) {
  places = value;
  placesWritable = writable;
  UpdatePlaces();
}
void Pane::UpdatePlaces() {
  while (auto item = placesMenu->RemoveItem(int32(0))) delete item;
  const char *labels[] = {"Save this place...", "Rename this saved place...",
                          "Forget this saved place..."};
  bool saved = places.Find(path.String()) != nullptr;
  for (int32 i = 0; i < 3; ++i) {
    auto message = new BMessage(kPlaceAction);
    message->AddInt32("action", i);
    auto item = new BMenuItem(labels[i], message);
    item->SetEnabled(placesWritable && (i ? saved :
        locationValid && !saved && places.Items().size() < Lister::SavedPlaces::Limit));
    placesMenu->AddItem(item);
  }
  placesMenu->AddSeparatorItem();
  for (auto &place : places.Items()) {
    auto message = new BMessage(kNavigate);
    message->AddString("path", place.path.c_str());
    message->AddBool("place", true);
    auto item = new BMenuItem(place.name.c_str(), message);
    item->SetMarked(place.path == path.String());
    placesMenu->AddItem(item);
  }
  if (Window()) placesMenu->SetTargetForItems(this);
}
void Pane::UpdateNavigation() {
  back->SetEnabled(history.Back() >= 0);
  forward->SetEnabled(history.Forward() >= 0);
}
void Pane::StartNavigation(const std::string &request, int target, bool trash) {
  stop_watching(this);
  reveal = {};
  savedSelection =
      (target < 0 && !trash && path == request.c_str())
          ? Selected() : std::vector<std::string>{};
  if (trash || path != request.c_str()) {
    filter->SetText("");
    clearFilter->SetEnabled(false);
  }
  path = request.c_str();
  auto view = prefs.folderModes.find(request);
  SetDisplayMode(view == prefs.folderModes.end() ? prefs.paneMode[id]
                                                 : view->second);
  edit->SetText(path);
  scanning = true;
  locationValid = false;
  UpdatePlaces();
  refreshPending = false;
  serial++;
  history.Begin(serial, request, target, trash);
  UpdateNavigation();
  generation->store(serial);
  ClearRows();
  SetStatus("Reading directory...");
  BMessage m(kScan);
  m.AddString("path", path);
  m.AddInt64("serial", serial);
  m.AddBool("icons", prefs.icons || mode > 0);
  m.AddInt32("iconSize", mode == 0 ? 16 : 96);
  m.AddBool("trash", trash);
  scanner->PostMessage(&m);
}
void Pane::Reload() {
  if (scanning) {
    refreshPending = true;
    return;
  }
  Navigate(path);
}
std::vector<std::string> Pane::Selected() const { return entries.Selected(); }
void Pane::Select(int mode) {
  entries.Select(mode);
  SendSelection();
}
void Pane::ContextMenu(const std::string &path, BPoint screen) {
  if (scanning) return;
  for (auto entry : entries.Visible()) {
    if (entry->path != path || entry->parent) continue;
    auto selected = entries.IsSelected(path) ? entries.Selected() : std::vector<std::string>{path};
    entries.SetSelection(selected, path);
    SendSelection();
    BMessage request(kContextMenu);
    request.AddInt32("pane", id);
    request.AddPoint("screen", screen);
    request.AddBool("directory", entry->directory);
    // Same window thread: no queued navigation can change this click's owner.
    Window()->MessageReceived(&request);
    return;
  }
}
bool Pane::CancelSelectionGesture() {
  bool a = list->CancelSelectionGesture();
  bool b = grid->CancelSelectionGesture();
  return a || b;
}
void Pane::SendSelection() {
  BString s;
  int32 visible = 0;
  for (auto entry : entries.Visible())
    if (!entry->parent)
      ++visible;
  s << int32(Selected().size()) << " selected / " << visible;
  if (!simple && filter->Text()[0])
    s << " of " << (visible + int32(entries.HiddenCount()))
      << " entries (filtered)";
  else
    s << " entries";
  SetStatus(s);
  list->SyncSelection();
  grid->Update();
  if (Window()) {
    BMessage changed(kPreviewSelection);
    changed.AddInt32("pane", id);
    auto selected = Selected();
    if (selected.size() == 1) {
      auto found = transfers.find(selected[0]);
      if (found != transfers.end() && !found->second.error.empty())
        changed.AddString("operationError", found->second.error.c_str());
    }
    Window()->PostMessage(&changed);
  }
}
void Pane::MessageReceived(BMessage *m) {
  switch (m->what) {
  case kExpireProgress: {
    std::vector<std::string> expired;
    bool pending = false;
    for (auto &item : transfers)
      if (item.second.state == 2) {
        if (item.second.expires <= system_time())
          expired.push_back(item.first);
        else
          pending = true;
      }
    for (auto &file : expired) {
      BMessage clear(kFileProgress);
      clear.AddString("path", file.c_str());
      clear.AddInt32("state", 0);
      MessageReceived(&clear);
    }
    if (!pending)
      progressTimer.reset();
    break;
  }
  case kFileProgress: {
    std::string file = m->GetString("path", "");
    auto &transfer = transfers[file];
    transfer.state = m->GetInt32("state", 0);
    transfer.fraction = m->GetFloat("fraction", 0);
    transfer.error = m->GetString("error", "");
    entries.Transfer(file, transfer.state, transfer.fraction);
    list->Redraw();
    grid->Invalidate();
    if (transfer.state == 2) {
      transfer.expires = system_time() + 2500000;
      if (!progressTimer) {
        BMessage expire(kExpireProgress);
        progressTimer.reset(
            new BMessageRunner(BMessenger(this), &expire, 250000));
      }
    }
    if (!transfer.state)
      transfers.erase(file);
    if (transfers.size() > 4096)
      transfers.erase(transfers.begin());
    break;
  }
  case kJobDone:
    for (auto &transfer : transfers)
      if (transfer.second.state == 1) {
        transfer.second.state = 3;
        transfer.second.error =
            "Transfer did not complete. See Operations for details.";
      }
    Reload();
    break;
  case kToggleTree:
    if (simple) break;
    tree->Reset();
    prefs.paneTree[id] = !prefs.paneTree[id];
    treeVisible = prefs.paneTree[id];
    if (treeVisible)
      tree->Show();
    else
      tree->Hide();
    display->Menu()->ItemAt(8)->SetMarked(treeVisible);
    tree->ShowPath(path.String());
    PublishDisplay();
    break;
  case kGridMode: {
    prefs.paneMode[id] = m->GetInt32("mode", 0);
    SetDisplayMode(prefs.paneMode[id]);
    bool remembered = prefs.folderModes.count(path.String());
    if (remembered)
      prefs.folderModes[path.String()] = mode;
    PublishDisplay(remembered);
    Reload();
    FocusList();
    break;
  }
  case kRememberView:
    if (prefs.folderModes.size() >= 4096 &&
        !prefs.folderModes.count(path.String())) {
      SetStatus("Folder view limit reached");
      break;
    }
    prefs.folderModes[path.String()] = mode;
    SetDisplayMode(mode);
    PublishDisplay(true);
    break;
  case kForgetView:
    prefs.folderModes.erase(path.String());
    SetDisplayMode(prefs.paneMode[id]);
    PublishDisplay(true, true);
    Reload();
    break;
  case kFilterChanged:
    ApplyFilter();
    break;
  case kClearFilter:
    filter->SetText("");
    ApplyFilter();
    FocusList();
    break;
  case kFilterDone:
    FocusList();
    break;
  case kSwitch:
    Window()->PostMessage(kSwitch);
    break;
  case kActivate:
    Activate();
    FocusList();
    break;
  case kPlaceAction: {
    if (!placesWritable) break;
    int32 action = m->GetInt32("action", 0);
    if ((action == 0 && (!locationValid || places.Find(path.String()))) ||
        (action != 0 && !places.Find(path.String()))) break;
    BMessage request(*m);
    request.AddString("path", path);
    request.AddInt32("pane", id);
    Window()->PostMessage(&request);
    Activate();
    break;
  }
  case kRevealSearch: {
    Lister::SearchHit hit{m->GetString("path", ""), m->GetInt64("device", -1), m->GetInt64("node", -1)};
    if (!Lister::SameSearchEntry(hit)) {
      savedSelection.clear(); reveal = {};
      entries.Select(0); SendSelection();
      SetStatus("Search result moved, disappeared or was replaced. Search again; nothing selected.");
      break;
    }
    size_t slash = hit.path.find_last_of('/');
    if (slash == std::string::npos) break;
    filter->SetText(""); clearFilter->SetEnabled(false);
    StartNavigation(hit.path.substr(0, slash ? slash : 1));
    // Do not select the path until the scan is complete and identity rechecked.
    savedSelection.clear(); reveal = hit;
    break;
  }
  case kNavigate:
    if (m->HasInt64("serial") && m->GetInt64("serial", -1) != serial)
      break;
    if (m->GetBool("place", false)) Activate();
    Navigate(m->GetString("path", edit->Text()));
    if (m->GetBool("tree", false))
      Activate();
    else
      FocusList();
    break;
  case kBack:
  case kForward: {
    int target = m->what == kBack ? history.Back() : history.Forward();
    if (target >= 0) {
      std::string destination = history.At(target);
      StartNavigation(destination, target);
      Activate();
      FocusList();
    }
    break;
  }
  case kParent: {
    try {
      Navigate(Lister::ParentLocation(path == "@disks" ? "/" : path.String())
                   .c_str());
    } catch (const std::exception &e) {
      SetStatus(e.what());
    }
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
  case kTrashFolder: {
    if (path == "@disks" || Lister::IsVirtual(path.String())) {
      SetStatus("Trash requires a local supported volume.");
      break;
    }
    StartNavigation(path.String(), -1, true);
    break;
  }
  case kDisks:
    Navigate("@disks");
    break;
  case kSelection:
    if (!scanning)
      SendSelection();
    break;
  case kInvoke: {
    auto r = entries.Focused();
    if (r) {
      if (r->directory)
        Navigate(r->path.c_str());
      else {
        BMessage open(kOpenPaths);
        open.AddString("path", r->path.c_str());
        open.AddMessenger("pane", BMessenger(this));
        open.AddInt64("serial", serial);
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
      auto row = std::make_unique<PaneEntry>();
      row->name = e.GetString("name", "");
      row->path = e.GetString("path", "");
      row->directory = e.GetBool("directory", false);
      row->parent = e.GetBool("parent", false);
      row->kind = e.GetInt32("kind", 0);
      row->size = e.GetInt64("size", 0);
      row->modified = e.GetInt64("date", 0);
      auto transfer = transfers.find(row->path);
      if (transfer != transfers.end()) {
        row->transferState = transfer->second.state;
        row->fraction = transfer->second.fraction;
      }
      const void *bits;
      ssize_t length;
      int32 pixels = e.GetInt32("iconSize", 16);
      if (pixels >= 16 && pixels <= 128 &&
          e.FindData("icon", B_RAW_TYPE, &bits, &length) == B_OK &&
          length == pixels * pixels * 4) {
        row->icon = std::make_shared<BBitmap>(
            BRect(0, 0, pixels - 1, pixels - 1), B_RGBA32);
        if (row->icon->InitCheck() == B_OK)
          memcpy(row->icon->Bits(), bits, length);
        else
          row->icon.reset();
      }
      entries.Add(std::move(row));
    }
    entries.Filter(simple ? "" : filter->Text());
    entries.RestoreSelection(savedSelection);
    list->Refresh();
    grid->Update();
    break;
  }
  case kScanDone: {
    if (m->GetInt64("serial", -1) != serial)
      break;
    scanning = false;
    path = m->GetString("path", path);
    locationValid = !m->HasString("error");
    UpdatePlaces();
    history.Complete(serial, path.String(), locationValid);
    UpdateNavigation();
    auto view = prefs.folderModes.find(path.String());
    SetDisplayMode(view == prefs.folderModes.end() ? prefs.paneMode[id]
                                                 : view->second);
    edit->SetText(path);
    tree->ShowPath(path.String());
    const char *error;
    if (m->FindString("error", &error) == B_OK) {
      BString s("Cannot open: ");
      s << error;
      SetStatus(s);
    } else {
      bool stale = false;
      if (!reveal.path.empty()) {
        stale = !Lister::SameSearchEntry(reveal);
        entries.SetSelection(stale ? std::vector<std::string>{} : std::vector<std::string>{reveal.path},
                             stale ? "" : reveal.path);
        if (!stale && entries.Selected().empty()) stale = true;
        reveal = {};
      }
      SendSelection();
      if (stale) SetStatus("Search result changed during navigation. Search again; nothing selected.");
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
    SetSource(active);
    break;
  default:
    BView::MessageReceived(m);
    break;
  }
}
