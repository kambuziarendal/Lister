# Listi 0.31.0 stability candidate

Private successor to 0.30.0. No new user-facing functions and no publication.
The audit baseline and original reproductions remain in
[STABILITY-AUDIT-0.30.0.md](STABILITY-AUDIT-0.30.0.md).

## Changes

* Directory batches retry transient delivery failures without discarding rows.
  Exhausted retries produce a reloadable error, not a successful partial view.
  A shared terminal-failure signal lets the actual pane recover even when the
  completion message fails. Stale generations do not complete new requests.
* Critical queue events are retained until the window drains them. Timed wakeups
  recover missed notifications, including clipboard completion tags. Late
  progress cannot revive a finished operation. Shutdown does not wait on an
  unbounded worker-to-window send.
* Verified archive/local moves still decode and compare every destination byte.
  Full local destination snapshots are checked at verification boundaries and
  immediately before source-archive publication, not in every decoding callback.
* One-job, worker-local directory metadata reuse is capped at four listings and
  8 MiB of entry/name/path accounting. Archive fingerprints and password
  generations invalidate it. One sequential libarchive reader is reused only
  for qualified ZIP/TAR/CPIO formats; changed archives and out-of-order reads
  reopen it. XAR uses fresh readers. No open reader or callback survives its job.
* Network replacement records the backup name before sending the rename. A lost
  response after server commit can therefore recover the original or report its
  exact recovery path. Unacknowledged publication never overwrites the current
  destination with an older backup.
* View-owned thumbnail, preview, directory, folder-tree and search readers are
  cancelled and joined before owner destruction returns. Posting a quit alone
  left BHandlers alive during application token-space teardown and caused a
  real installed-app exit crash. Directory wakeup timers also stop first.

## Verification

Targeted native checks include actual pane recovery, twenty queued jobs with
failed notifications, paused/conflicting cancellation, stale conflict answers,
archive mutation/corruption, password invalidation and all writable formats.
A first sequential-reader attempt failed XAR qualification without removing the
source; the final implementation explicitly excludes XAR reader reuse.

Instrumented ZIP-to-local moves, 1 KiB per file, native Haiku VM:

| Files | Linked lstat calls | Move seconds | Result |
|---:|---:|---:|---|
| 64 | 12,754 | 0.885 | exact bytes, selected source removed |
| 128 | 25,134 | 1.906 | exact bytes, selected source removed |
| 512 | 99,232 | 8.859 | exact bytes, selected source removed |
| 1024 | 198,048 | 20.774 | exact bytes, selected source removed |
| 5000 | 965,488 | 132.127 | exact bytes, selected source removed |

Baseline: 64 files took 9.768 s / 1,141,851 calls; 128 took 59.481 s /
7,168,091 calls. These are individual instrumented VM measurements, not a
universal throughput guarantee. Creation, final independent checks and fixture
cleanup are outside the measured move interval.
The final regression repeated 64/128 files in 1.003/1.781 s with the exact same
metadata-call counts; larger measurements precede the reader-lifecycle-only fix.

Task-owned real SMB, FTP and SFTP servers: each protocol passed normal transfer,
mid-download break, mid-upload break, pre-publication break, lost backup-rename
reply, lost publication reply and cleanup break. Server-side bytes/recovery names
were independently checked and a fresh client operation reconnected. Two initial
fixture hooks did not fire; those were corrected and rerun, not counted as product
passes. The deterministic state-machine tests also cover rollback failures.

The final native regression passed 1412 checks across 41 suites and the app
self-test (1406 PASS lines plus six scaling assertions). Two 64 MiB TAR members
also moved with exact destination bytes and
source removal (7.659 s, 35,396 linked lstat calls). Its first work-budget
assertion counted files but not read chunks; the corrected structural bound
accounts for both. The payload and source checks passed on both attempts.

Active-download cancellation was measured separately on all three live
protocols: 33.8 ms SMB, 23.4 ms FTP and 74.5 ms SFTP from the cancellation hook
to return. These were healthy local connections, not blackholed sockets. The
first cancellation fixture required an overly large individual callback; it was
corrected to accumulate bytes and then rerun with a proven trigger.

Installed package, verified byte-identical to the tested build:

* Native mouse/keyboard pause, Commander/Workbench switches, filtering and
  navigation during a queued 5000-file copy. Every destination payload matched.
* A second queued job rejected a replaced source inode without touching either
  source version or creating a destination. A visible directory receiving 1200
  create/delete pairs settled to the exact final 301 entries.
* Navigation-to-complete measurements during this workload were 0.465, 6.304 and
  7.046 s, including repeated scans while the directory changed. These are not
  input/event-loop latency measurements.
* Quit while a clipboard conflict was pending was refused. Escape and Cancel
  preserved both sides and allowed retry; successful local and archive cuts
  consumed their clipboard leases and retained unrelated archive members.
* Real dark palette and resizing retained readable controls and settled state.

The first GUI pause attempt ran after the initial copy had already finished;
that attempt was not counted as a paused-load pass. The integrated rerun used a
fresh destination and verified the Pause state before proceeding.

The first final quit closed the main window but crashed in the thumbnail
reader's BHandler destructor. That candidate was rejected, not counted as a
successful shutdown. A native regression holds each of the five actual readers
inside its looper while closing its owner: all five lifetime assertions fail
before the fix; all 15 lifecycle checks pass after it. The first test harness
attempt used an invalid empty saved-search message and was corrected to use the
real serialized empty store; it is not a product failure or a test pass.
The corrected installed package subsequently passed eight complete launch/quit
cycles (0.327 to 0.418 s to observed process exit), alternating main-window and
application quit, including actual thumbnail/preview rendering, workspace
switches and opening/closing search. An earlier UI driver left a diagnostic
dialog present and did not establish keyboard focus; that incomplete run was
not counted. The final driver asserted the active image directory and window
count and checked process exit, not just disappearance of the main window.
Two more installed cycles quit while a header-encrypted 7z directory was
genuinely waiting in the password window. Both processes exited (0.421/0.380 s),
and restarting prompted again rather than retaining a stuck reader.

Package/source hashes and data-preserving 0.30.0/0.31.0 round-trip receipts are
kept outside the package. No preference or saved-search format migration is
introduced. Roll back by installing the retained 0.30.0 HPKG, not by restoring
older user data.

## Scope and limits

No claim of atomic filesystem snapshots, protection against every hostile parent
swap, same-size remote changes with unreliable timestamps, power-loss atomicity
across two locations, or every server implementation. No claim of a global
cancellation deadline while blocked inside a native networking library. The
joined reader shutdown likewise has no universal deadline for stalled native
filesystem I/O or third-party translators. Cancellation-aware reads and bounded
result delivery avoid waiting for a reply from the locked owning window. The
qualified servers are synthetic loopback fixtures, not WAN/FTPS qualification.
The 5000-file move establishes correctness and measured work for that shape;
7z/solid archives and arbitrary member order have different costs. The metadata
cache's budget is not a whole-process memory limit.

New features remain behind stability qualification. Batch rename is not included.

## Reproduction

On an owned Haiku guest after `make`, run `python3 tests/run-native-audit.py`.
Larger move probes use the resulting `build/native-audit-tests/archive-move-scaling`
with `LISTI_SCALE_FILES="512 1024 5000"`, or `LISTI_SCALE_FILES=2`,
`LISTI_SCALE_BYTES=67108864` and `LISTI_SCALE_FORMAT=tar`.

The real network lane uses `tests/network-failure-fixture.py --directory
<owned-report>/network` on the host (impacket, pyftpdlib and paramiko), isolated
loopback services and task-owned SSH reverse forwards for ports 1445, 2121,
2222 and 31000 to 31019. Compile `tests/network-interruptions.cpp` against the
native application objects as in the native runner, then invoke the host
`tests/run-network-interruptions.py <owned-report>` with an `ssh` wrapper in that
report directory. The guest known-hosts file is `/boot/home/listi-network-known-hosts`.
The fixture control endpoint 23990 is host loopback only. Stop owned services
and tunnels after the lane; never use real shares or accounts.
