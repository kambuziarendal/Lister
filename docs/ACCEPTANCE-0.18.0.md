# 0.18.0 private acceptance

Runtime source: `4e0051d`, branch `work/smb-first`. No public push or release.
Native environment: Haiku hrev59866+79, x86_64, own standalone KVM disk.
The retained 0.17.0 disk was copied, not modified or used as a writable backing.

- Package SHA256: `9353bf52ca43d43e4f4634d9f907b00662f72bcba42ae358d7781a3489524ecf`.
- Installed and built binary SHA256:
  `1260ea04d459d8f720e1260352060c59c7df472f0bb20b994cd51b60f6b35277`.
- 56 new archive-job/settings checks on a 256 KiB native worker stack.
  Five writers, levels 0/6/9, exact bytes, Unicode, empty entries, selected
  members, changed sources/destinations, late targets, cancellation and cleanup.
- 31 queued archive-fixture jobs, including negative traversal, links,
  corruption, encryption and multipart cases. 111 existing reader checks pass.
  Zstandard fixtures remain excluded in this guest because its optional package
  would require an unrelated system-library upgrade. No new Zstandard claim.
- Existing native core suite passes. Final changes to archive publication were
  followed by the archive-job suite, snapshot suite, 55 built-in checks and
  relinked archive-fixture jobs. Repeated checks are not additional coverage.
- Installed creation/extraction in Commander, Simple and Explorer preserves
  exact root/nested bytes and the main-window frame. Right-click entry, shortcut,
  keyboard format selection, favorites, default, explicit ask-every-time,
  native folder-picker accept/cancel, source replacement and existing-name
  refusal were exercised. Real light/dark palettes were inspected.
- A 64 MiB incompressible-source job was paused and cancelled via actual UI.
  Resume label confirmed pause; source SHA256 unchanged, no published archive
  and no staging leftovers after cancellation. The native window-message probe
  had 100/100 replies, p95 0.642 ms, maximum 1.546 ms while paused. This is a
  VM UI-loop measurement, not a physical input-latency or memory-use guarantee.
- Actual 0.18.0 -> accepted 0.17.0 -> 0.18.0 package roundtrip passes. Settings,
  places, searches, archive preferences, file bytes/modes, representative BFS
  attributes and a symbolic link are preserved. Archive choices reload after
  restart; opening/cancelling options does not change persisted data.

Preliminary failures retained locally: invalid BGroupView layout ownership,
missing-settings sentinel accidentally saved as a real settings message, and
an automation attempt that clicked old progress-button coordinates. The first
 two were fixed before final package qualification. The last was not counted as
pause coverage; its replacement asserts the actual Resume label before testing
cancellation. Source/binary manifests identify the final candidate independently.

Remaining scope is explicit in ARCHIVES-0.18.0.md: new local archives and new
extraction folders only, no overwrite/merge/update, passwords, split archives,
link packing or BFS-backup guarantee. Bulk extraction still uses the existing
multi-pass reader; a single-pass path is a future optimization.

Local evidence: `/home/lobster/kambuzi/reports/listi-archive-workflows-20261115`.
