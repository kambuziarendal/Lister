# Private candidate 0.6.0: saved places and targeted bughunt

## Behaviour

Each panel has Go > Places. Save the successfully loaded location with a name;
open it in either panel, rename it, or forget it after confirmation. Up to 64
shared names persist across restarts. Names are trimmed, unique, 1 to 80 UTF-8
bytes, with no control characters. Addresses must also be unique. Names may
contain slashes; these labels are not file names. A pending question retains its
original address even when navigation changes. Failed opens cannot be newly
saved, but existing offline entries can still be renamed or forgotten.

Local folders, @disks, archive-member folders and supported remote addresses use
the same navigation/history machinery. No username, password, domain, key path
or known-hosts configuration is copied from a connection into a saved place.
Credential-bearing authority URLs are rejected. After restart, use Connect to
server to authenticate before opening a saved authenticated remote folder.
This is an address list, not a sync service, mounted-volume manager or secret store.

The separate `~/config/settings/Lister_places` BMessage file has format version 1.
A worker writes a unique temporary sibling, syncs it, and renames it atomically.
Only a successful acknowledgement commits the shared UI list. A failed write
keeps the previous in-memory list; a subsequent write can recover. A corrupt or
unknown-version file is preserved and mutations are disabled for that session.
For recovery, quit Listi, retain a copy of the damaged file, then restore a known
good copy or move the damaged file aside and restart with an empty list.

The existing `Lister_settings` format is unchanged. Older 0.5.0 does not touch
the separate places store; rollback must keep both files. Changes to folders or
files are not rolled back by changing the app package.

## Bughunt fixes

1. Command input previously executed when focus left a modified field. It now
   requires Enter, using the explicit-submit filter already used by name and
   connection forms. Reproduced with a harmless marker-writing command in the VM.
2. Saving an older Preferences window could replace newer pane modes, trees,
   preview state and folder overrides. Only fields owned by that dialog apply.
3. NaN font sizes bypassed range comparisons. Both dialog and settings loading
   now require finite values in the supported 8 to 40 range. Invalid persisted
   values use 12; invalid dialog input is rejected without saving.
4. The embedded native application version still identified 0.3.0 although the
   installed package/title were 0.5.0. Native version metadata now matches 0.6.0.

5. A long network error in the panel status expanded the minimum window width
   beyond the screen. Status text now truncates to the available width and the
   complete message remains available in its tooltip.

6. Existing text runs in inline questions retained dark-theme colours after
   a live switch to light mode. A small shared native text view now updates
   glyph colours on theme changes, including operation history and the viewer.

Tracker is an additional requirements baseline, not copied implementation.
See TRACKER-COVERAGE.md. Full Tracker parity, full Dolphin parity, arbitrary
third-party add-ons, physical hardware and every possible filesystem/backend
are not claimed by this candidate.

## Verification

Final installed-artifact receipt is recorded after native acceptance. Targeted
checks cover persistence, both panels, frozen addresses, labels, failed writes,
corrupt stores, offline servers, archive places, explicit command submission,
preferences state, native version metadata and package rollback. The broad
native local/virtual suites and transactional fault injection remain required.

Native pre-package checks: 29 saved-place, 12 preferences-state, 23 navigation
history, 9 deterministic transfer-fault, 6 preview ownership/decoder and 7 BFS
attribute checks passed. These are native checks, not 86 GUI actions. Typed file
and folder attributes and a 150,000-byte multi-chunk attribute were copied.
The empty-attribute test idea did not produce a valid native fixture with the
examined APIs; no empty-attribute preservation claim or speculative fix was made.

Actual UI checks exercised add/rename/forget, frozen addresses, duplicate labels,
both panels, restart, SMB/SFTP/archive locations, failed writes, corrupt-store
preservation, explicit command Enter, NaN rejection and stale preferences.
The offline test found and reproduced the long-error width bug before its fix;
the corrected SSH error stayed within 1024 by 650. The SMB fixture permits guest
listing, so it was not used as proof of missing credentials after restart. The
pinned SFTP connection configuration was absent after restart as expected.

Live light/dark changes were visually checked with an open name question,
completed operation history and a text viewer. Readable glyph colours now follow
the theme without closing/reopening the view. A discarded early theme run had
both old and new apps open; it is not acceptance evidence. The corrected run
asserted no prior Listi process and verified the sole candidate's title first.

Rollback testing must compare preference values after explicitly saving in an
older app, not only flattened-file bytes: BMessage routing header fields can
change during a Save while every persisted setting remains identical. The
separate places file must still remain byte-identical when the old app saves.

## Final installed receipt, 14 November 2026

Code/package source: `b5bb09f12b4da761e8162f5bc5e50dd329a83505`.
This receipt is a later documentation-only addition, not a repack.
All 44 native source/resource/build inputs matched the guest. All 73 tracked
package inputs matched the isolated package stage.

- Installed executable SHA-256:
  `1ef66f16a25b30273fec85cf2a4b39879e6b8d36b7930a917f6446cf661e9ee3`
- HPKG SHA-256:
  `331cc0d2298346e4ed4f39faf8d9aa1756aa37554b1324326721aa30e2bcae5a`
- Private ZIP SHA-256:
  `b50a8905f799762fcb5f8267dd2500c5952075d9cb7731c48be61c07cf05cf96`

The final installed binary passed 55 local and 96 archive/network checks.
Together with the 86 native module/metadata checks above, the qualifying suites
contain 237 checks with zero failures. The module implementations remained
unchanged by the later text-colour fix. These counts do not include exploratory
failed runs, repeated pre-final runs or GUI checks.

17 final installed UI assertions plus screenshot inspection cover both pane
menus, persisted-name changes, frozen copy conflicts, explicit replacement into
the original destination, source retention, command blur, offline errors and
compact light/dark layout. The pending question's text is readable in both themes.

Actual final-package 0.6.0 -> 0.5.0 -> 0.6.0 roundtrip passed. The prior app was
launched and its Preferences Save used. Places remained byte-identical, all
persisted preference values matched, and owned file data was unchanged. The
re-upgraded executable matched the final hash and opened the saved location with
its retained name. The old 0.5.0 executable hash was verified as
`6c0438775dc9733bd746d62d80483a570d0064462f45f7050014422f0cf89af4`.

Evidence: `/home/lobster/kambuzi/reports/listi-places-20261114/`, especially
`final-installed-native.log`, `native-modules.log`, `attributes-final.log`,
`final-installed-ui.log`, `final-installed-complete-ui.log`, `final-roundtrip.log`
and `final-ui/`. Haiku beta6 hrev59866+79 x86_64 VM, not physical hardware.
The app and owned fixture servers were stopped after acceptance; inherited VM
retained. No public release, push or Depot change.
