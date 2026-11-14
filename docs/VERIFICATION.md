# Version 0.2.1 candidate verification

14 November 2026. Native Haiku beta6 x86_64 in an owned QEMU overlay based on the
retained 0.2.0 test VM. Source edited on the host, compiled/tested inside Haiku.
Candidate only: this record does not claim public publication or physical testing.

Passed:
- Native build/package and 36 self-test checks: previous 29 filesystem/Trash
  checks plus seven name-filter cases (empty, substring/ASCII case, literal
  wildcard/punctuation, no match, parent navigation and exact UTF-8 text).
- Installed 0.2.0 -> 0.2.1 upgrade. Built and installed executable SHA256 agree:
  `0fce97c842235373171ebcd1b760c9468866c28a013d733954c78d4466957853`.
  Preferences SHA256 unchanged across package installation.
- Actual native controls: Select All, Alt+F, type `.txt`, Enter, F5. Only the two
  visible selected text files were copied. Contents compared; hidden picture and
  directory were absent from destination and retained at source.
- Escape cleared the filter and returned focus to the list. The two previously
  hidden entries did not regain selection. A no-match filter showed zero entries
  while retaining `..`. Creating a matching file externally updated the filtered
  list through node monitoring; Refresh preserved the filter. Opening the parent
  cleared it. Clicking/typing in the other pane's filter selected that pane as
  source, confirmed visually and through SourcePath scripting.
- Native light/dark palettes at 1140x650 and 800x600. Field, clear control, counts
  and actions remain accessible. A narrow window uses the existing list horizontal
  scrollbar for columns. No new external scripting suite/settings migration.

Not qualified: physical hardware, very large-directory filter latency, broad
Unicode case equivalence, allocation failure, full disks/power loss. Existing
file-operation limits below remain. Filter state is transient, not saved across
restart. The generated design mockup is explicitly conceptual, not GUI evidence.

Manual regression for the next round:
1. In an owned folder create two `.txt` files, a `.jpg` and a directory.
2. Select All, filter `.txt`, Enter, copy to an empty owned destination. Confirm
   only the two matching files arrive and compare their contents.
3. Clear filter; verify hidden entries remain unselected. Try a zero-match filter,
   create a matching file externally, Refresh, then navigate via `..`.
4. Repeat from the right pane; inspect SOURCE/DESTINATION and confirmation target.
5. Check compact/normal windows and both real system palettes.

Recovery: reinstall retained 0.2.0 HPKG without restoring old files or settings.
The portable model experiment remains on a separate local draft branch and is
not part of this candidate. Tests above concern the native application.

---

# Version 0.2.0 verification

14 November 2026, native Haiku beta6 x86_64, GCC 13.3.0, installed haiku_devel.
All application source editing, build and tests took place in our own Haiku VM.

Passed:
- Clean build/link and 29 native filesystem tests (18 previous, 11 Trash checks).
- Native Trash tree/metadata preservation, same-name collision, restore conflict,
  round-trip, already-trashed rejection, restore outside Trash rejection, symlink
  preservation/restoration, cancellation, volume-root refusal, missing-parent
  retention. Tests clean only their own unique fixture entries.
- Installed 0.1.0 -> 0.2.0 upgrade, settings retained, installed/built binary hashes
  compared; final artifact hash is in the release SHA256SUMS.
- Actual F8 confirmation, move to Trash, Go > Trash navigation and Files > Restore
  from Trash, with on-disk content readback. Haiku Tracker's own File > Restore
  also successfully restores a directory trashed by Lister.
- Native UI copy/move/permanent-delete of a small owned tree. Copy conflict,
  Pause/Resume and responsive main-window scripting while paused.
- Double-click navigation from / to /boot, from /boot to home, and from mounted
  ext4 root into FolderTest. This ext4 volume rejects Trash with an error and the
  original tree is retained; no permanent-delete fallback.
- Dark and light system themes, normal 1140x650 and compact 800x600 window.
  Captures of the installed main and copy windows in dark theme.

Limits: no physical hardware, full-disk/power-loss/unplug durability tests or
qualification of every filesystem. Restore requires its original parent to exist
and does not overwrite conflicts. General undo, FTP/SMB and the proposed portable
core/Plan 9 architecture are not implemented. Tracker's restoration attribute is
an on-disk convention, verified against beta6; no private Tracker header was added.

Recovery: keep previous HPKGs. Reinstalling does not undo file operations.
Preferences remain ~/config/settings/Lister_settings. Never restore a filesystem
snapshot merely to roll back the application package. HPKG container byte-for-byte
reproducibility is not claimed (BFS access/creation timestamps); the source build
and installed executable identity are verified separately.

---

# Version 0.1.0 verification

14 November 2026. Native Haiku R1 beta6 hrev59866+79 x86_64 in our own QEMU/KVM
VM, GCC 13.3.0, haiku_devel installed. All coding and compilation ran in Haiku.

Passed:
- Clean native build and link, make test: 18 filesystem checks. Recursive copy is
  exercised on a worker thread, including symlinks, BFS attributes, skip/replace/
  abort, self/descendant rejection, cancellation without a partial output file,
  move/rename/mkdir/delete, merged move, changed-source retention and mount guard.
- Installed HPKG executable equals the built binary, SHA256:
  `44fc810eb3152ebbda9d436a48c2748c83456d777a8e1d5f4d0ceca0b529d2a7`.
- Actual installed UI: double-click /boot from /, home from /boot and FolderTest
  from a mounted ext4 fixture, with path readback. No root fallback.
- Actual UI copy of a small tree, overwrite prompt, paused copy retains the old
  destination and resumes successfully. Main-window scripting remains responsive
  while paused. File contents compared after completion.
- Actual UI move and permanent delete, with filesystem readback.
- Skip conflict and cancel a waiting queue checked during native development.
- Tab swaps source/destination; preference toggling, saving and restart retain
  the command line. Dark and light system themes, 1140x650 and 800x600 windows.
- Native screenshots of the installed main and operation windows in dark theme.

The first recursive worker copy exposed a stack overflow from large automatic
buffers; the buffers now live on the heap and the worker-thread regression passes.
A new conflicting copy remains resolvable after previous jobs finish.

Not verified: physical Haiku hardware, every filesystem, large-volume durability,
full disks, unplugging a disk mid-operation, power-loss recovery and every external
concurrent-modification race. The mounted ext4 fixture was used for navigation,
not a destructive-operation qualification. No FTP/SFTP or advanced features are
claimed. This is a first preview, not a general backup or recovery product.

Recovery: retain previous versioned HPKGs, reinstall the previous package.
Preferences live in ~/config/settings/Lister_settings. Package changes do not
reverse completed file operations. There is no undo/trash yet.

Packaging: repeated `make package` succeeds from the same source and executable.
Container checksums differ because Haiku records BFS access/creation timestamps;
bit-for-bit HPKG reproducibility is not claimed. The installed executable hash
is verified separately above.
