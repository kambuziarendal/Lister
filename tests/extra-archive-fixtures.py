#!/usr/bin/env python3
"""Additional synthetic fixtures for Listi --virtual-test. No network needed."""
import bz2
import gzip
import io
import lzma
from pathlib import Path
import sys
import tarfile

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
for suffix, compress in [('gz', gzip.compress), ('bz2', bz2.compress),
                         ('xz', lzma.compress)]:
    (root / ('single.txt.' + suffix)).write_bytes(compress(b'single content\n'))
with tarfile.open(root / 'metadata.tar', 'w') as archive:
    directory = tarfile.TarInfo('explicit')
    directory.type = tarfile.DIRTYPE
    directory.mode = 0o750
    directory.mtime = 1600000000
    archive.addfile(directory)
    entry = tarfile.TarInfo('explicit/run.sh')
    entry.mode = 0o4751
    entry.mtime = 1600000020
    data = b'#!/bin/sh\necho synthetic\n'
    entry.size = len(data)
    archive.addfile(entry, io.BytesIO(data))
