# Listi 0.23.0 private candidate acceptance

Runtime source: `6d0b9cc791d58e35843fe2035ee82afff06d2c56`

Package SHA-256:
`484a94e958bbc13e4de17a3241583fe60573cf15acd430eee8ac659450795b4c`

Installed executable SHA-256:
`60a0be5b6a4418c8480831374002248afcff2c81d3e35fa2775dc84ce804ef6d`

## Verified scope

- The complete native audit passed 695 checks on Haiku x86_64.
- Installed Commander UI added and explicitly replaced an existing ZIP member.
  Independent p7zip extraction matched the selected source bytes exactly.
- Installed UI removed the final ZIP member and produced a valid empty archive.
- Conflict prompts name the archive and member, without exposing the private
  rebuild folder. The private folder is not listed and is removed after the job.
- Archive actions were inspected in Commander, Simple and Explorer, in the real
  light and dark system palettes.
- The package executable matched the tested build executable byte for byte.
- Actual 0.23.0 to 0.22.0 to 0.23.0 rollback and upgrade preserved settings,
  places, searches, archive preferences, file contents, mode bits and BFS
  attributes.
- Every tracked source, resource, build, packaging, test and documentation file
  in the native build tree matched the runtime source commit.

## Boundaries

This candidate edits local, single-volume ZIP and 7z archives. Split, remote,
nested and RAR archives must still be extracted and recreated. Archive comments
and format-specific data that Listi does not expose are not promised to survive
a transactional rebuild. Qualification used an isolated Haiku VM, not a
physical Haiku machine. Nothing was published publicly.

Evidence is stored locally in
`reports/listi-archive-edit-20261115`.
