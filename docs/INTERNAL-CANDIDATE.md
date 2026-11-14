# Internal development checkpoint, 14 November 2026

Private 0.4.0 candidate. Development remains local on `work/smb-first`;
no public branch push, release or Depot publication is authorized. Package and
UI versions agree. Installed-package acceptance is recorded separately from the
development checks below, with the exact artifact in the private test receipt.

Installed qualification is complete for this private candidate. See
[the exact acceptance receipt](ACCEPTANCE-0.4.0.md), including upgrade/rollback,
artifact identity and the distinction between app tests and native object tests.

## UI/package continuation

Final development executable SHA256:
`4b0f72581d6e333987532b3b91529248bd212825540b8654747e243a69f13a4f`.
The previous control build passed 55 local, 96 archive/network, 9 fault and
6 native preview checks. The only executable change since that control build
refreshes uncached folder-tree routes; package acceptance reruns the native suites.

- Open questions replace the redundant action-button bank temporarily. Compact
  panel minima, bounded operation history height and a two-row advanced form keep
  conflict controls and connection fields available at 1024x650 with the default
  font. Both trees and preview are supported at that size. Larger fonts can need
  a larger window. Closing the prompt restores the user's button preference.
- Both trees navigate independently; keyboard navigation and Tab are checked.
  Folder overrides survive restart; Use default removes the override. Newly
  created uncached directories rebuild the route instead of retaining a stale
  tree selection. Toggle a tree off/on to refresh its loaded branches.
- Preview/thumbnail replies use bounded shared ownership, not owning pointers in
  messages that may be dropped during shutdown. Native cancellation during decode
  and 12 rapid UI navigation cycles were exercised with a synthetic PNG.
- Bounds: 256 cached thumbnails per pane, 64 outstanding thumbnail requests/replies,
  one preview reply, 64 MiB decoded bitmap stream, 32 MiB remote preview download,
  20,000 cached tree nodes, 10,000 folders per branch, 4,096 folder overrides and
  64 KiB operation history. Bounds are not a whole-process memory guarantee.
- Single-file gzip/bzip2/xz tests found libarchive's mtree guessing ordinary text.
  Explicit standalone compression now uses raw format; `.tar.*` containers retain
  archive detection. Nine single-file checks and three explicit-directory/mode/
  timestamp checks pass, including stripping setuid and retaining existing
  destination-directory permissions. Other libarchive formats remain unqualified.
- Package requirements include libarchive, libcurl and libsmbclient. The build
  script rejects mismatched package filename and PackageInfo versions.

Private continuation evidence: `reports/listi-ui-package-20261114/` under the
Kambuzi workspace. Test fixture credentials are synthetic. No physical hardware,
external NAS, FTPS, long-duration soak or Dolphin performance comparison is claimed.

## Verified checkpoint

### Resumed safety/module increment

Native development executable SHA256:
`53981103328791379530b851352a37bf2b6f01c4345fe4260f00b916f4f1bd48`.
This supersedes the development checkpoints below, not the installed package.
40 source/build inputs were read back from the isolated Haiku guest and match
the worktree. On this exact build: **55 local/selection checks, 84 archive/network
checks and 9 deterministic commit/rollback fault checks pass**. The fault test
links the native build's actual VirtualFile/VirtualOperation objects to an
in-memory network adapter; real protocol tests use the separate owned fixtures.

- `PaneModel` owns entries, visible selection and focus. `DirectoryScanner`,
  `FileDetails` and `FileGrid` are separate workers/renderers. Only FileDetails
  includes private column-list headers. The grid no longer depends on a hidden
  detail renderer. Shift range shrinking and multi-selection drag are implemented.
- `InlineAction` owns one frozen, token-identified question. New operations and
  connections cannot replace it. Closing destroys its controls immediately.
  `ConnectionView` is named for its actual inline lifecycle. Escape closes the
  pending question first, without also aborting a separate file conflict.
- Conflict answers identify the exact request and accept only its first answer.
  A failed prompt delivery aborts rather than waiting forever. Cancelled/partial
  jobs do not fill the progress bar as if completed.
- Copy/cleanup moves emit completion only after source cleanup. Copy checks
  source identity, mode and nanosecond mtime/ctime, rejects special files without
  blocking, detects disappearing/replaced conflict targets, and reports directory
  enumeration failures. Source snapshots are retained on incomplete moves.
- Network commit fault tests cover cancellation after backup, failed commit,
  failed rollback, retained staging/backup, destination changes, skip and abort.
  Errors distinguish committed bytes with failed cleanup from an uncommitted
  transfer and include retained recovery paths. No success event is emitted for
  those incomplete outcomes.
- Extraction/download preserves available POSIX modes and modification times,
  stripping privileged mode bits. Explicit archive directory metadata is retained;
  existing destination directories are not chmodded. Unknown remote timestamps
  are still unknown. Upload metadata, ACLs, ownership and arbitrary BFS attributes
  are not a parity claim.

19 native UI assertions passed: grid range/shrink, copy/filter/drag with filesystem
readback, frozen question/name, independent prompt/conflict Escape, real Replace,
details-parent navigation, Tab/focus and responsive light/dark layouts. Basic
connection form visually checked at 1024x650; normal dark view at 1140x650.
The UI harness's original text-view range query and assumed native Home behavior
were corrected; their failed attempts remain in the evidence, not counted as
passes. The tested parent action uses an actual click and Enter.

Private evidence: `reports/listi-safety-20261114/` under Kambuzi's local workspace.
Graceful quit was acknowledged; the Listi UI was then absent from the guest process list.
No public push, package installation, release or physical-hardware qualification.
Not all UI/release gates below are closed by this increment.

### Earlier checkpoints (historical binary identities)

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

## Qualification and remaining product work

The safety/module increment and UI/format continuation are internal beta work.
Installation and rollback must use the retained private 0.3.0 package, preserve
`~/config/settings/Lister_settings`, compare installed/build executable hashes,
and rerun the native checks. Do not restore a filesystem snapshot to roll back an
application. Exact results and private ZIP identity belong in the acceptance
receipt; a successful development run alone is not installed-package acceptance.

Public launch still needs the named-version Dolphin feature matrix, representative
performance datasets and broader protocol/filesystem qualification in ROADMAP.md.
The limitations below remain intentional and visible.

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
