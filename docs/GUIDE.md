# Workflow guide

This guide describes the 0.31.1 alpha. Test with disposable copies and keep
independent backups: file operations can fail or complete only partially.

## Panels and jobs

The highlighted panel is the source. Direct Copy or Move uses the other panel
as the destination; clipboard Paste uses the active panel. Accepted jobs retain
their selected paths and target even if you navigate or switch workspace.

The operation queue offers progress, pause, cancellation and conflict choices.
Cancellation does not undo completed work. A partially copied directory may
remain. After a failed or partial move, inspect both source and destination.
Concurrent external edits are not prevented by a filesystem snapshot lock.

Trash and permanent deletion are separate actions. Trash restore does not
overwrite an existing file and requires the original parent folder to exist.
Reinstalling an older application package does not undo file operations.

## Navigation, selection and native tools

Panels offer details, icon and thumbnail views, history, saved places and optional
folder trees. Switching workspace retains the panel objects rather than
restarting file work. **Select same file type** replaces the active panel's
selection with visible ordinary files sharing an extension; it is not MIME or
content detection. `.jpg` and `.jpeg` remain distinct.

Local drag-and-drop asks Copy here, Move here or Cancel for the receiving
panel's current folder, not the row under the pointer. Duplicate and symbolic
link creation are local, single-entry actions; links can be relative or absolute.
Archive and network drops remain copy-only.

**Files > Properties** pins one local entry and exposes timestamps, numeric
ownership, ordinary octal permissions and supported typed BFS attributes. Saves
are explicit. System namespaces, unsupported binary data and metadata beyond the
editor's limits remain read-only; this is not a MIME editor or bulk attribute tool.
Detected concurrent changes block editing, but checks cannot lock out all other
programs or guarantee power-loss atomicity.

![Local Properties with typed attributes and explicit save controls](screenshots/listi-properties-0.10.0.png)

*Properties, captured in 0.10.0 with synthetic data; cropped to the native window.*

**Files > Open with...** offers installed native applications for one local
regular file. **Open once** does not change associations. Remote/archive members
must be copied locally for that dialog; ordinary archive Edit has its own
save-back workflow described below.

## Filtering and search

A panel's name filter is a local view filter, not recursive search or shell
wildcard expansion. Hidden rows are deselected; Select All and file operations
act on visible selections. New navigation clears the filter; refreshing the same
folder preserves it.

**Listi > Find files** combines a name fragment or BFS formula with extension,
size and date criteria. Saved searches store criteria, not copies of results.
Advanced BFS queries can subscribe to live changes. Search results refer to
original files: deleting a result's file deletes the original.

Search is currently limited to one persistent local volume, 5,000 displayed
hits and 250,000 examined entries, with explicit incomplete/error outcomes.
Content, archive-internal, remote and multi-volume search are not supported.
Date filters apply to current files, not historical snapshots.

![Search with extension, size and date criteria](screenshots/listi-search-filters-0.30.0.png)

*Find files, captured in 0.30.0 with synthetic fixtures.*

## Folder comparison

**Listi > Compare folders...** compares relative names and types recursively.
Size mode does not verify equal content; SHA-256 mode reads content on both
sides. Comparison is read-only: it does not repair or synchronise anything.
Permissions, ownership and BFS attributes are not compared.

Keep both folders idle for meaningful results. Comparison is not a snapshot;
remote checks depend on the server's metadata, and hashing can download entire
files. Limits are 100,000 entries per side, 256 levels and 5,000 displayed rows;
truncation and incomplete scans are reported.

![SHA-256 comparison distinguishing equal-size files with different contents](screenshots/listi-folder-compare-0.28.0.png)

*Comparison, captured in 0.28.0; the selected files have equal size but different content.*

## Archives

Available formats depend on optional system libraries and engines. Listi can
browse many formats, but reading, creation and editing are different
capabilities. A successful listing does not establish member-data integrity.

- Supported local single-volume writers include ZIP, 7z, TAR, TAR with gzip,
  xz, bzip2, LZMA or lzip, CPIO and XAR. Zstandard/LZ4 wrappers are gated by the
  installed engine rather than universally promised. Reading support is broader
  than writing support.
- Writable archive folders support Copy/Paste, new folders, rename, entry removal
  and supported moves. Removing an archive member is not local Trash. Editing
  rebuilds the container and needs scratch space for extracted content and the
  new archive; layout, comments and format-specific metadata may change.
- Supported encrypted ZIP, 7z and RAR inputs use a masked password flow.
  Passwords are kept in process memory rather than saved in settings.
- Selected local multipart 7z and RAR naming families are supported for reading.
  All parts must remain together and unchanged. This does not imply split ZIP,
  split creation or repair support.
- Supported local archive-to-archive and archive/local moves verify destination
  contents before source cleanup. They are not an atomic two-location
  transaction and have no automatic crash-resume journal.
- Archive/local moves refuse unsupported metadata and file types rather than
  silently discarding them, including BFS attributes, source symlinks, special
  files and some hard-link cases. Use Copy to retain originals.
- RAR writing and mutation of remote, split or nested archives are not supported.

A failed or cancelled merge may leave destination copies, and cleanup may stop
after some verified entries have moved. Inspect both locations before retrying.

### Editing an archive member

Select a regular member in a supported writable local archive and choose **Edit**.
Listi opens a private working copy in the configured external editor. While the
edit window remains open, stable editor saves can be written back through a
verified archive rebuild. **Save now** retries a paused save; external archive
changes stop automatic saving rather than being silently overwritten.

![Archive edit window after verified save-back, with retained working copy](screenshots/listi-archive-saveback-0.27.0.png)

*Archive save-back, captured in 0.27.0 with a synthetic text file.*

Working copies and recovery records remain under
`~/config/settings/Listi_archive_edits/edit-*/`. **Decrypted contents are not
encrypted at rest.** Keeping a copy, closing or cancelling does not remove it.
Use **Files > Archives > Resume archive edits** to resume a retained session.
After closing edit windows and preserving needed work, remove obsolete copies
manually. This is not write-back from arbitrary Open or Open With applications.

## Network locations

SMB, SFTP, FTP and FTPS use the panel and job interface. Capabilities and recovery
behaviour vary by protocol and server. Comparison or previews can cause network
reads, including downloading complete files. Keep credentials out of shared URLs,
screenshots and bug reports.

## Settings and older versions

The 0.31.1 naming update retains existing workspace values and settings aliases.
Saved search criteria written since 0.30 use a newer store format: 0.29 and older
cannot read it. Those versions preserve the file and disable saved-search changes;
upgrading restores access. Do not replace the store with a stale backup merely
to make an older version read it.
