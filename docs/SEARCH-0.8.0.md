# Live and saved searches, private 0.8.0 candidate

## Workflow

Find files remains Command+Shift+F, normally Alt+Shift+F on Haiku. Literal name
searches remain cancellable snapshots. Enable BFS formula and Live for changing
results on a persistent local query-capable volume. Root, expression, mode and
Trash choice are locked while running. Stop or Escape stops the subscription;
received results remain a stopped snapshot. Close releases the worker off the UI
thread. Generation and refresh revision reject late replies after a new search.

Live BFS uses BQuery notifications, batched at 750 ms, and a three-second bounded
snapshot reconciliation. Reconciliation also covers ancestor-directory moves and
relative-time expressions which do not necessarily emit matching-entry events.
This is eventual refresh, not instantaneous or a transactionally consistent view.
Only one scan runs per worker. Existing limits remain 5,000 results and 250,000
examined entries, with explicit incomplete/error states. A limit or skipped area
ends live mode rather than presenting an incomplete set as a complete live view.
Volume-wide BFS matching work can exceed the number of descendants displayed.
Large-volume idle cost has not been qualified as a performance improvement.

Refresh is staged before replacing the visible result set. Unchanged path and
node identities retain their rows and selection. Replacement, movement or removal
never silently transfers selection to another file. Show original still validates
identity in the owning pane before and after directory scanning. Existing jobs
retain their original source/target; refreshing a search does not rewrite jobs.
The root is canonicalized and its device/node identity retained. A missing or
replaced root ends the subscription and requires an explicit fresh search.

## Saved criteria

The Saved menu loads named criteria without running a search. Search explicitly
obtains current results. A definition contains name, local root, expression,
BFS/literal mode, Trash choice and Live choice. It contains no result list,
credentials, selected files or pending operations. Missing/offline roots can be
loaded and edited; executing them reports a visible failure.

Save updates the selected definition if its name is unchanged. A new name saves
a separate copy; collision with another saved name is refused. Forget removes
only the selected definition and is disabled while the name differs from that
selection. It never removes results or original files. Text blur does not save or
submit. The native menu and buttons support keyboard focus as well as the mouse.
Definitions are shared between searches started in either panel; the newly opened
search window, not the definition, determines the owning panel for Show original.

Up to 64 names, 80 bytes each, and 4,096-byte expressions are accepted. Criteria
are stored separately in B_USER_SETTINGS_DIRECTORY/Lister_searches, format 1.
Older packages do not load or rewrite this file. This is Listi's criteria store,
not Tracker .query files, templates, imports or exports. Those remain separate
interoperability requirements, along with a friendly attribute/date builder,
content search, multiple search volumes and virtual-provider searches.

The main window owns the committed list and revision. Services writes a validated
candidate on its worker with a private sibling temporary, full write, fsync, close
and atomic rename. Only success replaces the in-memory committed list. Closing a
search window does not own or interrupt that write. Main-window quit is deferred
while criteria are being saved. Failed saves preserve the previous list and return
an error; stale revisions are rejected. Missing stores start empty. Corrupt,
unknown-format, nonregular and over-1-MiB stores are preserved and disable edits.
Saved places now share the bounded atomic message-store IO, with their existing
format unchanged. This is not a promise of crash/power-loss transactional storage
across unrelated settings files.

## Qualification commands and evidence

In the isolated Haiku build tree, compile tests with -std=c++17 -Wno-multichar
and -Isrc, linking libbe:

- tests/live-search.cpp + src/SearchWorker.cpp + src/FileSearch.cpp.
- tests/saved-searches.cpp + src/SavedSearches.cpp + src/MessageStore.cpp.
- tests/file-search.cpp + src/FileSearch.cpp.
- tests/saved-places.cpp + src/SavedPlaces.cpp + src/MessageStore.cpp plus the
  built VirtualFile, ArchiveFile and NetworkFile objects.

The source tests cover native notifications, file/attribute entry and exit,
ancestor movement, replaced roots and file identity, cancellation/restart,
validated criteria, corrupt/unknown stores and failed writes. Native UI checks
cover retained/cleared selection, fresh saved results, explicit update/forget,
failed commit, corrupt-store preservation, compact size and both themes.

Installed artifact identity, final UI checks and 0.8.0 to 0.7.0 to 0.8.0 recovery
must be recorded in the acceptance receipt before delivery. No public publication
is part of this candidate.
