# Listi 0.26.0 private candidate acceptance

2026-11-15. Branch: work/archive-move-20261115. Local candidate only.
No upload to Telegram, public release, repository push or Depot publication.
Evidence: /home/lobster/kambuzi/reports/listi-archive-move-20261115.

## Identity

Task-owned standalone Haiku R1beta6 hrev59866+79 x86_64 KVM disk, 2 vCPU,
3 GiB RAM, restricted user networking and loopback SSH. Previous backing
chain inspected and flattened; no parent images modified. Initial installed
0.25.0 matched the retained acceptance hash.
Final installed binary equals the final native build (cmp), SHA-256:
6be8928111d880cd94eba029a0a41f46bc7d495faadd3fed04f731939802a9d2.

## Verification

- Full native runner: 971 PASS, zero FAIL, 28 suites plus 55-check app self-test.
  The 100 new checks comprise 91 move/clipboard checks and 9 fault checks.
  Matrix covers ten writers as both source and destination, recursive/empty
  directories, last-member removal, encrypted ZIP/7z, password cancellation,
  stale source/destination snapshots, name conflicts, cancellation before/after
  publication and during cleanup, hardlink aliases and clipboard leases.
  Cross-format jobs run on a native 256 KiB worker stack.
- Injected destination ENOSPC and source rename failure preserve exact source
  archive bytes and emit no successful move. Deliberate post-write payload
  corruption is detected by read-back comparison before source removal.
- After the full runner, same-archive moves were additionally bound to original
  job guards throughout execution. Final backend passed all 200 archive move,
  fault and folder-operation checks plus 55 self-tests. The only subsequent
  code change distinguishes same-archive confirmation wording. That final
  installed application passed 55 self-tests and the UI scenarios below.
  Repeated checks are not added to the unique total. No paid CI was used.
- Final installed mouse/keyboard flows: archive Cut/Paste, Cancel and Escape
  followed by retry, consumed Cut selection, panel Move, same-archive Cut into
  a subfolder, and quit during confirmation followed by successful Paste after
  restart. Source/target bytes and unrelated members checked independently
  with Python zipfile/tarfile. Confirmation/result screenshots inspected.
- Actual 0.26.0 -> retained 0.25.0 -> final 0.26.0 package round trip preserved
  all eight sampled archive/settings files. Baseline hash and final build
  equality were asserted, then the final installed application was exercised.
- git diff --check passed. Candidate source manifest verified against the guest.

## Limits and recovery

See ARCHIVE-MOVE-0.26.0.md. Two-archive moves are not atomic: interrupted
cleanup can leave two copies; inspect both and reselect before retrying.
No persistent automatic resume, physical-machine or power-loss qualification.
No general archive/local-folder Cut, RAR writer, split/nested/network archive
editing or external-editor save-back. No comprehensive special-metadata
preservation. Reinstalling 0.25.0 rolls back the application, not completed file
operations; retain resulting archives and never restore stale data as app rollback.
