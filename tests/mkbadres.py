#!/usr/bin/env python3
"""mkbadres.py - derive NE files with a malformed resource table or NAMETABLE from DLGTEST.EXE, for the
loader's up-front resource check (host tests and, through /root/bad on the test disk, w16 -n on the target).
Usage: mkbadres.py DLGTEST.EXE OUTDIR  -> OUTDIR/<name> and OUTDIR/expect.txt (name code substring)"""
import struct, sys
from pathlib import Path

good = Path(sys.argv[1]).read_bytes(); out = Path(sys.argv[2]); out.mkdir(parents=True, exist_ok=True)
u16 = lambda b, o: struct.unpack_from('<H', b, o)[0]
ne = struct.unpack_from('<I', good, 0x3C)[0]
rs = ne + u16(good, ne + 0x24)
shift = u16(good, rs)
p = rs + 2
nt_off = nt_len = 0
named_type_field = first_entry = None
while u16(good, p):
    t, cnt = u16(good, p), u16(good, p + 2)
    if not t & 0x8000 and named_type_field is None:
        named_type_field = p                             # a type stored by name: offset into the table
    p += 8
    for k in range(cnt):
        if first_entry is None:
            first_entry = p
        if t == 0x800F:
            nt_off, nt_len = u16(good, p) << shift, u16(good, p + 2) << shift
        p += 12
assert nt_off, 'no NAMETABLE in ' + sys.argv[1]
MSG = 'resource table or its NAMETABLE is malformed'
cases = []


def case(name, off, fmt, val, code=11, text=MSG):
    b = bytearray(good); struct.pack_into(fmt, b, off, val)
    (out / name).write_bytes(b); cases.append((name, code, text))


sz0 = u16(good, nt_off)
case('ntwrap.exe', nt_off, '<H', 0xFFF0)                  # size that wraps p + sz in a 16-bit int
case('ntsmall.exe', nt_off, '<H', 4)                      # below the 8-byte minimum
case('ntlong.exe', nt_off, '<H', nt_len + 2)              # past the end of the table
case('ntnonul.exe', nt_off + sz0 - 1, '<B', 0x78)         # resource name not terminated
case('ntnoroom.exe', nt_off + 6, '<B', 0x41)              # type name runs into the resource name
case('rtbadoff.exe', first_entry, '<H', 0xFFF0)           # resource data beyond the file
if named_type_field is not None:
    case('rttype.exe', named_type_field, '<H', 0x7FF0)    # type name offset outside the table
(out / 'expect.txt').write_text(''.join('%s %d %s\n' % c for c in cases))
print(len(cases), 'cases written to', out)
