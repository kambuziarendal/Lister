# Private Listi 0.16.0 acceptance

Source: `321f6d1` (main change `ce42d8e`), branch `work/smb-first`.
All 139 tracked inputs matched the Haiku source before final packaging.
No public push/release. Existing license and private development boundary retained.

- Binary: `50028dd6e4f5fa50218b424d52921e1d4c98b671857b8c1cc2abed280490f89e`
- HPKG: `12e71477b8adbee7c04ad4eaafada3cd935eac31be5518e0aab582ae33c33201`

227 native assertions passed: 55 file operations, 17 preferences, 119 generated
archive-matrix checks, 23 upstream-format checks, 2 optional-library retry checks,
2 staged archive extraction/cancel checks and 9 virtual-transfer fault checks.
Archive matrix includes 28 generated variants and four upstream fixtures. Some
negative cases intentionally run in both matrix invocations. Archive reads run
on a 256 KiB pthread stack, matching the constrained native worker environment.
Retry uses controlled dlopen fault injection, not a claim of uninstalling the OS
library. Actual LZ4 package installation was separately followed by successful
internal browsing in the same already-running app process.

Installed native controls verified:

- Minimal Simple chrome, editable address, top-level file view selection, two-pixel
  status/right inset, compact light/dark, dialog frame stability and restart.
- Suspended hidden filters in Simple and restored Commander filter text/results.
- Detail/icon rectangles, empty click, Ctrl extension, Escape restoration, stable
  re-entry, icon gaps, parent exclusion and selected-file clipboard transfer.
- Edge scrolling in both renderers and navigation cancelling an active rectangle.
- Item dragging still opens the ordinary Copy/Move/Cancel question.
- Internal archive folders and exact Commander extraction of LZ4, 7z and RAR
  fixtures, concurrent preview, conflict cancellation and preserved target bytes.
- The final exact package passed extraction, corrected conflict abort, minimal
  detail rectangle and launch checks after actual rollback/upgrade.

Bughunt caught two archive defects. A real 7z file required a seek callback.
Native preview/queued extraction exposed two 128 KiB automatic buffers overflowing
small worker stacks, although main-thread format tests passed. Buffers now use
heap storage and the regression matrix uses a constrained worker. Native Debugger
captured both failing worker stacks; final installed reproduction no longer
crashes. An archive-to-local conflict abort crossed differing cancellation types
and appeared as a generic error; it now reports cancellation with zero errors.
Known split filenames are refused rather than presenting an incomplete volume.

The original UI harness clicked a Commander tree instead of the file list and
parsed only the first line of a multiline question. These test errors were fixed;
failed logs are retained separately and not counted as passes.

Actual 0.16.0 -> delivered 0.15.0 -> final 0.16.0 package roundtrip preserved
preferences, places, searches, extracted contents, permissions and link text.
0.15.0 was launched against the same settings before upgrading again. No stale
user-data restore was used. Tests used an owned standalone Haiku R1 beta6 VM,
not Arne's physical machine or a production account.

Limits: archive creation/editing, passwords and split volumes remain unsupported.
Additional adapters for other families remain planned. Native Tracker clipboard
supports local entries only; extraction currently uses Commander's panel Copy,
not Simple clipboard extraction. See ARCHIVES-0.16.0.md for exact module scope.

Synthetic fixture generator and native regressions are in tests/. Upstream data:
https://github.com/libarchive/libarchive/tree/v3.7.9/libarchive/test
RAR/RAR5/CAB/LHA expected bytes were cross-checked with Haiku p7zip. No upstream
implementation was imported. Local evidence:
`/home/lobster/kambuzi/reports/listi-archives-20261115/` and
`/home/lobster/kambuzi/reports/listi-simple-20261114/`.
Private delivery receipt is recorded separately. MímisSendir remains paused.
