# Safe ZIP and 7z editing (0.23.0)

Listi can add or replace local files and folders in an existing ZIP or 7z
archive, and remove selected archive entries. The actions are available under
**Files > Archives** and in the archive context menu. When an archive folder is
open, additions are placed in that folder.

Editing is a transactional rebuild, not an in-place mutation:

- Listi freezes the archive, its parent folder and every added source before
  confirmation;
- the complete archive is extracted into a private folder beside the original;
- requested removals and additions are applied only to that private tree;
- a new archive is written and flushed before one atomic rename publishes it;
- cancel, read/write failure, a changed source, a replaced archive or a changed
  destination folder leaves the original archive untouched;
- existing names require an explicit Replace or Skip decision;
- archive permissions and BFS attributes are copied to the replacement.

Password-protected ZIP and 7z archives retain encryption. Editing a protected
archive requires its password through the existing masked, memory-only flow.
7z name encryption is retained. Passwords are not added to job snapshots,
settings, paths or command lines.

The rebuild needs temporary free space for the extracted contents and the new
archive. This version edits only local, single-volume ZIP and 7z archives.
Split archives, remote archives, nested archives, RAR and other formats must be
extracted and recreated. A rebuild may change compression layout and byte-level
archive representation even when member contents are unchanged. Archive
comments and format-specific data that Listi does not expose are not promised
to survive.
