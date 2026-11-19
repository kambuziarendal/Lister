# Changelog

## 0.36.1 (unreleased)

- Move refuses transfers between different local filesystem devices, including
  moves between a local archive and another volume. Use Copy and verify the
  destination before removing the original. Same-volume moves remain available.

## 0.36.0 (unreleased)

- Smiðja can keep search results and previews in temporary panel tabs without navigating away from a file list.
- Open the layout editor directly from Settings, or move the running shell from its own header.

## [0.35.0 alpha](https://github.com/kambuziarendal/listi/releases/tag/v0.35.0)

Download `Listi-0.35.0-1-x86_64.hpkg` and its checksum from the release. See [what was tested](docs/VERIFICATION.md#0350-package-results).

- Quick view follows the active selection across workspaces. The inspector adds
  local photo controls, optional camera details and explicit short media previews.
- The source editor can edit bounded UTF-8 files up to 256 KiB, check for
  external changes and preserve original bytes for recovery before saving.
- Saved Shelf collections, typed BFS attribute columns and indexes, and a shell
  that can move between a panel and its own window expand the native workflows.
- Merging a moved folder retains a source folder if removing it would discard
  BFS metadata. Upload verification checks the staged file before publication.
- Select by pattern previews the number of matching visible names before
  replacing the selection. It does not change the panel filter.
- Local directory scans with icons deliver a small first batch so the panel
  can start drawing before the next regular batch is ready.
- Partial and interrupted jobs no longer display full progress as if they
  completed successfully.
- Find files restores its controls if the worker cannot deliver its result.
  A damaged recovery journal no longer hides other recoverable archive edits.
- Editing an encrypted archive keeps its protection even if the cached password
  has expired. Ordinary ZIP creation does not require AES support.

### Known limits

- Cancelling a job does not undo completed work. After an interruption, compare
  source and destination before retrying. A hidden staging file is not a backup.
  Local copies refuse empty typed BFS attributes rather than dropping them.
- Avoid renaming or replacing the same entries in another application during a
  move or rename. Haiku cannot atomically check identity and rename in one step.
- Settings writes preserve the previous file on reported write, file-sync,
  close or rename failures. This is not a guarantee against sudden power loss.
- Large remote folders may still take time to show their first entries.
  Archive support depends on installed engines. See the [workflow guide](docs/GUIDE.md)
  and [verification limits](docs/VERIFICATION.md#verification-limits).

## [0.32.1 alpha](https://github.com/kambuziarendal/listi/releases/tag/v0.32.1)

- Corrected device-icon scanning and retained shared ancestor columns when
  navigating in Einn.
- Download: `Listi-0.32.1-1-x86_64.hpkg`. See the release for the matching
  SHA-256 checksum.
