# Private Listi 0.4.0 acceptance, 14 November 2026

Source/package commit: `93a0138` on local `work/smb-first`. This receipt is a
subsequent documentation-only change, not a different compiled/package artifact.

- Executable SHA256: `4b0f72581d6e333987532b3b91529248bd212825540b8654747e243a69f13a4f`.
- HPKG SHA256: `fdf26147df6eaebdca539b9108c003b5590ed7812a5f846aee31ed6a3e2a904e`.
- Private installer ZIP SHA256: `b9ef5ae81b9bfad39b481aeddf3bc1637ae78981dc02ddea085f5d21218d0ef9`.

Native Haiku beta6 hrev59866+79, GCC 13.3, isolated two-vCPU QEMU/KVM guest.
39 compiled source/build inputs matched the native build tree. All 56 tracked
source, documentation and packaging files matched the package staging tree.
The source archive is exactly the package commit, not a later working directory.

## Passed

- Installed executable matches the build. On that installed binary: 55 local
  filesystem/selection tests and 96 archive/network tests against synthetic SMB,
  SFTP and FTP servers. Separately, the same candidate's native object files pass
  9 deterministic operation fault tests and 6 native preview/ownership tests.
- Installed app launch/version, selected PNG copy with byte comparison, inline
  mkdir, connection keyboard focus/Escape, persisted per-folder mode and trees.
- Reproduced the expanded-layout case in the installed app: both trees, preview,
  pending copy conflict and advanced connection fields fit 1024x650 in real light
  and dark system palettes. Escape closes only the form; a second Escape aborts
  the conflict without replacing the original file. Window size remains stable.
- Development UI checks on the same executable cover folder overrides across
  restart, Use default, independent tree click/keyboard navigation, Tab, newly
  created tree routes and 12 thumbnail/preview navigation cycles. Synthetic PNG
  thumbnail and full preview were visually inspected.
- Actual 0.3.0 -> 0.4.0 installation, rollback to retained 0.3.0 and re-upgrade to
  0.4.0. Settings SHA256 stayed
  `16168ea688cb977f6cba5c3e715e0c1f88e0601f45f2be733ab4eb6073d68179` throughout
  package changes. Re-upgraded app launches, retains the folder override, and
  copied files/created folders remain intact. One Listi launcher, no stale Lister.
- ZIP CRC and extracted HPKG hash verified. It contains only the HPKG and a short
  Norwegian installation/limitation note. No private key material in source.

## Recovery and boundaries

Reinstall the retained private 0.3.0 HPKG to roll back the application. Do not
restore old settings or filesystem data. An old application may discard newer
preference fields if it rewrites settings; the verified package rollback kept
settings bytes unchanged without running the old app. File operations are not
undone by a package rollback.

The installed UI quit cleanly. Owned fixture listeners were stopped; the inherited
Haiku VM was left intact. No public push, Depot update or release. No physical
hardware, FTPS/external NAS qualification, prolonged soak or Dolphin parity claim.
Known capability/race limits remain in INTERNAL-CANDIDATE.md and ROADMAP.md.

Evidence and canonical artifacts are private under
`reports/listi-ui-package-20261114/` in the Kambuzi workspace. Failed control-build
single-file archive tests are retained alongside the corrected passing results.
