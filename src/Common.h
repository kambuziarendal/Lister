// SPDX-License-Identifier: MIT
#pragma once
#include "WorkspaceLayout.h"
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
// Stable installed-app identity; not a display name. Retained for upgrades.
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
  kClipboardSaveText = 'ctsf',
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
  kBulkRename = 'brnm',
  kBulkRenameAccepted = 'brac',
  kUndoBulkRename = 'ubrn',
  kView = 'view',
  kEdit = 'edit',
  kAll = 'sall',
  kSelectType = 'styp',
  kSelectPattern = 'spat',
  kCompareFolders = 'fcmp',
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
  kQueueWake = 'qwke',
  kPause = 'paus',
  kAbort = 'abor',
  kConflict = 'conf',
  kAnswer = 'answ',
  kShowJobs = 'jobs',
  kNameAccepted = 'name',
  kOpenPaths = 'opth'
};
struct Preferences : WorkspaceLayout {
  BFont font = *be_plain_font;
  bool icons = true, size = true, date = true;
  // -1 inherits the workspace layout setting. The quick toggle only
  // overrides Tvískipt and is not part of a saved layout profile.
  int32 tviskiptButtons = -1;
  bool ActionButtonsVisible() const {
    return workspace == Workspace::tviskipt && tviskiptButtons >= 0
               ? tviskiptButtons == 1
               : buttons;
  }
  WorkspaceProfiles profiles;
  WorkspaceLayout CaptureLayout() const { return *this; }
  void ApplyLayout(const WorkspaceLayout &layout) {
    static_cast<WorkspaceLayout &>(*this) = layout;
  }
  void ResetLayout() { ApplyLayout(WorkspaceLayout{}); }
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
