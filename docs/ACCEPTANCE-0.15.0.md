# Private Listi 0.15.0 acceptance, 2026-11-14

Source `4c00f5f`, branch `work/smb-first`. All 130 tracked inputs match the native
source, including packaging documentation. Clean source archive retained locally.
No public push, release, or license change.

- Executable: `49a8da40da211de64f994a11fb6db5ed85a9d9d5f1964759fdede8685c441efa`
- HPKG: `063acd5b5ad04a56d1da2d517b73a37837d1b4f52629c2ef94105a0039f4b44c`

181 native checks passed: 164 existing file-operation/drop/clipboard/metadata/
icon/theme checks and 17 preferences checks. The final one-line tree refresh
correction changes no operation or persistence code. The installed final package
passed 28 main workspace assertions and 9 additional UI assertions, with source
and installed executable equality checked before acceptance.

Actual native View > Workspace controls verified Simple/Commander, active right
and left pane ownership, preserved paths/selection/filter/history, clipboard
Copy/Paste and Cut/Paste, relative links in the visible destination, conflicts
surviving switches, conflict abort retaining the old file, pending naming input
and destination, compact Trash open/cancel without frame changes, restart,
icons/details, and operation log open/collapse. Dark/light screenshots inspected;
other-panel transfer entries are visibly disabled in the Simple context menu.

Bughunt found blank folder trees when starting in Simple then returning to
Commander. Showing a tree now refreshes its current route. Final installed
restart screenshot verifies both restored trees, preview, command line and bank.

Actual final 0.15.0 -> retained 0.14.0 -> final 0.15.0 package roundtrip preserves
settings, places, searches, fixture contents, permissions and symbolic-link text.
The downgrade launches and reads the new settings without changing them. If the
old version subsequently saves preferences, it drops the unknown workspace keys;
this is documented in WORKSPACES-0.15.0.md, not a promise to preserve new features
through writes by an older program. Never restore stale user data for rollback.

Verification used an owned standalone copy of the clean Haiku R1 beta6 test disk,
not a writable backing-chain descendant. No physical Haiku-machine acceptance
claimed. Initial UI harness expected the wrong popup selection and read the
hidden panel's status; corrected to actual native mode selection and owning-pane
status, and retained failed evidence. Source preflight caught an absent previous
acceptance note; synchronized it before packaging and all 130 checks passed.

Evidence: `/home/lobster/kambuzi/reports/listi-layout-20261114/`.
Private attachment delivery is recorded separately by channel receipt.
Explorer and Workbench remain planned. MímisSendir was paused at the user's
request, with no further changes or release beyond beta 15.
