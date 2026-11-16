# Design and architecture

Listi is a native Haiku file manager inspired by Directory Opus-style
source/destination file work. It is an independent implementation, not a port.
Its interface uses Haiku controls, system colours, MIME icons and Layout Kit.

## Design principles

- **Visible intent.** The active panel is the source; direct Copy and Move use
  the other panel. Clipboard Paste uses the active panel. Accepted jobs retain
  their selected paths and target when focus, navigation or workspace changes.
- **Shared actions.** Menus, buttons and shortcuts should use the same validation
  and file-operation logic.
- **Native keyboard behaviour.** Use Haiku's Command modifier and preserve text
  editing and list type-ahead. Function keys are unassigned in the current alpha.
  See the [shortcut reference](../README.md#basic-workflow).
- **Optional by design.** Useful defaults come first. Advanced views, inspectors,
  rules and integrations should be opt-in rather than required for ordinary
  browsing. The long-term goal is to add, remove, move and reorder buttons and
  workspace modules, save layouts and restore defaults. Current visibility
  preferences are not yet a free-form layout editor. Configurability must not
  disable safety checks or hide operation targets.
- **Honest outcomes.** Report cancellation, errors and partial completion.
  Cancellation is not rollback, and a job log is not a backup.

## Native components

- Panels use asynchronous directory scanning and node monitoring. Scan
  generations distinguish current results from obsolete replies.
- `OperationQueue` runs file work on a worker thread and communicates with the
  native progress interface. The filesystem engine does not manipulate widgets.
- Regular copies use temporary sibling files. Symlinks are copied as links rather
  than followed recursively. Moves check source identities before cleanup;
  concurrent external changes and partial moves remain important limitations.
- File reads, launches and settings writes use a service looper. Layout Kit
  arranges controls rather than relying on fixed widget coordinates.
- Haiku's private ColumnListView dependency is isolated in `src/Pane.cpp`.
  Changes to that OS interface may require a rebuild.
- Archive and network adapters share the panel and queue workflow. Format and
  protocol capabilities depend on the backend and must be reported explicitly.

## Trash and recovery

`src/Trash.cpp` uses native Haiku filesystem APIs and Tracker's
`_trk/original_path` attribute convention. Trash is on the source volume.
Restore preserves conflicts and requires the original parent folder to exist.
Unsupported volumes fail rather than falling back to permanent deletion.
Permanent deletion is a separate, confirmed action.

Listi is not a transactional backup system. See the
[workflow guide](GUIDE.md) for operation limits and the
[testing guide](VERIFICATION.md) for verification boundaries.

## Names and compatibility

The application, executable and launcher are **Listi**; the repository and
package ID are `listi`.

| Workspace | ASCII ID | Stored numeric value |
|---|---|---|
| Einn | `einn` | 1 |
| Askr | `askr` | 2 |
| Tvískipt | `tviskipt` | 0 |
| Smiðja | `smidja` | 3 |

The native `Workspace` scripting property returns the display name. Scripts
comparing that value must account for the 0.31.1 rename.

Existing numeric workspace values and legacy settings aliases are retained.
Unknown workspace values fall back to Tvískipt. Compatibility-only identifiers
still use the historical `Lister` spelling, including the application signature
`application/x-vnd.Kambuzi-Lister`, scripting suite, settings stores and package
aliases. No manual settings migration is required by the rename.

## Future architecture

Listi currently targets Haiku. Portable core extraction and additional platform
adapters are future work, not shipped components. Tracker coexistence remains
the default; replacing the desktop is outside the current scope. See the
[roadmap](ROADMAP.md).
