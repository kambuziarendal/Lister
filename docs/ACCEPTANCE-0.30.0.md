# Private 0.30.0 acceptance, 15 November 2026

Scope: date/size/extension filters for local search, advanced/live BFS results and
saved criteria. Product direction and the next two steps are recorded in DESIGN
and ROADMAP: primarily DOpus-inspired file work, not a ROX clone.

## Native checks

Owned isolated Haiku hrev59866+79 x86_64 VM, continuing the 0.29 overlay under the
same sole owner. Immutable 0.28/0.26 backings were not booted or modified. No
network fixture or external service was needed for this search-only increment.

- Targeted search suites passed before the final build.
- Full native audit: **1,292 PASS, zero FAIL**, 35 suites plus 55 app self-tests.
- 50 new filter checks and four added live-query checks cover combined conditions,
  literal/BFS parity, traversal through excluded directories, link exclusion,
  UTF-8/case/compound extensions, zero/exact sizes, units/overflow, valid/invalid
  calendar dates, inclusive boundaries, birth time, and 23/25-hour Oslo DST days.
- Filter-only search and saved definitions, format-1 migration, format-2 refusal
  of missing filters, wrong types, atomic disk persistence, live growth/shrink
  membership, cancellation and existing identity checks passed.

## Installed app

The installed binary was byte-identical to the audited build:
`8e2ea9932183c631ea439a52d3dedd1ccf8c5cdde279bad17b76d177cbf8b1ad`.
Actual QMP keyboard/mouse input, native result/status reads and screenshots tested:

- Combined extension, inclusive byte-size and one-day date interval returned
  exactly the intended nested PDF, not the different date, small file, directory,
  different extension or symlink. The result retained its full original path.
- Enter submits; blur does not. Reversed sizes and invalid dates are rejected
  without erasing the previous selection. Creation time differs from modification.
- Clear filters preserves the name. Save, close/reopen, actual saved-menu selection
  and fresh Search retained all criteria, without restoring cached result rows.
- Advanced filtered Live, Escape, restart and Stop worked. Light 910x630 and dark
  760x560 search windows were visually inspected, with readable controls/results.
- 0.30 -> retained 0.29 -> 0.30 package roundtrip preserved sampled source bytes,
  settings, saved filters and editor recovery copies. The old app displayed the
  unsupported-format notice and disabled saving, leaving the criteria bytes
  untouched after an attempted Save and clean quit. Upgraded saved search again
  returned the intended result. See SEARCH-FILTERS-0.30.0.md for this boundary.

## Evidence and limitations

Private runtime evidence: `reports/listi-search-filters-20261115` under the Kambuzi
root. It contains full-native.log, targeted logs, installed UI screenshots,
rollback.log, source/package identity and the final local artifact receipt.
First DST test used a POSIX TZ rule string unsupported by Haiku's ICU backend;
it was corrected to the real Europe/Oslo ID, and both DST assertions passed.
Two UI harness assumptions were corrected: End did not select the last native
menu item, and stripping every trailing quote misread a formula's literal quote.
Actual menu clicking and exact outer-quote parsing passed. Early logs are retained;
these failed attempts are not counted as successful UI tests. No product-code
change was needed after the full regression run.

No physical-machine claim, broad performance benchmark, remote/archive search,
MIME/content filter, relative-date preset or snapshot implementation. No package
uploaded, public release or branch push. Batch rename and configurable selected
file actions are queued, not delivered by this version.
