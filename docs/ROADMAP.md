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

For implemented behaviour, see the [README](../README.md) and
[workflow guide](GUIDE.md). The ideas below are not a release schedule or a
commitment to implement them all at once.

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

The published 0.31.1 alpha has four presets. The development branch adds a
workspace editor: choose and order command buttons, place modules in supported
zones and save named layouts. Optional adaptive sizing reflows the button bank;
it does not choose the active panel or view mode. This is not arbitrary docking
or a system for creating new commands. Safety checks, confirmations and
clear operation targets are not optional just because a tool is configurable.

## First: make existing file work easier to trust

Implemented on the development branch, not in the published 0.31.1 alpha:

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

See the [workflow guide](GUIDE.md) for interpreting partial results and the
[verification guide](VERIFICATION.md) for reproducing fault tests. Job reports
are session information, not a persistent audit log or an undo mechanism.

## Feature directions and current foundations

Status here refers to the 0.31.1 alpha. A related foundation is not the same as a
finished feature.

| Direction | Available now | Planned or exploratory work |
|---|---|---|
| Duplicate finder | SHA-256 folder comparison; Duplicate creates another copy | Search one or more chosen folders for duplicate contents and review groups |
| Bulk rename | Single-entry rename | Multi-file preview, transformations, numbering and collision checks |
| Miller columns and adaptive views | Four preset layouts and optional Miller columns, default in Einn | User-controlled adaptive layouts |
| Directory synchronisation | Read-only recursive size/SHA-256 comparison | Previewed, conflict-aware one-way or two-way synchronisation |
| Clipboard contents to a new file | Copy/Cut/Paste of file references; development build saves text and PNG images | Explore additional clipboard formats when useful |
| Rich built-in inspectors | Image preview/thumbnails, a basic text viewer, archive browsing, local properties | Broader code/document inspection, detailed images and camera EXIF/GPS metadata |
| Copy queues and pause | Worker queue, progress, pause/resume and cancellation | Stronger job results and finer per-job control |
| Automatic rules and drop folders | Manual file actions, search filters and live BFS queries | Opt-in sorting rules and configured drop targets for extract/rename/move workflows |
| Colour coding and tags | Configurable file/folder/link colours and typed BFS attribute editing | Dedicated per-item labels, tag editing/filtering and optional colour rules |
| Saved project sessions | Four workspace layouts, saved places and folder view modes | Named sets of open locations, future tabs/cards, views, sorting and layout |
| Git status in file lists | No built-in Git status display | Optional status badges or columns without blocking browsing |
| Built-in text/file diff | Read-only folder comparison by size or SHA-256 | Side-by-side text differences and file-level comparison tools |

## More depth without more friction

### Find duplicates and work with selections

A duplicate finder should search explicitly chosen locations, narrow candidates
by size, then verify content rather than equating a filename or size match with a
duplicate. Show groups, origins and the reason for a match. Handle unreadable or
changing files, links and cancellation honestly. Keep review separate from any
cleanup action; finding a duplicate must never delete it automatically.

The development build previews local bulk renames with search/replace,
numbering, prefix/suffix, collision checks and guarded execution. Configurable
actions should operate on frozen selections and pass filenames as data, not
shell code.

### Follow deep folder trees without losing context

Miller columns are a file view, independent of the workspace. Einn uses them
by default, and each workspace has a configurable default view. Explore adaptive views only
with explicit user control and sensible minimum sizes.

Readable, BeOS-inspired folder cards and saved paired work sets remain goals.
Named project sessions—such as Accounting, Video editing or Personal—could
restore open locations, future tabs/cards, view modes, sort choices and layout.
They must distinguish saved UI state from active operations: restoring a session
must not replay old file jobs or store credentials in plain layout files.

### Inspect files without leaving the task

Expand the optional Smiðja inspector to suitable code, text, Markdown, HTML, PDF,
audio and video formats through appropriate components. Add useful high-resolution
image navigation and camera EXIF metadata, including GPS coordinates where
present. Decoding an image today does not mean those inspection tools already
exist. Location display must not contact map services or expose coordinates
without an explicit choice.

Preview must not execute scripts or modify files. Explicit editing, read-only
states and unsaved-change handling remain important. An optional terminal should
follow the active folder, clearly distinguishing an integrated view from a
separate application window.

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

The development build can save plain clipboard text or an image as a new named
file in a local folder. Images are saved as PNG. File references still use
ordinary Paste.

### Collect files from several folders on a Shelf

The unreleased development build has a separate Shelf window for local
references, with original-folder and availability columns, drop support,
Command+Shift+Y and a confirmed Move all action using the normal queue.
Create ZIP packs references from different folders through the same queue
without moving the originals. The collection is session-only. Sharing is
still planned. Verify Stack & Tile ergonomics during hands-on testing.

### Send a selection to a phone

Plan a **Send to mobile** context-menu action. On request, start a temporary
local web server for the selected files and show a QR code. A phone on the same
network scans the code to download them in its browser, without a cloud account.
Serve only the selection through an unguessable, short-lived link, not its
parent folders. Show when sharing is active, when it expires and how to stop it.

### Use Haiku's desktop tools

- Let tool windows such as the Shelf participate in native **Stack & Tile** or
  remain independent, rather than implementing Listi's own docking system.
- The development build offers **Convert here** for a single local image drop,
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
