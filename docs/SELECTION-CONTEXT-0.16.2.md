# Selection information and current-folder actions

The selection status includes the sum of cached file sizes, in native readable
units. Hover for exact known bytes. Folder contents and link targets are not
traversed or included; folders and links have separate counts. Unknown sizes
remain visibly unknown rather than displaying zero. Archive entries use their
uncompressed size when the engine supplies one. Selection never performs disk
or network I/O to calculate size, preserving drag-selection responsiveness.

Right-click empty file-pane space for current-folder actions: New folder, Paste
here, Refresh, Terminal here and Properties. The same menu appears as **This
folder** at the very top of the file context menu, including multi-selection.
It always refers to the panel's current folder, not the folder entry clicked.
File actions remain below it. Both icon and detail views use this menu.

Empty-space context clicks preserve selection. Clicking an already selected
entry preserves multi-selection; clicking an unselected entry selects it as
before. Cancelling either menu has no file-operation side effects. The owner
panel, folder and file selection are captured before menu tracking; replaced
folders are rejected before executing a folder action. File actions keep their
existing source identity checks. Background actions are available even with no
selected files. Volume roots support the folder menu without requiring them to
be eligible for clipboard copying. Archive/network views expose Refresh but
leave unqualified local folder actions disabled.

Native model check: compile tests/selection-summary.cpp with src/PaneModel.cpp,
-Isrc and -lbe. Installed menu/status/performance verification is recorded in the
candidate acceptance note. No settings format changes.

## Active panel

Commander no longer labels the panes Left panel or Right panel. The active pane
has a two-pixel theme navigation-color outline, a 7.5% navigation-color tint in
the address field and a 3.5% tint in the file-area background. Selection colors
are unchanged. Simple retains its neutral single-pane appearance.

Only the empty outer border animates: six messages over 150 ms, with an owned
finite message runner that stops after completion. Address and file-area colors
change once per actual activation, not on each animation frame or repeated click
inside the same active pane. Rapid switches supersede queued frames using a
serial token. Theme/workspace changes settle and cancel the effect.
