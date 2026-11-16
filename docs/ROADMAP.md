# Roadmap

## The ambition

Listi aims to be a capable native working environment for files on Haiku: simple
enough for quick browsing, but deep enough for sustained, selection-oriented
file work. Directory Opus inspires the source/destination model and freedom of
action. Haiku's own controls, attributes, queries and application ecosystem shape
the implementation.

The long-term ambition is broad coverage of the practical tasks people use
established file managers for, with a coherent Listi workflow rather than copied
interfaces. Coverage, responsiveness and resource use must be demonstrated task
by task. This is a direction to work toward, not a claim that the alpha already
matches or outperforms Directory Opus, Tracker or Dolphin.

For what is available now, see the [README](../README.md) and
[workflow guide](GUIDE.md). This page is not a release schedule.

## First: make existing file work easier to trust

The next stages concentrate on foundations rather than feature count:

1. **File safety and partial moves.** Extend targeted failure tests for full
   disks, permissions, interrupted writes and real cross-volume moves. Report
   moved, copied-with-source-retained, skipped, failed and unprocessed outcomes
   clearly enough to find what remains.
2. **Job identity and results.** Assign identities when jobs are accepted, retain
   structured results independently of UI updates, and add per-job cancellation
   alongside Cancel all. Cancellation must not imply undoing completed work.
3. **Shared command handling.** Gradually route menus, buttons and shortcuts
   through common validation, confirmation and queueing logic. Reuse the current
   file-safety engine rather than creating parallel implementations.

A local automation interface would need a separate access and trust model after
these foundations. Limited undo and a durable journal also need a defined scope:
a job log is not a backup, and recovery must not overwrite newer data.

## Then: more depth without more friction

### A richer Smiðja workbench

Expand preview beyond images to suitable text, source, Markdown, HTML, PDF, audio
and video formats through appropriate components. Actual format support must be
qualified individually. Preview must not execute scripts or modify files.

Add explicit text editing, clear read-only states and unsaved-change handling.
Offer configurable modules, saved layouts and an optional terminal following the
active folder. Distinguish an integrated view from a separate terminal window.

### Navigation that remembers the task

Explore readable, BeOS-inspired folder cards, saved paired work sets, remembered
workspace sizes and clearer per-folder view controls. Preserve active folders,
selections and running jobs through layout changes. Avoid surprising automatic
switches or making extensive configuration a prerequisite.

### Strong selection-oriented tools

Add bulk rename with a preview of old and proposed names, collision detection,
source-change checks and clear partial outcomes. Build configurable actions on
frozen selections; pass filenames as data, not shell code. Extend selection
summaries and reusable actions without turning context menus into long catalogues.

### Search and file intelligence

Build on existing saved searches, date/size/extension filters and live BFS
queries. Candidate improvements include exclusions, attribute tools, content
search and broader volume scopes. Search results must retain a clear connection
to originals. Existing read-only size/SHA-256 comparison is a foundation, not a
finished synchronisation engine.

### Broader archives and network workflows

Make archives feel like folders wherever each format permits, while showing
capability and metadata limits explicitly. Selected encrypted and multipart
reading, local editing, and archive/local moves already exist; remaining work
includes broader variants, metadata fidelity and supported write engines.

Continue testing interrupted transfers and recovery across protocols, server
implementations and larger datasets. Availability of SMB, SFTP, FTP or FTPS does
not establish qualification for every server or WAN condition.

## Longer-term possibilities

- **Conflict-aware recovery and synchronisation:** build only after job results
  and conflict reporting are strong enough. No universal undo promise.
- **Automation and customisation:** consider a local CLI or socket with explicit
  access boundaries. Scripting, hooks and plugins are not implied by an internal
  command registry.
- **Portability:** extract tested core logic incrementally, keeping Haiku UI,
  BFS, clipboard, Trash and application integration behind adapters. Other
  platforms need their own filesystem, package and UI verification. Native UI
  remains the preference; a port is not a reason to replace the Haiku experience.
- **Desktop integration:** coexist with Tracker by default. A replacement role
  would require a separate compatibility and recovery study.
- **Optional peer sharing:** a future adapter could connect to a separately
  developed sharing system; it is not a current dependency or shipped feature.

## How progress should be judged

Deliver small, complete workflows, measure them on documented environments, and
publish known limits alongside capabilities. A feature name, successful build or
large test count is not enough to establish a dependable user experience.

The aim is useful depth with a native feel, not a race to the longest checklist.
There are no fixed delivery dates, promised performance ratios or claims of
universal crash consistency.
