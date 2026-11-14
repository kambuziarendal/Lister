# Listi

A native Haiku file commander by Kambuzi EDB, inspired by the source/destination
workflow of Directory Opus 5. New MIT implementation, not a port or a compatible
replacement. Development candidate: **0.3.0**. Published preview: **0.2.0**.

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

## Narrow, select, act

Each panel has a literal name filter. Use **Listi > Filter names** (Alt+F on
default Haiku), type part of a name, then Enter to return to the list. Escape in
the field or Clear removes the filter. The status shows the visible/total count.
Hidden entries are deselected and stay deselected when revealed again. Select All
and file operations affect only visible selections; `..` remains navigable.
Changing folder clears the filter; refreshing the same folder preserves it. This
is a local view filter, not recursive search or wildcard expansion.

## Install and build

Install the privately supplied candidate in Haiku:

    pkgman install ./Listi-0.3.0-1-x86_64.hpkg

Listi replaces Lister in Apps and Deskbar. The package identity (`lister`),
application signature (`application/x-vnd.Kambuzi-Lister`), scripting suite and
preferences (`~/config/settings/Lister_settings`) intentionally remain compatible.
There is no settings migration or duplicate Lister launcher. Keep the old HPKG
for application rollback; the GitHub repository name has not changed.

Build the current native application **inside Haiku x86_64**. A future portable
C core will have separate POSIX tests; it does not exist in this release.

    gcc --version
    pkgman install haiku_devel python3
    make
    make test
    make package

`make` uses Haiku makefile-engine. The executable is in
`src/objects.*-release/Listi`, not the project root. `make package` uses
PackageInfo and produces `dist/Listi-0.3.0-1-x86_64.hpkg`. Retain previous
packages for rollback; reinstalling one does not restore or roll back files you
have copied, moved or deleted.

See [design](docs/DESIGN.md), [roadmap](docs/ROADMAP.md) and
[verification](docs/VERIFICATION.md). No accounts, telemetry or paid services.
