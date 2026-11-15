# Archive folder actions, private candidate 0.25.0

Ordinary panel Copy, Copy/Paste and drag-and-drop into an open writable archive
now prepare the same guarded, transactional archive edit as Add or replace files.
Local files and entries from local archives can be added. Copying within one
archive and between writable local archives uses the same flow. Originals are
kept. File clipboard Copy also works from an archive into a local folder.

New folder, Rename and Delete work inside writable archives. Panel Move supports
moving entries between directories within the same archive. Rename/move never
overwrite an existing destination. Deleting archive members is confirmed as an
archive removal, not falsely described as recoverable local Trash.

The writer detects the existing container and compression wrapper from its bytes.
ZIP, 7z, uncompressed TAR, TAR+gzip, TAR+xz, TAR+bzip2, TAR+LZMA, TAR+lzip,
CPIO (newc writer) and XAR passed native folder-operation qualification. TAR+zstd
and TAR+LZ4 adapters are capability-gated; the current Haiku libarchive build does
not offer them as qualified writers. Optional compression engines remain system
packages, not bundled executables.

ZIP aliases such as ordinary CBZ archives use the same ZIP container writer.
Changing extension is not used to pretend that an unsupported format is writable.
Encrypted ZIP and 7z retain password protection through the existing in-memory
password broker. RAR, split archives and other reader-only types remain read-only
with an explicit writer/capability error. Their source bytes are not converted.

## Not full filesystem parity

- Cross-container/local Move and archive clipboard Cut are not implemented.
  Use Copy; originals are retained. Same-archive panel Move is supported.
- Opening a member for editing in another application does not save it back into
  the archive. Nested archives and network-backed containers are not writable.
- Links and special archive entries are refused by the rebuilding path, not
  followed. Signed packages, bootable images and formats requiring preserved
  special metadata are not qualified for rewriting.
- Rebuilding preserves ordinary member bytes, exposed mode/mtime and container
  permissions/BFS attributes. Archive comments, ACLs, ownership, special mode
  bits and format-specific extra data are not comprehensively preserved. Do not
  use this candidate as a general-purpose backup/archive repair tool.
- CPIO is written as newc and TAR as restricted pax. Exact format subtype,
  compression layout and byte-for-byte container representation can change.
- A whole-archive rebuild uses temporary storage for unpacked contents and the
  new container. One final rename publishes it. Cancellation and pre-commit
  errors leave the original container intact. Read-only or ambiguous inputs
  fail without publishing partial output.

The user's requirement is full archive-as-folder behavior wherever the format
can represent it. This increment does not close that requirement.
