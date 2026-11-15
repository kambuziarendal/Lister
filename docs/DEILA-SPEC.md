# Deila: staged product plan and threat model

Status: long-term planning draft v0.2, reviewed 15 November 2026.
No implementation, dependency, protocol or cryptographic format is approved by
this document.

Companion to the Listi design documents. Deila is a separate product that Listi
may use through an optional adapter. Listi must never depend on Deila to perform
ordinary file management.

"Deila" is Icelandic for "to share".

## 1. Purpose

Deila lets people share folders peer to peer, encrypted, and browse them like a
server folder. A recipient opens a link, lists directories without downloading
all content, and fetches only the files or ranges they use.

The long-term product may support public links and private groups. The first
implementation target is deliberately narrower: one publisher, one secret link,
one read-only drive and command-line clients.

## 2. Fixed product boundaries

- Content and metadata are encrypted in every sharing mode.
- Deila is cross-platform. Targets are Linux, Windows, macOS, BSD and Haiku.
- No central service is required. An optional always-on seed may improve
  availability, but must not become a control plane or mandatory dependency.
- The core is usable independently through a CLI before any Listi integration.
- Recipients are told that peers can see their IP addresses. Deila does not
  promise anonymity or traffic-analysis resistance.
- Shares are read-only to recipients in the first stages. Multi-writer editing
  and conflict resolution are outside the initial scope.
- A compromised endpoint is outside the threat model.

## 3. Staged scope

### Stage A: decision spikes only

No product code or persistent format is created in this stage.

1. Build throwaway transport probes for a mature swarm engine and a known-peer
   engine.
2. Measure both on real machines, including Haiku.
3. Record cold and cached directory-listing latency, time to first streamed
   bytes, range prioritisation, memory, idle CPU, NAT behaviour, reconnect and
   build/distribution effort.
4. Use the evidence to decide transport, core language and the smallest stable
   foreign-function boundary.
5. Review all proposed cryptographic constructions before accepting a format.

### Stage B: narrow MVP

- One publisher and one immutable read-only drive version.
- Secret-link access only. No public discovery, groups or mutable roster.
- CLI publish, inspect, list, fetch and verify commands.
- Small encrypted per-directory manifests for lazy listing.
- Ranged and streaming fetch of a selected file.
- Local bounded cache with explicit size limit and clear-cache command.
- Exact test vectors, hostile-input tests and reproducible cross-platform
  fixtures.
- No Listi integration, standalone browser or virtual filesystem yet.

The MVP is successful only when a fresh recipient can open a secret link, list a
directory quickly, stream part of a large file, resume after interruption and
verify exact bytes without downloading the complete drive.

### Stage C: reliable private sharing

- Mutable signed root pointer and version history.
- Offline listing of the last verified state.
- Optional always-on seed with bandwidth, disk and retention limits.
- Publisher key backup, documented recovery and secure device migration.
- Signed application updates and a documented compromise/revocation procedure.
- Minimal standalone browser after the CLI and format are stable.

### Stage D: groups

- Signed membership roster and explicit invite approval model.
- One published drive per member, merged as separate subtrees to avoid write
  conflicts.
- Epoch key rotation on removal, with clear notice that downloaded old content
  cannot be revoked.
- Membership privacy review before claiming that the roster is hidden.

### Stage E: public mode

- Explicit public-share choice. Secret sharing remains the default.
- No global index in the first public release.
- Local blocking controls and abuse-reporting documentation.
- External security review and legal review are required gates before release.

### Stage F: optional Listi integration

- A `deila://` provider is implemented as an optional module after the standalone
  product is stable.
- Listi communicates with a local authenticated daemon API. It does not link the
  network engine into its core.
- Removing Deila leaves all ordinary Listi functions intact.
- Integration must preserve Listi's frozen source/target semantics, cancellation
  and resource limits.

## 4. Access model, long-term

Every share uses one encrypted format. Modes differ by key distribution.

1. Secret link: default. The link is a bearer secret shared deliberately.
2. Public: the link is intentionally published.
3. Group: access material is distributed only to members in a signed roster.

Revocation cannot erase content already downloaded. Secret-link revocation needs
a new version with new access material. Group removal needs a new epoch. The UI
must explain these limits before sharing.

The exact link encoding is an ADR decision. Candidate designs must demonstrate
that key material is not sent to trackers or DHT, and is not accidentally placed
in HTTP logs, crash reports, shell history or ordinary telemetry.

## 5. Threat model

Assets include file content, file names, directory structure, access material,
group membership and publisher identity keys.

| Adversary | Threat | Required mitigation | Residual risk |
| --- | --- | --- | --- |
| Network observer | Reads or fingerprints traffic | Encrypted content and metadata; no claim that transport obfuscation provides security | Timing, sizes, peer IPs and protocol fingerprint remain visible |
| Random swarm peer or DHT crawler | Enumerates or downloads shared data | Opaque encrypted manifests and content; secret-derived discovery only after review | Sees peer IPs, opaque identifiers, sizes and timing |
| Holder of a leaked link | Reads the share | Bearer-secret warning and rotation workflow | Past access and downloaded content cannot be revoked |
| Removed group member | Reads new content | Signed roster and epoch rotation | Keeps old keys and downloaded data |
| Malicious publisher or member | Sends hostile metadata | Memory-safe parsers where practical, strict validation, hard limits and fuzzing | Parser defects remain possible |
| Impostor publisher | Forges an update | Pinned identity and signatures on pointers, manifests and rosters | First-link social engineering |
| Public-mode abuser | Shares illegal or infringing material | No initial global index, local blocking and legal review | Swarm content cannot be centrally removed |
| Resource attacker | Exhausts bandwidth, CPU, memory or disk | Per-peer limits, cache quota, bounded parsing, opt-in seeding and rate limits | Basic denial of service remains possible |
| Compromised release path | Ships malicious client updates | Reproducible builds where practical, signed updates and key-recovery procedure | Compromised signing authority remains serious |

Out of scope: malware on the endpoint, anonymity, traffic-analysis resistance and
revoking data that another person already possesses.

## 6. Cryptographic review gates

No primitive or protocol is accepted merely because it is named here. Each item
requires an ADR, test vectors and external review before a stable format.

- Use an audited library such as libsodium or a reviewed equivalent.
- Candidate content design: fixed-size independently authenticated blocks for
  random access.
- Candidate hierarchy: drive-level material and independently derived per-file
  material with a new random file identity for every content version.
- Nonce construction and domain separation must be specified and reviewed. A
  test must prove that no key and nonce pair is reused.
- Encrypt names, sizes and directory structure. Wire identifiers are opaque.
- Sign root pointers, manifests and later group rosters with a reviewed signature
  scheme.
- Protect local private keys with platform facilities where available, with a
  reviewed passphrase fallback and explicit backup/recovery flow.
- Version the format from day one. Every format change includes migration rules
  and test vectors.
- Study Cryptomator and gocryptfs as reference designs. Do not copy a protocol
  without checking its threat model and licence.

## 7. Architecture direction, not yet a dependency decision

- `libdeila`: reviewed format, manifests, drive model, links and later group
  roster.
- `deilad`: networking, cache, seeding and a local authenticated API through a
  Unix socket, named pipe or authenticated loopback endpoint.
- Clients: CLI first, standalone browser later, optional Listi provider last.
- Directory tree: small independently encrypted per-directory manifests to keep
  listing lazy and bounded.
- Root: a small mutable signed pointer only after the underlying transport's
  actual limits and semantics have been verified.

No transport engine, crypto library or language becomes a production dependency
until its spike and ADR are approved.

## 8. Untrusted input rules

All network metadata is hostile.

- Reject `..`, absolute paths, NUL, reserved platform names and invalid encoding.
- Detect case-folding and normalisation collisions before materialising files.
- Never follow a symlink supplied by a manifest.
- Bound link length, manifest bytes, entry count, name length, path length,
  directory depth, redirects, peers, concurrent reads and decompressed output.
- Fuzz link and manifest parsers and keep a permanent regression corpus.
- Prefer a memory-safe language for exposed parsers. If a native boundary is
  needed, keep it narrow, ownership-explicit and fuzzed from both sides.
- Fail closed on unknown versions, invalid signatures and resource-limit hits.

## 9. Product and recovery requirements

- Cache has a visible quota, current usage, eviction policy and clear control.
- Publisher keys have an encrypted backup and tested restore flow before users
  can depend on a share.
- Moving to a new device must not require exposing raw keys in ordinary files or
  chat messages.
- The UI distinguishes unavailable publisher, missing seed, invalid signature,
  revoked epoch, wrong secret and local resource limit.
- Logs and diagnostics redact links, secrets, private keys and decrypted names.
- Background seeding is off until deliberately enabled and has rate, schedule,
  disk and battery controls.

## 10. Decision log

Decided:

- Deila is long-term work and remains separate from Listi.
- Listi integration is optional and last, not an early architecture driver.
- The first MVP is secret-link, single-publisher, read-only and CLI-first.
- Groups and public mode are later stages.
- Transport evidence comes before language selection.
- External security review is required before a stable format or public release.

Pending explicit human approval:

- Whether Agder EDB may run an optional always-on seed.
- Licence for Deila.
- Transport engine after measured spikes.
- Core language and foreign-function boundary after measured spikes.
- Exact cryptographic format, link encoding and local key protection.
- Group invite approval model.
- Legal reviewer and timing before public mode.

## 11. Working rules

- Ask before cryptographic design decisions or new dependencies.
- Record protocol, security, privacy and compatibility decisions in ADRs.
- Every parser and format change includes tests. Stable format changes also
  include test vectors and migration behavior.
- No implementation work begins from this planning document alone.
- Spikes are disposable and must not silently become the production format.
- Report what was measured on Haiku separately from what merely compiled.
- Keep the binary and operational model small enough for ordinary users to
  install, understand and recover.

