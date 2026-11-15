# Archive Cut and Move, private candidate 0.26.0

Cut/Paste and Move to other panel support a selection from one local writable,
single-volume archive into another writable archive, including internal folders.
Clipboard Cut also works within the same archive. Drag/drop remains Copy.
Use the normal keyboard shortcuts or file/context menus; confirm Move entries
before the worker starts. Cancelling the question or quitting before acceptance
releases the clipboard reservation without changing either archive.

## Data preservation

Cross-container moves snapshot the selected entries privately, commit and fsync
the destination archive, then read back every moved file and compare its complete
bytes with that snapshot. Empty directories and recursive member types are also
checked. Only then does a transactional source rebuild remove the selection.
The original source and verified destination fingerprints remain guarded through
source cleanup. Only completed source removal consumes the Cut clipboard.
A late completion cannot clear a newer clipboard selection.

This is deliberately **not** an atomic two-archive transaction. Cancellation,
process interruption or cleanup failure after destination publication can leave
both copies. Errors report incomplete cleanup; inspect both archives and select
again before retrying. There is no rollback to stale destination data and no
persistent automatic-resume journal. A filesystem flush error after a rename
requires inspection rather than assuming the rename was undone. Actual power
loss is not qualified.

Conflicting files require Replace or Skip/Abort. Skip/Abort does not publish a
partial destination or remove any source member. Directory merges can preserve
additional destination children. Same-archive moves remain one transactional
rename rebuild, never replacing an existing name.

## Scope and limits

ZIP, 7z, TAR, TAR+gzip, TAR+xz, TAR+bzip2, TAR+LZMA, TAR+lzip, CPIO and XAR are
covered by the native move matrix, in both directions. Encrypted ZIP and 7z
retain protection and use the existing memory-only password workflow.

Local filesystem-to-archive and archive-to-local Cut/Move remain refused; Copy
is available. RAR writing, split/nested/network-backed archives and automatic
external-editor save-back remain separate work. Special entries, comprehensive
archive metadata and protected/signed packages retain the limitations documented
in ARCHIVE-FOLDERS-0.25.0.md. Moves rebuild whole containers and need temporary
space for a source snapshot and rewritten containers. This is a private candidate,
not full archive filesystem parity or a public release.
