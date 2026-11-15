# Listi 0.20.0 private candidate

Runtime/source: `28ee86d2774920124839d33e5c5088a845f29a91`.
Haiku hrev59866+79 x86_64; optional p7zip 17.05-2 and libarchive 3.7.9.
Standalone disposable KVM image copied from stopped accepted 0.19.0.
No public release, feed change or repository push.

## Verified

- All tracked inputs matched the native build tree after the runtime commit.
  Full native audit suite passed. New native engine tests: 47 checks on an
  actual 256 KiB worker stack. Existing password checks: 39. Format regression:
  111 reader checks and 31 queued fixture jobs. Repeated runs are not extra coverage.
- Encrypted 7z data and headers with solid nested/Unicode content; RAR4 data and
  headers; RAR5 headers and mixed-password members. Exact decrypted contents,
  whole-archive publication, wrong/correct retry, bounded wrong attempts without
  leaked partial bytes, cancel, forget, source replacement, symlink rejection,
  truncated archives, renamed archive detection and unsafe paths.
- Fixed a plugin teardown crash found by the initial probe: successful native
  modules are process-lifetime objects, avoiding calls into already finalized
  plugin globals. Final native test executables and installed app exited normally.
- Actual installed UI: Simple/7z/light, Commander/RAR4/dark,
  Explorer/RAR5/dark. Masked password, wrong/correct input, exact extracted
  content, unchanged window frame and cached RAR5 text preview.
- Cancelled extraction leaves no published target or staging. Queue Cancel
  interrupts header-password waiting. Replaced source aborts and closes the
  prompt automatically. App exits while header listing waits; restart asks again.
  No synthetic secret in saved Listi settings.
- Optional package absent: explicit install-p7zip error and no output.
  Reinstalled the exact retained p7zip package while Listi remained open;
  the same process then extracted encrypted 7z successfully.
- Password wait: 100 native main-window message queries, zero failures,
  p95 0.429 ms, max 0.909 ms. VM UI-loop latency, not physical input latency.
- Installed 0.20.0 -> accepted 0.19.0 -> exact 0.20.0 round trip preserved
  settings, places, searches, archive options, fixture content, modes, links and
  BFS attributes. Post-upgrade encrypted-header prompt cancelled without data changes.

## Identity and evidence

HPKG SHA256:
`a5ae3c3f23b753dbc3b9da216f4f9ae4f4403d35d159112c646fe367c22e427e`

Built and installed binary SHA256:
`bfcc96253606d779a39b435259ea23d438292517e1d39ae106692ab5390ed5fd`

Private ZIP SHA256:
`b9fb66a8378559683946d4689e1ee5f249e8beebf89432de576346b34a282738`

Report: `/home/lobster/kambuzi/reports/listi-7z-rar-20261115`.
`full-tests-build.log`, `fixture-tests.log`, `inputs-verified.log`,
`final-install.log`, `final-ui.log`, `ui-faults.log`, `optional-ui.log`,
`roundtrip.log`, screenshots, `final-processes.log` and `shutdown.log`.

Failed preliminary runs remain recorded, not counted as passes. The first
Unicode fixture was created under POSIX locale and contained mojibake; regenerated
with UTF-8. Haiku's 7z CLI mislabeled a synthetic symlink; a Linux-generated
symlink fixture replaced that oracle. In the UI source-replacement test the
job guard closed the prompt before the script typed Enter, which opened the
selected archive separately. Native stacks confirmed the queue was idle and
the independent launcher was awaiting a password. Checked continuation verified
no publication and normal prompt closure. Queue-Cancel coordinates were corrected
for the expanded history panel before the passing round trip.

Only interface headers/notices are bundled. Native decoders are optional system
packages. See ARCHIVES-0.20.0.md for temporary-disk requirements, plugin ABI,
secret-copy limitations, solid-archive performance and remaining scope.
Split volumes, encrypted archive creation and editing are not implemented.
