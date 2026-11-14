# Lister

A native Haiku file commander by Kambuzi EDB, inspired by the source/destination
workflow of Directory Opus 5. New MIT implementation, not a port or a compatible
replacement. First public preview: **0.1.0**.

Two independent resizable panels, editable paths and location menus, sortable
name/size/date columns, multiselection, drag-copy, node monitoring, a command
button bank, viewer, editor and terminal launch, and saved display preferences.
Native Haiku controls and system colours, including dark themes.

## Workflow

The bold **SOURCE** panel supplies selected files; the other panel is the
**DESTINATION**. Click a panel or press Tab in a list to switch roles. Double-click
a directory to enter it; Backspace goes up. F3 View, F4 Edit, F5 Copy, F6 Move,
F7 New folder, F8 Delete, F9 Preferences. Menus expose additional shortcuts.

Operations run on a worker queue. A separate window shows progress, Pause/Resume,
Cancel all and Replace/Replace all/Skip/Skip all/Abort conflicts. Closing that
window only hides it; use Operations to reopen. The main window will not quit
while an operation is active. A delete confirmation means **permanent deletion**,
not Trash. Cancellation keeps completed entries and removes the current partial
regular file. A partially copied directory can remain.

## Install and build

Download the HPKG from GitHub Releases, then:

    pkgman install ./Lister-0.1.0-1-x86_64.hpkg

Build **inside Haiku x86_64**, never on a Linux host:

    gcc --version
    pkgman install haiku_devel python3
    make
    make test
    make package

`make` uses Haiku makefile-engine. The executable is in
`src/objects.*-release/Lister`, not the project root. `make package` uses
PackageInfo and produces `dist/Lister-0.1.0-1-x86_64.hpkg`. Retain previous
packages for rollback; reinstalling one does not restore or roll back files you
have copied, moved or deleted.

See [design](docs/DESIGN.md), [roadmap](docs/ROADMAP.md) and
[verification](docs/VERIFICATION.md). No accounts, telemetry or paid services.
