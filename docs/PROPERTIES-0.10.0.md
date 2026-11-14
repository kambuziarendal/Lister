# File properties and BFS attributes, private 0.10.0

Select one local entry, then Files > Properties or Command+Shift+I
(Alt+Shift+I in the standard Haiku keyboard layout). The separate window stays
attached to that entry, not later panel selections. No metadata is saved on
focus loss. Use Save attribute or Apply permissions explicitly.

The window displays path, entry size, timestamps, numeric owner/group and octal
permissions. Entry size is not recursive folder size or allocated disk usage.
Permissions support ordinary 000 through 777, with three digits required.
Existing special permission bits are inspection-only; ownership changes are
not implemented. Symbolic links show their target without editing it.

Attributes support creating, editing and removing String, Boolean, Int32,
UInt32, Int64, UInt64, Float and Double values. Existing names and types cannot
be changed in place. Removal requires a second explicit confirmation. Edit
selected loads a draft; merely selecting another row does not replace it.
Save or Discard a draft before Refresh, another edit, or closing the app.

System namespaces (`BEOS:`, `_trk`, `_Haiku`, `_listi`), unsupported binary
values, malformed UTF-8/typed data and oversized values are read-only. This is
not a MIME editor, bulk attribute tool, Tracker column editor or a replacement
for a native type-specific application such as People. Metadata editing for
virtual archive/network entries is not included.

## Safety and limits

- One pinned native node per window; changed/missing pathname identity blocks
  edits. A replacement file cannot silently become the editor's target.
- Compare the loaded attribute's name, type, size and bytes again before writing.
  Compare permissions separately. Refuse detected concurrent changes.
- Native node locking is cooperative. Haiku does not expose a general atomic
  compare-and-swap for attributes. This does not promise serialization against
  every non-cooperating program or power-loss atomicity.
- Before each attribute mutation, flush a private recovery record containing
  original bytes/type/existence and file identity to
  `~/config/settings/Lister_attribute_recovery/change-*`.
- On verified success remove that record. On partial-write, flush or readback
  failure report the recovery path and retain it. Inspect the file and record
  before retrying. Do not automatically restore over another application's
  newer write. Automatic recovery UI and a durable undo journal remain future work.
- File contents are not rewritten by property edits. A failed metadata operation
  can still have changed metadata; the error explicitly reports uncertainty.
- Editing is limited to 4096 text bytes. Inspection reads at most 64 KiB per
  attribute, 256 attributes and 1 MiB total, with an explicit limit notice.
- The app cannot close a property window while its worker is active or a draft
  is unsaved. File-operation queue protections remain independent.

Native backend tests live in `tests/file-properties.cpp`. Linker-wrapped fault
injection in `tests/properties-faults.cpp` exercises partial writes, node sync
failure and recovery-record flush failure without production fault hooks.
Qualification results belong in ACCEPTANCE-0.10.0.md when completed.
