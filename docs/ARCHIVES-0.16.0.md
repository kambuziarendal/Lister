# System-backed archive modules

All browsing and extraction take place inside Listi, using its existing queued,
staged copy operations. No Expander or other archive GUI is launched as fallback.

The first adapter is `LibarchiveBackend`, loaded dynamically from the optional
HaikuDepot `libarchive` package. `ArchiveBackend` separates engine operations from
panes and transfers. Additional adapters can be added without replacing those
flows. This is not a new package manager or a public third-party plug-in ABI.
Missing libraries are retried on subsequent worker operations. Compression
helpers used by system libarchive are optional system packages too:

- `lz4`: LZ4 decoding when libarchive lacks its compiled-in filter.
- `zstd_bin`: Zstandard decoding when libarchive lacks its compiled-in filter.

Install packages through HaikuDepot. Listi identifies these missing helpers in
its error message; retry the same archive afterward. No restart, bundled engines,
automatic package installation or separate archive window is required.

## Scope

Read-only local archives, internal directory browsing, entry extraction and
cancellation. Extract with Commander: archive in one panel, destination folder in
the other, then Copy to other panel (Command+Shift+C). The native Tracker
clipboard remains local-files-only; clipboard extraction in Simple is a separate
follow-up, not claimed as working in this version. Detection uses content, with recognized extensions giving a visible
error rather than silently launching another application. ZIP-derived JAR, EPUB
and CBZ are exposed as their archive contents, not rendered documents.

The first format matrix covers ZIP/7z, RAR and RAR5, tar, newc CPIO, BSD ar,
ISO9660, XAR, CAB and LHA LH6; tar gzip, bzip2, xz, lzma, LZ4, Zstandard, compress
and lzip; standalone gzip, bzip2, xz and lzma; common tar suffix aliases.
A format family being supported is not a promise about every codec or variant.

Passwords/encrypted archives and multipart archives are not yet supported by
this adapter's user flow. Archive creation and in-place editing are not present.
ARJ, ACE and StuffIt need additional engines/adapters; merely installing their
GUI programs does not add Listi integration. These are future adapter work, not
claimed as working in this candidate. HPKG remains a system package, not a
filesystem-mount or package-install operation inside Listi.

Unsafe absolute/parent paths are rejected. Links and special entries are not
extracted. Existing staging, conflicts, cancellation and destination protections
remain in the file-operation layer. Optional helper failures must not publish
partial decoded files. The libarchive reader supplies cancellation-aware read
and seek callbacks, required for real 7z containers.

## Verification

`tests/archive-backends.cpp` consumes a tab-separated fixture/member/expected-file
manifest. Paths to fixtures must be absolute. Generated fixtures contain known
binary bytes; RAR, RAR5, CAB and LHA fixtures are upstream libarchive v3.7.9 test
vectors, with reference extraction cross-checked using Haiku p7zip. No upstream
implementation code is copied into Listi. Exact candidate results are recorded
in the acceptance note; the format list above is the qualification scope.
