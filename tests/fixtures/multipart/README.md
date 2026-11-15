# Multipart archive fixtures

Synthetic original MIT fixtures contain note.txt and 70,000 deterministic bytes
(random.Random(21).randbytes(70000)); exact originals are in original/.
7z: 7-Zip 25.01, -v20k; encrypted uses -pListi-test-only -mhe=on.
RAR5: RAR 7.23 evaluation, -ma5 -m3 -md1m -v20k, encrypted adds
-hpListi-test-only. The evaluation tool was used only to generate original test
data on 2026-11-15; no tool/decoder binaries are redistributed with Listi.

legacy.rar/r00..r04 derive from public libarchive
 test_rar_multivolume_multiple_files.part1.rar through part6.rar:
clear RAR4 main-header NewVolName flag (0x10), recalculate header CRC,
rename parts to the old numbering convention. No member content changed.
The two HTML files each contain 20111 bytes and have identical contents.

test_read_format_rar5_multiarchive_solid.part01..04.rar are unchanged public
libarchive fixtures. Source: https://github.com/libarchive/libarchive/tree/master/libarchive/test
See LICENSE-libarchive.txt. All fixtures are synthetic, with no user data.
