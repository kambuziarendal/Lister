# Listi 0.24.0 private candidate acceptance

Runtime source commit: `6e7035f`. No public push or release was made.

## Native verification

- Fresh native build from a Git archive of the runtime commit.
- 699 checks across 23 compiled native suites passed with no failures. The
  suites cover archives, passwords, preferences, navigation, selection,
  search, clipboard, drag and drop, file operations, metadata, themes and the
  application self-test.
- The installed application binary matched the fresh native build byte for
  byte: `cb649fe9c99cd3f1377dee415755a501646cf87b80341acad71b521f06728a1f`.

## Installed Workbench UI

- View > Workspace > Workbench shows both file panes, both folder trees and
  image preview on first use. Filters, address controls, command bank and
  operation details remain available.
- Switching through Commander, Simple, Explorer and Workbench retained both
  paths, the active selection, window geometry and the pane split.
- A copy waiting on a real destination conflict remained owned by the queue
  through Commander and Workbench switches. Aborting preserved the existing
  destination bytes.
- Workbench tree and preview choices remained distinct from Commander, survived
  a restart and restored without rebuilding the pane objects.
- Light and dark appearance were visually inspected. A 980 by 600 window kept
  both trees, both panes, preview and the action controls usable without resizing
  the main window.

## Rollback and upgrade

The installed package was changed from 0.24.0 to the verified 0.23.0 package and
back to the final 0.24.0 package. Version 0.23.0 safely opened the unknown saved
workspace as Commander. Re-upgrading restored Workbench. Settings, places,
saved searches, archive preferences, test file contents and the typed BFS test
attribute remained byte-for-byte unchanged.

Final HPKG SHA-256:
`23abf0b77a477cd8bea11a11c0ce7a9c5898601a16fb803ff76696f2b4a0a9f5`.

## Scope boundary

Workbench is a layout preset built from existing qualified components. It adds
no file operation, background search or continuously open operation log. Search
and detailed operation history remain explicit, on-demand actions.
