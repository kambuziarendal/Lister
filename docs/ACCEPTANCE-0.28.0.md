# Listi 0.28.0 private candidate acceptance

2026-11-15. Branch: work/folder-compare-20261115. Local candidate only.
Evidence: /home/lobster/kambuzi/reports/listi-archive-editor-20261115/compare-*.
No upload, push, Depot publication or paid CI. This continues the task-owned
Haiku R1beta6 x86_64 KVM fixture used for 0.27.0, with immutable 0.26.0 backing.

## Verification

- Full native runner: 1123 PASS, zero FAIL, 32 suites plus app self-test.
  New coverage: 31 comparison/hash checks and 14 same-type selection checks.
  SHA-256 passes published empty, abc, two-block-padding and million-byte
  vectors. Comparison covers missing/extra entries, same-sized corruption,
  hidden/empty/nested paths, links, special files, read failure, cancellation,
  changed sources, entry limits and local-to-archive checks for ten writers.
  The guest runs as uid 0, so permission failure is injected at opendir rather
  than incorrectly relying on chmod(000) to deny its access.
- Six additional native integration assertions passed against task-owned
  SMB, FTP and SFTP fixtures: equal trees and same-sized changed content for
  each protocol. Restricted guest networking uses owned SSH reverse forwards;
  the initial direct-connection failure remains recorded separately.
- Post-suite window-only corrections invalidate results when paths change,
  keep both byte columns visible, allow width resizing, and use the existing
  ThemeTextView for live dark/light colors. Final native theme assertions and
  55 app self-tests passed. Backend and selection implementation are unchanged
  from the full suite. Repeated checks are not added to the unique total.
- Installed mouse/keyboard workflows verified the extension menu, actual native
  clipboard contents for three PDFs (including uppercase extension), exclusion
  of a similarly named directory/link, and restriction to one filtered PDF.
  Quick comparison correctly reports two name differences and five size-only
  matches. SHA-256 additionally finds the corrupted four-byte file. Both shown
  digests match independent Python hashlib results. Matching rows and detail
  views were inspected, including a local-folder versus ZIP comparison.
- Installed cancellation during a 256 MiB checksum read is incomplete and a
  subsequent comparison succeeds. A 5001-entry folder counts all differences
  and explicitly reports its 5000-row display cap. Changing roots invalidates
  previous results; a missing root cannot produce an empty successful result.
  Light/dark palettes and actual resize geometry are checked on the installed
  candidate. Archive-editor recovery still opens its retained copy in StyledEdit.
- Actual 0.28.0 -> retained 0.27.0 -> final 0.28.0 package round trip preserved
  20 sampled comparison/archive/settings/recovery files. The retained 0.27.0
  binary matched its known hash. Final installed binary equals the native build:
  edf653a1c4fd195ca892bb6c986e5ee18fc24d3029e08682744f3e9347fe3dde.

## Recovery and boundaries

See COMPARE-0.28.0.md and ARCHIVE-EDITOR-0.27.0.md. Comparison is read-only and
point-in-time, not synchronization or a locked filesystem snapshot. Size-only
matches never prove content equality. Attributes and permissions are excluded;
remote stability is limited to available server metadata. FTPS and real NAS/
physical-machine qualification remain unverified. Native implementation does not
require an external checksum command or add a runtime crypto dependency.
Application rollback retains files and editor copies, never restores stale data.
