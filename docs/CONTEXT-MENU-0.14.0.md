# File context menus, private Listi 0.14.0

Secondary-click a file or folder in either Details or icon/thumbnail views.
An already selected entry retains the full selection. An unselected entry becomes
the sole selection. Parent rows and empty space do not offer file actions.
The clicked panel becomes the source. Both renderers use the same native popup
and existing commands, not a separate file-operation implementation.

The menu offers Open, file View/Edit/Open with, clipboard Copy/Cut, direct panel
Copy/Move, Rename, Duplicate, Create symbolic link, Move to Trash and Properties.
Single-entry actions are disabled for multiple selections. File-only choices are
omitted on a directory. Local-only actions are disabled for virtual selections;
existing virtual opening/reading/copying remains available. Permanent deletion
stays in the main Files menu, not the context menu. Paste/new-folder on empty
space and folder-target Paste are not added in this increment.

The popup cannot replace an unanswered inline question. Selection, panel paths,
local source identities and destination identity are checked before dispatch;
changed entries cause refusal. Dispatch occurs on the owning window thread
before queued navigation can change the chosen action's source/destination.
Escape or clicking outside closes the popup without starting an operation.
Trash and named operations keep their existing confirmations and name validation.

0.14.0 also contains the privately qualified 0.13.0 changes: renamed `listi`
package, original native icon, duplicate/link workflows, unassigned F keys and
stable inline-dialog window geometry. The earlier 0.13.0 ZIP was staged but not
sent; this is the combined private delivery.

The four working-layout presets are a separate approved gradual plan in
ROADMAP.md. They are not implemented by this package.
