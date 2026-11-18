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
  kView = 'view',
  kEdit = 'edit',
  kAll = 'sall',
  kSelectType = 'styp',
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
// Values are the existing on-disk workspace IDs. Never reorder.
enum class Workspace : int32 { tviskipt = 0, einn = 1, askr = 2, smidja = 3 };
inline const char *WorkspaceId(Workspace workspace) {
  switch (workspace) {
  case Workspace::einn:
    return "einn";
  case Workspace::askr:
    return "askr";
  case Workspace::smidja:
    return "smidja";
  default:
    return "tviskipt";
  }
}
inline const char *WorkspaceName(Workspace workspace) {
  switch (workspace) {
  case Workspace::einn:
    return "Einn";
  case Workspace::askr:
    return "Askr";
  case Workspace::smidja:
    return "Smiðja";
  default:
    return "Tvískipt";
  }
}
inline const char *WorkspaceMenuLabel(Workspace workspace) {
  switch (workspace) {
  case Workspace::einn:
    return "Einn: simple single-panel browsing";
  case Workspace::askr:
    return "Askr: folder tree and file list";
  case Workspace::smidja:
    return "Smiðja: workbench with more tools";
  default:
    return "Tvískipt: side-by-side file panels";
  }
}
constexpr int32 MillerMode = 4;
constexpr int32 ViewModeCount = 5;
inline const char *ViewModeLabel(int32 mode) {
  static const char *const labels[] = {"Details", "Icons", "Large icons",
                                       "Thumbnails", "Miller columns"};
  return mode >= 0 && mode < ViewModeCount ? labels[mode] : "Details";
}
struct Preferences {
  BFont font = *be_plain_font;
  bool icons = true, size = true, date = true, buttons = true, command = false;
  int32 defaultModes[4] = {0, MillerMode, 0, 0};
  int32 paneOverrides[4][2] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
  int32 DefaultMode(Workspace w) const { return defaultModes[int(w)]; }
  void SetDefaultMode(Workspace w, int32 mode) {
    if (mode < 0 || mode >= ViewModeCount || DefaultMode(w) == mode)
      return;
    defaultModes[int(w)] = mode;
    paneOverrides[int(w)][0] = paneOverrides[int(w)][1] = -1;
  }
  bool HasPaneMode(int pane) const {
    return paneOverrides[int(workspace)][pane] >= 0;
  }
  int32 PaneMode(int pane) const {
    return HasPaneMode(pane) ? paneOverrides[int(workspace)][pane]
                             : DefaultMode(workspace);
  }
  void SetPaneMode(int pane, int32 mode) {
    paneOverrides[int(workspace)][pane] =
        mode >= 0 && mode < ViewModeCount ? mode : -1;
  }
  bool paneTree[2] = {false, false};
  bool smidjaTree[2] = {true, true};
  bool preview = false;
  bool smidjaPreview = true;
  Workspace workspace = Workspace::tviskipt;
  bool SinglePane() const {
    return workspace == Workspace::einn || workspace == Workspace::askr;
  }
  bool IsEinn() const { return workspace == Workspace::einn; }
  bool IsSmidja() const { return workspace == Workspace::smidja; }
  bool Tree(int pane) const {
    return IsSmidja() ? smidjaTree[pane] : paneTree[pane];
  }
  void SetTree(int pane, bool value) {
    (IsSmidja() ? smidjaTree[pane] : paneTree[pane]) = value;
  }
  bool Preview() const { return IsSmidja() ? smidjaPreview : preview; }
  void SetPreview(bool value) {
    (IsSmidja() ? smidjaPreview : preview) = value;
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
