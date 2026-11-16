# Design and boundaries

**Tilleggskrav 14. november 2026:** Alle Trackers brukeroppgaver skal også
dekkes i Listi, på vår egen måte. Se [Tracker-kartet](TRACKER-COVERAGE.md)
for dokumentert tilbud, udekkede behov og akseptkrav. Dette erstatter ikke
Dolphin-kravet og er ikke en påstand om ferdig funksjonslikhet.

## Names and compatibility

**Listi** is the project, application, executable and launcher name. The repository
and package ID are `listi`; our C++ namespace is `Listi`. Workspace names are:

| Display name | ASCII ID / enum value | Existing numeric value | Layout |
| --- | --- | --- | --- |
| Einn | `einn` | 1 | Simple, Tracker-like single panel; same-window navigation |
| Askr | `askr` | 2 | Folder tree on the left, file list on the right |
| Tvískipt | `tviskipt` | 0 | Two side-by-side file panels, inspired by Directory Opus |
| Smiðja | `smidja` | 3 | Workbench with more tools and options |

Menus use the display name plus a short description. Identifiers and filenames
use ASCII, never transliterations of UI strings inferred at runtime. The existing
native `Workspace` scripting property returns the new display name.

This is a naming-only change, not new layout behavior or a portable-core rewrite.
The enum values deliberately retain their on-disk numbers. New saves include
`workspaceId` plus the numeric `workspace` alias, and `smidjaLeftTree`,
`smidjaRightTree`, `smidjaPreview` plus their `workbench*` aliases. Existing numeric
and legacy fields take precedence if both exist, so edits made after downgrading
to an older app are not masked by stale new fields. Canonical-only settings also
load. Unknown workspace IDs/numbers safely fall back to Tvískipt.

No manual migration or new settings directory is introduced. Compatibility-only
names intentionally remain: `application/x-vnd.Kambuzi-Lister`,
`suite/vnd.Kambuzi-Lister`, `Lister_settings`, `Lister_places`, `Lister_searches`,
`Lister_archive_settings`, `Lister_attribute_recovery`, and package provides/
replaces/conflicts for `lister` and `app:lister`. They are stable upgrade/storage
contracts, not visible branding. Historical test reports and third-party names
retain their original spellings. Older screenshots are explicitly versioned.

## Functional ambition, clarified by the owner 14 November 2026

Dolphin is a coverage reference, not a product to copy in any respect. Listi must
cover the user needs/functions it offers, including applicable protocols and
extensions, through our own code, design and workflows. Do not reproduce its UI,
layout, assets or implementation. Build on Listi's identity and Haiku's native
capabilities. Native integrations count only when they deliver a coherent,
verified user workflow; an adjacent feature is not automatic equivalence.

The versioned inventory in [FUNCTION-COVERAGE.md](FUNCTION-COVERAGE.md) identifies
covered, partial, unqualified and missing needs. Deferred needs remain gaps,
not exclusions from the long-term ambition. This does not authorize public
publication, paid services or one large rewrite.

## Primary motivation, clarified 15 November 2026

We are building Listi primarily because we miss DOpus on Haiku. Its powerful
source/destination file work, selection-oriented actions and freedom of use are
the primary product direction, not incidental inspiration. Improve that model
with native Haiku capabilities and coherent modern workflows. ROX, Dolphin and
Tracker supply useful individual ideas or coverage references; none replaces
this motivation, and none is a UI to copy. Aim to surpass DOpus's limitations
without trading away its effective file-work character. Claims of superiority
require measured results and verified workflows, not feature counts.

## Product direction, 14 November 2026

Listi is a native Haiku working environment for files. DOpus inspires freedom
of action; contemporary workflows shape how it feels. Power must be discoverable,
the target of each action visible, and safety supported by evidence.

The first milestone is an excellent, distinctive file manager, not an architecture
rewrite or desktop shell. Ship small complete workflows that already embody the
future direction. Current priority: explicit source/destination, fast local view
filtering, keyboard/mouse parity and understandable operation progress.

- Judge features by today's work and native fit, not historical completeness.
- Menus, buttons and future scripts should share user actions; internal rendering
  and mouse events do not need a text protocol.
- Keyboard-first does not mean function-key-first. Prefer nearby letters with
  Haiku's native Command modifier (normally Alt, respecting the user's keymap):
  V view, E edit, C copy, M move, R rename, N new folder, F filter; Tab switches
  listers. Most already exist in the menu. Promote them in discoverability and
  future configurable bindings; function keys are unassigned (owner decision, 2026-11-14).
  Bare letters must not steal type-ahead. Text fields must retain normal text
  selection/copy/paste shortcuts rather than triggering filesystem actions.
  The generated mockup's F-key labels are superseded by this owner feedback.
- Resolve source/destination and freeze selected paths when accepting a job.
  Focus/layout/query changes must not retarget it. Paths do not freeze content;
  native identity/conflict checks remain necessary.
- Live queries later represent originals: show origin and scope, and distinguish
  removing a result from deleting its file. Start with local name filtering now.
- Keep safe Trash and explicit permanent deletion. Do not promise universal undo;
  a journal is not a backup, and recovery must not overwrite newer work.
- Grow from two reliable panels to multiple listers and saved layouts. No embedded
  window-manager project is required for the first milestone.
- Use Haiku strengths fully. Plan 9 is a later implementation, not a requirement
  for identical features or a reason to weaken native UX.
- Native icon identity: use Haiku/Tracker/MIME and per-file/application icons,
  respecting the user's selected system appearance wherever the public APIs
  expose it. Do not ship a competing Listi icon theme or copy generated mockup
  icons. Existing file rows use `BNodeInfo::GetTrackerIcon`; future Places and
  action controls should reuse suitable native resources. Do not assume a
  universal icon-theme API exists; verify each integration on supported Haiku.

### Adaptable without configuration overload

People work differently. Good defaults must make Listi useful before anyone opens
preferences. Offer depth progressively, not as a mandatory setup exercise.

- Put view mode, icon/thumbnail size, preview and folder-tree controls beside the
  pane they affect. Left and right panes are independently configurable.
- Offer explicit "Remember for this folder" and "Use defaults again" at that
  same point. Distinguish a temporary view choice from a saved folder override.
- Use a predictable precedence: application defaults, saved workspace when
  explicitly loaded, folder-specific override, then current session choice.
  This describes intended behavior, not an implemented workspace/config schema.
- Keep shared preferences small and grouped by user task. Avoid duplicating
  controls with different meanings in several menus. Advanced options are
  reachable without dominating normal use. Changes should be visible immediately
  where practical, reversible, and never silently reset unrelated customizations.
- Queue named workspaces such as Laptop and Large screen for later. Do not infer
  an automatic layout switch from plugging in a monitor. Preserve deliberate
  choices, and offer a simple reset of the current view without wiping everything.
- Evaluate with two acceptance flows: a new user can navigate/copy without setup;
  an experienced user can customize one pane and remember a photo-folder view
  without searching a global settings catalog. Both mouse and keyboard work.

This is a design constraint for each increment, not a request for a generic
settings framework or a large configuration subsystem now.

### First workflow increment: narrow, select, act

Each pane has a literal name filter, a visible Clear button and a filtered count.
The platform's normal Command+F shortcut focuses the source pane's filter
(Alt+F on default Haiku). Enter returns to the list; Escape in the filter clears
and returns. No recursive search, shell expansion or filesystem read per keystroke.

Rows are filtered in memory while the native scanner continues asynchronously.
Hidden rows are deselected, cannot be dragged or included by Select All, and do not
silently regain selection when shown again. The parent row remains navigable.
New navigation clears the filter; refresh in the same directory preserves it.
Node-monitor refresh applies the active filter to new results. No settings-format
change or external scripting contract is introduced.

### Short decision log

- Accepted: first deliver a good daily file manager in small increments that
  express the longer-term identity. Large portable-core migration is deferred.
- Implemented candidate: local native name filtering, distinct from later BFS
  queries/content search. This is a view operation, not a new filesystem provider.
- Preserved: existing worker queue, Trash, settings and public scripting suite.
- Deferred: grammar/config/platform proposals remain provisional. Obtain approval
  for concrete hard-to-reverse contracts before migration or external exposure.
- Deferred: Tracker replacement requires a separate compatibility/recovery report.

## Existing native foundation

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
operations. No remote filesystems, archive provider, general undo, saved tabs,
configurable button commands or content search yet. Built-in viewer reads text
up to 1 MiB. File browser metadata/icon reads are off the window thread.

## Native Trash backend (0.2.0)

`src/Trash.cpp` is deliberately Haiku-specific and independent of widgets. It
uses public BEntry/BNode/BVolume/find_directory APIs and the Tracker on-disk
`_trk/original_path` B_STRING_TYPE attribute. This attribute is a Tracker
convention, not a new private-header dependency. Checked against Haiku beta6
Tracker Attributes.h and FSUtils.cpp. No Tracker source was copied.

Trash is on the source volume. Names are disambiguated without replacement.
Restore only accepts a top-level Trash entry, keeps conflicts intact and reports
missing parent folders. Unsupported volumes fail without permanent-delete
fallback. The existing operation worker owns all Trash I/O. UI navigation also
resolves the volume's Trash on the directory-reader looper.

The portable-core / Plan 9 direction is specified as a proposal in
[PORTABLE-DESIGN-PROPOSAL.md](PORTABLE-DESIGN-PROPOSAL.md). It has not been
implemented or silently substituted for the working native preview.
