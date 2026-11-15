# Intermediate Listi 0.16.3 acceptance

Held and not delivered. Superseded by the direct context-selector extension.

Compiled source: ae4c8cf (private, no public push).
Binary SHA256: ffb0531e069155ab2c90beb6c96a95dcabf9ca33beab7f187abd5e226f2174ef
HPKG SHA256: 67fc68671dd285156ef62d294f089e855cc17ef9250823ad0d68bec3c9a5cbea
149 native source inputs matched the committed manifest. Installed binary matched build.

- Reproduced old panel frame fragments during incremental resize on 0.16.2.
- Final installed package: eight dark/light, details/icons, active-left/right
  grow/shrink comparisons match pristine panel pixels at identical geometry.
- 80 native checks: 55 operations, 10 sorting, 9 summary, 6 selection regression.
- Twelve real rendered sort cases: Name/Size/Modified, ascending/descending,
  Details/Icons. Selection and opposite pane preserved. Native header sort
  carries into icons; menu view change carries back into details.
- This folder in file menus exposes sorting/views and preserves selected file;
  background menus use the same actions. Folder Properties verified after restart.
- Commander horizontal bars track actual window and column overflow. Simple
  hides unneeded axes and handles 2000-file vertical overflow.
- Actual 0.16.3 -> retained 0.16.2 -> 0.16.3 roundtrip passed; hashes of settings,
  places, searches and fixture files, plus permissions and links, unchanged.

Evidence: /home/lobster/kambuzi/reports/listi-border-trails-20261115.
Failed automation attempts are retained: a splitter drag did not change geometry
and is not counted as coverage; initial popup timing and OCR probes were corrected.
The twelve-case sort matrix passed; its final ignored header-to-view menu gesture
was completed separately with native hidden-state and screenshot verification.
Sorting is session-local per pane. Physical-user hardware is not tested here.
No public release. Acceptance-only document is not part of the compiled package.
