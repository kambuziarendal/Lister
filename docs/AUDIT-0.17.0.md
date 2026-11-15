# 0.17.0 code audit

Scope: the complete application source tree, native build/package definitions,
and existing test coverage. Manual review covered event dispatch and lifetime,
panels/renderers/tree, workers/cancellation, operation/clipboard/drop/Trash,
properties/Open with, settings/search persistence, archive/network adapters and
preview decoding. This is a code review with regression verification, not proof
that no bugs remain or certification of third-party libraries.

## Corrected findings

- Delayed native Trash/Delete/Rename and new-folder prompts retained paths but
  not identities. A replaced entry or parent could be acted on. Local jobs now
  freeze identities before prompting/queueing, encode them with the question and
  recheck in the worker. Source-only jobs do not depend on the other pane.
- Preference saves used a predictable truncating .tmp path, unlike saved places
  and searches. They now use the same bounded, exclusive temporary-file writer
  with flush and atomic rename. Loading is bounded and rejects special files.
- Archive detection could block opening a FIFO. Nonblocking open plus regular
  file validation rejects it; text preview now reuses the guarded file reader.
  Image translators receive a pinned regular-file stream, not a second path open.
- Local recursive copy/delete snapshots and progress pre-counting had no depth
  bounds. Copy/deletion stop at depth 64; deletion snapshots also cap 200000
  entries before any removal. Mutation limits are reported. Progress pre-counting
  stops descending at depth 64 and remains an estimate.
- Attribute-read/enumeration failures now fail the staged copy rather than
  silently dropping metadata. Zero-length attributes, if supplied by another
  filesystem, fail explicitly: native BFS zero-byte writes do not create them.
  The attempted empty-attribute fixture was rejected, not counted as coverage.
- Local provider reads still put 128 KiB on worker stacks. Buffer ownership is
  now on the heap, consistent with the previous archive-stack fix.
- Superseded directory scan messages are rejected before any filesystem work.
- Folder-tree insertions reversed the sorted children. Reverse insertion now
  preserves ascending folder order.

## Optimizations

- A bounded weak icon index shares byte-identical native icons, retaining custom
  icon pixels and allowing all listing-owned allocations to be freed. It never
  caches by filename extension or substitutes an icon theme.
- Icon painting resolves focused entry once per paint instead of once per cell.
- Thumbnail completion and file progress invalidate the affected row/cell only.
  Progress for files outside a pane no longer repaints that pane.
- Disabled preview does not build an unused full selection vector.
- Attribute-copy buffer is reused across attributes instead of per attribute.

## Retained limitations / follow-ups

Remote metadata is not an atomic snapshot; same-size edits on servers without
reliable timestamps cannot be detected. Hostile concurrent ancestor replacement
is not fully prevented by path-based local recursion; a descriptor-relative I/O
redesign needs its own qualification. SMB calls have native timeouts, not instant
interruptibility, and the global SMB lock serializes sessions. Archive browsing
reopens/reindexes archives and recursive transfer may reread containers; a
validated archive index/streaming extraction is a later performance step.
Live BFS reconciliation intentionally checks periodically to cover missed
ancestor moves. Do not replace it with unverified event-only behavior.

Physical hardware, new FTPS/server families and all external archive variants
are not qualified by this code audit. Private status and prior feature limits
remain unchanged. Exact candidate test results and measurements are recorded in
the acceptance note after verification.
