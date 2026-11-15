# Password-protected archives (0.19.0)

ZIP reading and extraction now request a password when the system reader needs
one. Verified encryption methods: traditional ZipCrypto and WinZip AES-128/256
with stored/deflated content. Browsing, preview, clipboard copy and whole-archive
extraction stay inside Listi. Existing archive creation is unchanged and does
not create encrypted archives.

The native field masks typing. Wrong passwords can be retried, with four reader
attempts per operation. Cancel follows ordinary job cancellation and removes
private extraction staging. Archives replaced or modified while a password
prompt is open are rejected. Output targets are still never overwritten.

Successfully read passwords are cached in memory only, bound to device, inode,
size, modification and change timestamps. The cache holds at most 16 archives.
Files > Archives > Forget archive passwords clears it and cancels pending
password requests, including late answers. Closing Listi clears it too. Passwords
are not included in URLs, settings, job messages, logs or process arguments.
Owned secret buffers are overwritten on destruction. Native controls and
libarchive may make their own transient copies; this is not a guarantee against
process-memory inspection or OS paging. Current readers can finish using their
own transient copies after the cache is cleared.

The installed libarchive 3.7.9 does NOT decrypt 7z/RAR, encrypted ZIP headers,
or every ZIP encryption/compression combination. Native probes also rejected
several AES ZIP fixtures with uncommon compression methods. Unsupported
combinations fail explicitly; no external application is launched. Supporting
these cases needs another optional adapter. Split archives and archive editing
remain later steps.

Also corrected deferred per-member completion notifications after archive
publication, so successful extraction no longer leaves a stale failed transfer
marker when browsing the extracted source members.
