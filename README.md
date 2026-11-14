# Listi

A native Haiku file commander by Kambuzi EDB, inspired by the source/destination
workflow of Directory Opus 5. New MIT implementation, not a port or a compatible
replacement. Development candidate: **0.6.0**. Published preview: **0.2.0**.

Saved work locations: each panel's Go > Places menu can save the current
location, rename it or forget it. Names are shared across the two panels and
persist across restarts. Local folders, archive folders, server addresses and
the disks overview are supported. Credentials remain session-only; reconnect
to an authenticated server before reopening its saved location after restart.
See [saved places and bughunt](docs/PLACES-0.6.0.md) and the additional
[Tracker capability requirements](docs/TRACKER-COVERAGE.md).


Two independent resizable panels, editable paths and location menus, sortable
name/size/date columns, multiselection, drag-copy, node monitoring, a command
button bank, viewer, editor and terminal launch, and saved display preferences.
Native Haiku controls and system colours, including dark themes.

## Workflow

The bold active panel supplies selected files; the other panel is the
destination. Click a panel or press Tab in a list to switch roles. Double-click
a directory to enter it; Backspace goes up. Use Haiku's **Command modifier**
(normally **Alt**, following your system keymap) plus **V** View, **E** Edit,
**C** Copy, **M** Move, **R** Rename, **N** New folder, **D** Trash or **F** Filter.
Buttons show the letter; menus show the native modifier. Bare letters remain
available for list type-ahead. In path, filter, command and name fields,
Command+A/C/X/V selects/copies/cuts/pastes text, never files. Press Enter after
editing a path or filter to return to the list. Escape cancels a name dialog.
F3/F4/F5/F6/F7/F8/F9 remain secondary aliases outside text fields.

Each panel has its own **< Back / > Forward** path buttons. **Command+[ / ]**
work while browsing that panel, not while editing text or answering a question.
Up to 64 successful locations are kept for this session only, including local,
archive, server, volume and Trash locations. Failed or cancelled scans do not
become visits. Back from a failed open first retries the last successful place.
Refresh preserves the forward branch; visiting a new place discards it. Returning
reloads contents without restoring an old selection. History is not file undo.

Operations run on a worker queue. The inline Operations area shows progress,
Pause/Resume, Cancel and Replace/Replace all/Skip/Skip all/Abort conflicts.
Escape closes the active question first; otherwise it cancels a pending conflict
and collapses Operations. The action-button bank gives way to an open question
and returns when it closes; menu actions and shortcuts remain available. The main window will not quit
while an operation is active. Command+D / Trash moves entries to Haiku's per-volume Trash, with Tracker-compatible
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

    pkgman install ./Listi-0.6.0-1-x86_64.hpkg

Listi replaces Lister in Apps and Deskbar. The package identity (`lister`),
application signature (`application/x-vnd.Kambuzi-Lister`), scripting suite and
preferences (`~/config/settings/Lister_settings`) intentionally remain compatible.
There is no settings migration or duplicate Lister launcher. Keep the old HPKG
for application rollback; the GitHub repository name has not changed.

Build the current native application **inside Haiku x86_64**. A future portable
C core will have separate POSIX tests; it does not exist in this release.

    gcc --version
    pkgman install haiku_devel python3 libarchive_devel curl_devel samba4_devel
    make
    make test
    make package

`make` uses Haiku makefile-engine. The executable is in
`src/objects.*-release/Listi`, not the project root. `make package` uses
PackageInfo and produces `dist/Listi-0.6.0-1-x86_64.hpkg`. Retain previous
packages for rollback; reinstalling one does not restore or roll back files you
have copied, moved or deleted.

See [design](docs/DESIGN.md), [roadmap](docs/ROADMAP.md) and
[verification](docs/VERIFICATION.md). No accounts, telemetry or paid services.

## Internal 0.4.0 features and limits

Use **Command+K** to connect to SMB, SFTP, FTP or FTPS. Credentials stay in memory. Tab between fields without connecting; use Connect or Enter to submit.
SFTP needs an existing trusted known_hosts file. FTP is unencrypted. Package
requirements install libarchive, libcurl and libsmbclient; backend errors are
reported without pretending an unsupported operation succeeded. SMB, SFTP and
FTP are tested with synthetic servers; FTPS and external NAS models are not yet
qualified.

ZIP and tar containers open in a panel for browsing and extraction. Standalone
`.gz`, `.bz2` and `.xz` files expose a decoded file; use `.tar.gz`, `.tar.bz2` or
`.tar.xz` for compressed tar containers. Archives are read-only and must be local.
Cross-location moves are refused with Copy guidance; remote Trash is not emulated.

Each panel's View menu selects details, icons, large icons or thumbnails, remembers
a folder-specific view, restores its default, and toggles an independent folder
tree. **Command+P** toggles image preview. The expanded 1024x650 laptop layout is
qualified at the default font size; larger accessibility fonts may need more room.
Tree branches are refreshed by closing/reopening their tree. New uncached paths
rebuild the route automatically. Thumbnail replies/caches, preview decoding,
folder preferences, tree nodes and operation history have explicit bounds.

This is a private candidate, not a Dolphin-parity or performance claim. See
[the current qualification](docs/INTERNAL-CANDIDATE.md) for exact tested scope,
recovery and limitations. Dependency licenses remain their own; MIT covers Listi's
source, not every library provided by Haiku packages.
