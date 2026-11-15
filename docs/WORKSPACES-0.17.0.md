# Explorer workspace

View > Workspace > Explorer shows the active file pane with a folder tree,
editable address, back/forward, Go, file-view choice and filter. No bottom action
bank, command line or idle operation strip. Tab switches between tree and files.
The tree starts at roughly one quarter of the pane and its splitter is adjustable.

The same pane objects survive Simple/Explorer/Commander switches. Locations,
selection, history, display modes, existing Commander tree/preview preferences
and captured/running jobs remain intact. Simple alone suspends the hidden filter.
Explorer forces its tree visible without changing Commander's tree preferences.
Panel transfers remain unavailable in single-pane workspaces. Use local
Copy/Cut/Paste; archive single-pane extraction remains a separate planned step.

The new integer workspace setting recognizes older simpleWorkspace settings.
It also writes a Simple fallback for Explorer, so rollback opens a safe single
pane. Saving preferences in an older version may discard the newer workspace
key. Normal settings, places, searches and files are not migrated or removed.
