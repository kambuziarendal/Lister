# ROX-Filer comparison, 15 November 2026

Documentation/code comparison, not a runtime, memory, usability or speed benchmark.
ROX upstream tree inspected: `dbc77943c043dba62d68f633757349c131d2fee8`.
Sources: [manual](https://github.com/rox-desktop/rox-filer/blob/dbc77943c043dba62d68f633757349c131d2fee8/ROX-Filer/src/Docs/Manual.xml),
[project](https://github.com/rox-desktop/rox-filer), and its Help/Changes.
Listi baseline: private 0.29.0 candidate, source and documented tested scope.

| Area | ROX-Filer manual | Listi baseline |
| --- | --- | --- |
| Workflow | RISC OS-style folder windows, single-click and drag/drop | Source/destination panels, queue, plus single-panel workspaces |
| Search | Conditions combine name/type, size and timestamps, e.g. `mtime after 1 day ago` | Recursive name search; indexed/live BFS formulas and saved searches; no friendly date/size/type builder |
| Rename | Bulk rename table, editable proposed names and regex search/replace preview | Single-entry rename; bulk rename is a gap |
| Actions | Type-specific Send To and custom menus; shell minibuffer receives selected paths as positional arguments | Native Open with for one local file and command field; no equivalent general user-defined selected-file actions |
| Archives | Documents external AVFS/FUSE integration | Integrated archive providers, guarded updates, editor save-back and verified moves, with explicit format/metadata limits |
| Network | Can use externally exposed/mounted locations | Integrated SMB, FTP/FTPS and SFTP providers; capabilities vary by operation |
| Copy verification | No equivalent integrated SHA-256 folder comparison established by this manual review | Recursive size/SHA-256 diff; full-byte read-back for cross-container moves |
| Desktop | Pinboard, edge panels and application directories | Cooperates with Haiku/Tracker; not yet a desktop replacement |
| Common features | Bookmarks, image thumbnails, selections, permissions and symlinks | Existing features too, with documented operation/provider limits |

Suggested next priorities, not newly implemented features: friendly date/size/type
filters, bulk rename with preview and collision checks, then configurable actions
on selected files. Preserve ROX's directness without importing an X11 desktop
replacement or assuming newer code is faster. Snapshot browsing is a separate,
low-priority capability-gated feature; native Haiku BFS does not offer snapshots.
