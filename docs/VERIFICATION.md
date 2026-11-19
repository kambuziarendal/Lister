# Testing and verification

Listi is alpha software that works directly on files. Tests provide evidence for
specific cases, not proof that every filesystem, server or archive variant is
safe. Use disposable fixtures and independent backups.

## Native regression audit and smoke test

Build inside Haiku x86_64 with the dependencies listed in the
[README](../README.md#build-from-source), using a disposable test profile with Haiku's
`app_server` running. The full audit exercises optional features too: install
the p7zip native engine, LHA and ARJ tools and the package providing
`cmd:pdftoppm` for archive and preview coverage.

From a clean checkout:

```sh
make audit >native-audit.log 2>&1
```

This builds the application, links and runs all cases in the runner's required
list, and finally runs the build-tree `Listi --self-test`. Check the exit
status of **make**; a build error, missing required case or object, failed
assertion, timeout or failed self-test makes the audit fail. The log records
the selected set, compiler version, OS, system packages and Git revision and
working-tree status. Use a clean Git checkout (with Git installed) for
version-pinned evidence. An exported source tree reports its revision as
unknown. Keep the log even on failure; tests after the first failure have
not run. A case passing means the executable exited successfully; any optional
subchecks it explicitly skipped are not verified. This is not an installed-package
or network qualification.

For a quick iteration, or to rerun selected cases:

```sh
make test
make audit TESTS="selection-model search-filters"
```

`make test` builds and runs only the built-in smoke test. A focused audit still
ends with the self-test but does not qualify the full suite. To check the
failure path in a disposable checkout, request `TESTS=no-such-case`; make
must exit nonzero. To exercise a failing executable without touching any
existing assertion, create `tests/intentional-failure.cpp` containing only
`int main() { return 1; }` and select `TESTS=intentional-failure`; this
must also exit nonzero without printing a final pass.

After building, `python3 tests/run-native-audit.py [case ...]` is equivalent
to the audit step without the application rebuild. It asks the native build
for its exact target, objects, compiler, include and link flags.

For Shelf collections, run:

```sh
python3 tests/run-native-audit.py shelf shelf-collections shelf-collections-window
```

To test a restart and mount cycle, use a disposable BFS
volume and keep the test program and settings file on a different volume. With
the fixture mounted, run `shelf-collections --save-reference STORE FILE`.
Run `shelf-collections --check-reference STORE FILE ready` from a separate
process. Unmount the fixture and check for `missing`. After remounting it on
the same device path, check for `ready` again using the current absolute file
path. Use the full path to the test binary in `build/native-audit-tests`.

Network integration tests require disposable local services and explicit fixture
setup. These are not part of the default `make audit` set. Diagnostic probes under
[`tests/probes`](../tests/probes/) have historical reproduction exit meanings and
must not be mistaken for normal passing regressions.

### Inspector, Quick view and native source editing

After building on Haiku, run the inspector tests against the linked native
objects. PDF page tests require `pdftoppm`; media support depends on the codecs
and audio output present on the test machine.

```sh
python3 tests/run-native-audit.py inspector-content inspector-document inspector-editor inspector-photo
```

Also test the installed app with disposable files:

1. In each workspace select a source and use **Listi > Quick view**. In a
   two-panel workspace try both active panels. Change the selection, then
   toggle Quick view off. Both original paths and selections should survive;
   Space must still select. A permanent Inspector and a temporary Quick view
   should not change one another's saved settings.
2. Preview a script, HTML and Markdown. None should execute. Choose **Edit**,
   change the draft, then select another file in the main window. Try both the
   main window's Quit and app-level Quit: an unsaved draft must stay open.
   Exercise Find/Replace, undo/redo, go to line, wrap, Save and Cancel. Check
   the original bytes, line endings and a typed BFS attribute after saving.
   Try a read-only file, random bytes, a truncated preview and an unknown
   extension. A rejected edit must not change the source.
3. While a draft is open, change the source externally or replace the entry
   with a different file at the same path. A failed save must preserve the
   other file and the draft. The document tests also inject sync failures and
   check restoration or retention of the original-byte recovery file.
4. Inspect an image at fit and at actual size, then zoom and drag. Try a JPEG
   with known camera and GPS attributes. Coordinates must not appear until
   requested. Switch quickly between an image, a PDF and text; late replies
   must not replace the current preview.
5. Select audio/video without starting playback, play and stop an inline sample,
   and open a local clip in **MediaPlayer**. Try an unavailable codec or audio
   device separately. Seeing a frame is not evidence of audible output.

### Tvískipt function buttons

With both panes open, select files and click **Buttons** beside the menu bar.
Check that the bank hides and returns without changing the window frame, active
pane, selection, or command field, with the bank at the top and at the bottom.
Hide the bank, make the window too small for it, and click **Buttons** again.
Check that configured actions remain available in the menu without enlarging
the frame. Restart Listi to check that the toggle is remembered independently
of Smiðja. Run the `preferences-state` native test for settings compatibility.

### Flat View

Run `python3 tests/run-native-audit.py flat-view file-search live-search shelf`.
The Flat View fixture opens the native window over local files with duplicate
names and a directory-link cycle. It injects an unreadable branch, exercises
filtering and frozen Shelf selection, and stops a deliberately paused scan.
For a manual workspace check, select files in either panel, set a panel filter,
and open **View > File view > Flat View…**. Close it and confirm that the same
panel, selection, filter and workspace remain in place.

### BFS attribute columns

Run `python3 tests/run-native-audit.py attribute-columns attribute-columns-ui file-properties properties-faults`.
The UI fixture opens Properties and two panes looking at the same folder. It
checks stored, empty and missing values, adds a second column without touching
an open edit draft, then sorts and clears the columns. Clearing restores the
original pane view without changing the other pane. The separate attribute
test creates and removes its own disposable BFS index.

### File operation fault and job-result tests

On Haiku, run `python3 tests/run-native-audit.py file-operation-faults operation-queue bulk-rename`
after building. These tests use disposable entries to check staged output after
short/interrupted writes, permission errors during writing or source removal,
partial batches and job results when UI notifications are delayed or lost.
Injected syscall failures also run when normal permission bits alone would not
prevent the test user from writing.

### Bulk rename and undo

In the bulk rename preview, select a row, enter its exact new leaf name and
click **Set name**. To swap two names, do this for both rows, then apply the
checked preview. **Use rules** removes a row override. With the queue idle,
**Files → Undo last bulk rename** checks the names, entry identities and
contents again before moving anything. Undo is session-only, covers the latest
successful batch, and does not overwrite an occupied original name. An
unreadable, unusually deep or large batch can rename without undo; undo
verification is limited to 512 MiB and 200,000 entries per batch.

The native regression injects failures after staging, checks rollback and verifies
that an entry blocking recovery is retained. If rollback cannot finish, the
operation report lists verified locations (including hidden
`.listi-rename-*` names) or says when an entry could not be located. Inspect
these paths before retrying. There is no crash-recovery journal.

Haiku does not provide atomic no-replace or identity-conditional rename.
Listi pins the parent directory and checks the source and target immediately
before each step, but an unrelated writer changing either name between the
checks and the kernel call can defeat the guard. Do not run a batch while another
application is renaming entries in the same folder.

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

For the optional host-driven network interruption matrix, set
`LISTI_NETWORK_TEST_BINARY` to the absolute path of the freshly linked
`network-interruptions` test in the Haiku guest. Set
`LISTI_NETWORK_KNOWN_HOSTS` to the guest's known-hosts file for the disposable
SFTP fixture. This avoids testing a binary left by an older checkout.
