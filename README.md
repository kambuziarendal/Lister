# Lister

## Alpha / WIP: risk of data loss

**Listi is under active development and testing. It is not a stable release.**
It operates on files on your computer, and bugs can cause data loss or corruption,
including during copying, moving, editing or deleting files. **All use is at your
own risk. We do not recommend using it unless you understand and accept these
risks.** Test only on disposable copies of data and keep independent backups.

Binaries are primarily intended for bug testing, not dependable everyday use.
You are welcome to test, but please [contact us by reporting serious bugs on
GitHub](https://github.com/kambuziarendal/Lister/issues). Include the version,
Haiku version, what happened and reproduction steps where possible. Do not
include passwords or private files in a public report.

Latest development source: [0.31.0 working branch](https://github.com/kambuziarendal/Lister/tree/work/stability-fixes-20261115).
The documentation below describes the historical 0.2.0 version on this branch.

A native Haiku file commander by Kambuzi EDB, inspired by the source/destination
workflow of Directory Opus 5. New MIT implementation, not a port or a compatible
replacement. Historical alpha preview on this branch: **0.2.0**.

Two independent resizable panels, editable paths and location menus, sortable
name/size/date columns, multiselection, drag-copy, node monitoring, a command
button bank, viewer, editor and terminal launch, and saved display preferences.
Native Haiku controls and system colours, including dark themes.

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
