# Stability coverage audit, 15 November 2026

Requested scope: concurrent file changes, network interruption, large folders and
archives, and interaction between jobs and UI. New features are paused. Baseline
is private 0.30.0, commit `65178b1`, not a new application candidate. This review
adds evidence and priorities; it does **not** fix the two findings below.

## Existing coverage, with boundaries

| Area | Verified coverage | Still open |
| --- | --- | --- |
| Concurrent changes | Frozen local source/destination identities across prompts and queueing; source metadata before/after copying; target replacement checks; archive fingerprints; full destination byte read-back before cross-container source deletion; modified sources/targets, same-size damage, new children and partial cleanup in native regressions | Path-based recursion is not proof against every hostile ancestor swap. Ordinary directory copy is not an atomic snapshot of changing membership. Remote metadata cannot detect every same-size edit without reliable timestamps. Power-loss/recovery across two locations is not qualified. |
| Network loss | 0.25 real SMB/FTP/SFTP service tests, 96 checks; actual SFTP transport closure during download retains the original local target, no false success, next operation reconnects. Twelve deterministic network publication/recovery faults include backup/commit failure and exact retained recovery paths | No equivalent complete real-break matrix for SMB and FTP/FTPS, or loss during upload/rename/cleanup and lost acknowledgements. Server families and long blackholes remain gaps. SMB has a 10-second native timeout and a global serialization lock; Curl has 10-second connect and 20-second low-speed thresholds. These settings are not measured upper bounds for the whole operation or immediate cancellation. |
| Large folders/archives | 0.25 installed UI: 5,000 local entries ready in 3.605 s; 5,000 ZIP members in 6.064 s, exact counts and subsequent small-folder navigation. 0.28 compare counts 5,001 entries, explicitly limits displayed rows to 5,000; cancellation during 256 MiB checksum read tested | These are bounded listing/compare samples, not 5,000-file move or multi-GiB archive qualification. Repeated archive reads/indexing and destination-tree verification are expensive. Fresh move probe below confirms severe scale sensitivity even for small data. Memory/CPU/latency budgets, cold/warm runs and physical hardware remain unqualified. |
| Jobs and UI | Worker queue, generation-bound scans/searches, frozen job selection/target, conflict serials, cancellation, and clipboard leases. Installed conflict waits survive Commander/Workbench changes; cancellation retains destination bytes. 0.26/0.29 Cancel/Escape/retry, internal moves and pending Cut across restart tested. Current 0.30 native suite passed | No general stress qualification for many queued jobs plus continuous folder mutations, slow servers and rapid view changes. A row-delivery failure can be ignored and reported as successful completion, reproduced by injected failure below. Queue progress delivery, completion ordering and sustained reload pressure need targeted acceptance. |

Current 0.30 regression: 1,292 PASS across 35 suites plus app self-tests, recorded
in ACCEPTANCE-0.30.0.md. This number is **not** four-area certification. Protocol
and old installed-UI evidence above retains its original version; it was not all
rerun on 0.30 during this audit. Relevant source and historical logs were checked.

## Fresh native probes on unchanged 0.30

Owned isolated Haiku hrev59866+79 x86_64, KVM, 2 vCPU, 3 GiB RAM. New writable
overlay backed by the stopped 0.30 runtime disk; backing chain remains immutable.
Installed binary still hashes to
`8e2ea9932183c631ea439a52d3dedd1ccf8c5cdde279bad17b76d177cbf8b1ad`.
Probes link the already audited actual native implementation objects, not a Linux
simulation. Sources and reproduction notes are in `tests/probes/`.

### S01: a failed directory-row delivery can become false success

DirectoryScanner's batch send discards SendMessage status, empties the batch and
continues. Pane accepts a later error-free kScanDone as a valid location.

- Control: all 160 files delivered normally.
- Small-capacity receiver with a 750 ms handler delay: all 160 delivered. This
  did **not** reproduce natural backpressure loss and is not counted as a failure.
- Inject exactly one B_TIMED_OUT return for a kRows send in the real scanner:
  only **81 of 160** files delivered, followed by completion with an empty error.
- No file was deleted. The risk is a misleading/incomplete listing, including
  what the user thinks Select all covers. The injection proves missing failure
  handling; it does not establish incidence under ordinary UI load.

Fix acceptance: unsuccessful delivery must never be accepted as a complete
listing. Bound retry/backpressure, retain generation cancellation, expose a
reloadable error, and test a successful scan after the failure. Include local
and virtual providers and the final completion-message path.

### S02: archive-to-local move work grows sharply

Actual ZIP-folder Move through PrepareArchiveMove/Run, 1 KiB regular files, fresh
fixtures. lstat wrapper counts calls in linked implementation during the move;
no error injection or metadata guard is removed.

| Files | lstat calls | Instrumented move time | Integrity |
| --- | ---: | ---: | --- |
| 64 | 1,141,851 | 9.768422 s | All 64 payloads exact; selected source folder removed |
| 128 | 7,168,091 | 59.480642 s | All 128 payloads exact; selected source folder removed |

Doubling entry count caused about 6.3 times the metadata calls and 6.1 times the
elapsed time in this bounded sample. This is not an uninstrumented app benchmark
or proof of an exact complexity class. The count nevertheless demonstrates
excessive repeated work at small sizes. The existing 5,000-entry browsing samples
cannot be used to claim such transfers are acceptable.

Code connection: RunArchiveLocalMove's destinationGuard calls
LocalMoveTree::Verify over the entire destination tree on repeated proceed checks
through read-back and archive cleanup. Archive provider operations also reopen
and reindex containers. Measure their individual contributions before optimizing.

Fix acceptance: retain full byte read-back and source/target change detection;
avoid repeatedly validating the whole tree on every small I/O step. Add bounded
scale/latency checks, cancellation and mid-operation mutation trials. Measure
64/128 plus materially larger trees and large-byte archives with documented
cold/warm conditions. Do not exchange data safety for a faster green test.

## Priority before more features

1. S01: eliminate falsely successful incomplete listings, with a durable regression.
2. S02: remove repeated whole-tree/archive work while preserving verified moves.
3. Real network interruption matrix: SMB/FTP/SFTP download and upload, before
   publication, during rename/cleanup, lost reply after server commit, cancellation
   latency, next-operation recovery and any retained recovery paths.
4. Combined sustained job/UI stress and larger-data measurements: queue multiple
   jobs, mutate sources/targets, navigate/filter/switch layouts, pause/cancel/quit,
   and verify exact content, terminal state, bounded resources and recoverability.

Batch rename and custom selection actions remain planned, but are behind this
stability work. No universal archive parity, failure-proof claim or public
release follows from the current audit.

Evidence: `/home/lobster/kambuzi/reports/listi-stability-audit-20261115` contains
scanner-delivery.log (negative natural-reproduction attempt), targeted-probes.log,
probe sources, runtime ownership and teardown receipt. Historical evidence used:
listi-bughunt-20261115/{live-disconnect,large-ui,large-archive-ui,queue-ui,
network-faults-asan}.log and versioned acceptance documents for 0.24 to 0.30.
No new network server tests or installed interaction stress were claimed here.
