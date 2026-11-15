# Listi 0.16.2 private acceptance

Compiled source: `073b93d` on `work/smb-first`. All 146 tracked native source
inputs match the recorded manifest. No public publication. This acceptance-only
note postdates the compiled source and packaged documentation.

Haiku R1 beta6 x86_64, owned standalone VM, installed HPKG:

- 55 installed native file-operation checks and nine native selection-summary
  checks pass. Cached totals cover duplicates, unknown/negative sizes, filters,
  zero-byte files, overflow, parent exclusion, folders and links.
- Installed detail and icon selection show the exact known file sum and separate
  folder/link counts. No recursive directory traversal is performed.
- Empty-space folder actions and the top-level This folder submenu work in both
  views, preserve multi-selection and window size, and resolve Properties to the
  current directory, not the clicked child. Actual New folder and Copy/Paste
  retain the correct target and byte-identical contents. A folder replaced while
  the menu is open is rejected before action; both trees remain untouched.
- Commander no longer has left/right labels. Both palettes and views show the
  active outline, subtle address tint and file-area tint; selection and active
  ownership survive switches. Simple remains neutral.
- Frame sampling confirms intermediate border colors, exactly one address/body
  tint change per switch and settled pixels after the transition. The runner is
  bounded to six messages over 150 ms; only the outer inset is animated. Rapid
  switches supersede stale frames. This is not a physical display frame-rate or
  universal hardware performance guarantee.
- Final installed selection stress: 2,000 entries, 120 pointer events/second for
  five seconds, confirmed nonempty selection. Direct window-message latency:
  details 505 samples, p95 4,543 us, max 32,207 us; icons 510 samples,
  p95 2,440 us, max 40,055 us. No failed replies.
- Actual 0.16.2 -> delivered 0.16.1 -> 0.16.2 package roundtrip passes. Settings,
  places, searches, source/copied/nested file contents, permissions and a symbolic
  link remain unchanged. Selection totals and folder Properties work after
  restart. Recovery uses the retained package, not restored user data.

Installed executable SHA-256:
`92cb847af7d332eb87497a487914c780f940fdcee757751c2e4f6358beb1aa18`

HPKG SHA-256:
`7c5c2ec5510b4ee8f6f2b6e00ff0134688eb49af284fa6792b2f7521dc312021`

Evidence: `reports/listi-selection-size-20261115`. Earlier UI harness probes
used an unsupported menu End key, a child-window assumption for an inline notice,
and an off-border pixel coordinate. Corrected final probes verify the actual
interaction and settled pixels; those failed probes are not counted as passes.
