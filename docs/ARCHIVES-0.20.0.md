# Optional encrypted 7z and RAR reader (0.20.0)

Install HaikuDepot's optional **p7zip** package for encrypted 7z, RAR4 and RAR5.
Listi uses its native 7z.so/Rar.so decoder interfaces directly. Browsing,
preview and extraction stay inside Listi; no external archive application or
password-bearing command line is launched. Tested engine: p7zip 17.05-2 x86_64.
If absent, ordinary formats retain libarchive support and encrypted 7z/RAR
report the missing optional package. A newly installed engine is discovered
without restarting Listi. Updating an already loaded engine requires restart.

Includes encrypted names, solid 7z data, Unicode paths and a retry when different
RAR members use different passwords. Passwords use the existing masked prompt,
memory-only bounded cache, explicit Forget and cancellation from 0.19.0.
The plugin can hold transient password copies, so cache wiping does not promise
protection against memory inspection or OS paging.

The decoder receives bounded input/output streams, not filesystem output paths.
Decoded members are CRC-checked in an unlinked temporary file before any bytes
reach the caller. Wrong-password retries cannot duplicate partial output.
Whole-archive jobs still publish their private staging directory only after
success. This requires temporary disk space for a decoded member in addition
to the extraction directory. Paths, links, duplicates, special entries, volumes
and archive identity changes are checked. Progress/cancel callbacks remain active
while decoding; solid archives may decode preceding data for each requested file.

Only interface headers and original adapter code are included in Listi. The
optional decoder remains a system package. Upstream LGPL headers/notices are
included in the source and installed documentation. RAR decoding additionally
carries the upstream unRAR restriction; Listi does not implement RAR creation.

This step adds reading, not encrypted archive creation, archive editing or split
volumes. Unsupported methods still fail explicitly. It does not establish support
for every historic or future 7z/RAR codec. ZIP behavior remains as documented in
ARCHIVES-0.19.0.md; its earlier encrypted-7z/RAR limitation is superseded here.
