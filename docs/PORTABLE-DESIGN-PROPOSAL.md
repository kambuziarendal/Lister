# Portable architecture proposal (awaiting design approval)

Priority update, 14 November 2026: the owner wants a distinctive daily file
manager first, in small usable increments aligned with the future vision.
ROADMAP.md and DESIGN.md now govern sequencing. Broad portable extraction is
deferred; the grammar/config/platform contracts below remain proposals, not
prerequisites for improving the existing native workflow.

## Existing implementation

Lister 0.2.0 is a new MIT C++ BeAPI application, not a GenesisCommander fork or a
DOpus source port. Existing DOpus-inspired behaviour: two listers, explicit
source/destination roles, Tab switching, selection commands, a fixed button bank,
background operations with pause/cancel and conflicts, editable paths, sortable
columns, a simple viewer and display preferences. It is not yet Magellan-compatible.

Today sorting/selection live in the column view, preferences are a flattened
BMessage, and the command line runs a shell through Terminal. The current hey
suite exposes SourcePath, DestinationPath, PathReady and Busy, not a command
registry. These are migration inputs, not claims that the new architecture exists.

## Proposed layout

```
core/
  types.h                  project integer/size types, compile-time width checks
  platform.h               narrow host services and asynchronous result contracts
  command.h / command.c    registry, parser and bounded text results
  lister.h / lister.c       owned entries, sorting, selection, filters
  assign.h / assign.c      named virtual paths, cycle/depth checks
  filetype.h / filetype.c   portable matching and action rules
  config.h / config.c      versioned text configuration
  job.h / job.c            abstract jobs, states, progress, cancellation intent
frontend-haiku/
  App.cpp, Pane.cpp, Preferences.cpp
  HaikuPlatform.cpp        filesystem, MIME, monitoring, query adapters
  OperationQueue.cpp       native execution and UI progress
  Trash.cpp                native per-volume Trash and restoration
  Scripting.cpp            BMessage-to-command adapter
  Makefile, Lister.rdef
tests/core/               headless tests and POSIX temporary-directory fixture
tests/haiku/              native integration, hey and GUI acceptance
```

Move existing files in a separate mechanical commit after approval. Keep one
root Makefile: `make` native app, `make test-core` portable tests, `make test`
native + core, `make package` HPKG. No new framework/dependency. Current app build
stays in the Haiku VM; portable C tests are permitted on POSIX as requested.

## Proposed command grammar (approval required)

One command per line: `verb key=value ...`. ASCII lower-case verbs/keys;
UTF-8 values are opaque bytes to the core. Unquoted values contain no whitespace;
double quotes allow spaces. Within quotes only `\\`, `\"`, `\n`, `\t` escapes
are accepted. No shell expansion, variable evaluation, pipes or implicit globs.
Reject duplicate/unknown keys, malformed escapes, NUL, excessive input and unknown
verbs with a text error. Use a 64 KiB command limit initially, documented as a
protocol limit. Line-ending LF terminates a command, CRLF is accepted.

Examples:

```
copy from=source to=destination
trash lister=source
restore lister=source
navigate lister=1 path="HOME:Documents/My files"
select lister=1 pattern="*.txt"
```

Resolve source/destination and snapshot the selection when accepting a command,
not later when a queued job starts. Replies are `ok job=42` or
`error code=conflict message="..."`; asynchronous job results are queried by ID.
Buttons, keys, menus and BMessage commands use the same dispatcher. Shell command
execution remains an explicitly named external action, never a parser fallback.
Do not call a plain-text command API an ARexx implementation.

## Proposed text configuration (approval required)

UTF-8, `version=1` header, section names and key=value values using the command
quoting rules. Separate button banks, key bindings, filetypes, assigns and display
preferences. Unknown-version files are rejected without overwriting them; parse
errors leave the active config intact. Atomic replace through the backend and a
retained previous file. On first migration, read existing BMessage preferences
once, preserve that original and write new config only after validation. UI theme
and font keys are platform-qualified, not hardcoded into the portable model.

## Proposed platform boundary (approval required)

Core uses conservative C89 syntax plus // comments, project typedefs in types.h,
no VLAs/designated initializers/stdint/stdio/locale dependency. Width assumptions
must be checked by compilation on each backend, including Plan 9's 64-bit type.
Sorting is deterministic byte ordering initially; native collation must be an
explicit optional adapter, not a hidden locale dependency.

A caller-owned core context takes a small callback table for allocation/free,
job submission/cancellation and configuration persistence. Filesystem entries
arrive as backend-owned batches copied into the model. Ownership, lengths and
error codes are explicit. Core does not spawn threads, hold native refs, invoke
widgets, or wait for filesystem I/O. Haiku adapters own BEntry/node refs, MIME,
watch_node and BQuery. UI-thread adapter feeds model events; callbacks must not
re-enter the model. Keep the current cancellable worker until replaced by a
verified adapter, not by an all-at-once rewrite.

Abstract jobs describe source/destination provider IDs and paths, conflict policy
and requested operation. Haiku Trash is an optional backend capability; no BFS
attributes appear in the core ABI. Unsupported capability is a visible error,
never a destructive fallback. Plan 9 can supply different capabilities later.

## Mapping and later targets

- Amiga assigns -> core named-path resolver; Plan 9 bind/union namespaces later.
- ARexx-style automation -> Haiku BMessage registry adapter and hey tests; Plan 9
  `/mnt/opus/lister/1/ctl`, `selection`, `buttons` and rc scripts later.
- Filetypes -> portable rule ordering + Haiku BMimeType/sniffers; plumber later.
- Icons/comments -> Haiku BFS attributes in backend; not universal core fields.
- Haiku GUI -> native Layout Kit. Plan 9 GUI -> independent libdraw/rio client.

Plan 9 work is not being implemented now. A portable model/command protocol helps,
but is not proof that 9front toolchain or runtime compatibility has been tested.

## Incremental migration and gates

0. Approve grammar, config and platform contract; add short ADR records.
1. Extract model/sort/selection with POSIX and Haiku headless tests.
2. Adapt one existing lister, preserving keyboard and node-monitor behaviour.
3. Adapt both roles and existing queue/Trash operations without loss of safety.
4. Route command registry, configurable bank, keys and saved config.
5. Adapt filetype actions and viewer.
6. Expose registry through a tested BMessage scripting suite.
7. Providers/virtual views, find, sync/diff after local correctness.
8. Optional desktop shell, only after a separate research and approval report.

Desktop-shell report must cover Tracker signature/launch and restart behaviour,
BFilePanel and libtracker coupling, desktop/backdrop and volume lifecycle,
refs/open-with/MIME, Trash compatibility, Deskbar integration, compatibility
messages and recovery to stock Tracker. Do not change Tracker's signature or
autostart yet. Coexistence is the default.

## ADR log

- Accepted 2026-11-14: independent implementation of DOpus ideas. Preserve MIT;
  no source/assets copied from DOpus. Recheck exact upstream licence before reuse.
- Accepted 2026-11-14: native Trash first, isolated Haiku backend, no silent delete.
- Requested 2026-11-14: future portable C core and native C++ Haiku frontend,
  Plan 9 namespace/server design kept possible, no Plan 9 implementation now.
- Proposed: grammar, text config and callback boundary above. Await owner approval
  before treating these as a stable interface or rewriting the current app.

## Critical assessment and recommended scope

The product goals are sound; not every mechanism in the prompt is a prerequisite.
A big-bang C89 rewrite would discard a verified native app and delay everyday
file safety. Start with the command parser and lister data model, prove them in
small tests, and move only logic that genuinely benefits from portability.
Conservative C is reasonable for a future native Plan 9 compiler, but the exact
compiler, integer ABI and POSIX assumptions must be tested in a small later
portability spike. C89 by itself does not prove Plan 9 compatibility. No new C
implementation or stable ABI has been chosen in this release.

Keep UI-only state and native preferences out of the portable core. Do not create
an elaborate provider/plugin framework, generic threading layer or universal
filesystem interface before local commands require it. Treat the eventual 9P
server as a transport adapter with its own security, ownership and connection
semantics, not as a free consequence of a text parser. UTF-8 sorting and filenames
also require a deliberate user-facing policy beyond the initial byte-order test.

The text config proposal needs comment preservation/round-trip tests before it
becomes user-editable. Command argument escaping is a protocol contract and needs
malformed-input and length-limit tests before any external automation is exposed.

Recommended next deliverable after this safety round: approve the small protocol
proposal, then add portable parser/model tests without replacing both panes or
changing existing settings in the same commit. The roadmap is an extraction path,
not an instruction to remove already-working dual-pane features and start over.
