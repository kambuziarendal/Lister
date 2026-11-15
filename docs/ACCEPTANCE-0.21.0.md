# Listi 0.21.0 private multipart candidate

Runtime/source: `7629457f9302a22a268e0d1860add1150eaae5ef`.
Native Haiku hrev59866+79 x86_64, p7zip 17.05-2, libarchive 3.7.9.
Owned standalone disposable VM cloned from stopped accepted 0.20.0.
No public push, release or feed change.

## Verified on final candidate

- 245 source/build inputs matched. 76 new multipart checks on a 256 KiB native
  worker stack; 47 previous 7z/RAR, 39 password, 56 archive-job, 10 job-snapshot
  and 55 local self-test checks passed. Format regression: 111 reader checks
  and 31 queued fixture jobs. Repeated runs are not additional coverage.
- Split 7z plain/encrypted names, RAR5 plain/encrypted names, old RAR4 naming,
  nested solid RAR5. Opening first/middle/last parts canonicalizes the first.
  Exact original payload bytes and CRC-checked reads; atomic queued extraction.
- Missing first/middle/last, damaged middle/first, duplicate numbers, symlink
  parts, extra trailing/unconsumed parts, wrong/correct password, password cancel,
  nonfirst part replacement while prompting, mutation before confirmation and
  after first decoded member, added part after confirmation, cancellation after
  progress and no leftover private staging all pass.
- Installed UI: Simple/encrypted 7z/light, Commander/old RAR/dark,
  Explorer/encrypted RAR5/dark. Actual mouse/keyboard opens a nonfirst part,
  previews content and extracts with exact filesystem readback. Main frame
  unchanged. Masked password and wrong/correct retry exercised.
- Installed fault flows: Escape and queue Cancel close password waiting without
  publication. Replacing a nonselected part automatically closes the prompt and
  aborts. Missing part 3 is identified in an inline notice. Quit during encrypted
  multipart listing succeeds. No synthetic secret in Listi settings.
- 100 native window queries during password wait: zero failures, p95 0.363 ms,
  maximum 0.414 ms. VM message-loop latency, not physical input latency.
- Actual installed 0.21.0 -> accepted 0.20.0 -> same 0.21.0 round trip preserves
  settings, places, searches, archive options, fixture data, modes, links and BFS
  attributes. Restart asks for a password again; cancelled listing changes no data.

## Identity

Built/installed executable SHA256:
`67965b18dd266e5f13d2aa8b704b04dc532ed5ec514266fbae0abdfd74e505cc`

HPKG SHA256:
`df4b1869d160f86686082a138c08d2f46866906fd8d727a67de5d811dcf3c0c3`

Private ZIP SHA256:
`528ba4cd1e5821c1e0b11d871e9f8d74f7c20d0237bb00f96a7a4ac3ceb8b14f`

Report: `/home/lobster/kambuzi/reports/listi-multipart-20261115`.
Evidence: final-build-tests.log, fixture-tests.log, inputs-verified.log,
install.log, final-ui.log, ui-faults.log, ui-faults-continue.log, roundtrip.log,
screenshots, final-processes.log and shutdown.log. Owned VM608120 stopped;
SSH23317 closed after app exit and filesystem sync.

The first fault-UI script used a hey specifier that did not traverse the nested
notice view. This harness failure remains recorded. The screenshot was inspected:
"Archive part 3 is missing. Keep all parts together in one folder." The corrected
continuation checks unchanged source location and completes the remaining flows.
No runtime correction was needed after final commit/build.

Bounds and unqualified cases are in ARCHIVES-0.21.0.md. In particular, old RAR4
multipart fixtures are unencrypted; encrypted RAR4 single-file tests are not
presented as encrypted multipart coverage. No physical hardware qualification,
split ZIP, split creation, encrypted creation or existing-archive editing claim.
