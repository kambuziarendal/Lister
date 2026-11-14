# Private Listi 0.8.0 acceptance, 2026-11-14

- Source/package commit: `6b5725fd1de42bc336a927a1856c91ab1fae0212`.
- Native Haiku beta6 hrev59866+79 x86_64, GCC 13.3, inherited isolated QEMU VM.
- 88 tracked inputs matched both native build tree and fresh package stage.
- Installed executable SHA256: `ae662aca4fd6414073c151af3e3a9704bf02e3f99b68bcb4df694632134622e6`.
- HPKG SHA256: `4d5a1a042f2a7f7db8c1c995c406ced072773d0cce749db7e8798ade03524278`.
- Private ZIP SHA256: `099e0c604e02f83725e960a68ef584d796785f8219a4eeb496177c485967f8cf`.
- Acceptance and coverage documentation commits after packaging do not change
  the executable, HPKG, or preserved source archive.

## Completed verification

264 native checks: 20 live worker, 31 saved criteria, 33 snapshot search, 29 saved
places, and 55 local plus 96 archive/network checks on the actual installed app.
The first 113 checks exercise the candidate modules directly. They are not 113
additional installed-app UI checks. Source inputs were matched before packaging.

38 installed UI assertions plus 16 actual rollback/re-upgrade assertions passed.
The installed app demonstrates native live updates, ancestor moves, selection
retention for unchanged originals and clearing for replacement identities, Stop,
fresh saved results, close/reopen, keyboard saved-menu/Save access, both owning
panels, Unicode snapshot regression, 700x500 search and 1024x650 main frames.
Screenshots reviewed in light/dark themes. A pending copy conflict survived live
updates and navigation; completion used the original source and destination.

18 additional prepackage storage UI assertions cover explicit Save/update/forget,
blur without submission, failed atomic rename preserving prior committed bytes,
and a corrupt store preserved with Save disabled. The final candidate's storage
logic is unchanged from that UI run; version strings were updated afterwards.

Actual 0.8.0 -> exact private 0.7.0 -> exact 0.8.0 install sequence passed.
0.7.0 was launched and actually saved preferences. Preference values remained
semantically equal; saved places, saved searches and checked original file
contents remained byte-identical. After re-upgrade, saved places navigated,
saved criteria reopened, and new files appeared in live results.

Earlier harness failures are retained: the replaced row was appended, not at its
old row index; native menu Home did not implement the assumed selection semantics.
Scripts were corrected after screenshot inspection. The final installed suites
passed cleanly, including native arrow-menu operation. A standalone saved-places
test link initially omitted the archive/network objects; corrected linking passed
all 29 checks. No product tests were disabled or failures relabelled as passes.

## Boundaries and recovery

[Search behavior](SEARCH-0.8.0.md) documents limits and eventual reconciliation.
Local BFS only for live results. No content-search engine, friendly attribute/date
builder, multi-volume selector or Tracker query-file/template interoperability.
No full Dolphin/Tracker runtime qualification or large-volume performance claim.

Settings: Lister_settings unchanged format; Lister_places unchanged format;
new format-1 Lister_searches contains criteria only. Retain all three files for
recovery; do not overwrite newer user data with a test backup. The prior 0.7.0
package can be reinstalled; it leaves Lister_searches alone. A corrupt search file
is preserved and edits disabled, not silently replaced with an empty store.

Detailed logs, scripts, source archive, input manifest and screenshots:
`/home/lobster/kambuzi/reports/listi-live-search-20261114/`.
Final app stopped. Owned fixture PID 383948 stopped and ports 1445/2121/2222
closed. Inherited VM retained. No public publication, push or Depot mutation.
