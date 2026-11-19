# Contributing

Listi is a native Haiku development alpha. Small, focused changes with clear
behaviour and reproducible tests are welcome.

## Keeping changes small

Before adding a file action, trace it from its UI entry points to the operation
that owns it. For example, a new way to start a move should reuse the existing
move and conflict handling, not grow its own copy. If the existing boundary no
longer fits, extract the smallest useful piece and test the old callers before
switching them. Keep unrelated cleanup out of the feature diff.

If a change requires several subsystems to switch at once, lay out compatible
steps first. Each step should build, preserve existing workflows and be
reviewable on its own.

Before review, run `git diff --check BASE HEAD`, where `BASE` is the
commit your change builds on. For C++ changes, if clang-format and its Git
helper are installed, use `git clang-format --diff BASE HEAD` to inspect
formatting on the changed lines without rewriting unrelated code. These
checks do not replace native compilation or regression tests.

## Bugs and testing

For a release package, include the tag and whether its checksum matched. Include the Listi and Haiku versions, reproduction steps and whether files were
affected. Use disposable fixtures and independent backups when testing file
operations. Never attach passwords, private files or personal filenames to a
public issue. For the inspector, say whether you used Quick view, the panel
inspector or the separate viewer. For an editor save issue, note the file
encoding and line endings if known. For a photo issue, use a synthetic or
openly licensed example without personal GPS tags. See the
[testing guide](docs/VERIFICATION.md).

## Public documentation

- Write documentation, issue summaries and release notes in English. Product
  names and third-party credits retain their original spelling.
- Describe the product and its behaviour, not private conversations, personal
  requests, internal approvals or tool/agent activity.
- Keep local machine paths, private service addresses and internal acceptance
  logs out of public documentation and release assets.
- Distinguish what is in the source tree from what is in the downloadable
  package and from proposed features. Link a release by tag and use the exact
  HPKG asset name in installation examples.
- Use real, version-labelled screenshots, descriptive alternative text and
  attribution for the images they contain. Preserve reproducible tests and
  useful limitations alongside feature descriptions.
- Keep documentation links and package staging valid when moving or removing
  files. Keep implementation changes separate from editorial cleanup.
