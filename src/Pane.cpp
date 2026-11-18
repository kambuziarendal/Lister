// SPDX-License-Identifier: MIT
#include "Pane.h"
#include "PanelAppearance.h"
#include "DirectoryScanner.h"
#include "FileDetails.h"
#include "FileGrid.h"
#include "FileOperation.h"
#include "FolderTree.h"
#include "ImagePreview.h"
#include "MillerColumns.h"
#include "NameFilter.h"
#include "Trash.h"
#include "VirtualFile.h"
#include <Bitmap.h>
#include <StringForSize.h>
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
constexpr uint32 kRememberView = 'grem', kForgetView = 'gfor',
                 kExpireProgress = 'expr', kToggleTree = 'ttre',
                 kActivityFrame = 'actf', kCheckScanDelivery = 'scdl',
                 kColumnNavigate = 'mnav';
constexpr int32 kRememberViewItem = ViewModeCount + 1;
constexpr int32 kForgetViewItem = ViewModeCount + 2;
constexpr int32 kToggleTreeItem = ViewModeCount + 5;
} // namespace
namespace {
class GridScroll : public BScrollView {
  FileGrid *grid;
  Pane *pane;
  BRect laidOutBounds;
  float fullWidth = 0;
  bool laidOut = false, barVisible = true;

public:
  GridScroll(FileGrid *target, Pane *owner)
      : BScrollView("icons scroll", target, 0, false, true), grid(target),
        pane(owner) {}
  void DoLayout() override {
    auto bar = ScrollBar(B_VERTICAL);
    const bool resized = !laidOut || laidOutBounds != Bounds();
    if (resized) {
      BScrollView::DoLayout();
      fullWidth = grid->Bounds().Width() + bar->PreferredSize().Width();
      laidOutBounds = Bounds();
      laidOut = true;
    }
    bool show = !pane->IsSinglePane() ||
                grid->ContentHeight(fullWidth) > grid->Bounds().Height() + 1;
    if (resized || show != barVisible) {
      if (!resized)
        BScrollView::DoLayout();
      barVisible = show;
      if (!show)
        grid->ResizeTo(fullWidth, grid->Bounds().Height());
    }
    if (show == bar->IsHidden(this)) {
      if (show)
        bar->Show();
      else
        bar->Hide();
    }
  }
};
}
Pane::Pane(int32 number, const Preferences &p)
    : BView("pane", B_WILL_DRAW | B_SUPPORTS_LAYOUT | B_FULL_UPDATE_ON_RESIZE),
      id(number), prefs(p),
      generation(std::make_shared<std::atomic<int64>>(0)) {
  SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
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
  auto gridScroll = new GridScroll(grid, this);
  columns = new MillerColumns(this, &entries);
  auto columnScroll = new BScrollView("Miller horizontal scroll", columns, 0,
                                      true, false, B_NO_BORDER);
  cards = new BCardView("panel views");
  cards->AddChild(list);
  cards->AddChild(gridScroll);
  cards->AddChild(columnScroll);
  auto modes = new BPopUpMenu("View");
  for (int32 i = 0; i < ViewModeCount; ++i) {
    auto message = new BMessage(kGridMode);
    message->AddInt32("mode", i);
    modes->AddItem(new BMenuItem(ViewModeLabel(i), message));
  }
  modes->AddSeparatorItem();
  modes->AddItem(new BMenuItem("Remember view for this folder",
                               new BMessage(kRememberView)));
  modes->AddItem(new BMenuItem("Use pane view for this folder",
                               new BMessage(kForgetView)));
  auto workspaceView = new BMessage(kGridMode);
  workspaceView->AddInt32("mode", -1);
  modes->AddItem(new BMenuItem("Use workspace default", workspaceView));
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
  content->SetItemWeight(0, 0.24f, false);
  content->SetItemWeight(1, 0.76f, true);
  content->SetCollapsible(0, false);
  content->SetCollapsible(1, false);
  status = new BStringView("status", "Ready");
  status->SetTruncation(B_TRUNCATE_END);
  status->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
  status->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
  header = new BGroupView(B_HORIZONTAL, 5);
  header->SetName("panel toolbar");
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
  scanTimer.reset();
  stop_watching(this);
  if (scanner && scanner->Lock())
    scanner->Quit();
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
  scanDelivery = std::make_shared<DirectoryScanDelivery>();
  scanner = CreateDirectoryScanner(BMessenger(this), generation, scanDelivery);
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
void Pane::InvalidateActivityBorder() {
  const auto b = Bounds();
  // Only the empty outer inset is animated, never the file views or controls.
  Invalidate(BRect(b.left, b.top, b.right, b.top + 3));
  Invalidate(BRect(b.left, b.bottom - 3, b.right, b.bottom));
  Invalidate(BRect(b.left, b.top + 4, b.left + 3, b.bottom - 4));
  Invalidate(BRect(b.right - 3, b.top + 4, b.right, b.bottom - 4));
}
void Pane::Draw(BRect update) {
  // Resizing moves the frame through the exposed gaps between child controls.
  // Repaint those gaps instead of leaving previous frame edges behind.
  PushState();
  SetHighColor(ViewColor());
  FillRect(update);
  if (!prefs.SinglePane()) {
    const auto neutral = Listi::PanelBlend(ui_color(B_PANEL_BACKGROUND_COLOR),
                                           ui_color(B_PANEL_TEXT_COLOR), 0.12f);
    SetHighColor(Listi::PanelBlend(
        neutral, ui_color(B_KEYBOARD_NAVIGATION_COLOR), activityLevel));
    SetPenSize(2);
    StrokeRect(Bounds().InsetByCopy(1, 1));
  }
  PopState();
}
void Pane::UpdateActivityAppearance() {
  const bool highlighted = active && !prefs.SinglePane();
  list->SetActive(highlighted);
  grid->SetActive(highlighted);
  columns->SetActive(highlighted);
  auto text = edit->TextView();
  const auto background = Listi::PanelBlend(
      ui_color(B_DOCUMENT_BACKGROUND_COLOR),
      ui_color(B_KEYBOARD_NAVIGATION_COLOR), highlighted ? 0.075f : 0.f);
  if (text->ViewColor() != background) {
    text->SetViewColor(background);
    text->SetLowColor(background);
    text->Invalidate();
  }
  edit->SetToolTip(prefs.SinglePane() ? "Folder path"
                   : active           ? "Active file panel"
                                      : "Inactive file panel");
}
void Pane::SetSource(bool source) {
  if (active == source)
    return;
  active = source;
  UpdateActivityAppearance();
  activityTimer.reset();
  ++activitySerial;
  if (!prefs.SinglePane() && Window()) {
    activityFrom = activityLevel;
    activityStarted = system_time();
    activitySteps = 0;
    BMessage frame(kActivityFrame);
    frame.AddInt64("serial", activitySerial);
    activityTimer.reset(new BMessageRunner(BMessenger(this), &frame, 25000, 6));
    if (activityTimer->InitCheck() == B_OK)
      return;
    activityTimer.reset();
  }
  activityLevel = active ? 1.f : 0.f;
  InvalidateActivityBorder();
}
PaneSort Pane::Sorting() {
  return mode == 0 ? list->Sorting() : entries.Sorting();
}
void Pane::Sort(PaneSort value) {
  CancelSelectionGesture();
  entries.Sort(value);
  list->Sort(value);
  grid->Update();
  columns->Refresh();
  SendSelection();
}
int32 Pane::LocationMode() const {
  auto remembered = prefs.folderModes.find(path.String());
  return remembered == prefs.folderModes.end() ? prefs.PaneMode(id)
                                               : remembered->second;
}
void Pane::SetDisplayMode(int32 value) {
  auto focus = Window() ? Window()->CurrentFocus() : nullptr;
  while (focus && focus != cards)
    focus = focus->Parent();
  if (mode == 0)
    entries.Sort(list->Sorting());
  mode = std::clamp(value, int32(0), ViewModeCount - 1);
  cards->CardLayout()->SetVisibleItem(mode == 0            ? 0
                                      : mode == MillerMode ? 2
                                                           : 1);
  columns->SetEnabled(mode == MillerMode);
  if (mode == 0)
    list->SyncSelection();
  else if (mode == MillerMode) {
    columns->SetLocation(path.String());
    columns->Refresh();
  } else {
    grid->SetMode(mode);
    grid->SyncSelection();
  }
  UpdateTreeVisibility();
  if (focus)
    FocusList();
  auto menu = display->Menu();
  for (int i = 0; i < ViewModeCount; ++i)
    menu->ItemAt(i)->SetMarked(mode == i);
  menu->Superitem()->SetLabel(menu->ItemAt(mode)->Label());
  menu->ItemAt(kRememberViewItem)
      ->SetMarked(prefs.folderModes.count(path.String()));
  menu->ItemAt(kForgetViewItem)
      ->SetEnabled(prefs.folderModes.count(path.String()));
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
  else if (mode == MillerMode)
    columns->MakeFocus(true);
  else
    grid->MakeFocus(true);
}
void Pane::ToggleTreeFocus() {
  auto focused = Window()->CurrentFocus();
  while (focused && focused != tree)
    focused = focused->Parent();
  if (focused)
    FocusList();
  else if (treeVisible)
    tree->FindView("folder tree")->MakeFocus();
}
void Pane::FocusFilter() {
  if (einn)
    return;
  Activate();
  filter->MakeFocus(true);
  filter->TextView()->SelectAll();
}
void Pane::ClearRows() {
  entries.Clear();
  icons.Clear();
  list->Refresh();
  grid->Reset();
  columns->Refresh();
  columns->SyncSelection();
}
void Pane::ApplyFilter() {
  entries.Filter(einn ? "" : filter->Text());
  list->Refresh();
  grid->Update();
  columns->Refresh();
  clearFilter->SetEnabled(filter->Text()[0] != 0);
  if (!scanning)
    SendSelection();
}
void Pane::SetWorkspace(const Preferences &settings) {
  static_cast<WorkspaceLayout &>(prefs) = settings;
  const bool value = prefs.IsEinn();
  const bool changed = einn != value;
  einn = value;
  if (treeOnRight != prefs.treeRight) {
    treeOnRight = prefs.treeRight;
    content->GetLayout()->RemoveView(tree);
    content->AddChild(treeOnRight ? 1 : 0, tree, 0.24f);
    content->SetCollapsible(0, false);
    content->SetCollapsible(1, false);
  }
  if (filterAtBottom != prefs.filterBottom) {
    filterAtBottom = prefs.filterBottom;
    auto layout = static_cast<BGroupLayout *>(GetLayout());
    layout->RemoveView(filterRow);
    // Keep the filter directly adjacent to the file list on either side.
    layout->AddView(filterAtBottom ? 3 : 2, filterRow, 0.0f);
  }
  list->SetSinglePane(prefs.SinglePane());
  if (chromeVisible == einn) {
    chromeVisible = !einn;
    for (BView *control :
         {static_cast<BView *>(header), static_cast<BView *>(filterRow),
          static_cast<BView *>(back), static_cast<BView *>(forward)}) {
      if (chromeVisible)
        control->Show();
      else
        control->Hide();
    }
  }
  auto layout = static_cast<BGroupLayout *>(GetLayout());
  layout->SetInsets(einn ? 2 : 6);
  layout->SetSpacing(einn ? 2 : 5);
  content->SetSpacing(einn ? 0 : 4);
  content->SetSplitterSize(einn ? 0 : contentSplitterSize);
  // A hidden filter must not silently hide entries in the minimal workspace.
  // Keep its text for Tviskipt, but suspend its effect while it is hidden.
  if (changed)
    ApplyFilter();
  SetDisplayMode(LocationMode());
  activityTimer.reset();
  ++activitySerial;
  activityLevel = active ? 1.f : 0.f;
  UpdateActivityAppearance();
  InvalidateActivityBorder();
}
void Pane::UpdateTreeVisibility() {
  const bool visible =
      mode != MillerMode &&
      (prefs.workspace == Workspace::askr || (prefs.Tree(id) && !einn));
  if (treeVisible != visible) {
    treeVisible = visible;
    if (visible) {
      tree->Show();
      tree->ShowPath(path.String());
    } else {
      tree->Hide();
    }
  }
  display->Menu()->ItemAt(kToggleTreeItem)->SetMarked(treeVisible);
  display->Menu()
      ->ItemAt(kToggleTreeItem)
      ->SetEnabled(!prefs.SinglePane() && mode != MillerMode);
}
void Pane::Apply(const Preferences &p) {
  prefs = p;
  SetWorkspace(p);
  tree->ShowPath(path.String());
  list->Apply(p);
  grid->SetFont(&p.font);
  grid->Update();
  columns->ApplyFont(p.font);
  UpdateActivityAppearance();
}
void Pane::Navigate(const char *p) {
  if (!scanner)
    return;
  std::string request = p;
  if (request.empty())
    return;
  try {
    if (request[0] != '/' && request != "@disks" && !Listi::IsVirtual(request))
      request = Listi::IsVirtual(path.String())
                    ? Listi::ChildLocation(path.String(), request)
                    : Listi::Join(path.String(), request);
    if (Listi::IsVirtual(request))
      request = Listi::ParseLocation(request).URL();
  } catch (const std::exception &error) {
    SetStatus(error.what());
    return;
  }
  StartNavigation(request);
}
void Pane::NavigateFromColumn(const std::string &folder,
                              const std::string &selection, bool invoke,
                              bool context, BPoint at) {
  BMessage message(kColumnNavigate);
  message.AddString("path", folder.c_str());
  message.AddString("selection", selection.c_str());
  message.AddBool("invoke", invoke);
  message.AddBool("context", context);
  message.AddPoint("screen", at);
  message.AddInt64("serial", serial);
  Window()->PostMessage(&message, this);
}
void Pane::SetStatus(const char *text) {
  status->SetText(text);
  status->SetToolTip(text);
}
void Pane::SetPlaces(const Listi::SavedPlaces &value, bool writable) {
  places = value;
  placesWritable = writable;
  UpdatePlaces();
}
void Pane::UpdatePlaces() {
  while (auto item = placesMenu->RemoveItem(int32(0)))
    delete item;
  const char *labels[] = {"Save this place...", "Rename this saved place...",
                          "Forget this saved place..."};
  bool saved = places.Find(path.String()) != nullptr;
  for (int32 i = 0; i < 3; ++i) {
    auto message = new BMessage(kPlaceAction);
    message->AddInt32("action", i);
    auto item = new BMenuItem(labels[i], message);
    item->SetEnabled(placesWritable && (i ? saved
                                          : locationValid && !saved &&
                                                places.Items().size() <
                                                    Listi::SavedPlaces::Limit));
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
  if (Window())
    placesMenu->SetTargetForItems(this);
}
void Pane::UpdateNavigation() {
  back->SetEnabled(history.Back() >= 0);
  forward->SetEnabled(history.Forward() >= 0);
}
void Pane::StartNavigation(const std::string &request, int target, bool trash) {
  stop_watching(this);
  reveal = {};
  columnAction.MakeEmpty();
  savedSelection = (target < 0 && !trash && path == request.c_str())
                       ? Selected()
                       : std::vector<std::string>{};
  if (trash || path != request.c_str()) {
    filter->SetText("");
    clearFilter->SetEnabled(false);
  }
  path = request.c_str();
  SetDisplayMode(LocationMode());
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
  m.AddBool("icons", prefs.icons || (mode > 0 && mode < MillerMode));
  m.AddInt32("iconSize", mode == 0 || mode == MillerMode ? 16 : 96);
  m.AddBool("trash", trash);
  BMessage check(kCheckScanDelivery);
  check.AddInt64("serial", serial);
  scanTimer.reset(new BMessageRunner(BMessenger(this), &check, 250000));
  if (scanTimer->InitCheck() != B_OK || scanner->PostMessage(&m) != B_OK) {
    BMessage failed(kScanDone);
    failed.AddInt64("serial", serial);
    failed.AddString("error",
                     "Cannot start directory reader. Reload to try again.");
    MessageReceived(&failed);
  }
}
void Pane::Reload() {
  if (scanning) {
    refreshPending = true;
    return;
  }
  Navigate(path);
}
std::vector<std::string> Pane::Selected() const { return entries.Selected(); }
bool Pane::SelectSameFileType() {
  if (scanning || !entries.SelectSameFileType())
    return false;
  SendSelection();
  return true;
}
void Pane::Select(int mode) {
  entries.Select(mode);
  SendSelection();
}
void Pane::ContextMenu(const std::string &path, BPoint screen) {
  if (scanning)
    return;
  if (path.empty()) {
    BMessage request(kContextMenu);
    request.AddInt32("pane", id);
    request.AddPoint("screen", screen);
    request.AddBool("folderContext", true);
    const auto selected = entries.Selected();
    if (selected.size() == 1)
      for (auto entry : entries.Visible())
        if (entry->path == selected.front()) {
          request.AddBool("directory", entry->directory);
          break;
        }
    Window()->MessageReceived(&request);
    return;
  }
  for (auto entry : entries.Visible()) {
    if (entry->path != path || entry->parent)
      continue;
    auto selected = entries.IsSelected(path) ? entries.Selected()
                                             : std::vector<std::string>{path};
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
void Pane::SendSelection(bool gesture) {
  BString s;
  const auto summary = entries.Summary();
  s << uint64(summary.selected) << " selected";
  if (summary.selected) {
    s << ": ";
    bool part = false;
    if (summary.files) {
      if (summary.files == summary.unknown)
        s << "size unknown";
      else {
        char text[64];
        if (summary.overflow)
          s << "at least ";
        s << string_for_size(summary.bytes, text, sizeof(text));
        if (summary.unknown)
          s << " + unknown size";
      }
      part = true;
    }
    if (summary.folders) {
      if (part)
        s << " + ";
      s << uint64(summary.folders)
        << (summary.folders == 1 ? " folder" : " folders");
      part = true;
    }
    if (summary.links) {
      if (part)
        s << " + ";
      s << uint64(summary.links) << (summary.links == 1 ? " link" : " links");
    }
  }
  s << " / " << uint64(summary.visible);
  if (!einn && filter->Text()[0])
    s << " of " << uint64(summary.visible + entries.HiddenCount())
      << " entries (filtered)";
  else
    s << " entries";
  SetStatus(s);
  if (summary.selected) {
    BString tip(s);
    if (summary.files && !summary.overflow)
      tip << "\nKnown file sizes: " << uint64(summary.bytes) << " bytes.";
    if (summary.folders || summary.links)
      tip << "\nFolder contents and link targets are not counted.";
    if (summary.unknown)
      tip << "\nSize unavailable for " << uint64(summary.unknown)
          << " selected file(s).";
    status->SetToolTip(tip);
  }
  if (mode == 0)
    list->SyncSelection();
  else if (mode == MillerMode)
    columns->SyncSelection();
  else
    grid->SyncSelection();
  // Preview decoding is useful after the gesture, not for transient rows.
  if (Window() && !gesture) {
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
  case kActivityFrame: {
    if (m->GetInt64("serial", -1) != activitySerial || !activityTimer)
      break;
    const bool finished =
        ++activitySteps >= 6 || system_time() - activityStarted >= 150000;
    const float fraction =
        finished
            ? 1.f
            : std::min(1.f, float(system_time() - activityStarted) / 150000.f);
    activityLevel =
        activityFrom + ((active ? 1.f : 0.f) - activityFrom) * fraction;
    InvalidateActivityBorder();
    if (finished)
      activityTimer.reset();
    break;
  }
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
    if (entries.Transfer(file, transfer.state, transfer.fraction)) {
      list->RedrawEntry(file);
      grid->RedrawEntry(file);
      columns->RedrawRows();
    }
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
    if (m->GetBool("beforeStart", false))
      break;
    for (auto &transfer : transfers)
      if (transfer.second.state == 1) {
        transfer.second.state = 3;
        transfer.second.error =
            "Transfer did not complete. See Operations for details.";
      }
    Reload();
    break;
  case kToggleTree:
    if (prefs.SinglePane() || mode == MillerMode)
      break;
    tree->Reset();
    prefs.SetTree(id, !prefs.Tree(id));
    UpdateTreeVisibility();
    PublishDisplay();
    break;
  case kGridMode: {
    auto requestedMode = m->GetInt32("mode", 0);
    prefs.SetPaneMode(id, requestedMode);
    const bool remembered = prefs.folderModes.count(path.String());
    if (remembered) {
      if (requestedMode < 0)
        prefs.folderModes.erase(path.String());
      else
        prefs.folderModes[path.String()] = prefs.PaneMode(id);
    }
    SetDisplayMode(LocationMode());
    PublishDisplay(remembered, requestedMode < 0);
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
    SetDisplayMode(LocationMode());
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
    if (!placesWritable)
      break;
    int32 action = m->GetInt32("action", 0);
    if ((action == 0 && (!locationValid || places.Find(path.String()))) ||
        (action != 0 && !places.Find(path.String())))
      break;
    BMessage request(*m);
    request.AddString("path", path);
    request.AddInt32("pane", id);
    Window()->PostMessage(&request);
    Activate();
    break;
  }
  case kRevealSearch: {
    Listi::SearchHit hit{m->GetString("path", ""), m->GetInt64("device", -1),
                         m->GetInt64("node", -1)};
    if (!Listi::SameSearchEntry(hit)) {
      savedSelection.clear();
      reveal = {};
      entries.Select(0);
      SendSelection();
      SetStatus(
          "Search result moved, disappeared or was replaced. Search again; nothing selected.");
      break;
    }
    size_t slash = hit.path.find_last_of('/');
    if (slash == std::string::npos)
      break;
    filter->SetText("");
    clearFilter->SetEnabled(false);
    StartNavigation(hit.path.substr(0, slash ? slash : 1));
    // Do not select the path until the scan is complete and identity rechecked.
    savedSelection.clear();
    reveal = hit;
    break;
  }
  case kNavigate:
    if (m->HasInt64("serial") && m->GetInt64("serial", -1) != serial)
      break;
    if (m->GetBool("place", false))
      Activate();
    Navigate(m->GetString("path", edit->Text()));
    if (m->GetBool("tree", false))
      Activate();
    else
      FocusList();
    break;
  case kColumnNavigate: {
    if (mode != MillerMode || m->GetInt64("serial", -1) != serial)
      break;
    auto before = serial;
    Navigate(m->GetString("path", path.String()));
    if (serial != before && scanning)
      columnAction = *m;
    FocusList();
    break;
  }
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
      Navigate(Listi::ParentLocation(path == "@disks" ? "/" : path.String())
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
    if (path == "@disks" || Listi::IsVirtual(path.String())) {
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
    if (m->HasString("column") &&
        (scanning || path != m->GetString("column", "") || !r ||
         r->path != m->GetString("selection", "")))
      break;
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
  case kCheckScanDelivery: {
    if (scanning && m->GetInt64("serial", -1) == serial &&
        scanDelivery->failedSerial.load() == serial) {
      BMessage failed(kScanDone);
      failed.AddInt64("serial", serial);
      failed.AddString(
          "error",
          "Directory completion was not delivered. Reload to try again.");
      MessageReceived(&failed);
    }
    break;
  }
  case kRows: {
    if (!scanning || m->GetInt64("serial", -1) != serial)
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
      row->sizeKnown = e.GetBool("sizeKnown", true);
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
        row->icon = icons.Get(bits, length, pixels);
      }
      entries.Add(std::move(row));
    }
    entries.Filter(einn ? "" : filter->Text());
    entries.RestoreSelection(savedSelection);
    list->Refresh();
    grid->Update();
    columns->Refresh();
    break;
  }
  case kScanDone: {
    if (!scanning || m->GetInt64("serial", -1) != serial)
      break;
    scanning = false;
    scanTimer.reset();
    path = m->GetString("path", path);
    locationValid = !m->HasString("error");
    UpdatePlaces();
    history.Complete(serial, path.String(), locationValid);
    UpdateNavigation();
    SetDisplayMode(LocationMode());
    edit->SetText(path);
    tree->ShowPath(path.String());
    const char *error;
    if (m->FindString("error", &error) == B_OK) {
      ClearRows();
      BString s("Cannot open: ");
      s << error;
      SetStatus(s);
    } else {
      bool stale = false;
      BMessage action(columnAction);
      columnAction.MakeEmpty();
      std::string picked = action.GetString("selection", "");
      if (!picked.empty())
        entries.SetSelection({picked}, picked);
      if (!reveal.path.empty()) {
        stale = !Listi::SameSearchEntry(reveal);
        entries.SetSelection(stale ? std::vector<std::string>{}
                                   : std::vector<std::string>{reveal.path},
                             stale ? "" : reveal.path);
        if (!stale && entries.Selected().empty())
          stale = true;
        reveal = {};
      }
      SendSelection();
      if (stale)
        SetStatus(
            "Search result changed during navigation. Search again; nothing selected.");
      const void *p;
      ssize_t n;
      if (m->FindData("node", B_RAW_TYPE, &p, &n) == B_OK &&
          n == sizeof(node_ref))
        watch_node(static_cast<const node_ref *>(p), B_WATCH_ALL, this);
      if (action.GetBool("context", false))
        ContextMenu(picked, action.GetPoint("screen", BPoint()));
      else if (action.GetBool("invoke", false) && !entries.Selected().empty()) {
        BMessage invoke(kInvoke);
        invoke.AddString("column", path);
        invoke.AddString("selection", picked.c_str());
        Window()->PostMessage(&invoke, this);
      }
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
