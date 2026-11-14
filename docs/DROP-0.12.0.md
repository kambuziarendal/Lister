# Explicit drops, private 0.12.0

Drag local selections between panels or from Tracker into Listi. Drop into the
receiving panel's **current folder**, including when the pointer is above a row.
The inline question names that folder and the item count. Choose Copy here,
Move here, or Cancel. Escape cancels. No action is inferred from modifiers or
same/different volumes. Cancel initially has keyboard focus; Tab reaches Copy
and Move. There is no destructive default. Normal keyboard copy/move remains.

The current pane destination and source selection are frozen at drop time.
Navigating, filtering or changing selection while the question is open cannot
redirect that job. A second drop or competing question cannot replace it.
Listi-origin local drags capture device/inode at drag start. External native
refs are captured at drop receipt, since generic Tracker refs contain no source
inode snapshot. Queue guards recheck sources and destination after confirmation
and conflicts. This is not a filesystem transaction against arbitrary external
writers. Source contents may change on the same inode before transfer begins.

The existing operation queue handles progress, conflicts, skip, cancellation,
BFS metadata, symlinks and cross-volume copy-then-remove. No new deletion
mechanism, system clipboard mutation or settings migration is introduced.
Local roots, protected Desktop/Trash directories, special files, duplicate and
overlapping selections, same-folder drops and destinations inside the source
are rejected. Limit: 1024 entries / 2 MiB message.

Existing archive/network drop transfers remain **copy only**, using the existing
provider backend checks. This increment does not freshly qualify remote services
or provider replacement races. No new link action, modifier shortcuts, auto-open
on hover, row-target drop, Tracker-side move semantics, or desktop replacement.
Outgoing local drags remain native refs; another receiving app controls its own
drop behavior. Long destination text is also available as a tooltip.

Rollback: quit Listi and install retained private 0.11.0. Do not restore old
settings or user files. No public publishing is authorized by this increment.

Bughunt also corrects native text recoloring after system palette changes: all
existing text runs, not only the insertion style, follow the new foreground.
This fixes unreadable old operation-log lines after switching light/dark.
