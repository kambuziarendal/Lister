# Listi 0.9.0, private clipboard candidate

2026-11-14. No public release, push or Depot publication.
Source/package commit: `a55817dc3e454dbeb65965143b0da03f2bb741f7`.
Implementation commit: `2af283a`; follow-up closes discarded queued jobs' leases.
This receipt and updated coverage matrices are a subsequent documentation commit.

## Qualified scope

Native local Copy/Cut/Paste, both directions with actual Haiku Tracker. Mouse
commands use a three-item Files > Clipboard submenu. Command+C/X/V is the file
clipboard; direct panel copy uses Command+Shift+C/F5 and View uses Shift+V/F3.
Text fields retain native editing. See CLIPBOARD-0.9.0.md for deliberate limits.

**201 native checks passed:** 34 clipboard, 7 BFS metadata, 9 virtual-operation
fault injection, 55 installed local-operation tests and 96 installed archive /
network tests. Clipboard and metadata helpers link the candidate operation
objects. The fault suite compiles the candidate sources with deterministic
provider faults. Installed CLI tests ran on the final executable hash below.

Actual UI qualification includes:

- Tracker and Listi copy/cut/paste in both directions, multiple files, active
  target panel, mouse submenu and keyboard. Tracker updates moved references.
- Direct panel copy retained, text cut/paste without starting file work, viewer
  shortcut, light/dark controls, compact 1024 x 650 main window.
- Frozen conflicts survive navigation, newer clipboard content and source
  replacement. Neither a replacement source nor an unrelated target is touched.
- Duplicate pending paste is refused. Cancel all ends both active and queued
  clipboard work. Cancelled cuts retain sources and can be pasted successfully
  afterward; successful moves consume only their own cut entries.
- Actual final 0.9.0 -> retained 0.8.0 -> exact final 0.9.0 package roundtrip.
  Old app launches and really saves preferences. Preferences compare semantically,
  excluding transient BMessage routing fields. Places/searches remain byte-exact.
  Native clipboard and original payloads survive; mouse paste succeeds afterward.
- Saved place, saved query and live BFS updates work after re-upgrade.

## Rejected attempts and recovery

A source review found that Cancel all discarded queued jobs without terminal
messages, which could retain a clipboard lease. Fixed in `a55817d` and qualified
with active and queued cancellation followed by successful retries.

The first attempt to replace the installed 0.9.0 with changed same-version
package bytes was rejected by packagefs (`Name in use`). The old executable
remained installed. A prematurely started UI run reproduced its known lease bug;
that run does not qualify the final package. Source files were unchanged, queue
idle verified, and only that owned stale app was stopped. Installing retained
0.8.0 followed by the final candidate succeeded. Final UI startup explicitly
asserts executable SHA256 before launch. All final installed qualification and
the roundtrip above used the final hash. No failed result is counted as a pass.

A UI probe assumed the wrong internal name and byte range for the native name
field. Screenshot/readback identified the actual control; only unresolved checks
were resumed. Exact `text roundtrip` readback and subsequent controls passed.

## Artifact identity and recovery

93 tracked source/build/resource/package/documentation inputs matched the native
build tree and fresh package stage. Final executable in stage and installed:
`216ef6bb354f2988cea1b1041214b1fc687cdeef2e571aecc270e26590417e91`.

`Listi-0.9.0-1-x86_64.hpkg`:
`1ecf4bf4a9d199aa1f6874c76867cafc2b18affb499e26991231631bfbcf88c3`.

Retained 0.8.0 executable:
`ae662aca4fd6414073c151af3e3a9704bf02e3f99b68bcb4df694632134622e6`.

Quit Listi before installing the retained private 0.8.0 HPKG for rollback.
Reinstall this exact 0.9.0 HPKG to upgrade again. Do not restore old user settings
or file contents. Original settings/stores are retained separately in the VM
fixture; no persistence schema changed in this increment.

Local evidence: `/home/lobster/kambuzi/reports/listi-clipboard-20261114/`:
`native-final.log`, `installed-native.log`, `installed-ui.log` with resumed
`installed-ui-resume.log`, `roundtrip.log`, `install-final.log`, package input
manifest and original screenshots. Earlier rejected attempts are retained in
`initial-package/` and the original package installation log. The final source
archive identifies the package commit, not the later documentation commit.

Not a full Dolphin/Tracker equivalence claim, physical-hardware certification,
malicious filesystem-race proof, or crash-safe persistent operation journal.
