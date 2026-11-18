# Roadmap

## The ambition

Listi aims to be a capable native working environment for files on Haiku: simple
enough for quick browsing, but deep enough for sustained, selection-oriented
file work. Directory Opus inspires the source/destination model and freedom of
action. Haiku's controls, attributes, queries and application ecosystem shape the
implementation.

The long-term ambition is broad coverage of the practical tasks people use
established file managers for, with a coherent Listi workflow rather than copied
interfaces. Coverage, responsiveness and resource use must be demonstrated task
by task. This is a direction, not a claim that the alpha already matches or
outperforms Directory Opus, Tracker or Dolphin.

For the 0.32.0 GitHub test release, see the [README](../README.md) and the
[workflow guide](GUIDE.md). The ideas below are not a release schedule.

## Optional by design

**Depth should be available, not compulsory.** A straightforward file browser
should remain a complete way to use Listi. Advanced views, inspectors, automation,
Git integration and other specialist tools should be optional and enabled when
useful—not imposed on every workspace.

The customisation goal goes beyond toggling a few controls:

- Add, remove, move and reorder action buttons, toolbars and workspace modules.
- Arrange panels, trees, inspectors, filters and an optional terminal around the
  task. Keep keyboard access and readable layouts at different window sizes.
- Save personal layouts and restore defaults without losing unrelated settings.
- Keep a useful standard layout for people who never customise anything.
- Make adaptive behaviour an explicit choice, with manual control and a
  predictable way back. Do not silently rearrange someone's workspace.

Version 0.32.0 has four presets and a workspace editor: choose and order command buttons, place modules in supported
zones and save named layouts. Optional adaptive sizing reflows the button bank;
it does not choose the active panel or view mode. This is not arbitrary docking
or a system for creating new commands. Safety checks, confirmations and
clear operation targets are not optional just because a tool is configurable.

## First: make existing file work easier to trust

Included in the 0.32.0 alpha:

1. **File safety and partial moves.** Local operations report what happened to
   selected entries. In a move that copies a tree before removing the source,
   individual entries distinguish committed moves from copies whose sources
   Listi did not remove. Native fault tests cover permissions, interrupted
   writes, full volumes and cancellation at different stages of a move.
2. **Job identity and results.** Jobs have IDs from acceptance. In Operations,
   Details lets you review a job after another one starts. Completed results
   do not depend on progress notifications. Cancel one job or cancel the queue;
   neither action undoes completed work.
3. **Shared command handling.** File actions share command IDs across menus,
   buttons and shortcuts. Transfer entry points reuse the same preparation
   and guarded worker operations rather than implementing their own copies.
   Clipboard ownership, archive verification and drop choices retain their
   specific checks.

4. **Selection and retries.** Refreshing a local folder no longer selects a
   replacement file merely because its name matches. Colliding paste/drop names
   are rejected before work starts. If the original parent now points into
   Trash, restore keeps the entry and its metadata so you can retry.

See the [workflow guide](GUIDE.md) for interpreting partial results and the
[verification guide](VERIFICATION.md) for reproducing fault tests. Job reports
are session information, not a persistent audit log or an undo mechanism.

## Priorities after 0.32.0

### Immediate usability fix: paste clipboard contents directly

In a writable local file panel, ordinary Paste (keyboard shortcut or context
menu) should use the clipboard type. Keep existing file-reference Copy/Cut
behaviour first. For plain text, open the existing filename prompt immediately
with the name selected so the user can type `Listi-forslag.md` and press Enter.
For an image, offer the existing PNG save flow. Keep ordinary text paste in
editable fields unchanged. Check cancellation creates nothing and an existing
file is never overwritten. No submenu should be necessary.

### Next workflows, in priority order

1. **Flat View follow-up.** The development build has a read-only [Flat
   View](GUIDE.md#filtering-and-search) for selecting local files and adding
   references to the Shelf. Multi-folder rename comes only once preview and
   job results identify every original.
2. **Extend bulk rename rules.** Chains, swaps and guarded session undo are
   available. Later add regex, date/time and typed BFS attribute placeholders
   with per-row previews. External-editor name lists can wait until row
   identity survives edits and reordering.
3. **BFS attribute columns, then searchable attributes.** Let users choose
   typed columns in Details and distinguish missing values from empty ones.
   Start with existing indexes and live queries, including tag searches.
   Prototype “Make searchable” separately: show the affected volume, check
   type, index support and permissions, and verify whether existing files
   actually appear in a query. Never silently create a volume-wide index.
4. **Saved collections on the Shelf.** Save named sets of references, not
   copies; removing a member never removes the original. Verify identity on
   Haiku across rename, same-volume move, restart, unmount and path reuse.
   A stored path or node_ref alone is not proof that a rediscovered file is
   the same entry. Show missing or ambiguous members and allow explicit relink.
5. **Faster selection and paired navigation.** Add wildcard selection with a
   visible match count, separate from the literal filter. In Tvískipt, offer
   linked browsing relative to two independently chosen roots. If the peer
   subfolder is missing, show that and leave the other panel in place; never
   create or synchronise files just because someone navigates. Follow with
   independent panel tabs and named paired sets that restore locations and
   view state, never file jobs.
6. **Quick view and reusable buttons.** Reuse the Smiðja preview from every
   workspace, temporarily in the opposite panel when available. Closing it
   restores the panel, focus and selection; Space still selects. Let the
   workspace editor save presets for existing commands, starting with rename
   previews rather than a scripting language.

### Focused follow-ups, not prerequisites

- Feed a panel from a saved BFS query before considering arbitrary command
  output. Show origins and distinguish a live result from a saved collection.
- Add an on-demand, cancellable folder-size scan. Distinguish logical bytes
  from allocated space, define hard-link counting and flag unreadable subtrees.
  Do not slow down ordinary selection summaries.
- Investigate Tracker add-on discovery, MIME matching and invocation from a
  non-Tracker host on Haiku, using a frozen selection. Prefer proven native
  compatibility to a new plugin API; defer execution until that contract works.
- Extend folder comparison into a reviewable one-way sync plan before two-way
  or remote writes. Distinguish new, changed and equal entries; absence alone
  cannot distinguish a deletion from a new file without prior sync state.
- Consider per-transfer network rate limits after measuring contention.
  After rename undo is proven, evaluate undo for creating a still-empty folder
  or restoring an unchanged Trash entry; do not promise general undo.

### Engineering work that supports this sequence

- **Keep the native test runner as the reference.** Add focused suite selection
  and a failing exit status for missing or failed required checks where needed.
  Start CI with checks that genuinely run on the available host; label those
  separately from native Haiku builds, fault tests and installed-package checks.
  Do not make paid or hosted Haiku capacity a prerequisite.
- **Split App.cpp at a real change boundary.** At 2,314 lines in 0.32.0, extract
  the command or panel responsibility touched by the first workflow, with tests
  for existing menu, button and keyboard behaviour. Avoid a wholesale rewrite.
- **Localise incrementally.** Introduce Haiku translation contexts in the first
  changed workflow, including errors and plural counts. Verify catalogue
  extraction, fallback text and layout with a longer translation before rollout.
- **Make releases easier to follow and report against.** Start a concise
  changelog with user-visible changes and compatibility notes; add SECURITY.md
  once a monitored private reporting route is confirmed. Keep reproduction
  instructions in the existing verification guide.
- **Review the duplicate-finder prototype rather than importing it wholesale.**
  Gate integration on native tests for changed files, hard links, unreadable
  inputs and cancellation, followed by a read-only group-review flow. The
  existing Duplicate command must still mean “make a copy.”

## Feature directions and current foundations

“Available now” means included in 0.32.0. A related foundation is
not the same as a finished feature.

| Direction | Available now | Planned or exploratory work |
|---|---|---|
| Duplicate finder | SHA-256 folder comparison; Duplicate creates another copy | Search one or more chosen folders for duplicate contents and review groups |
| Bulk rename | Local multi-file preview, text replacement, numbering and collision checks | Broader rename transformations |
| Miller columns and adaptive views | Four presets, optional Miller columns and opt-in compact buttons | Automatic file-view changes remain exploratory |
| Directory synchronisation | Read-only recursive size/SHA-256 comparison | Previewed, conflict-aware one-way or two-way synchronisation |
| Clipboard contents to a new file | Copy/Cut/Paste of file references; save clipboard text or PNG images as a new file | Explore additional clipboard formats when useful |
| Rich built-in inspectors | Smiðja file inspector, image thumbnails, archive browsing, local properties | Detailed image navigation and camera EXIF/GPS metadata |
| Copy queues and pause | Per-job results, pause/resume and targeted cancellation | Persistent history is not implemented |
| Automatic rules and drop folders | Manual file actions, search filters and live BFS queries | Opt-in sorting rules and configured drop targets for extract/rename/move workflows |
| Colour coding and tags | Configurable file/folder/link colours and typed BFS attribute editing | Dedicated per-item labels, tag editing/filtering and optional colour rules |
| Saved project sessions | Named layout profiles, four presets, saved places and folder view modes | Named sets of open locations, future tabs/cards, views, sorting and layout |
| Git status in file lists | No built-in Git status display | Optional status badges or columns without blocking browsing |
| Built-in text/file diff | Read-only folder comparison by size or SHA-256 | Side-by-side text differences and file-level comparison tools |

## More depth without more friction

### Find duplicates and work with selections

A duplicate finder should search explicitly chosen locations, narrow candidates
by size, then verify content rather than equating a filename or size match with a
duplicate. Show groups, origins and the reason for a match. Handle unreadable or
changing files, links and cancellation honestly. Keep review separate from any
cleanup action; finding a duplicate must never delete it automatically.

Listi previews local bulk renames with search/replace,
numbering, prefix/suffix, collision checks and guarded execution. Configurable
actions should operate on frozen selections and pass filenames as data, not
shell code.

### Follow deep folder trees without losing context

Miller columns are a file view, independent of the workspace. Each workspace starts
in Details and has a configurable default view. Explore adaptive views only
with explicit user control and sensible minimum sizes.

Readable, BeOS-inspired folder cards and saved paired work sets remain goals.
Named project sessions—such as Accounting, Video editing or Personal—could
restore open locations, future tabs/cards, view modes, sort choices and layout.
They must distinguish saved UI state from active operations: restoring a session
must not replay old file jobs or store credentials in plain layout files.

### Inspect files without leaving the task

The optional Smiðja inspector shows images, code, text, Markdown and HTML
source, PDF pages, and supported audio/video previews. Next, add high-resolution
image navigation and camera EXIF metadata, including GPS coordinates where
present. Decoding an image today does not mean those inspection tools already
exist. Location display must not contact map services or expose coordinates
without an explicit choice.

Preview must not execute scripts or modify files. Explicit editing, read-only
states and unsaved-change handling remain important. **Terminal here** offers
an integrated shell or a separate terminal window for a local working folder.

Built-in text/file diff should complement existing folder comparison with
readable side-by-side differences. Comparison and editing/merging are separate
capabilities; support should be labelled rather than implying every document or
binary format can be meaningfully diffed.

### Synchronise with a plan, not a surprise

Directory synchronisation should start with a proposed action list: direction,
new or changed entries, conflicts and any deletion. Make one-way and two-way
semantics explicit, recheck changed sources and let the user review before
writing. Do not treat a successful folder comparison as permission to synchronise
or delete. This follows the stronger job-result and conflict model.

### Turn repeated chores into opt-in workflows

Explore rules based on names, extensions/types and dates, with named target
folders and explicit extract, rename or move actions. Configured **drop folders**
could apply a chosen workflow when files are dropped there. These are not the
same as read-only live BFS query results.

Start with visible, explicitly configured triggers and previews/dry runs. Avoid
hidden background moves, recursion loops or overwriting files without review.
A watched folder, a drop target and a general scripting hook have different
trust and lifecycle requirements; none is implicitly enabled by this roadmap.

Listi can save plain clipboard text or an image as a new named
file in a local folder. Images are saved as PNG. File references still use
ordinary Paste.

### Collect files from several folders on a Shelf

Listi has a separate Shelf window for local
references, with original-folder and availability columns, drop support,
Command+Shift+Y, and queued copy and move actions.
Create archive packs references from different folders in a chosen format
without moving the originals. Named snapshots preserve references across
sessions; the working Shelf itself remains session-only. Sharing is still
planned. Verify Stack & Tile ergonomics during hands-on testing.

### Send a selection to a phone

Plan a **Send to mobile** context-menu action. On request, start a temporary
local web server for the selected files and show a QR code. A phone on the same
network scans the code to download them in its browser, without a cloud account.
Serve only the selection through an unguessable, short-lived link, not its
parent folders. Show when sharing is active, when it expires and how to stop it.

### Use Haiku's desktop tools

- Let tool windows such as the Shelf participate in native **Stack & Tile** or
  remain independent, rather than implementing Listi's own docking system.
- Listi offers **Convert here** for a single local image drop,
  using installed Translation Kit output formats without overwriting files.
  Ordinary Copy and Move choices remain available for cross-folder drops.
- Explore an optional desktop **Replicant** as a drop zone. A first use could be
  adding files to the Shelf without bringing a Listi window to the front.

### Organise and inspect project context

Explore optional file labels/tags, colour coding and filtering using native
metadata where practical. Category colours and a generic attribute editor are
useful foundations, not a finished tagging system. Explain persistence and what
happens when files move to a provider that cannot preserve the metadata; colour
must not be the only signal.

Optional **Git status** could show modified, untracked, staged and ignored state
in the file list. Keep discovery and refresh bounded and asynchronous, with a
clear unavailable state outside repositories or when support is missing. Merely
browsing must not change repository state or fetch from a remote.

### Broader archives, search and network workflows

Make archives feel like folders wherever each format permits, while showing
capability and metadata limits explicitly. Selected encrypted and multipart
reading, local editing, and archive/local moves already exist; remaining work
includes broader variants, metadata fidelity and supported write engines.

Build on saved searches, date/size/extension filters and live BFS queries with
clearer scopes, exclusions, attribute tools, content search and broader volume
coverage. Results must retain a clear connection to originals.

Continue testing interrupted transfers across protocols, servers and larger
datasets. Availability of SMB, SFTP, FTP or FTPS does not qualify every server or
WAN condition.

## Longer-term possibilities

- **Conflict-aware recovery:** limited undo and a durable journal need a defined
  scope. A job log is not a backup; recovery must not overwrite newer data.
- **Automation interfaces:** consider a local CLI or socket with explicit access
  boundaries. Scripting, hooks and plugins are not implied by an internal
  command registry or an optional rule editor.
- **Portability:** extract tested core logic incrementally, keeping Haiku UI,
  BFS, clipboard, Trash and application integration behind adapters. Other
  platforms need native filesystem, package and UI verification. A port is not a
  reason to replace the Haiku experience.
- **Desktop integration:** coexist with Tracker by default. A replacement role
  requires a separate compatibility and recovery study.
- **Optional peer sharing:** a future adapter could connect to a separately
  developed sharing system; it is not a current dependency or shipped feature.

## How progress should be judged

Deliver small, complete workflows, measure them on documented environments, and
publish known limits alongside capabilities. A feature name, successful build or
large test count is not enough to establish a dependable user experience.

The aim is useful depth with a native feel, not a race to the longest checklist.
There are no fixed delivery dates, promised performance ratios or claims of
universal crash consistency.
