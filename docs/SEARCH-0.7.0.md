# Find files, private 0.7.0 candidate

## Delivered workflow

Listi > Find files, or Command+Shift+F, opens an independent native search
window. Command+F remains the current-folder name filter. The search window
retains its scope and owning panel until closed, even if focus changes in the
main window. Opening Find files again raises that existing search.

Choose a local folder and a non-empty literal name fragment. Search is recursive
and case-insensitive using Haiku's Unicode character lowercase mapping (no Unicode
normalization or multi-character case expansion). Spaces, UTF-8 and wildcard
characters are literal in this mode. Search or Enter submits; focus loss does not.

Enable **BFS formula** to use Haiku's BQuery syntax, for example
`name=="*.txt"` or `(name=="*.txt")&&(size>0)`. This is the native query language,
not a new Listi grammar. Queries use the selected folder's volume and filter
results to its descendants. Indexed attributes such as `BEOS:TYPE` can be used.
Native index availability and predicate semantics apply. For example, our test
volume does not have a BEOS:TYPE index; Listi does not create indexes automatically. A syntactically valid
query does not promise that an unindexed attribute can be found. Formula editing
is an initial advanced route, not the finished attribute-search interface.

Names and original folders are separate columns. Selecting a result displays
its full original path, also available in a tooltip. **Show original in panel**
opens its containing directory in the panel that started the search and selects
the original entry. Existing copy/move/Trash/view actions then operate normally.
The other panel and already queued operations retain their own paths. Search
itself never modifies files. Before reveal and after directory loading, device
and inode must still match the result. Missing, moved or replaced entries are
not substituted by name; previous selection is cleared on rejection.

## Scope, cancellation and limits

- Results are explicitly **snapshots**, not live queries. Search again to refresh.
- Name traversal stays on one persistent, non-shared volume, never recurses into
  symlinks, and may return a symlink itself. Root aliases are resolved once.
- Other mounted filesystems and unreadable/changed entries are reported as
  skipped areas. A whole volume can be searched by choosing its mount root.
- Trash is excluded by default. Include Trash explicitly to include it, including
  when the selected root itself is inside Trash. No Trash directory is created
  by the search engine.
- Stop/Escape preserves only received results and labels them incomplete. A new
  generation ignores all late batches/status from an earlier search. Closing
  the window cancels without waiting for worker filesystem calls on the UI thread.
- 5,000 results and 250,000 examined entries bound the search. Directory depth is
  bounded at 256, pending directories at 4,096. Limits/skips are visible, never
  silently labelled a complete search. BFS examined count includes candidates
  outside the folder scope before filtering, so a broad formula may reach its
  limit before all in-scope hits are found.
- Invalid roots, unsupported providers and malformed predicates report failure,
  not a successful empty list. Long paths/errors remain within the window and
  can be read through tooltips. Input is bounded to Haiku's path limit and 4,096
  expression bytes.
- Cancellation is cooperative between filesystem calls. It does not forcibly
  interrupt a kernel call, but it does not block the window thread.

## Requirements still pending

This increment is not full Tracker/Dolphin search parity. Live BFS updates,
saved queries/templates, attribute/type builder, explicit multi-volume selection,
time-expression UI/qualification, searching virtual providers and direct
multi-folder result operations remain required. No desktop replacement or
public publication is implied. See TRACKER-COVERAGE.md and FUNCTION-COVERAGE.md.

## Verification

Source-built search engine: 33 native checks passed, including duplicate names,
UTF-8 case mapping, symlinks, result/work limits, cancellation, stale original
identity, BFS formulas, native indexed string attribute, invalid predicates and
Trash inclusion. A task-owned BFS index was created before its synthetic
attribute and removed afterward; no existing index was changed.

Actual source-built UI checks passed for explicit submission, duplicate paths,
original reveal/copy, stale replacement rejection, BFS/error states, cancellation
and a new generation, both panels, compact sizing and live light/dark palettes.
The final Unicode change and additional selection guard are qualified again in
the installed candidate. Installed-package receipts follow after final checks.

The first expanded unit run exposed ASCII-only BString name matching and an
incorrect fixture assumption that BEOS:TYPE was indexed. Search now uses native
Unicode lowercase mapping; the indexed-attribute test explicitly owns its index.
Initial UI harness errors concerned BTextControl scripting syntax and an overly
long text readback; these are retained as failed exploratory logs, not passes.
The qualification environment is the isolated Haiku beta6 hrev59866+79 x86_64 VM,
not physical hardware or a comparative performance benchmark.


## Final installed receipt, 14 November 2026

- Package source: `dba04a158ea48ca70de079e3dae3d11e0e9e2d14`, local branch
  `work/smb-first`. This acceptance section was added after packaging and does
  not change the qualified binary or package.
- Build and installed executable SHA256:
  `ce79a0e7acbb7c48c170b05aa28c71a81688e7552f3e74f33e0429f2443f7a82`.
- HPKG SHA256:
  `6dbbd21e68f7947897ec7d37f400696c4d989c869d09b830c416943f99b2cc0b`.
- Private ZIP SHA256:
  `a2a98325945549a7ed4adc913ac793b0a80ba8721502471b1da4d71ec51f9ecf`.
- 67 source/resource/package inputs match both native build tree and isolated
  packaging stage. Package/native application version is 0.7.0.
- The final installed executable passed 55 local and 96 archive/network checks.
  The candidate search object passed 33 native engine checks. Unchanged history,
  places and preference helper suites also passed 23 + 29 + 12 checks; these
  helpers reuse the unchanged 0.6.0 module implementations, not UI evidence.
- Installed UI: 42 assertions across the main search and conflict lanes, plus
  screenshot review. Unicode original reveal/copy and indexed META:name query
  passed on the actual installed app. Closing an active search then opening a
  fresh one ignored old worker replies. Pending copy retained its original
  source and destination despite search-driven navigation in both panels.
- Exact package rollback 0.7.0 -> 0.6.0 -> 0.7.0 passed, including launching the
  predecessor, actual Preferences Save, semantic preferences preservation,
  byte-identical places/file data, saved-place navigation and Unicode search
  after re-upgrade. Search adds no persistent settings schema.
- Rollback: close Listi after file jobs finish, install the retained private
  0.6.0 HPKG with `pkgman install -R -y <package>`. Do not restore stale settings
  or user files. The same 0.7.0 HPKG can be reinstalled afterward.
- Final app and task-owned fixture servers stopped; inherited VM retained.
  No public push, release or Depot change. Physical hardware, full Tracker/Dolphin
  parity, live searches and comparative resource benchmarks remain unqualified.

Local evidence: `reports/listi-search-20261114/` under the Kambuzi workspace,
including final-native-search.log, installed-native.log, installed-ui.log,
installed-conflict-{start,finish}.log, roundtrip.log and installed-ui screenshots.
