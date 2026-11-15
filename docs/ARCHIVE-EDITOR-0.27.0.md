# External archive editing, private candidate 0.27.0

Select one ordinary file in a writable local archive and choose **Edit**.
Listi opens a private working copy in the configured editor (StyledEdit by
default). Save in that editor. While the Listi edit window remains open,
stable saved changes are written back automatically. **Save now** retries a
paused save. Unsaved changes held only in editor memory cannot be captured.

Each save snapshots the working file, rebuilds the container, reads back and
compares its contents before replacing the original archive. Original member
permissions and supported encryption are retained. Editor saves arriving during
packing remain in the working copy for the next transaction. Archive writes from
the edit windows and operation queue serialize, then check their frozen guards.

If the original archive changes externally, automatic saving pauses. It does
not silently merge or overwrite that version. **Start from current archive...**
explicitly opens a fresh copy and retires the previous session without deleting
its working file. Close the old document first, then manually reconcile any
needed edits. The confirmation displays the retained old copy's path.

## Recovery and privacy

Working files, last-saved baselines and password-free session records are kept
under `~/config/settings/Listi_archive_edits/edit-*/`. Directories are private
to the current user. Decrypted working copies are retained too; they are not
encrypted at rest. **Open working folder** exposes the copy in Tracker.

**Keep copy and close**, application quit or cancellation does not delete copies.
Use **Files > Archives > Resume archive edits**, or Edit the same archive member,
to resume a retained session. Recovery rechecks the original archive fingerprint.
If publication succeeded but recovery metadata failed, the error explicitly says
the archive was saved. Inspect both versions; do not assume rollback occurred.
Old copies are not automatically removed. After closing their edit windows and
preserving anything needed, remove obsolete `edit-*` folders manually in Tracker.

## Scope

The ten currently writable single-volume variants are covered, including
encrypted ZIP/7z. This is the **Edit** workflow, not write-back from ordinary
Open, View or arbitrary Open With applications. It does not add RAR writing,
split/nested/remote archive editing, general archive/local Cut, automatic conflict
merging, or full special-entry/metadata preservation. Large containers require
space for working copies and complete rebuilds. Physical power-loss recovery is
not qualified. No package publication or upload is part of this candidate.
