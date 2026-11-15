# Systematic bughunt, private candidate 0.25.0

Baseline: 0.24.0 runtime 6e7035f, repository 89cffc6.
Evidence: /home/lobster/kambuzi/reports/listi-bughunt-20261115.

## Reproduced and corrected

1. Archive/network downloads did not retain local destination identity across
   queueing and staging. Replacing a target directory, including with a symlink,
   redirected output. Local target directories are now frozen and checked through
   staging, recursion, conflict handling and publication. Checks track only the
   active ancestry, not an ever-growing quadratic directory map.
2. A destination file edited while downloading could be treated as a fresh
   replacement candidate afterward. Its full local metadata snapshot is checked
   before the staged file enters the normal guarded copy path.
3. Local upload sources and virtual drops lacked the identity guards used by
   local copies. Local source/destination guards now survive mixed-provider
   snapshots. Archive clipboard selections additionally retain container-set
   fingerprints and refuse changed queued sources.
4. libarchive listings silently merged duplicate normalized member names and
   file/directory collisions. TAR/ZIP extraction could report success while
   dropping data. Whole listings now reject these ambiguous namespaces before
   publishing any extraction. 16 adversarial native cases failed before the fix.
5. Equivalent URL host casing and percent escaping bypassed remote self-copy
   protection. Transfer identities are normalized, including explicit default
   ports. Deterministic network fault tests retain transactional failure coverage.
6. Ordinary Copy/Paste/drop still reached an obsolete blanket archive read-only
   path despite the archive editor. The UI now prepares guarded archive jobs.
   Expanded archive folder behavior and limitations are documented separately.

## Verification discipline

Native tests link actual Haiku objects. New regressions supplement, not replace,
the full existing suite. Live synthetic SMB/FTP/SFTP services and an SFTP server
that closes its transport mid-download run behind task-owned loopback reverse
forwards; the guest has restricted user networking. No production service or
real user files are used. Final counts, installed-package identity, UI checks and
recovery evidence belong in ACCEPTANCE-0.25.0.md after completion.

## Retained boundaries

Path-based local recursion is not a descriptor-relative defense against every
hostile ancestor replacement race. Remote metadata is not transactional, and
same-size edits without reliable timestamps may be undetectable. SMB calls are
serialized and cancellation can wait for native timeouts. Large archives are
still repeatedly indexed/read during rebuilding. No claim of unbounded-scale,
physical hardware or every external archive/server-family qualification is made.
