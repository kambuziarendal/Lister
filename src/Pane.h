// SPDX-License-Identifier: MIT
#pragma once
#include "Common.h"
#include "IconCache.h"
#include "PaneModel.h"
#include "NavigationHistory.h"
#include "SavedPlaces.h"
#include "FileSearch.h"
#include <Button.h>
#include <Looper.h>
#include <StringView.h>
#include <TextControl.h>
#include <View.h>
class FileDetails;
class FileGrid;
class FolderTree;
class MillerColumns;
class BCardView;
class BGroupView;
class BSplitView;
class BMenuField;
class BMenu;
class BMessageRunner;
class Pane : public BView {
public:
  Pane(int32 id, const Preferences &);
  ~Pane();
  void AttachedToWindow() override;
  void Draw(BRect) override;
  void MessageReceived(BMessage *) override;
  void Navigate(const char *path);
  void NavigateFromColumn(const std::string &folder,
                          const std::string &selection = {},
                          bool invoke = false, bool context = false,
                          BPoint at = {});
  void Reload();
  void SetPlaces(const Listi::SavedPlaces &, bool writable);
  void SetSource(bool);
  void Apply(const Preferences &);
  void SetWorkspace(const Preferences &);
  void Select(int mode);
  bool SelectSameFileType();
  std::string SelectedFileType() const { return entries.FocusedFileType(); }
  std::vector<std::string> Selected() const;
  const char *Path() const { return path.String(); }
  bool CancelSelectionGesture();
  void FocusList();
  void ToggleTreeFocus();
  void FocusFilter();
  void Activate();
  bool IsEinn() const { return einn; }
  bool IsSinglePane() const { return prefs.SinglePane(); }
  bool IsScanning() const { return scanning; }
  void SendSelection(bool gesture = false);
  void ContextMenu(const std::string &entry, BPoint screen);
  void ShowStatus(const char *text) { SetStatus(text); }
  PaneSort Sorting();
  void Sort(PaneSort);
  int32 CurrentView() const { return mode; }
  int32 PaneOverride() const {
    return prefs.HasPaneMode(id) ? prefs.PaneMode(id) : -1;
  }
  bool TreeVisible() const { return prefs.Tree(id); }
  void FolderModes(const std::map<std::string, int32> &modes) {
    prefs.folderModes = modes;
  }

private:
  int32 id;
  Preferences prefs;
  BString path = "/boot/home";
  PaneModel entries;
  IconCache icons;
  FileDetails *list;
  FileGrid *grid;
  MillerColumns *columns;
  FolderTree *tree;
  bool treeVisible = true, einn = false;
  BCardView *cards;
  BGroupView *header, *filterRow;
  BSplitView *content;
  bool chromeVisible = true;
  bool treeOnRight = false, filterAtBottom = false;
  float contentSplitterSize = 0;
  BMenuField *display;
  int32 mode = 0;
  void SetDisplayMode(int32);
  int32 LocationMode() const;
  void UpdateTreeVisibility();
  void PublishDisplay(bool folder = false, bool forget = false);
  BMenu *placesMenu;
  Listi::SavedPlaces places;
  bool placesWritable = true, locationValid = false;
  void UpdatePlaces();
  BTextControl *edit;
  BButton *back, *forward;
  Listi::NavigationHistory history;
  void StartNavigation(const std::string &, int target = -1,
                       bool trash = false);
  void UpdateNavigation();
  BTextControl *filter;
  BButton *clearFilter;
  BStringView *status;
  void SetStatus(const char *);
  void UpdateActivityAppearance();
  void InvalidateActivityBorder();
  std::unique_ptr<BMessageRunner> activityTimer;
  int64 activitySerial = 0;
  bigtime_t activityStarted = 0;
  int activitySteps = 0;
  float activityLevel = 0, activityFrom = 0;
  bool active = false;
  struct Transfer {
    float fraction = 0;
    int32 state = 0;
    std::string error;
    int64 expires = 0;
  };
  std::unique_ptr<BMessageRunner> progressTimer;
  std::map<std::string, Transfer> transfers;
  BLooper *scanner = nullptr;
  std::shared_ptr<struct DirectoryScanDelivery> scanDelivery;
  std::unique_ptr<BMessageRunner> scanTimer;
  std::shared_ptr<std::atomic<int64>> generation;
  int64 serial = 0;
  bool scanning = false, refreshPending = false;
  std::vector<std::string> savedSelection;
  BMessage columnAction;
  Listi::SearchHit reveal;
  void ApplyFilter();
  void ClearRows();
};
