# Listi 0.16.1 private acceptance

Compiled source: `f3832ff` on `work/smb-first`; 142 tracked inputs match the
native source manifest. No public publication.

Haiku R1 beta6 x86_64, owned standalone VM, installed HPKG:
- 55 installed native file-operation/model checks plus six native 10,000-entry
  selection-model checks pass. Twenty full selections fell from 9.36 seconds
  to 0.168 seconds in the same native fixture.
- Detail and icon rectangle selection, Ctrl extension, Escape restoration,
  exclusion of the parent, inter-cell start, exact clipboard copy, edge scrolling
  and cancellation on navigation pass.
- Both palettes: settled pixels after release exactly match a forced repaint;
  no leftover border. Selection survives resizing.
- Simple: neither bar, horizontal only, vertical only and both axes pass.
  Icon/large-icon/thumbnail navigation restores/removes the vertical bar;
  column resizing and repeated window resizing reevaluate overflow correctly.
  Commander controls and selection survive workspace switches.
- Final installed stress test: 2,000 entries, 120 pointer events/second for five
  seconds, confirmed nonempty selection before release. Direct window-message
  latency: details 504 samples, p95 5,249 us, maximum 32,573 us; icons 517 samples,
  p95 2,691 us, maximum 35,517 us. No failed replies. These are native UI-loop
  measurements, not a physical display frame-rate or hardware latency guarantee.
- The earlier candidate's reproducible 1.2-second stall was not accepted.
  Detail field formatting/font-state queries were reduced and overdue ticks
  no longer replay consecutively after a slow paint.
- Actual 0.16.1 -> delivered 0.16.0 -> 0.16.1 package roundtrip passes. Settings,
  saved places/searches, copied files, archive output, permissions and a symbolic
  link remain unchanged. The final installed selection and hidden-bars checks
  pass after restart. Recovery uses the retained package, never stale user data.

Installed executable SHA-256:
`cffa9bb34c8059e6abfe13e09b3c8d3cd84139db7613d7c6fa1fca98589473ac`

HPKG SHA-256:
`db422a14138e071855115335977ff325da40b24f77ff699fc7ca5abbb25fb309`

Local evidence: `reports/listi-selection-performance-20261115`.
This acceptance-only document postdates the compiled source and packaged docs.
