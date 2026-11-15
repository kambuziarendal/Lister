# Listi 0.19.0 private candidate

Runtime/source commit: `613dccd235a52bd5b057ed3a93b91923af5713ba`.
Haiku hrev59866+79 x86_64, system libarchive 3.7.9. Standalone disposable
KVM image copied from the stopped accepted 0.18.0 guest, isolated networking.
No public release, feed update or repository push.

## Evidence

- Full existing native audit suite passed. Final archive-specific tests passed:
  39 password checks, 56 archive workflow checks, 55 built-in checks.
  Existing fixture suites relinked against final objects: 111 reader checks and
  31 queued format jobs. Repeated checks are not additional coverage.
- Original synthetic ZipCrypto/AES-128/AES-256 fixtures: exact two-member reads
  and extraction, cache reuse/forget, wrong/correct retry, bounded failures,
  cancellation, late replies, source replacement, bounded cache, concurrent
  prompt cancellation and shutdown. Encrypted 7z rejected explicitly.
- Native engine probes additionally cover public upstream WinZip AES and
  encrypted 7z/RAR samples. Several unusual encrypted ZIP compression methods
  fail on this system engine. See ARCHIVES-0.19.0.md for exact scope.
- Actual installed UI: Simple/ZipCrypto/light, Commander/AES128/dark and
  Explorer/AES256/dark. Typed secrets masked; exact output; unchanged main frame;
  preview reuses the cache; no stale failed-transfer marker after publication.
- Wrong password followed by correct password, Escape/cancel, explicit forget,
  forget during a pending prompt, queue Cancel during a prompt, source replaced
  before answering, quit while preview waits, restart asking again. No target or
  staging survives cancelled extraction. No password in any saved Lister settings.
- While a native prompt waited: 100 main-window message queries, zero failures,
  p95 0.543 ms, maximum 0.833 ms. This measures native UI-loop response in KVM,
  not physical typing latency or general hardware performance.
- Actual installed 0.19.0 -> accepted 0.18.0 -> exact 0.19.0 round trip preserved
  settings, places, searches, archive preferences, fixture files, modes, BFS
  attribute and symlink. Post-roundtrip cancelled preview leaves data unchanged.

## Identity and recovery

HPKG SHA256:
`4c2b7cd1fea8269205bfabe33999aad7fb28078933de521984354f0547a9af45`

Final build and installed binary SHA256:
`3aef46e4a958224a5e401b49bd220d1cf494e20d2fd21a9fa2380328b1637712`

Private ZIP SHA256:
`4bedd8a202668bbb6d81b44f478c30b1a51eb5213ec4a69c909fba54b7f551a4`

Report: `/home/lobster/kambuzi/reports/listi-archive-passwords-20261115`.
`inputs-verified.log`, `capabilities.log`, `full-tests.log`,
`final-archive-tests.log`, `final-fixtures.log`, `final-ui-verified.log`,
`queue-cancel.log`, `roundtrip.log`, final screenshots and `shutdown.log`.
Early automation failures are retained: a nonexistent result member in a test,
process grep matching itself and querying an unsupported hidden text-view
specifier. Corrected checked scripts completed; these failures are not passes.
Owned VM 588148 was synced and stopped after acceptance. Retained predecessor
0.18.0 package supports data-preserving rollback after closing the app.

## Remaining scope

Encrypted 7z/RAR, ZIP encrypted headers/unusual compression combinations,
creation of encrypted archives, split volumes and archive editing. Password
cache is memory-only, not a guarantee against process inspection/OS paging.
No real user archives/passwords were used in the verification.
