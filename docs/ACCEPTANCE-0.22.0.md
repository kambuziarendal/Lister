# Listi 0.22.0 private encrypted-creation candidate

Runtime/source: `e6e5f06928f05efc0d4ec418da2fd58d52764484`.
Native Haiku hrev59866+79 x86_64, libarchive 3.7.9 and p7zip 17.05-2.
Owned standalone disposable VM cloned from the stopped accepted 0.21.0 image.
No public push, release or feed change.

## Verified on the final candidate

- The complete native audit passed with 664 checks and no failures. Archive
  workers ran on the 256 KiB native stack used by the installed application.
- ZIP AES-256 and 7z AES-256 creation round-trip exact file bytes, Unicode
  names, literal shell characters, empty files and empty directories.
- Independent p7zip verification rejects an omitted or wrong password, accepts
  the correct password and reports AES-256. Encrypted 7z names are hidden until
  the password is supplied. This is not only a self-read test.
- Job snapshots contain only a bounded, one-shot process token. Password bytes
  are absent from messages, settings, paths and command lines. Closing the
  archive window, queueing the job, forgetting passwords or stopping Listi
  clears the applicable in-memory secret.
- Cancel, source replacement, source mutation and a destination that appears
  after confirmation publish no archive and leave no private staging data.
  Existing destinations are never replaced.
- Installed UI was exercised with actual controls in Simple, Explorer and
  Commander, in light and dark appearance. Masked entry, confirmation mismatch,
  closing and reopening with empty fields, ZIP creation and 7z hidden-name
  creation passed without resizing the main window.
- The final installed package created another encrypted ZIP which p7zip tested
  successfully. Its installed executable matches the executable built for the
  package.
- Installed 0.22.0 to accepted 0.21.0 to the same 0.22.0 package passed.
  Settings, test files and metadata were byte-for-byte preserved, and archive
  passwords were requested again after restart.

## Identity

Built and installed executable SHA256:
`463506ba4eaee84577082a6d61f2293ece0dce15fe225911bced87ffabaa487c`

HPKG SHA256:
`2f4503dd8cda274a6d7c17c0e8c4c316705ef4e67c9fee384cd6e53bf5ec575d`

Private ZIP SHA256:
`b4f2c57efd73e976a2a69587c77b2f0c2c46c5ddd4d95e43cd940fba9a8a9af8`

Report: `/home/lobster/kambuzi/reports/listi-encrypted-create-20261115`.
The final native log is `final-native-audit.log` in the guest image. The report
also contains the final HPKG, private ZIP, UI captures and rollback evidence.

Bounds are documented in ARCHIVES-0.22.0.md. ZIP member names remain visible.
Encrypted 7z creation requires the optional p7zip package. Encrypted RAR
creation, split archive creation and editing existing archives are not claimed.
No physical-hardware qualification is claimed.
