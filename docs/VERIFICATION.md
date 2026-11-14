# Version 0.1.0 verification

14 November 2026. Native Haiku R1 beta6 hrev59866+79 x86_64 in our own QEMU/KVM
VM, GCC 13.3.0, haiku_devel installed. All coding and compilation ran in Haiku.

Passed:
- Clean native build and link, make test: 18 filesystem checks. Recursive copy is
  exercised on a worker thread, including symlinks, BFS attributes, skip/replace/
  abort, self/descendant rejection, cancellation without a partial output file,
  move/rename/mkdir/delete, merged move, changed-source retention and mount guard.
- Installed HPKG executable equals the built binary, SHA256:
  `44fc810eb3152ebbda9d436a48c2748c83456d777a8e1d5f4d0ceca0b529d2a7`.
- Actual installed UI: double-click /boot from /, home from /boot and FolderTest
  from a mounted ext4 fixture, with path readback. No root fallback.
- Actual UI copy of a small tree, overwrite prompt, paused copy retains the old
  destination and resumes successfully. Main-window scripting remains responsive
  while paused. File contents compared after completion.
- Actual UI move and permanent delete, with filesystem readback.
- Skip conflict and cancel a waiting queue checked during native development.
- Tab swaps source/destination; preference toggling, saving and restart retain
  the command line. Dark and light system themes, 1140x650 and 800x600 windows.
- Native screenshots of the installed main and operation windows in dark theme.

The first recursive worker copy exposed a stack overflow from large automatic
buffers; the buffers now live on the heap and the worker-thread regression passes.
A new conflicting copy remains resolvable after previous jobs finish.

Not verified: physical Haiku hardware, every filesystem, large-volume durability,
full disks, unplugging a disk mid-operation, power-loss recovery and every external
concurrent-modification race. The mounted ext4 fixture was used for navigation,
not a destructive-operation qualification. No FTP/SFTP or advanced features are
claimed. This is a first preview, not a general backup or recovery product.

Recovery: retain previous versioned HPKGs, reinstall the previous package.
Preferences live in ~/config/settings/Lister_settings. Package changes do not
reverse completed file operations. There is no undo/trash yet.

Packaging: repeated `make package` succeeds from the same source and executable.
Container checksums differ because Haiku records BFS access/creation timestamps;
bit-for-bit HPKG reproducibility is not claimed. The installed executable hash
is verified separately above.
