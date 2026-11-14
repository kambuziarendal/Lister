# Rectangle selection responsiveness

Mouse movement is sampled at approximately 60 Hz using the latest pointer
position, with an exact final update on release. This is a scheduling target,
not a hardware frame-rate guarantee. Edge scrolling retains its previous speed.
Escape restores the pre-gesture selection; navigation cancels the gesture.

The detail renderer traverses flat rows in native sort order once, rather than
calling the native linear GetRowRect search for every row. Selection restoration
uses a membership set instead of a linear search per entry. Unchanged selections
do not cause synchronization or status notifications. Only the visible renderer
is synchronized; switching views explicitly resynchronizes the new renderer.
Grid selection invalidates changed cells. The selection rectangle invalidates
its old and new borders, not the entire file pane. Preview notifications are
sent after the gesture, rather than scheduling decoding for transient selections.

Native regression: compile tests/selection-model.cpp with src/PaneModel.cpp and
-Isrc and -lbe. It checks 10,000-entry selection, ordering, focus, duplicates, absent,
parent and filtered entries, extension and clearing. Runtime measurements and
installed-package acceptance are recorded separately.
