# Encrypted archive creation (0.22.0)

Listi can create password-protected ZIP and 7z archives from local files and
folders. ZIP uses AES-256 through Haiku's optional libarchive package. 7z uses
the optional p7zip module through its in-process library interface and can also
encrypt member names.

The archive window requires the password twice. Both controls are masked and
cleared when the window closes or after the job is queued. Only a bounded,
single-use process-local token enters the job message. Password bytes are never
written to archive settings, job snapshots, file names or command lines.

Creation retains the existing archive safety contract:

- all selected source identities are frozen before the question is shown;
- links and special files are rejected;
- changes to a source or destination abort publication;
- output is first written inside a private staging directory;
- cancel, error or shutdown publishes no partial archive;
- an existing or newly appearing destination is never replaced.

ZIP does not hide member names. Empty ZIP members contain no encrypted data.
For 7z, `Encrypt file names` is enabled by default when password protection is
selected and can be turned off explicitly. p7zip must be installed for encrypted
7z creation. Plain 7z creation can still use libarchive.

This release does not create encrypted RAR files, split output volumes, or edit
an existing archive in place.

Independent verification uses p7zip to reject an omitted or wrong password,
accept the correct password, report AES encryption, and compare every extracted
byte with the original input. The same checks cover ZIP AES-256. These are
format interoperability checks, not just Listi reading its own output.
