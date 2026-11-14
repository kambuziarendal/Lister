# Private combined Listi 0.14.0 acceptance, 2026-11-14

Source `6f34ad7`, branch `work/smb-first`. 128 tracked inputs matched native
source and clean packaging stage. Includes all qualified 0.13.0 changes;
that earlier ZIP was staged but never sent. No public push or publication.

- Executable: `873232a33ee5b43e688050f9717c609d0a4c448a3cafc87d45f68147e6b69b9d`
- HPKG: `e4f0f3bd465c681dd84ece2c09da228d5902b74b82a2e52cf557dd205e38160d`
- ZIP: `f622f5d092ae70002fbc79c046dec571c1f05544aeb6d33715d1dfd2793e80a9`

All 164 native checks rerun successfully on the final candidate, with the same
suite breakdown as ACCEPTANCE-0.13.0.md. Native binaries were rebuilt from the
final source. The installed app and active package matched the delivery hashes.

Installed context UI: secondary click in Details and Icons, unselected single
entry vs retained multiselection, native clipboard Copy/Paste of exactly the
selected pair, direct panel Copy, clicked folder Open, right-panel ownership,
Duplicate, refusal of replaced source, blocked competing question, Escape and
normal app exit. Native light/dark popup captures were inspected. Single-entry
commands are visibly disabled on multi-selection. Folder menus omit file-only
commands. Scrolled-row hit testing relies on ColumnListView's own outline-space
RowAt and shared selection model; no duplicate renderer-owned selection state.

After final package reinstallation, actual right-click Duplicate created the
correct sibling. The reported Trash open/cancel retained actual 760x600 geometry
with both panels in Details. F5 remained inert. Final HaikuDepot local-package
UI shows listi 0.14.0-1, Open Listi and Uninstall listi. Application icon and
self-tests passed on the exact final executable.

Actual 0.14.0 -> retained 0.12.0 -> final 0.14.0 roundtrip passed. Preferences
survived the predecessor's Save semantically; places/searches and original/copied
payloads remained byte-identical, and permissions were retained. Package-name
rollback procedure is unchanged: quit, uninstall package listi only, install old
lister 0.12.0. Forward upgrade replaces lister automatically. Never restore old
user data/settings to undo a package upgrade.

One new harness assertion was corrected: successful Copy replaces the selection
status with a clipboard-status sentence. The screenshot already showed the
retained two-entry selection, and the corrected assertion plus actual clipboard
paste proved the exact selected pair. Original failed log retained.

Scope and exclusions: CONTEXT-MENU-0.14.0.md and NAMED-ENTRIES-0.13.0.md.
No new physical-device or network-provider qualification is claimed. Working
layout presets are approved planning only, to be implemented gradually.
Final app/package window closed; pre-test fixture settings restored; guest synced.
Evidence: /home/lobster/kambuzi/reports/listi-named-entries-20261114/context-*.
Delivery is recorded separately by the channel receipt.
