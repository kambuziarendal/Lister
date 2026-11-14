# Open with, private 0.11.0 candidate

Select one local regular file, then Files > Open with, or Command+Shift+O
(Alt+Shift+O with the default Haiku keyboard map). The native dialog displays
stored MIME metadata, or an inferred type without saving it. Installed native
applications registered for the type are shown, with a valid preferred app first.
Choose application opens a native file picker. Selecting an executable does not
launch it: Open once is the separate explicit action. A list double-click also
opens it. The existing system-opening and archive navigation commands are unchanged.

No MIME attributes, per-file preferred apps, global defaults, settings schema or
association database entries are written by this feature. The receiving app may
of course edit a document after opening it. Application registration by Haiku's
roster is managed by the OS, not an association edit performed by Listi.

The dialog retains the original pathname and open file identity. File replacement,
removal, rename, mode/content/metadata timestamp changes and application changes
are rejected before dispatch. Opening uses B_REFS_RECEIVED, never a shell command.
Application choice validates the native app signature, executable mode and refs
support. Listi itself, plain executable scripts and argv-only programs are refused.
Discovery and launch run off the UI thread. Quit is vetoed while a request runs,
including app-level Quit, so the main workspace cannot vanish beneath a pending
request. Closing an idle dialog closes its owned file picker too.

This is not an atomic lock on another application's later file access. An external
process can still replace a pathname after our last check or after delivery.
Success means Haiku accepted the native launch/reference request, not that the
receiving program successfully rendered the document. Tests separately verify
actual receipt and a real StyledEdit window.

## Boundaries

- One local regular file per dialog. Document symlinks require opening the target
  directly; remote/archive members must be explicitly copied locally first.
- No batch open, stored/global association editing, MIME editing, automatic upload,
  or claims of full Tracker file-type coverage.
- Application discovery is bounded to 512 database entries; Choose application
  remains available for other installed native programs and unknown file types.
- UI follows system colors and supports keyboard, menu and native picker paths.
- Existing application settings, places, saved searches and metadata are retained.

## Verification and recovery

`tests/open-with.cpp` exercises MIME precedence, read-only inference, registered
and missing applications, fresh/running native receipt, unusual filenames and
stale-source/application refusal. `tests/open-with-receiver.cpp` is the synthetic
receiver and is not included as an installed application in the package.
Installed UI, regressions, exact predecessor roundtrip and final artifact hashes
are recorded in ACCEPTANCE-0.11.0.md after qualification. Until that receipt exists,
this document describes the implementation, not completed package acceptance.

Quit Listi before installing the retained private 0.10.0 HPKG for rollback. No data
migration or settings restoration is required. Never restore stale user documents.
