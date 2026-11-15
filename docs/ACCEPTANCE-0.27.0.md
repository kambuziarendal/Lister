# Listi 0.27.0 private candidate acceptance

2026-11-15. Evidence: /home/lobster/kambuzi/reports/listi-archive-editor-20261115.
Task-owned Haiku R1beta6 x86_64 KVM overlay, based on the retained immutable
0.26.0 test disk. No upload, push, Depot publication or paid CI.

- Full native runner: 1078 PASS, zero FAIL, 30 suites and app self-test.
  New editor coverage: 97 checks over ten writers, encryption, atomic editor
  replacement, changes during save, stale archives, retained recovery copies
  and competing transactions. Ten fault checks cover corrupt output before
  publication, failed publication and failed recovery metadata after publication.
- Installed UI testing found and fixed a window-lock violation at editor-window
  startup. The final window-only correction passed the 55-check installed app
  self-test and the actual UI scenarios below. Backend sources are unchanged
  from the full suite. Failed attempts remain in the evidence directory.
- Actual mouse/keyboard Edit launched StyledEdit; Alt+A, typing and Alt+S updated
  the ZIP member automatically. Python zipfile independently verified changed
  content and untouched siblings. External archive replacement stopped saving
  and retained local edits. Start from current archive created a new edit,
  preserved the conflicting old copy and saved successfully. After application
  restart, Files > Archives > Resume archive edits reopened the correct copy.
- Actual package 0.27.0 -> 0.26.0 -> 0.27.0 round trip preserved all eight sampled
  settings/archive/recovery files. Prior binary hash matched the retained 0.26.0
  baseline; final installed binary equals the built candidate. SHA-256:
  bc2bee8940e737aac2983fa2988deaf923516c384e5db10e60241e424cbaae41.
- git diff --check passed. Native compiled inputs were copied without substituting
  host implementations. Test VM remains owned by the ongoing comparison increment.

Scope, retained decrypted-copy behavior and recovery: ARCHIVE-EDITOR-0.27.0.md.
No physical-machine or power-loss qualification. Application rollback never means
restoring stale user archives. Folder comparison and same-type selection are the
next user-requested increment, not included in this checkpoint.
