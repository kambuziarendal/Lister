# Private Listi 0.10.0 qualification

2026-11-14. No public release, push or Depot publication. License unchanged.
Behavior and deliberately incomplete coverage: [PROPERTIES-0.10.0.md](PROPERTIES-0.10.0.md).

## Identity

- Package source: `18a9cec63e7b2d86ea24457a408e925f7acca005`, `work/smb-first`.
- All 101 tracked source/package inputs matched the native incremental source
  and the fresh archive-based package stage. Documentation qualification is
  committed separately after the immutable candidate.
- Haiku beta6 hrev59866+79, x86_64, GCC 13.3, inherited isolated BFS VM.
- Executable SHA256, both build and installed:
  `5f68e77122cbd8a502735772390aca5b8fc394c8b44937924c0bcd969f96fcf3`.
- HPKG SHA256:
  `22e690945a045359a11fd4fc63af208dd70c285d58eef8cfa847f7e495e98a87`.
- Private ZIP SHA256:
  `e151c1a1d41dc1cf22c94ca51196093753242a8aa67a4e28aff4c61862c1ee29`.
- Source archive SHA256:
  `5447dd0b3e9507fe8182a55262ad5671dd02d9c46324fe184373e1556ff1e832`.

## Verification

184 native assertions passed: 54 properties, 14 metadata fault-injection,
55 installed executable self-tests, 34 clipboard, 7 attribute-copy and 20 live
query regressions. Helpers were linked against the matching native source objects.
Fault injection covers partial writes, node-sync failure and recovery-record
flush failure; original file contents survive and recovery bytes are verified.

28 final installed UI assertions passed, including exact executable/version
preflight, keyboard/mouse entry, binary inspection, Unicode editing, no implicit
focus-loss save, stale values, explicit removal, invalid numeric input, separate
permissions, pinned identity across panel navigation and replaced paths. The
700 x 600 window was visually inspected in actual light and dark system palettes.
File copying retains edited typed metadata.

Bughunt reproduced a child draft veto that left the main window already closed.
Both main-window Quit and application-level Quit now preflight property windows
and retain the main workspace and draft. Review also corrected initial worker
session ownership, false dirty state/type display for unknown types, malformed
UTF-8 handling and recovery-record error reporting. No production fault hooks.

The installed conflict probe confirms that editing metadata during a waiting
copy invalidates its captured source state. The operation reports a changed
source and preserves the original destination. A fresh explicit copy succeeds
with the updated metadata. The first script incorrectly expected the stale copy
to proceed; a later history assertion read only the first line of native output.
Both rejected script attempts are retained separately. Corrected assertions and
continuation verified original/new panel entries, error reporting and safe retry.
No code change was needed for this existing source guard.

18 actual package-roundtrip assertions passed: 0.10.0 -> 0.9.0 -> 0.10.0,
exact old/new executable hashes, actual launches, old-version preference Save,
semantic settings preservation, byte-exact saved places/searches, original file
contents and edited attributes/permissions. Properties, saved places and a saved
live query reopen, and fresh live updates arrive after re-upgrade.

## Recovery and remaining scope

Quit the app before installing the retained private 0.9.0 HPKG. No settings or
metadata migration is required. Reinstall this exact 0.10.0 package to return.
Do not restore old settings files over newer user changes.

This is an isolated VM candidate, not physical-device qualification or a claim
of full Tracker/Dolphin parity. Network/archive metadata editing, ownership
changes, special permission bits, other attribute types, general undo and
automatic metadata recovery remain outside this increment. Dependency licensing
must be audited before deciding on a different future distribution license.

Local evidence: `/home/lobster/kambuzi/reports/listi-properties-20261114/`.
The app is stopped after qualification. The inherited VM remains available;
no new fixture server or unattended development job is left running.
