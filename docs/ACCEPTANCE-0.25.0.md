# Listi 0.25.0 private candidate acceptance

Date: 2026-11-15. Runtime commit: 91e2773.
Branch: work/bughunt-20261115. No public publication.
Evidence root: /home/lobster/kambuzi/reports/listi-bughunt-20261115.

## Identity and environment

Haiku R1beta6 hrev59866+79 x86_64, task-owned KVM overlay, 2 vCPU and
3 GiB RAM. Backing images were kept immutable. Native build and installed
/boot/system/apps/Listi were byte-identical (cmp), SHA-256:
38be3d5e049611d72a5d4b286cfc5e3ebe90f2308a4466f55553f102ed018a8b.
All 259 tracked runtime-commit files matched the guest source manifest after
synchronizing one baseline acceptance document that had differed. No code
mismatch remained. Physical hardware is not qualified.

## Native and protocol checks

- Full native runner: 871 PASS lines, zero failures, 26 compiled suites plus
  the application's 55-check self-test. Includes 100 folder-operation checks
  across 10 writer variants, 20 transfer/clipboard regressions and 16 malformed
  namespace cases. Evidence: full-native.log.
- The only subsequent runtime change was the visible version string in App.cpp;
  it was rebuilt. Final installed binary: 55 self-tests and 96 live virtual
  checks passed. Evidence: final-installed-tests.log.
- Real loopback SMB/FTP/SFTP services: listing, Unicode names, transfers,
  conflicts, cancellation, rename/delete and rejected credentials. 96 checks.
- Real SFTP transport closure during download preserved the existing destination
  and emitted no successful completion. A following operation reconnected and
  succeeded. Two checks in live-disconnect.log.
- Portable deterministic network faults: 12 checks under AddressSanitizer and
  UndefinedBehaviorSanitizer, no reported violation. network-faults-asan.log.
- git diff --check passed. Counts above are separate observations; repeated
  runs are not added together to inflate a unique test total.

## Installed UI

Actual mouse/keyboard input to the installed final-runtime application covered:
clipboard paste into TAR.gz, New folder, Rename, member Delete, ZIP-member
folder drag/drop into TAR.gz, archive refresh, and queue conflict cancellation
through Commander/Workbench switches. Archive member contents were checked
with Python's independent zipfile/tarfile readers. Ordinary panel Copy into
ZIP was additionally exercised on the preceding binary with the identical
operation implementation, before correcting its visible version string.
Screenshots and queue-ui.log retain the evidence.

A 5,000-file local directory became PathReady in 3.605 seconds; a 5,000-member
ZIP in 6.064 seconds. Both displayed the exact count and subsequently navigated
back to a one-file directory with no stale selection. These are single bounded
VM samples, not throughput guarantees. large-ui.log, large-archive-ui.log.

## Recovery

Actual installed final 0.25.0 -> retained 0.24.0 -> final 0.25.0 round trip
succeeded. Baseline binary SHA-256 was
cb649fe9c99cd3f1377dee415755a501646cf87b80341acad71b521f06728a1f.
All six sampled settings and user archive files retained their hashes. The
upgraded application started again. Evidence: final-recovery.log.
Haiku refused a same-version replacement with `Name in use`; it did not activate
the replacement. The documented rollback/upgrade path resolved it and hashes
were checked, rather than treating package-manager output as installation proof.

## Limits and remaining work

See ARCHIVE-FOLDERS-0.25.0.md and BUGHUNT-0.25.0.md. Full archive-as-folder parity
is NOT complete. RAR and other formats without qualified writers, split or
nested/network containers, clipboard Cut/cross-container Move, external-editor
save-back and comprehensive special metadata retention remain outside this
candidate. TAR+zstd/LZ4 are capability-gated and not qualified by this Haiku
engine build. No claim of arbitrary archive sizes, every server family, hostile
race-proof local filesystem traversal or launch readiness is made.
