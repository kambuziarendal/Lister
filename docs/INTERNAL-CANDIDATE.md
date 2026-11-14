# Internal development checkpoint, 14 November 2026

Not a release or installed-package qualification. Development work remains local
on `work/smb-first`; publication is prohibited by the current roadmap. The UI says
0.4.0, but package metadata has not yet been advanced from 0.3.0.

## Verified checkpoint

Native Haiku build succeeds. Executable SHA256:
`2c14373a249fb15924f0ccf96bb15452d3ad3c25733935cd1c46ab940ade4560`.

- 42 local filesystem/Trash/filter checks pass. Six added cases verify local
  Trash, Restore and Delete while the opposite location is an offline SMB URL
  or an archive. Source-only operations must ignore the opposite panel.
- 69 archive/network checks pass against isolated fixtures. Includes ZIP,
  tar.gz/xz/bz2 navigation/extraction, traversal/link rejection, cancellation,
  SMB/FTP/SFTP listing, reads, staged uploads, conflicts, rename and deletion.
- Added local source-change detection and nonblocking rejection of special
  files. Added cancelled-upload checks for all three protocols: original content
  retained, no completed-file callback, no leftover remote staging entry.
- Source staging verifies bytes when size is known and compares available
  metadata before allowing commit. Unknown uncompressed archive size is not
  confused with an empty file. Local reads also compare inode and nanosecond
  modification/change timestamps. This does not make remote reads snapshots.
- Current development UI visually checked in dark theme: thumbnails and inline
  connection form fit within the main window. Earlier thumbnail/image-preview/
  copy/conflict checks predate the last changes and are not a full UI regression
  for this exact executable. Compact/light-theme/package checks remain due.

### Late focus fix

After this checkpoint, Escape in the inline connection form crashed because
native views can have a null `Name()`. Crash report confirmed `strcmp(nullptr)`
inside `Keys::Filter`. Both Escape and Tab now guard unnamed controls. Native
rebuild SHA256:
`e8932fe97737e6326ade22956658e0ed9908a4072252da9777877a1bcb0cf3fe`.
Actual keyboard checks passed: connection-address Escape, Tab through connection
fields then Escape, new-folder Escape, filter Escape, and acknowledged graceful
quit. Main window remained responsive and idle after each scenario. This later
binary has only the focus-filter change since the 42+69 backend checks above;
those are not presented as reruns on this exact hash. Evidence:
`focus-regression.log`, `focus-after-fix.png`, `escape-crash.report` in the same
private report directory. The failed check is retained, not counted as a pass.

Run from the native build directory with owned fixtures only:

```
./Listi --self-test
./Listi --virtual-test /boot/home/listi-fixtures/archives /boot/home/listi-fixtures/known_hosts
```

Private evidence: `reports/listi-network-20261114/safety-local-test.log`,
`safety-virtual-test.log`, `inline-connect-checkpoint.png` under Kambuzi's local
workspace. Synthetic fixture credentials only. No real NAS account was used.

## Next review gates, not completed work

1. Split `Pane.cpp` into directory scanning, entry/selection state, detailed/grid
   views and pane coordination. The hidden detailed view currently holds shared
   rows/selection for the grid; make that ownership explicit before expanding it.
   Isolate private column-list APIs to the detail renderer. Split inline prompt
   ownership out of `App.cpp`; rename `NetworkWindow` to reflect its inline view.
2. Audit cancellation and conflict ownership, source/destination races and
   partial-directory results. Inspect network commit/rollback boundaries and
   retained-backup errors. File progress must not imply a whole move succeeded
   before source cleanup. A prompt must not silently replace a pending decision.
3. Preserve supported file metadata on extraction/download. Do not claim complete
   metadata parity merely because local BFS copy preserves attributes.
4. Verify grid range selection/drag, tree navigation on both sides, folder-view
   preference round trips, preview cancellation, shortcut focus and Escape.
   Recheck layout with details/conflicts/advanced connection options expanded,
   at laptop size and with both system palettes. Review idle CPU and bounded caches.
5. Installed-package and upgrade verification only after the above, then a private
   ZIP candidate. Do not push this work to a public branch or publish a release.

## Current capability limits

- Archives are read-only; extraction is supported, archive editing is not.
  Current virtual archive URLs require a local outer file. Nested and remote
  archive opening needs a separate tested materialization lifecycle.
- Runtime format support comes from installed libarchive, not arbitrary installed
  command-line extractors. The current four tested formats are not evidence that
  every format supported by the system works. Test single-file compression too.
- Cross-location move is refused with Copy guidance, leaving the source intact.
  Remote Trash is not emulated as permanent deletion. Same-endpoint rename/move
  is supported without replacement.
- SFTP requires a trusted known_hosts entry; no first-use host-key enrollment UI.
  FTP fallback listing supports Unix-style listings. No plaintext credential
  persistence, no remote file editing and no broad external-server qualification.
- Remote metadata may lack reliable timestamps. A same-size concurrent edit can
  evade metadata checks; protocol rename semantics do not guarantee race-free
  no-replace commits on all servers. These need explicit policy and fault tests.
- Dolphin parity, superior performance and a particular resource reduction have
  not been measured or achieved. Public-launch gates are in ROADMAP.md.
