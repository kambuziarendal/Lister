# Listi 0.16.3 private candidate

- Repaint exposed pane background on resize, removing old active-border traces.
- Both background context menus and This folder include Sort by (Name, Size,
  Modified, Ascending, Descending) and View (Details, Icons, Large icons,
  Thumbnails). Choices affect only the originating pane. Sorting uses cached
  listing metadata, preserves selection, and carries across view changes.
- Sort is session-local per pane; view persistence follows existing settings.
- Detail view horizontal scrollbars appear only when visible columns overflow,
  in Commander as well as Simple. Filename truncation does not require scrolling.
