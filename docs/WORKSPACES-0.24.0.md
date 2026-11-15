# Workbench workspace

Private 0.24.0 candidate scope. View > Workspace > Workbench combines the
existing two file panels, both folder trees, image preview, filters, address
controls, command bank and on-demand search and operation details. It does not
add new file operations.

Workbench defaults to both trees and preview visible. Those three choices are
stored separately from Commander, so customizing one workspace does not alter
the other. Older settings without Workbench fields receive the full Workbench
defaults. Older Listi versions safely open a saved Workbench choice as Commander.

The existing pane, tree, preview and operation-queue objects are relaid out, not
recreated. Workspace switches therefore retain paths, selections, history,
display modes, splitter positions and captured or running jobs. No directory
rescan is initiated solely by selecting a workspace.

Simple and Explorer remain single-pane workspaces. Direct panel copy and move
are available in Commander and Workbench. Search and the operation log remain
on-demand so Workbench does not start background work merely by being selected.
