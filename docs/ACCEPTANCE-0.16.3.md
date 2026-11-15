# Listi 0.16.3 final private acceptance

Compiled source: f5b8895. No public push or release.
Binary SHA256: 1d86f48dfb2ac3dbd4d938f9ff45f0b81a878bbb0d630fa23753c117c95efda2
HPKG SHA256: 2c12f25420cb60eab928604d9cfc789a4d640336161404ca896278b452768e63
150 committed native inputs matched; installed bytes matched the build.

The direct Selected files / This folder selector supersedes the undelivered
intermediate package. Native installed checks cover repeated one-click switches,
Escape, multiselection retention, correct folder Properties, empty-space return
to selected-file actions, selected directories, disabled file context with no
selection, sorting after switching, Simple mode, and stable bottom-right anchoring.
55 native operation checks passed on this final installed binary. The unchanged
model also passed 10 sorting, 9 summary and 6 selection regression checks.

The unchanged border/scrollbar/sorting paths were qualified before the selector
extension: eight dark/light, Details/Icons, active-left/right incremental-resize
pixel comparisons; twelve rendered sort cases retaining selection and opposite
pane; native header to icon handoff; actual window and column width scrollbar
transitions, Simple zero-scroll state and 2000-entry vertical overflow.

Actual final 0.16.3 -> retained 0.16.2 -> final 0.16.3 roundtrip passed. Settings,
places, searches, fixture bytes, permissions and symlink targets were unchanged.
Selection totals and folder Properties passed after restart and roundtrip.

Evidence: /home/lobster/kambuzi/reports/listi-border-trails-20261115.
Failed harness attempts are retained, not counted as passes: ignored splitter
motion, popup timing/OCR probes, an initial empty-space test that hit an actual
row, and a lost Quit acknowledgement. After the lost reply the app had exited,
with no new crash report; qualification resumed from verified process state.
Sort is session-local per pane. Physical-user hardware was not tested here.
This acceptance-only document is not part of the compiled package.
