// SPDX-License-Identifier: MIT
#pragma once
#include "Common.h"
#include "PaneModel.h"
#include <Button.h>
#include <Looper.h>
#include <StringView.h>
#include <TextControl.h>
#include <View.h>
class FileDetails;
class FileGrid;
class FolderTree;
class BCardView;
class BMenuField;
class BMessageRunner;
class Pane : public BView {
public:
  Pane(int32 id, const Preferences &);
  ~Pane();
  void AttachedToWindow() override;
  void MessageReceived(BMessage *) override;
  void Navigate(const char *path);
  void Reload();
  void SetSource(bool);
  void Apply(const Preferences &);
  void Select(int mode);
  std::vector<std::string> Selected() const;
  const char *Path() const { return path.String(); }
  void FocusList();
  void FocusFilter();
  void Activate();
  bool IsScanning() const { return scanning; }
  void SendSelection();
  int32 DisplayMode() const { return prefs.paneMode[id]; }
  bool TreeVisible() const { return prefs.paneTree[id]; }
  void FolderModes(const std::map<std::string, int32> &modes) {
    prefs.folderModes = modes;
  }

private:
  int32 id;
  Preferences prefs;
  BString path = "/boot/home";
  PaneModel entries;
  FileDetails *list;
  FileGrid *grid;
  FolderTree *tree;
  bool treeVisible = true;
  BCardView *cards;
  BMenuField *display;
  int32 mode = 0;
  void SetDisplayMode(int32);
  void PublishDisplay(bool folder = false, bool forget = false);
  BTextControl *edit;
  BTextControl *filter;
  BButton *clearFilter;
  BStringView *status;
  BStringView *role;
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
  std::shared_ptr<std::atomic<int64>> generation;
  int64 serial = 0;
  bool scanning = false, refreshPending = false;
  std::vector<std::string> savedSelection;
  void ApplyFilter();
  void ClearRows();
};
