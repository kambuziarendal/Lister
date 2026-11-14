# Simple and Commander workspaces

First bounded step of the approved workspace plan. View > Workspace switches
between Simple (one file panel) and Commander (two panels). Explorer and Workbench
remain planned, not exposed as unfinished presets.

Simple retains the currently active panel. The other panel is hidden, not
recreated: both locations, selections, filters, navigation history and display
modes survive switching. No navigation or rescan is triggered by a workspace
switch. Pending questions retain their captured operation and input; running
jobs continue independently of the visible workspace.

Simple hides the action bank, command line, folder trees and image preview.
Commander restores those existing preferences. Operation controls appear during
work, conflicts, errors or explicit Listi > Operations; idle collapsed controls
are hidden in Simple. Escape collapses the log but does not hide a running job.
Preferences can still customize Commander without exposing its tools in Simple.

Use clipboard Copy/Cut and Paste to transfer files in Simple. Direct other-panel
Copy/Move, Same folder and Switch panel are disabled and their message handlers
refuse hidden-target actions. New symbolic links are created in the visible
folder in Simple; the naming question explicitly shows the destination.
Right-click, duplication, properties, navigation and icon/detail selection remain
available. Function keys remain unassigned.

The workspace choice and chosen single-panel side are persisted in the existing
preferences file. Old or missing settings default to Commander. A stale
preferences dialog cannot overwrite a newer workspace choice. Folder locations
are preserved across live switches; this does not add session path persistence.
Rollback to 0.14.0 ignores the new preference keys. If preferences are saved by
0.14.0, those unknown keys are dropped and the subsequent upgrade starts in
Commander. Existing preferences, places, searches and user files remain intact.
