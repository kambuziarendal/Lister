// SPDX-License-Identifier: MIT
#pragma once
#include <Font.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <Messenger.h>
#include <String.h>
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>
constexpr const char *kSignature = "application/x-vnd.Kambuzi-Lister";
enum : uint32 {
  kGridMode = 'gmod',
  kSort = 'sort',
  kWorkspace = 'wksp',
  kContextMenu = 'ctxm',
  kActivate = 'actv',
  kNavigate = 'navi',
  kBack = 'back',
  kForward = 'frwd',
  kPlaceAction = 'plac',
  kPlaceAccepted = 'pacc',
  kSavePlaces = 'psav',
  kPlacesSaved = 'psvd',
  kScan = 'scan',
  kRows = 'rows',
  kScanDone = 'done',
  kInvoke = 'invo',
  kSelection = 'sels',
  kPreviewSelection = 'pvsl',
  kPreviewToggle = 'pvtg',
  kPaneDisplay = 'pnds',
  kParent = 'prnt',
  kRoot = 'root',
  kHome = 'home',
  kDesktop = 'desk',
  kDisks = 'disk',
  kProperties = 'prop',
  kOpenWith = 'opwi',
  kClipboardCopy = 'ccpy',
  kClipboardCut = 'ccut',
  kClipboardPaste = 'cpst',
  kDuplicate = 'dupl',
  kCreateLink = 'link',
  kCreateArchive = 'arcr',
  kExtractArchive = 'arex',
  kAddArchive = 'arad',
  kRemoveArchive = 'arrm',
  kCopy = 'copy',
  kMove = 'move',
  kDelete = 'dele',
  kPermanentDelete = 'pdel',
  kRestore = 'rest',
  kTrashFolder = 'trsh',
  kMkdir = 'mkdr',
  kRename = 'rnam',
  kView = 'view',
  kEdit = 'edit',
  kAll = 'sall',
  kNone = 'snon',
  kInvert = 'sinv',
  kSwap = 'swap',
  kSame = 'same',
  kRefresh = 'rfsh',
  kSearch = 'srch',
  kRevealSearch = 'srev',
  kFilter = 'fltr',
  kFilterChanged = 'flch',
  kClearFilter = 'flcl',
  kFilterDone = 'fldn',
  kTerminal = 'term',
  kPreferences = 'pref',
  kApplyPreferences = 'aprf',
  kCommand = 'cmdl',
  kDrop = 'drop',
  kSwitch = 'swch',
  kJobUpdate = 'jupd',
  kFileProgress = 'fprg',
  kFileErrorSelected = 'ferr',
  kJobDone = 'jdon',
  kPause = 'paus',
  kAbort = 'abor',
  kConflict = 'conf',
  kAnswer = 'answ',
  kShowJobs = 'jobs',
  kNameAccepted = 'name',
  kOpenPaths = 'opth'
};
enum class Workspace : int32 {
  Commander = 0,
  Simple = 1,
  Explorer = 2,
  Workbench = 3
};
struct Preferences {
  BFont font = *be_plain_font;
  bool icons = true, size = true, date = true, buttons = true, command = false;
  int32 paneMode[2] = {0, 0};
  bool paneTree[2] = {false, false};
  bool workbenchTree[2] = {true, true};
  bool preview = false;
  bool workbenchPreview = true;
  Workspace workspace = Workspace::Commander;
  bool SinglePane() const {
    return workspace == Workspace::Simple || workspace == Workspace::Explorer;
  }
  bool Simple() const { return workspace == Workspace::Simple; }
  bool Workbench() const { return workspace == Workspace::Workbench; }
  bool Tree(int pane) const {
    return Workbench() ? workbenchTree[pane] : paneTree[pane];
  }
  void SetTree(int pane, bool value) {
    (Workbench() ? workbenchTree[pane] : paneTree[pane]) = value;
  }
  bool Preview() const { return Workbench() ? workbenchPreview : preview; }
  void SetPreview(bool value) {
    (Workbench() ? workbenchPreview : preview) = value;
  }
  int32 singlePane = 0;
  std::map<std::string, int32> folderModes;
  BString editor = "/boot/system/apps/StyledEdit",
          terminal = "/boot/system/apps/Terminal";
  BString folders = "system", files = "system", links = "system";
  rgb_color Color(const BString &value) const;
  void Load(const BMessage &);
  void ApplyDialog(const BMessage &);
  BMessage Save() const;
};
void OpenPreferences(BMessenger target, const Preferences &);
