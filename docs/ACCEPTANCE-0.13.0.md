# Private Listi 0.13.0 acceptance, 2026-11-14

Source: `85bb68b` on `work/smb-first`. All 124 tracked inputs matched the native
Haiku source tree and clean packaging stage. No public push or release.

- Executable: `9b5f749861bf0838118de3d39684cd37468869239f48d2a14be03967a29c61f6`
- HPKG: `cc0008e5cce9131885831187348b7071d200916377b08546648e2fa7a2f59b76`
- ZIP: `f2834cb737c78fb949f17fda9aa394e93094820972b54b5bf9fa7513ddd08506`

164 native checks passed: 34 named-entry, 24 drop, 34 clipboard, 7 attribute-copy,
3 theme-text, 7 native icon and 55 self-tests. The final installed executable's
icon/self-tests were repeated after the last footer layout adjustment. The other
engine/test objects were unchanged. Native icon renders at 16/32/64/128, transparent
margins and Tracker resolution passed; artwork was visually inspected.

Installed final UI: Duplicate through shortcut and native menu, collision retaining
the name draft, frozen navigation, metadata, relative/absolute link target bytes
and resolution, replaced source/destination refusal, mouse and Enter, Escape,
and former F3 through F9 aliases inert. F8 is also inert in a rename field.
Alt+Shift+V opens the viewer; system Alt+W closes only that viewer. Alt+Q exits
an idle main app. Main Alt+W's normal Haiku exit behavior was reproduced in 0.12
and intentionally retained following the owner's clarification.

Reported shrink reproduced with two Details panels and no tree/preview:
1024px before, 605px while the prompt was open, 701px after cancellation.
The fixed installed package preserves actual 1024x650 and 760x600 frames through
Trash, Rename, New folder, Duplicate, Link and Connection open/cancel. Separate
checks verified actual Connection controls, not just the frame. Compact light/dark
link dialogs and normal/compact main windows were visually inspected. The footer
no longer consumes spare space while its buttons are hidden.

Native package upgrade installed `listi` and automatically removed `lister`.
HaikuDepot's local-package UI explicitly shows `listi 0.13.0-1`, Open Listi and
Uninstall listi. Its local-package icon remains HaikuDepot's generic package icon;
the executable and Deskbar/Tracker resolve our new application icon.

Actual rollback and re-upgrade passed, retaining semantic preferences after the
old preference dialog's Save, byte-identical places/searches, original/duplicated
content and attributes. Reverse name migration requires uninstalling only the
`listi` package before installing retained `lister` 0.12.0. Direct downgrade was
correctly rejected by the declared conflict; no settings/data removal is required.
The exact predecessor executable was verified. Forward re-upgrade removes the old
package automatically and restores the final executable hash above.

Harness corrections retained in logs: omitted package-script version argument,
unsupported `pkgman list` syntax, assumed uppercase installed filename, and viewer
Window 0 assumption (actual viewer Window 1). Final continuation checks passed.
The original geometry failure and initial binaries/logs are retained separately.

Scope: NAMED-ENTRIES-0.13.0.md. No new physical hardware, remote link/duplicate,
bulk naming, hard links or Show Target support is claimed. Final app closed,
pre-reproduction fixture settings restored, guest synced; inherited VM retained.
Evidence: /home/lobster/kambuzi/reports/listi-named-entries-20261114/.
Private delivery is established separately by the channel receipt.
