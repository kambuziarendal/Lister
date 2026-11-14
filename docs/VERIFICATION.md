# Private 0.4.0 acceptance

[Installed results and recovery](ACCEPTANCE-0.4.0.md) identify the exact
private candidate. No public release is authorized.

# Internal 0.4.0 continuation

See [INTERNAL-CANDIDATE.md](INTERNAL-CANDIDATE.md) for the current source-built
candidate and [ROADMAP.md](ROADMAP.md) for the publication boundary. Generate the
additional native archive fixtures with:

```
python3 tests/extra-archive-fixtures.py /boot/home/listi-fixtures/archives
```

`tests/native-preview.cpp` links the candidate's ImagePreview, VirtualFile,
ArchiveFile and NetworkFile objects with `-lbe -ltranslation`. Pass a synthetic
320x240 PNG; it checks decode, scaling, reply ownership and cancellation. These
are native tests, not Linux stand-ins. Operation fault checks below retain their
separate deterministic adapter.

A 50-query, ten-second idle native window probe after UI navigation measured
0.155% of one CPU core, p95 579 microseconds and maximum 733 microseconds, no
failed replies and 11 threads in the two-vCPU QEMU guest. Allocated area size was
160,288,768 bytes, not resident memory or heap use. This is a short local sample,
not a hardware/Dolphin comparison, memory-leak proof or sustained benchmark.

# Internal safety/module continuation, 14 November 2026

The latest internal build and open gates are recorded in
[INTERNAL-CANDIDATE.md](INTERNAL-CANDIDATE.md). Historical installed-package
results below do not qualify the current uninstalled development executable.

For the deterministic network fault lane, compile without a live server:

```
c++ -std=c++17 -Isrc tests/virtual-operation-faults.cpp \
  src/VirtualOperation.cpp src/VirtualFile.cpp -o /tmp/listi-faults
/tmp/listi-faults
```

On Haiku, link those two `.o` files from the candidate's native build instead of
recompiling the implementations. Run `Listi --self-test` and `Listi --virtual-test`
with owned fixtures as documented in INTERNAL-CANDIDATE. Record executable hash,
compare source/build input hashes with the guest, and identify installed versus
development runs separately. Never count an earlier hash's results as a rerun.

# Version 0.3.0 Listi candidate verification

14 November 2026. Private x86_64 candidate, built and exercised in native Haiku
beta6 hrev59866+79 / GCC 13.3.0 in an owned restricted-network QEMU/KVM overlay.
No public release, paid CI, Depot change or physical-hardware qualification.

Passed on the installed candidate:
- Native build, 36 filesystem/Trash/filter checks, package install and executable
  identity: `6b3fbf13779cb13869045ec9f97e0f198b55239584924530ef4785e47d04a11f`.
- Retained original 0.2.0 -> Listi 0.3.0 upgrade and package rollback/re-upgrade.
  Preferences SHA256 stayed `5eb533065816341d1ef2e03d3a10701b6e5011ba44bca3c8f9c867d9b21c9aca`.
  One Listi app/Deskbar launcher, no stale Lister executable/launcher. Package
  name, signature, settings and scripting identity remain compatible; the package
  provides both app:listi and the previous app:lister capability.
- Reproduced 0.2.0's path-field Command+A/C invoking file selection/copy. Fixed:
  actual Command+A/C/X/V clipboard roundtrips in both paths, both filters,
  command, new-folder and rename fields. E/M/R/N/D and F5/F8 in main-window
  text fields do not start file operations. Enter returns path/filter focus to
  the list. Both source roles and Tab transitions verified.
- Command+V native viewer and Command+E StyledEdit launch; Command+F filtering,
  filtered Command+C copy with hidden entries excluded, right-source Command+M,
  Command+R rename, Command+N folder creation, Command+D native Trash.
- Actual dialogs and filesystem readback: confirmation cancel, conflict Skip,
  Replace, Pause/Resume and Cancel all. Main navigation stays responsive while
  paused. Accepted copy/rename/new-folder jobs retain their original paths when
  roles and folders change. Completed file contents compared.
- Trash and restore roundtrip; restore conflict keeps both the trashed original
  and newer destination intact. No overwrite or permanent-delete fallback.
- Normal 1140x650 and compact 800x600 frames read back; real light/dark palettes,
  readable controls and native file icons inspected. Compact dark name dialog
  opens and cancels. Existing preferences, including visible command field, kept.

A late Escape defect in the name dialog was corrected using B_NO_COMMAND_KEY.
The corrected installed binary passed explicit window-count cancellation checks,
new-dialog text checks, snapshot rename/mkdir, clipboard and operation regressions.
Initial shell assertions could hide intermediate failures; their affected results
were discarded and replaced by fail-fast assertions and visual checks. Failed
and corrected evidence remain in the private overnight task directory.

Recovery: reinstall the retained Lister 0.2.0 HPKG. Do not restore an old filesystem
snapshot or settings file; package rollback must not undo the user's file work.
The retained settings identity needs no migration in either direction. Verify the
version in the title and exactly one launcher after changing packages.

Not qualified: physical Haiku hardware, alternate modifier mappings, full disk,
power loss, every filesystem, very large directories or external concurrent-edit
races. Shortcuts use native B_COMMAND_KEY rather than a hardcoded physical Alt.
General undo, tabs, network/archive providers and Tracker replacement remain out
of scope. This is a usable private candidate, not a durability/backup guarantee.

Regression: operate only in unique owned folders, use fail-fast filesystem checks,
assert dialogs actually close, and compare installed/build hashes after any late
fix. Retain both the final HPKG checksum and predecessor separately from the source.

---

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
