# Listi 0.17.0 private acceptance

2026-11-15. Runtime source: `ead0ad1`. No public push, release or feed change.
The later acceptance/test-runner commit does not change the qualified binary.

## Artifact and runtime

- HPKG SHA256: `54cbe102df3634368fdfaedb806ebeebec9842e79e0142e728343edd1f5bf8ea`
- Installed/built app SHA256: `0d0ce6547432a26a9f7bc2e8b859c260ec1a9c424066658e9d8841171a91908a`
- 159 source/build inputs verified against host manifest before native build.
- Fresh standalone copy of the retained Haiku disk, not an unidentified old VM.
  Haiku R1 beta6 hrev59866+79, x86_64, 2 KVM vCPUs, 3072 MiB RAM,
  1280x800 display, isolated user networking and port 23313.
- Package installed normally with pkgman; installed executable matched build.
  Same-version replacement was rejected by packagefs before activation; the
  final package was installed via the retained 0.16.3 baseline, then rechecked.

## Native checks

581 unique successful assertions, without counting repeated self-tests:

- Final core regression runner: 428 (preferences, history, selection/sorting,
  saved places/searches, recursive/live search, clipboard, drop, named entries,
  properties and injected persistence failures, attribute copy, delayed-job
  identity guards, deep trees, native icon sharing, FIFO rejection, small-stack
  reads, theme text and 55 file-operation self-tests).
- Open with: 25, including a native receiver, exact refs and stale identity.
- Native image translation/mailbox/cancellation: 6.
- Actual archive copy/conflict cancellation: 2.
- Archive backend suite on a 256 KiB thread stack: 111.
- Virtual operation fault injection: 9. These are simulated commit failures,
  not new live-server protocol qualification.

Run `make` then `python3 tests/run-native-audit.py` inside Haiku for the core
suite. Optional arguments select individual tests. Extra native image/archive
fixtures and Open-with receiver setup are recorded with the acceptance evidence.

Test corrections are retained in logs: the fault-injection test originally
lacked its linker wrap flags; the receiver initially lacked native app flags;
the first filter assertion expected the wrong status wording. A zero-byte BFS
attribute creation attempt was not a valid fixture. It was not counted as a
passing test; unexpected zero-length source attributes now fail explicitly
instead of being silently omitted. Ordinary typed and multi-chunk attributes pass.

Optional archive modules: missing LZ4 and Zstandard are reported explicitly.
LZ4 was installed and retested. Zstandard's retained CLI package required newer
system dependencies, so its installation was stopped without upgrading the OS.
The two Zstandard fixtures are excluded from the 111 successful checks. Their
prior qualification is not represented as a new test in this VM. Encryption,
split archives and additional live network server families remain unqualified.

## Installed UI

- Real View > Workspace menu, Explorer tree plus one list, no bottom action bank.
- Left/right-active transitions preserve selection, path and window size.
- Clipboard copy from Explorer works, including starting with right pane active.
- Explorer filter works; Simple suspends it; Explorer restores it.
- Tab switches tree/list focus; tree arrow navigation and list Backspace work.
- Sorted folder tree remains populated after restart; Explorer setting persists.
- Compact light/dark icon/detail views inspected as native screenshots.
- Context menu retains direct Selected files / This folder selector; hidden-panel
  transfers are disabled in Explorer.
- Replacing a selected file while Trash confirmation is open leaves both the
  replacement and original untouched and reports the error.
- New-folder question keeps its original destination through navigation and
  workspace switch. Running copy conflict survives switching; Escape preserves
  source and target bytes.

## Measured resource change

Same VM, app window size, 2000 synthetic 14-byte text files, Commander with both
folder trees and preview enabled, native Icons view. Three consecutive warm-cache
navigation runs per installed version. The tree split defaults changed from
50% to 24%; this is an application comparison, not an isolated microbenchmark.

| Metric | 0.16.3 | 0.17.0 |
| --- | ---: | ---: |
| Median complete directory load | 3.354545 s | 2.844500 s |
| Median total allocated area bytes | 220979200 | 149123072 |
| Median CPU during 100 window queries | 18043 us | 17757 us |
| Window-query p95 range | 0.382 to 0.480 ms | 0.384 to 0.517 ms |

About 15.2% shorter warm listing time and 32.5% lower total allocated area size
in this fixture. The area sum includes mapped areas; it is not RSS, private heap
or proof of equivalent physical RAM savings. No meaningful idle-CPU improvement
is claimed. Initial exploratory timing (10.61 s) is not used for the comparison.
Select-all on 2000 entries also completed with responsive window queries.
Physical hardware, cold-cache performance and arbitrary custom-icon mixes are
not established by these measurements.

## Rollback and preservation

Final 0.17.0 -> delivered 0.16.3 -> final 0.17.0 was actually installed.
The predecessor binary matched `1d86f48dfb2ac3dbd4d938f9ff45f0b81a878bbb0d630fa23753c117c95efda2`.
Explorer opens safely as Simple in 0.16.3. Settings, places, saved searches,
fixture contents, permissions, typed attribute listing and symlink target matched
before/after rollback launch. After an intentional Commander choice in the old
app, the upgrade preserved that choice and all compared data. Explorer still
works after the roundtrip. No stale settings or data backup was restored.

Evidence and private source/package retained under
`/home/lobster/kambuzi/reports/listi-explorer-audit-20261115/`.
See AUDIT-0.17.0.md for review scope, fixes and remaining architectural limits.
