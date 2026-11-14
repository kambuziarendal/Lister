# Design and boundaries

The original Directory Opus 5.5 manual and DOpus5 guide were consulted, especially
listers, source/destination selection, button banks and keyboard/mouse operation:
https://github.com/mheyer32/dopus5allamigas/tree/master/documents

DOpus 5 is more than two panels: independent listers, locked roles, configurable
commands and filetypes are important later steps. This preview starts with two
listers and explicit source/destination roles. No DOpus code/assets are included.
GenesisCommander was considered but not used; a small native foundation avoids
inheriting its synchronous operations and fixed-coordinate UI.

- Pane owns a native column list and an asynchronous directory scanner. Scan
  generations suppress obsolete results. Node monitoring requests a new scan.
- OperationQueue owns a worker thread, queue, pause/cancel signalling and native
  progress window. The filesystem engine does not access widgets.
- Regular copies use a temporary sibling, preserve attributes, permissions and
  timestamps, then replace the destination only after a successful copy.
- Symlinks are copied as links, never recursively followed. Merged/cross-volume
  moves compare source snapshots and remove only copied entries. New/changed
  source entries are retained with a visible error; partial moves are possible.
- Main-window file reads, launches and settings writes use a service looper.
- Layout Kit arranges all controls. BRect is used only for initial window bounds,
  native bitmap bounds and drawing coordinates, not fixed widget positioning.
- Haiku's private ColumnListView API is isolated in `src/Pane.cpp` and statically
  linked. All other UI uses public BeAPI. A future OS ABI change may need a rebuild.

Limitations: not a transactional backup tool; concurrent external edits can race
operations. No remote filesystems, archive provider, trash/undo, saved tabs,
configurable button commands or content search yet. Built-in viewer reads text
up to 1 MiB. File browser metadata/icon reads are off the window thread.
