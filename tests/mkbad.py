#!/usr/bin/env python3
"""mkbad.py - derive malformed and unsupported NE files from a good one, for loader rejection tests.
Usage: mkbad.py GOOD.EXE OUTDIR    -> writes OUTDIR/<name> and OUTDIR/expect.txt (name code substring)
Codes: 10 EIO, 11 EFORMAT, 12 EUNSUPP, 13 EIMPORT, 14 ENOMEM, 15 ERELOC."""
import struct, sys
from pathlib import Path

good = Path(sys.argv[1]).read_bytes(); out = Path(sys.argv[2]); out.mkdir(parents=True, exist_ok=True)
ne = struct.unpack_from('<I', good, 0x3C)[0]
u16 = lambda b, o: struct.unpack_from('<H', b, o)[0]
nseg, nmod, segtab, modref, impn, align = (u16(good, ne + 0x1C), u16(good, ne + 0x1E), u16(good, ne + 0x22),
                                          u16(good, ne + 0x28), u16(good, ne + 0x2A), u16(good, ne + 0x32))
segs = [struct.unpack_from('<HHHH', good, ne + segtab + 8 * i) for i in range(nseg)]


def relocs(seg):
    fo, sz, fl, mn = segs[seg]
    p = (fo << align) + sz
    n = u16(good, p)
    return [(p + 2 + 8 * i,) + struct.unpack_from('<BBHHH', good, p + 2 + 8 * i) for i in range(n)]


def first_import(mod_name=None, ordinal=None):
    for s in range(nseg):
        if not segs[s][2] & 0x100:
            continue
        for rec in relocs(s):
            pos, st, fl, src, t1, t2 = rec
            if fl & 3 == 1:
                mo = u16(good, ne + modref + 2 * (t1 - 1)); n = good[ne + impn + mo]
                name = good[ne + impn + mo + 1: ne + impn + mo + 1 + n].decode()
                if (mod_name is None or name == mod_name) and (ordinal is None or t2 == ordinal):
                    return s, rec
    raise SystemExit('import not found')


cases = []


def case(name, data, code, text):
    (out / name).write_bytes(bytes(data))
    cases.append((name, code, text))


def patched(off, fmt, val):
    b = bytearray(good); struct.pack_into(fmt, b, off, val); return b


case('empty.exe', b'', 11, 'too small for an MZ header')
case('trunc100.exe', good[:100], 11, 'outside the file')
fo3, sz3 = segs[2][0] << align, segs[2][1]
case('truncseg.exe', good[:fo3 + sz3 // 2], 10, 'runs past the end of the file')
case('notmz.exe', b'XZ' + good[2:], 11, 'not an MZ executable')
case('dosexe.exe', patched(0x18, '<H', 0x1C), 11, 'plain DOS program')
case('badlfa.exe', patched(0x3C, '<I', 0xFFFF00), 11, 'outside the file')
b = bytearray(good); b[ne:ne + 2] = b'PE'; case('pe.exe', b, 12, '32-bit PE')
flags = u16(good, ne + 0x0C)
case('dll.exe', patched(ne + 0x0C, '<H', flags | 0x8000), 12, 'library (DLL)')
case('pmode.exe', patched(ne + 0x0C, '<H', flags | 0x0008), 12, 'protected-mode only')
case('i286.exe', patched(ne + 0x0C, '<H', flags | 0x0020), 12, 'needs an 80286')
case('selfload.exe', patched(ne + 0x0C, '<H', flags | 0x0800), 12, 'self-loading')
case('os2.exe', patched(ne + 0x36, '<B', 1), 12, 'OS/2 1.x')
case('iter.exe', patched(ne + segtab + 4, '<H', segs[0][2] | 0x0008), 12, 'iterated')
case('seg64k.exe', patched(ne + segtab + 6, '<H', 0), 12, '64 KB')
case('noseg.exe', patched(ne + 0x1C, '<H', 0), 11, '0 segments')
case('manyseg.exe', patched(ne + 0x1C, '<H', 40), 12, '40 segments')
case('autods.exe', patched(ne + 0x0E, '<H', 9), 11, 'out of range')
case('bigdata.exe', patched(ne + 0x10, '<H', 0xF800), 14, 'over 64 KB')
# imports
mo = u16(good, ne + modref + 2 * 1)          # second module name (GDI in HELLO16)
b = bytearray(good); n = b[ne + impn + mo]; nm = b[ne + impn + mo + 1: ne + impn + mo + 1 + n]
b[ne + impn + mo + n] = ord('X'); case('badmod.exe', b, 13, 'which this layer does not provide')
s, rec = first_import('USER', 420)
case('badord.exe', patched(rec[0] + 6, '<H', 999), 13, 'unsupported import USER.999')
case('msgbox.exe', patched(rec[0] + 6, '<H', 1), 13, 'USER.1 (MessageBox)')
case('osfix.exe', patched(rec[0] + 1, '<B', 3), 15, 'OS fixup')
# relocation range and chain errors
case('reloob.exe', patched(rec[0] + 2, '<H', 0xFFF0), 15, 'outside the segment data')
src = rec[3]; fo = segs[s][0] << align
case('chainloop.exe', patched(fo + src, '<H', src), 15, 'loops back to itself')
case('badint.exe', patched(relocs(0)[[r[2] & 3 for r in relocs(0)].index(0)][0] + 4, '<H', 7), 15,
     'internal reference to segment 7')
(out / 'expect.txt').write_text(''.join('%s %d %s\n' % c for c in cases))
print(len(cases), 'cases written to', out)
