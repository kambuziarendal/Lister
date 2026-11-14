# Native file clipboard, private candidate 0.9.0

## Interaction

- Command+C copies references to the selected local entries. Command+X marks
  them for moving, without removing or relocating anything yet.
- Command+V pastes into the active panel's local directory. Files > Clipboard
  offers the same three commands with the mouse. Haiku normally uses Alt as
  Command; follow the configured keymap.
- Direct panel copying remains a separate operation: Command+Shift+C or F5,
  and the Copy button. View moves to Command+Shift+V or F3. Other existing
  panel shortcuts are unchanged. Bare letters still support type-ahead.
- In editable text fields, Command+A/C/X/V edits text. Clipboard keyboard
  actions do not operate on files while focus is outside a panel. Explicit
  file-menu clicks operate on the selected files.
- An open name/connection question must be finished or cancelled first.
  Existing operation conflicts remain attached to their frozen file job.

## Transfer guarantees and boundaries

Listi freezes the selected references, their device/inode identity and the
canonical destination directory identity before queueing. It checks those
identities at execution, after conflict answers, and before file commits.
Replacing a source or destination while a job waits does not grant permission
on the replacement. Existing staging, conflict, cancel, metadata and source
cleanup checks remain in force.

Copy references remain reusable. Listi consumes only successfully moved root
entries from a cut clipboard. Skipped, failed and cancelled roots remain cut.
A newer clipboard selection is never cleared by completion of an older job.
A private lease prevents another Listi paste from reusing a pending cut. If the
process was interrupted or clipboard reconciliation failed, explicitly copy or
cut the remaining entries again. This is not a persistent file-operation journal.
Listi waits for clipboard reconciliation before allowing a normal quit.

Tracker interoperability uses its native reference/mode message format and
node-change notifications, not pasted textual paths or shell commands. Tracker
updates references when nodes move. When **Tracker** performs the paste, its
own conflict, cancellation and clipboard behavior applies: beta6 switches cut
references to copy when dispatching its move, before the move is known to have
succeeded. Listi does not replace Tracker's operation engine.

Current deliberate limits:

- Ordinary local files, directories and symbolic links only. Links themselves
  are transferred, not their targets. Local copies preserve BFS attributes.
- No virtual archive/network clipboard sources or destinations in this step.
  Existing direct panel transfers retain their provider-specific behavior.
- No volume roots, Desktop/Trash roots, special devices, overlapping source
  roots, duplicate nodes, or unidentified generic refs/text-path clipboards.
- Maximum 1024 references and 2 MiB message data. A malformed/stale reference
  rejects the whole paste before queueing rather than silently omitting files.
- Same-folder paste is explicitly rejected. Duplicate naming, append-to-clipboard
  commands, paste-as-link, and persistent cut decoration are separate work.
- Listi can read a mixed copy/cut clipboard from Tracker, as separate guarded
  jobs. It cannot stop another application independently operating on the same
  source. Normal filesystem operations are not a cross-application transaction.

## Interoperability reference

Wire fields were verified against Haiku `r1beta6`:
`src/kits/tracker/FSClipboard.cpp`, `FSClipboard.h`, `Commands.h` and
`headers/private/tracker/tracker_private.h` in
https://github.com/haiku/haiku/tree/r1beta6 .
The implementation is Listi's own adapter, not imported Tracker source.

Each node has `r<device>_<inode>` (entry_ref) and `m<device>_<inode>` (int32),
with `Tcsl` for copy and `Tmsl` for cut. `TCch` notifications to Tracker carry
native `TCNR` node/mode records. No credentials are stored. Notifications are
sent individually so that a Tracker folder's first absent node cannot suppress
its later applicable entries. Private paste leases are supplementary strings,
not mode integers, preserving Tracker's first-ref/first-mode lookup convention.

Native regression coverage is in `tests/file-clipboard.cpp`. UI interoperability
must additionally be qualified using the actual Tracker and installed Listi.
