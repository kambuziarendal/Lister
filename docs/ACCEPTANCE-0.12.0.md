# Private Listi 0.12.0 acceptance, 2026-11-14

Source/package: `1f0f95b` on `work/smb-first`. All 114 tracked package inputs
matched both the native source tree and fresh packaging stage. Haiku R1 beta6
x86_64 in the inherited Listi QEMU overlay. No public push, release or license change.

## Final artifact identity

- Executable: `9f38f68d04529a0f7966029566e469170c0e3a3b9b35fdeebf249499c5777d53`
- HPKG: `7bebbeb95f9df999fc15d0c5f05bd628685ada0ccb1982b5aa41932f2eb2d899`
- Private ZIP: `b09de27d8dd9c4742ee64b62f1efb6631860741a3d81439e0ffd8bade65d6fa9`
- Delivery is established separately by the channel receipt, not local readiness.

## Verification

123 native checks passed on the final candidate: 24 drop, 34 clipboard,
7 attribute-copy, 55 installed self-tests, and 3 theme-text regressions.
Tests live in tests/file-drop.cpp, file-clipboard.cpp, attribute-copy.cpp,
theme-text.cpp and the app's self-test. Build natively with make -C src -j2;
test compile/link commands and complete logs are retained in the evidence folder.

Installed UI exercised actual grid/Details drags, multi-selection, keyboard and
mouse Copy/Move/Cancel, Escape, actual Tracker incoming refs, frozen navigation,
competing drops, replaced source/destination, conflict Abort and successful retry.
Existing inline name submit/cancel remained functional. Compact actual 912x600
and normal 1024x650 frames were visually inspected in real light/dark palettes.
The window clamps a requested 760px width to 912px in this existing configuration.
No claim of 760px acceptance is made.

Bughunt found unreadable existing operation log runs after a palette switch.
ThemeTextView now recolors the full text range as well as the insertion style.
After the fix, the exact final installed package passed 14 focused UI assertions:
explicit drops, keyboard copy/move/cancel, metadata, palette transition with old
and newly appended history, conflict Abort, pending-question Quit and final hash.
Final dark history and conflict screenshots were visually inspected.

Actual 0.12.0 to retained 0.11.0 to final 0.12.0 package roundtrip passed.
The predecessor launched and saved preferences; semantic preferences, byte-identical
places/searches, copied/moved payloads, attributes and permissions survived.
Both installed executable hashes were checked. Final package launched and passed
the UI checks above after re-upgrade.

Exploratory harness failures are retained: the generic text parser read only the
first line of a native multiline string; missing-view checks initially treated
expected nonzero status as failure; expanding queue errors moved click coordinates;
and requested compact width was clamped. They were corrected/continued against
actual native state. The new theme test first required an explicit initializer_list
include. These are not hidden passing runs or unresolved product failures.

## Scope and recovery

See DROP-0.12.0.md. Receiving pane's current folder is the destination, not a
hovered row. Local transfers have identity guards; virtual providers retain their
existing copy-only backend behavior. Remote services, new physical hardware and
new cross-volume scenarios are not freshly qualified by this increment.
No row-target drop, link creation, auto-open on hover or modifier-selected action.
Generic external refs can be snapshotted only when received, not at remote drag start.

Quit Listi and install the retained private 0.11.0 HPKG to roll back. Its executable
is `30481ee2de96bd79da5a4a8a28defaa7e6af9d4e61c1a2c17e936334f86a30b9`.
Never restore stale files/settings to undo this executable-only upgrade.

Evidence: /home/lobster/kambuzi/reports/listi-drop-20261114/.
The app and task-owned Tracker window are closed; guest filesystem synced.
The inherited VM is retained, not a continuing background implementation job.
Next bounded workflow in the roadmap: links and duplication.
