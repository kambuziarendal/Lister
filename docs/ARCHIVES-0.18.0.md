# Archive workflows in 0.18.0

Private candidate. Archive engines remain optional system packages. No external
archive application is launched, and Listi does not install packages itself.

## Create

Select local files/folders, then **Files > Archives > Create archive**, the
right-click **Archives** submenu, or **Command+Shift+P**. The selected roots are
captured with their node identities. Choose destination, a new name and format.
The runtime probes libarchive's actual writers: ZIP, 7z, tar.gz, tar.xz and tar
are offered only when their writer/filter can be opened successfully.

Favorites appear first, other supported writers under **More formats**.
Compression levels are saved separately for each format. **Use this format by
default** is explicit. **Ask for a format every time** defaults to enabled;
when disabled, the remembered format remains visible and **Change format** is
available before submitting. Nothing is started merely by choosing a format.
Settings are saved atomically in `Lister_archive_settings` when submitting.

## Extract

Select one local archive, or selected entries inside an archive, then
**Extract to folder** / **Command+Shift+E**. Choose a local destination and the
name of a new folder. This works in Simple, Explorer and Commander and does
not depend on a hidden panel. An archive may also be opened and browsed as
before. Existing Commander Copy from an archive remains available.

Both commands use the ordinary cancellable, pausable operation queue. Files
are written inside a private staging directory on the destination volume.
The completed archive or extraction folder is published using Haiku's native
no-replacement move. Existing files, directories and links are never replaced;
a collision is an error asking for a different name, not an overwrite prompt.
Navigating or changing workspaces cannot retarget an already captured job.

## Bounds and remaining work

- Local source and destination only. Network/nested archives need separate
  materialization/cache ownership before this workflow can support them.
- Creation stores ordinary file data, permissions and times, not BFS attributes
  or ACLs. Links and special files fail explicitly instead of being followed or
  silently skipped. Extraction retains the existing archive reader's safe-name,
  link and metadata rules. Portable archive formats are not a BFS backup format.
- Creation is bounded to 64 directory levels and 200,000 entries. Source node,
  size, mode and nanosecond modification/change-time checks reject observable
  changes while writing. They are not filesystem snapshots; hostile concurrent
  ancestor changes remain a path-based API limitation.
- No update/append, overwrite/merge, password or multipart support in this step.
  No RAR writer is advertised. The earlier read matrix still applies.
- Extraction currently reuses the existing archive copy engine, which may
  reopen/rescan compressed input for each selected entry. A single-pass bulk
  extractor is a subsequent performance improvement for large archives.
- Compression settings can change file size, elapsed time and memory usage.
  No fixed compression ratio or cross-machine performance is promised.

The UI uses existing BGroupView layouts rather than replacing their owned
layouts. The preliminary layout failure is retained in the local test report;
only the corrected installed candidate qualifies for delivery.
