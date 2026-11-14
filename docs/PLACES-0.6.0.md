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
