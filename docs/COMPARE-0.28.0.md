# Folder comparison and same-type selection, private candidate 0.28.0

## Compare a copy with its source

Open the source folder in one panel and the copied folder in the other, then
choose **Listi > Compare folders...**. The two current paths are captured in a
separate window. You may edit the paths or use **Choose...** for local folders.
No file operation is started by comparison.

- **Compare names and sizes** recursively matches relative names and entry
  types and checks file byte counts. Equal size is explicitly **content
  unchecked**, never proof of equal content.
- **Compare with SHA-256** reads equal-sized files on both sides and compares
  streaming SHA-256 digests. Same-sized corrupted files are detected. Files
  already known to differ in size need no checksum to establish a difference.
- Results distinguish only-left, only-right, different types, different sizes,
  different content, matching files and unverified/error entries. Select a row
  to see paths, full digests and error details. **Show matching entries** exposes
  matches; by default the list concentrates on differences.

The comparison includes nested and empty folders and dotfiles, independently of
panel filters. Listi's internal archive staging directories are excluded by the
shared provider. File names are case-sensitive. Local symbolic links are compared
by their literal target, without following them. Provider links that cannot expose
that target and special files are explicitly unverified. No files are changed,
removed, repaired or synchronized. Permissions, ownership, BFS attributes and
other metadata are not compared.

Names and identities are rechecked after scanning. Changed/unreadable trees,
limits or cancellation are explicitly incomplete. This is a point-in-time read,
not a filesystem snapshot lock: subsequent edits can invalidate any result.
Remote identity checks are limited to the metadata the server exposes. Keep both
folders idle for a meaningful copy check. SHA-256 needs to read both copies and
may download complete remote files. Hashes are not cached across comparisons.

Supported providers use the existing folder adapters: local files, archive
folders and connected SMB/FTP/SFTP/FTPS locations. Native qualification covers
local folders, ten archive writers and synthetic SMB/FTP/SFTP services. FTPS and
external NAS models have not been qualified for this increment. Archive files in
a local folder are ordinary files; navigate inside an archive to compare members.

Scanning stops with an incomplete result at 100,000 entries per side or 256
levels. At most 5,000 matching/difference rows are displayed at once; the window
explicitly reports truncation, while totals still include all scanned entries.

## Select files of the clicked type

Right-click a file and choose **Select same file type (.pdf)**, or use
**Select > Same file type** with a focused file. This replaces the selection in
that panel with visible ordinary files with the same case-insensitive extension.
The panel's current filter applies. Directories, links and the parent row are
excluded. No file operation occurs until a subsequent Copy, Move or other action.

This is extension-based, not MIME/content sniffing. `.jpg` and `.jpeg` remain
separate; compound TAR suffixes such as `.tar.gz` are distinct from `.gz`.
Extensionless files, bare dotfiles and trailing dots do not define a type and
leave selection unchanged. No hidden or recursively nested entries are silently
selected. The clicked file, including within a mixed selection, determines type.
