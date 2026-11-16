# Listi

## Alpha / WIP: risk of data loss

**Listi is under active development and testing. It is not a stable release.**
It operates on files on your computer, and bugs can cause data loss or corruption,
including during copying, moving, editing or deleting files. **All use is at your
own risk. We do not recommend using it unless you understand and accept these
risks.** Test only on disposable copies of data and keep independent backups.

Binaries are primarily intended for bug testing, not dependable everyday use.
You are welcome to test, but please [contact us by reporting serious bugs on
GitHub](https://github.com/kambuziarendal/listi/issues). Include the version,
Haiku version, what happened and reproduction steps where possible. Do not
include passwords or private files in a public report.

Latest development source: [0.31.0 working branch](https://github.com/kambuziarendal/listi/tree/work/stability-fixes-20261115).
The documentation below describes the historical 0.2.0 version on this branch.

## Screenshots: DOpus view

**DOpus** is our chosen name for Listi's dual-panel workspace, previously called
**Commander**. Existing builds still use the Commander menu label; the in-app
rename is planned, not implemented. Listi is an independent implementation,
not Directory Opus or an affiliated product.

These are actual Commander-view captures from earlier native Haiku tests,
labelled with their original versions. They are not screenshots of 0.31.0.
Folder trees and preview are optional in this workspace and are enabled here.

### DOpus view: light appearance, Listi 0.24.0

![Listi 0.24.0 Commander workspace, now named DOpus in the plan, with two file panels in light appearance](docs/screenshots/listi-dopus-light-0.24.0.png)

### DOpus view: dark appearance, Listi 0.16.2

![Listi 0.16.2 Commander workspace, now named DOpus in the plan, with two file panels in dark appearance](docs/screenshots/listi-dopus-dark-0.16.2.png)

A native Haiku file commander by Kambuzi EDB, inspired by the source/destination
workflow of Directory Opus 5. New MIT implementation, not a port or a compatible
replacement. Historical alpha preview on this branch: **0.2.0**.

Two independent resizable panels, editable paths and location menus, sortable
name/size/date columns, multiselection, drag-copy, node monitoring, a command
button bank, viewer, editor and terminal launch, and saved display preferences.
Native Haiku controls and system colours, including dark themes.

## Workspaces in the development alpha

Listi offers four implemented workspaces in the **0.31.0 development alpha**,
selected through **View > Workspace**. These descriptions do not apply to the
historical 0.1.0 and 0.2.0 downloads. The comparisons below describe workflows,
not full compatibility with Tracker, Windows Explorer or Directory Opus.

- **Simple:** a minimal, Tracker-like single-folder view. Opening a folder
  navigates in the same window by default rather than opening a new window for
  each folder. It keeps the editable path and file view without the folder tree
  or bottom action bank. Use Copy/Cut and Paste to transfer files.
- **Explorer:** the familiar Explorer-style layout: a folder tree alongside one
  file panel, with an editable path, back/forward navigation, view controls and
  filtering. Useful for browsing a hierarchy without a second destination panel.
- **DOpus** (currently labelled **Commander** in the app): the classic Directory
  Opus/Commander-style two-panel workflow. The active panel supplies the selected
  files and the other panel is the destination for direct panel Copy/Move.
  Action buttons keep common file operations close at hand; clipboard Paste
  still targets the active panel. DOpus is our chosen workspace name, not a claim
  that Listi is Directory Opus or an affiliated product.
- **Workbench, becoming Smiðja:** the current alpha's expanded working layout,
  with two file panels, folder trees, image preview, filters and action buttons.
  Search and operation details are available on demand. Tree and preview choices
  are remembered separately from DOpus/Commander. **Smiðja** means "smithy";
  this is the chosen name for the workspace as it develops into a more
  customizable workbench. The app still labels it Workbench today.

The expanded Smiðja plans include broader file previews, an Edit action for
text-editable content, configurable modules and an optional terminal. **Those
extensions are planned, not implemented in this alpha.**

Changing workspace reuses the existing panels and preserves their locations,
selections and running jobs. Simple and Explorer use the active file panel;
the other panel is hidden rather than discarded. This is live workspace
switching, not a promise to restore every folder after restarting the app.

## Workflow

The bold **SOURCE** panel supplies selected files; the other panel is the
**DESTINATION**. Click a panel or press Tab in a list to switch roles. Double-click
a directory to enter it; Backspace goes up. F3 View, F4 Edit, F5 Copy, F6 Move,
F7 New folder, F8 Trash, F9 Preferences. Menus expose additional shortcuts.

Operations run on a worker queue. A separate window shows progress, Pause/Resume,
Cancel all and Replace/Replace all/Skip/Skip all/Abort conflicts. Closing that
window only hides it; use Operations to reopen. The main window will not quit
while an operation is active. F8 / Trash moves entries to Haiku's per-volume Trash, with Tracker-compatible
restore metadata. Open it using a panel's Go menu, then select **Files > Restore
from Trash**. Restore never overwrites an existing file; the original parent
folder must still exist. Unsupported/read-only volumes return an error without
silently deleting anything. **Files > Delete permanently...** is a separate,
confirmed action and cannot be undone. Cancellation keeps completed entries and removes the current partial
regular file. A partially copied directory can remain.

## Install and build

Download the HPKG from GitHub Releases, then:

    pkgman install ./Lister-0.2.0-1-x86_64.hpkg

Build the current native application **inside Haiku x86_64**. A future portable
C core will have separate POSIX tests; it does not exist in this release.

    gcc --version
    pkgman install haiku_devel python3
    make
    make test
    make package

`make` uses Haiku makefile-engine. The executable is in
`src/objects.*-release/Lister`, not the project root. `make package` uses
PackageInfo and produces `dist/Lister-0.2.0-1-x86_64.hpkg`. Retain previous
packages for rollback; reinstalling one does not restore or roll back files you
have copied, moved or deleted.

See [design](docs/DESIGN.md), [roadmap](docs/ROADMAP.md) and
[verification](docs/VERIFICATION.md). No accounts, telemetry or paid services.
