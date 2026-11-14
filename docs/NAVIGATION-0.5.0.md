# Private 0.5.0 navigation candidate

14 November 2026. Internal only. No public release or Depot change.

## Behavior and ownership

Each Pane owns a `NavigationHistory`, bounded to 64 successful locations for
this session. Neither credentials nor file selections are stored there. The
existing location parser rejects authority-embedded credentials. Authentication
remains in the separate session connection store. No settings migration.

Path buttons `<` / `>` navigate that pane. Native Command+[ / Command+] apply
only when focus belongs to a browsing pane, not a text field or inline question.
The existing scanner generation identifies the pending visit. History commits
only on a successful, current completion, using the resolved location. Failed
and superseded scans do not move the committed cursor. Back from a failed target
first reloads the last valid place. A further explicit action is needed if that
place is also unavailable; there is no automatic retry loop.

Same-place scans and Refresh preserve the forward branch. A successful new
place truncates that branch. History traversal reloads contents and does not
restore old selections. Existing filter reset and folder-display overrides
remain. Trash now shares the same request lifecycle, with its actual volume
Trash address resolved by the worker. File-operation snapshots are unchanged.

Native UI verification also exposed an existing connection/name-form defect:
BTextControl invokes its action on focus loss. Address/username/password fields
therefore used to submit before credentials were complete. A new-folder name was also accepted by clicking Back. Field blur now does
nothing; the shared `ExplicitSubmit` filter accepts Enter, and form buttons
retain explicit acceptance. Inline answers retain their question token and
frozen target even when navigation changes focus. This is a safety fix,
not a new authentication/storage model.

## Regressions

`c++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/navigation-history.cpp -o history-tests`
then `./history-tests`: 23 checks, portable and native Haiku. Covers branch
truncation, resolved same-place aliases, bounded retention, independent panes,
failed opens/traversals, superseded and duplicate completions, cancellation,
resolved Trash and the volume overview.

Native Haiku UI lanes exercised local mouse/keyboard history, independent
panes, branch/Refresh/alias behavior, failure recovery, filter and selection
reset, folder overrides, text editing, Go Parent/Disks/Trash, tree selection,
asynchronous ZIP invocation, ZIP/tar inner folders, SMB and SFTP. Synthetic
server cases covered an offline port, permission denial and a deliberately
three-second SFTP response released after cancellation. The late response did
not change the selected path or add a history entry.

A paused copy conflict survived both panes navigating backward and changing
source roles. Explicit Replace still wrote only the original frozen destination.
Both trees, preview, advanced connection fields and conflict controls fitted
1024x650 in actual light/dark palettes. Navigation also worked with an 18-point
chosen file font, subsequently restored to 12.

Initial form-focus failures are retained in the private report and superseded
by explicit blur/Enter/button regression checks. A delayed fixture assertion and
menu-close timing assertions were corrected to wait for actual completion;
those early failures are not counted as passes. Installed package qualification
is recorded separately after packaging.

## Boundaries

No favorites, persistent session locations, tabs, universal undo, new protocol,
remote edit or public publication. Existing 0.4.0 transport and file-operation
limits still apply. This is our own Haiku implementation, not copied code,
controls or workflow from Dolphin. Next planned need: saved work locations.

## Installed acceptance receipt

Code/package source: `e9b48f9dc4034e5a8a92e11425559c161d613353`.
This receipt is a later documentation-only addition, not a repack of that source.
All 41 native source/resource/build inputs matched the build guest; all 64 tracked
package inputs matched the isolated packaging tree.

Final installed executable SHA256:
`6c0438775dc9733bd746d62d80483a570d0064462f45f7050014422f0cf89af4`.
HPKG SHA256:
`ebfe4f982394ab1f73dc008468870e7df29c3ec126f60a8324764dfdd13f635b`.
Private installer ZIP SHA256:
`0f87f2b3f00a4889b5e72944a93e1b1055b4cabce84bdee37aab7d5613e94117`.

Passed on the installed executable: 55 local checks and 96 archive/network
checks. The separate native history executable passed 23 checks against the
same history header: 174 native checks in these lanes, not 174 GUI scenarios.
Earlier fault-injection/preview checks were not rerun or included in this count.

Ten additional installed-UI assertions checked version, session-only history,
independent mouse Back, keyboard Forward, preserved pending name, no blur side
effect, explicit acceptance against the frozen target, complete credential entry,
explicit connection submission and Back from SMB to local. The final two form
fixes were reproduced against the installed binary, not inferred from unit tests.
Earlier broader UI lanes exercised unchanged navigation code; final package
checks do not claim a repeat of every earlier screenshot or scenario.

Actual package sequence: 0.4.0 -> 0.5.0 -> 0.4.0 -> 0.5.0. Installed binary
hashes matched both retained versions. Settings hash remained
`1a1121486991a3979ae3984e3d19a5749cb6599f372ccc5581a551f5ea9962b4`
through all package operations and installed UI checks. Owned test-file contents
and created directories survived rollback. Re-upgraded 0.5.0 launched, retained
the existing folder override and started with empty session history. The old
0.4.0 app was not launched during rollback. No filesystem/settings snapshot was
restored.

Recovery: install the retained private 0.4.0 HPKG without reverting file data.
Native qualification used Haiku beta6 hrev59866+79, x86_64/GCC 13.3, in the owned
QEMU overlay with synthetic loopback servers. No physical-machine, external NAS,
FTPS or long-duration qualification is implied. App and owned fixtures stopped;
inherited VM retained. No push, public release or Depot update.
