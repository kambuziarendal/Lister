# Private Listi 0.11.0 acceptance, 2026-11-14

Source/package: `36ea3ed` on `work/smb-first` (implementation `9017b5c`).
All 108 tracked inputs matched the native build source and fresh packaging stage.
Native Haiku R1 beta6 x86_64 in the inherited isolated QEMU overlay. No physical
machine qualification, public push, public release or license change.

## Delivered candidate identity

- Installed/build executable SHA256: `30481ee2de96bd79da5a4a8a28defaa7e6af9d4e61c1a2c17e936334f86a30b9`
- HPKG SHA256: `9061a6cbfee46dda8405df640403d90d8261908f2fd7ea1b37e2b3ea9e2a552a`
- Private ZIP SHA256: `481ab549f95b6b3802bb1770808c47ab676086da1f65660599c521d18f1c5e27`
- Delivery is a separate channel receipt, not inferred from these local artifacts.

## Verification

175 native checks: 25 Open with, 54 properties, 34 clipboard, 7 attribute-copy,
and 55 installed self-tests. The final GUI-only deduplication change was then
qualified on the exact final installed package with 23 installed UI assertions,
6 delayed-receiver assertions and 16 roundtrip/recovery assertions.

Actual mouse Files menu and keyboard entry, native application picker and explicit
Open once, real StyledEdit document contents, panel-navigation isolation, replaced
source refusal, unknown MIME fallback, compact 560 by 330 light/dark system
palettes, picker cancel/reopen/owner disposal and app exit passed. Visual inspection
confirms three unique rows after choosing an already listed program.

Bughunt fixed duplicate rows after repeated explicit application selection.
Native fixtures verify direct refs delivery of Unicode/shell-like filenames to
fresh and already running applications, missing/invalid/recursive app refusal,
and unchanged type/preferred-app metadata. The delayed real receiver received its
file even after Listi exited; the main-title probe took 0.171 seconds during its
startup delay. This is one message latency sample, not a performance benchmark.

Exact 0.11.0 to 0.10.0 to 0.11.0 installation roundtrip passed, including old-version
preferences Save, semantic preference comparison, byte-identical places/searches,
file payloads and unchanged MIME/user attributes. Native properties and Open with
both worked again after re-upgrade. Exact installed hash verified after cleanup.

Earlier exploratory scripts incorrectly assumed an absent-app error string, that
StyledEdit's document was Window 0 (its hidden file picker occupies that index),
and that Haiku Launch waits for delayed application registration. Logs are retained;
corrected assertions use native process/window identity and actual receipt.
The delayed-start test does NOT establish a long in-flight roster wait or test the
busy quit veto: Haiku accepts dispatch before the recipient registers.

## Scope and recovery

See OPEN-WITH-0.11.0.md. One regular local file, one-time app choice only.
MIME editing, saved/global associations, batch open, symlink document targets and
remote/archive opening are not qualified by this increment. No schema migration.

Quit Listi and install retained private 0.10.0 HPKG to roll back. Its exact executable
is `5f68e77122cbd8a502735772390aca5b8fc394c8b44937924c0bcd969f96fcf3`.
Never restore stale documents or settings to undo this executable-only upgrade.

Evidence: `/home/lobster/kambuzi/reports/listi-open-with-20261114/`.
App, StyledEdit and owned receiver fixtures stopped; filesystem synced. Inherited
VM retained for subsequent work. Next bounded workflow: explicit drop semantics;
association editing remains a separately documented file-type gap.
