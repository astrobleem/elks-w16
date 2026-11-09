#!/usr/bin/env python3
"""mknametab.py - write a Windows 3.0 style NAMETABLE resource (type 15) for DLGTEST, so the loader's
NAMETABLE path can be tested without a Microsoft-built program. Open Watcom's wrc keeps resource names in
the resource table itself; this table maps the name "NtDlg" to DIALOG #2 and "NtData" to the DATA
resource #2 the way RC 3.0 did. Usage: mknametab.py OUT.bin"""
import struct, sys

entries = [(5, 2, b'', b'NtDlg'),            # numbered type 5 (DIALOG): empty type name
           (0xF006, 2, b'DATA', b'NtData')]  # named type: the ordinal RC gave the type name, and the name
out = bytearray()
for t, i, tn, rn in entries:
    body = struct.pack('<HH', t, 0x8000 | i) + tn + b'\0' + rn + b'\0'
    out += struct.pack('<H', len(body) + 2) + body
out += b'\0\0'                               # a zero size ends the table
open(sys.argv[1], 'wb').write(out)
print('wrote', sys.argv[1], len(out), 'bytes')
