# Testing and verification

Listi is alpha software that works directly on files. Tests provide evidence for
specific cases, not proof that every filesystem, server or archive variant is
safe. Use disposable fixtures and independent backups.

## Built-in self-test

Build inside Haiku x86_64 with the dependencies listed in the
[README](../README.md#build-from-source), then run:

```sh
make test
```

This builds the application and invokes `--self-test` on the release executable
under `src/objects.*-release/Listi`. It is not the entire native test suite.

## Focused native tests

The `tests/` tree contains tests for file operations and transfer races, queue
behaviour, archives and passwords, directory delivery, search, comparison,
clipboard, attributes, navigation and settings.

After a native build, with Haiku's `app_server` available:

```sh
python3 tests/run-native-audit.py
```

Selected tests can be requested by name, for example:

```sh
python3 tests/run-native-audit.py selection-model search-filters
```

The runner currently expects objects under
`src/objects.x86_64-cc13-release`. A different compiler/object-directory name
requires adjusting that path. Some tests require optional packages or prepared
synthetic fixtures; inspect the selected test before running it.

Network integration tests require disposable local services and explicit fixture
setup. They are not all run by `make test`. Diagnostic probes under
[`tests/probes`](../tests/probes/) have historical reproduction exit meanings and
must not be mistaken for normal passing regressions.

### Moving between volumes

The opt-in `cross-volume-move` audit needs a writable directory on a different
mounted volume from `/tmp`. It creates unique test folders and checks both
volumes after cancellation during copying, before source removal, after the
last removal and between two selected files.

```sh
LISTI_TEST_VOLUME=/path/to/test-volume python3 tests/run-native-audit.py cross-volume-move
```

To also test running out of space during replacement, use a **dedicated,
disposable** test volume (for example, a freshly mounted 32 MiB BFS RAM disk)
and set `LISTI_TEST_DISK_FULL=1` in the same command. This case deliberately
exhausts the volume during staging. It verifies the original file, the existing
destination and removal of the temporary file. Do not use a shared volume.
The audit does not format or mount disks itself.

## UI and package checks

Automated tests do not replace installed-package checks in Haiku. Relevant
workflows should also be exercised through the interface, including conflicts,
cancellation, compact windows, light/dark appearance, workspace switching and
settings compatibility. A source-built executable and an installed package are
separate artifacts and should be identified separately.

## Verification limits

- A test source or a previous successful run is not a fresh qualification of the
  current checkout. Record the commit, binary and environment for each run.
- Results do not cover every removable device, filesystem, NAS, server or WAN
  condition. Archive capabilities depend on installed engines.
- Concurrent edits, partial moves and power loss require special care; Listi is
  not a transactional backup system and does not promise universal undo.
- Performance claims need documented hardware, datasets and cache conditions.

For behaviour and known limits, see the [workflow guide](GUIDE.md).

## Reporting a problem

[Open an issue](https://github.com/kambuziarendal/listi/issues) with the Listi and
Haiku versions, exact reproduction steps, error messages and whether files were
changed or left in a partial state. Prefer a minimal synthetic example. Do not
include passwords, private filenames or private file contents in a public report.
