# Private 0.29.0 acceptance

Verified on Haiku x86_64 hrev59866+79 in a task-owned overlay backed by the stopped
0.28.0 overlay and immutable standalone 0.26.0 disk. No public release or upload.

- 1238 distinct native checks: 34 suites plus 55 app self-tests. 115 new checks
  exercise local/archive moves and publication/deletion fault injection. The full
  run stopped on one obsolete test expecting local Cut refusal; it was updated to
  require the leased move contract and no deletion before acceptance. That suite
  and all remaining suites then passed. Prior failed logs are retained, not counted.
- All ten available writable formats tested in both directions, including nested
  content and empty directories, on a 256 KiB worker stack and decoded job snapshot.
  Encrypted ZIP/7z, password cancellation, source/destination races, skips/aborts,
  same-size corruption, ENOSPC/EIO publication and partial local cleanup exercised.
- Real BFS does not support hard links in this fixture. The refusal check exists,
  but that branch is not qualified by creating hard links on this filesystem.
- Installed HPKG binary matched the built binary before GUI acceptance. Actual
  keyboard Cut/Paste and Cancel/Escape/retry passed both directions. Actual panel
  Move passed both directions for a nested folder and an empty subdirectory.
  Independent Python ZIP reading and local SHA-256/content checks verified outputs
  and retained unrelated entries. Unsupported BFS attributes retained source bytes
  and attribute value, without changing the archive.
- Readable confirmation at normal/light and 1000-pixel-wide dark layout checked.
  One harness attempt used bare M (type-ahead), not the Move command; its unchanged
  screen is not confirmation evidence. The actual Move button was then exercised.
- An early test harness incorrectly accessed an unlocked BClipboard and stopped in
  the debugger. The harness now locks and copies the message. The owned failed test
  and alert were terminated before installed app qualification.
- Directory flushing is limited to published entries and their immediate parents;
  unrelated ancestors may be packagefs and must not be fsynced as writable targets.

Backend and GUI limits are in ARCHIVE-LOCAL-MOVE-0.29.0.md. This does not qualify
power loss, large-tree performance, physical hardware, every metadata type or
full archive/filesystem parity. Snapshot browsing remains low-priority roadmap only.

Evidence: /home/lobster/kambuzi/reports/listi-archive-local-move-20261115.
Binary SHA-256: 7d1b422cbcca3bfc3c5b07bc21ec9c7bff4140fc105a26753ab7a0e4089e14fb.
