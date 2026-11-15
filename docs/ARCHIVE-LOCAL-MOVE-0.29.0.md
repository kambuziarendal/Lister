# Archive/local moves, private candidate 0.29.0

Cut/Paste and Move to the other panel now support local folders to writable local
archives, and archive members to local folders. Both directions use the existing
confirmation, queue and clipboard lease. Drag/drop remains Copy for archives.
Archive-to-archive and within-archive moves retain the 0.26 behavior.

## Preservation contract

The worker snapshots the complete selection privately, publishes the destination,
flushes it, then reads back every moved file and compares complete bytes with the
snapshot. Directory types and empty directories are checked. Source removal starts
only after the whole selection passes. A different-size or same-size corrupt copy,
Skip, cancellation, read/write failure or detectable concurrent change prevents
source cleanup. Local tree identity, size, nanosecond modification/change times,
parent identity and resolved paths are guarded; archive fingerprints remain frozen
through source rebuild. Cleanup visits only recorded names, never new children.

A local destination merge may leave partial copies if cancelled or skipped. Local
source cleanup can also stop after some roots or children were removed: these have
verified destination copies. Only completely removed roots count as moved and
consume Cut clipboard entries. Failures retain remaining sources and report that
both locations require inspection. There is no rollback to stale destination data,
no automatic crash-resume journal, and no atomic two-location transaction. Actual
power loss and adversarial simultaneous filesystem changes are not qualified.

## Boundaries

Local source symlinks, special files, mounted subtrees, hard-linked regular files,
special permission bits and filesystem attributes are refused rather than silently
lost by the archive writer. Use Copy to retain their originals. This includes BFS
attributes such as custom metadata or MIME attributes; it is a deliberate remaining
limit, not complete folder/archive parity. A source selection cannot contain its
target archive, overlap itself, or overwrite its own source archive on extraction.

Directories may merge and retain extra destination entries. File replacement needs
the existing Replace choice. Source and target must be local, single-volume writable
archives or ordinary local folders. Encrypted ZIP/7z use the existing memory-only
password workflow. RAR writing, network-backed/split/nested archive mutation and
full metadata preservation remain out of scope. Moves need scratch space for the
selection and archive rebuild. Normal Copy behavior is unchanged.

Historical snapshot browsing is recorded separately in ROADMAP.md, at low priority.
It is not implemented by this candidate and is unavailable on native Haiku BFS.
