# Multipart archive reading (0.21.0)

Keep all parts in one local folder and open any part in Listi. The first part is
located automatically and becomes the archive location. Use the existing
Archives > Extract to folder action, or browse/preview members inside Listi.
Install optional HaikuDepot p7zip; no external archive GUI or password-bearing
process is launched. Tested p7zip version: 17.05-2, x86_64.

Qualified naming families:

- `name.7z.001`, `.002`, ... as a seekable concatenated input (not copied together).
- `name.part1.rar`, `part2.rar`, ... including zero-padded names.
- `name.rar`, `name.r00`, `name.r01`, ... for old RAR naming.

7z/RAR5 encrypted headers and data use the existing memory-only password flow.
Old RAR volume assembly is qualified with unencrypted RAR4 fixtures; encrypted
RAR4 single-file support remains qualified separately by 0.20.0, not a claim
that every encrypted legacy multipart variant has been exercised.

Only captured regular sibling files are given to the decoder. Symlink parts,
ambiguous duplicate numbers, gaps, missing first parts and more than 256 parts
are refused. The native decoder additionally checks completeness, final-part
requirements, CRC and exact member sizes. A structurally readable listing is
not a full data-integrity test; corruption in member data can surface on read.
All parts and the part-set fingerprint are frozen before extraction confirmation,
checked after queueing/password waits and again before publication. A changed,
missing or newly added part aborts the job. Whole-archive extraction stages into
a private directory and publishes only on success. Cancel/error cleans staging.
Existing destinations are not overwritten. Individual member reads retain the
0.20.0 CRC-checked temporary-file lifecycle and its temporary disk requirement.

Bounds and limitations:

- Parts must be local, together and consistently numbered. No manual cross-folder
  part picker, split ZIP/tar, split creation, remote/nested volumes or repair.
- Available codecs and memory/disk requirements remain those of the optional
  system engine. Solid archives may decode earlier data repeatedly per member;
  this change is not a single-pass extraction optimization.
- RAR5 Unix mode metadata is read from native properties; privileged bits are
  stripped. The tested engine does not expose Unix modes/link metadata for some
  old RAR4 entries. Such old link payloads may be ordinary files, never followed
  as filesystem links. Do not rely on RAR4 Unix metadata preservation.
- Encrypted archive creation and modification of existing archives remain later
  work. No claim of support for every historic/future archive codec.

Native regression source: tests/archive-volumes.cpp. Synthetic fixture provenance
and licensing: tests/fixtures/multipart/README.md. Private installed acceptance
is recorded separately after the final package is tested.
