# Native stability diagnostic probes

These historical diagnostic reproductions target the Listi 0.30.0 baseline
(`65178b1`). They are **not** normal passing regression tests and are not
included in `run-native-audit.py`. Their original exit meanings are retained.
Use the current regressions listed below to test the fixes; see the
[testing guide](../../docs/VERIFICATION.md) for the main test entry points.

Run only in an owned disposable Haiku VM with app_server available. Each creates
its own temporary fixture. A successful normal run removes its fixture; on early
failure the uniquely named probe directory may remain for inspection.

Compile with g++ -std=c++17 -O2 -Wno-multichar, -Isrc, private/interface and
private/shared system headers, and the actual native objects from
src/objects.x86_64-cc13-release except App.o, SelfTest.o, VirtualTest.o and names
starting with _. Link -lbe -lshared -ltranslation -lcolumnlistview -ltracker.

- scanner-delivery.cpp: blocks a small-capacity receiver and checks normal versus
  delayed delivery. Exit 0 means silent loss was reproduced; exit 1 means it was
  NOT reproduced. Its exit status is a diagnostic outcome, not a normal
  regression pass/fail result or proof of a natural timeout.
- scanner-send-fault.cpp: additionally link
  `-Wl,--wrap=_ZNK10BMessenger11SendMessageEP8BMessageP8BHandlerl`. Injects one
  B_TIMED_OUT response for a directory-row batch. Exit 0 means the defect was
  reproduced: missing rows followed by completion without an error. This is
  deterministic delivery-failure coverage, not proof of typical UI congestion.
- archive-move-scaling.cpp: additionally link `-Wl,--wrap=lstat`. Executes actual
  guarded ZIP-to-local moves for 64 and 128 files of 1 KiB each. Counts linked
  lstat calls and wall time, then verifies every destination payload and source
  removal. Exit 0 means these integrity checks passed, not acceptable speed.
  One sample per size, 90-second budget for the entire probe; no general
  throughput or precise asymptotic-complexity claim.

Keep diagnostic outcomes distinct from ordinary regression assertions.
Instrumented timings are not general UI or throughput benchmarks; performance
changes must preserve the file-integrity checks.

The 0.31.0 fixes are exercised by `tests/directory-delivery.cpp`,
`tests/operation-queue.cpp`, `tests/archive-read-scope.cpp` and
`tests/archive-move-scaling.cpp` in the ordinary native runner. The historical
probes above keep their original diagnostic exit meanings.
