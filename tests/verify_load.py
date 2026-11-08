#!/usr/bin/env python3
"""verify_load.py - independent check of the loader's output (host-only test).

Rebuilds each segment image from the NE file in Python (data, zero fill, relocation chains, import
bindings from the api_table order in src/api.c, Windows prologue patching) and compares it byte for
byte with the segments w16host dumped.
Usage: verify_load.py PROGRAM.EXE DUMPDIR SRC/api.c"""
import re, struct, sys
from pathlib import Path

exe, dump, apic = Path(sys.argv[1]).read_bytes(), Path(sys.argv[2]), Path(sys.argv[3]).read_text()
u16 = lambda b, o: struct.unpack_from('<H', b, o)[0]
ne = struct.unpack_from('<I', exe, 0x3C)[0]
nseg, nmod, segtab, modref, impn, align = (u16(exe, ne + x) for x in (0x1C, 0x1E, 0x22, 0x28, 0x2A, 0x32))
entoff, entlen = u16(exe, ne + 4), u16(exe, ne + 6)
align = align or 9
mods = []
for i in range(nmod):
    o = u16(exe, ne + modref + 2 * i); n = exe[ne + impn + o]
    mods.append(exe[ne + impn + o + 1: ne + impn + o + 1 + n].decode().upper())

# api_table order defines the stub index; constants carry a value
table = []
body = apic[apic.index('const struct api api_table[]'):apic.index('const int api_count')]
for m in re.finditer(r'\{\s*([KUG]),\s*(\d+),\s*"(\w+)",\s*(\d+),\s*([\w|]+)\s*,\s*([^,}]+?)(?:,\s*(0x[0-9A-Fa-f]+|\d+))?\s*\}', body):
    table.append(dict(mod={'K': 'KERNEL', 'U': 'USER', 'G': 'GDI'}[m[1]], ord=int(m[2]), name=m[3], flags=m[5],
                      value=int(m[7], 0) if m[7] else 0))
index = {(t['mod'], t['ord']): i for i, t in enumerate(table)}

load = {}
for line in (dump / 'load.txt').read_text().splitlines():
    p = line.split()
    if p[0] == 'seg':
        load[int(p[1])] = (int(p[3], 16), int(p[5]))
segs = [struct.unpack_from('<HHHH', exe, ne + segtab + 8 * i) for i in range(nseg)]
img = {}
for i, (fo, sz, fl, mn) in enumerate(segs, 1):
    sel, paras = load[i]
    b = bytearray(paras * 16)
    if fo:
        b[:sz] = exe[fo << align: (fo << align) + sz]
    img[i] = b


def entry(ordinal):
    p, n = ne + entoff, 1
    while p < ne + entoff + entlen:
        cnt, typ = exe[p], exe[p + 1]; p += 2
        if cnt == 0:
            break
        size = 0 if typ == 0 else 6 if typ == 0xFF else 3
        if ordinal < n + cnt:
            q = p + (ordinal - n) * size
            return (exe[q + 3], u16(exe, q + 4)) if typ == 0xFF else (typ, u16(exe, q + 1))
        n += cnt; p += cnt * size
    raise KeyError(ordinal)


nfix = 0
for i, (fo, sz, fl, mn) in enumerate(segs, 1):
    if not fl & 0x100:
        continue
    p = (fo << align) + sz
    for r in range(u16(exe, p)):
        st, rf, src, t1, t2 = struct.unpack_from('<BBHHH', exe, p + 2 + 8 * r)
        kind = rf & 3
        if kind == 0:
            if t1 == 0xFF:
                s, o = entry(t2); tseg, toff = load[s][0], o
            else:
                tseg, toff = load[t1][0], t2
        else:
            assert kind == 1, 'only ordinal imports in this test'
            t = table[index[(mods[t1 - 1], t2)]]
            if 'API_CONST' in t['flags']:
                tseg = toff = t['value']
            else:
                tseg, toff = 0xE000, 8 * index[(mods[t1 - 1], t2)]
        b = img[i]
        while True:
            nxt = u16(b, src)
            if st == 3:
                struct.pack_into('<HH', b, src, toff, tseg)
            elif st == 2:
                struct.pack_into('<H', b, src, tseg)
            elif st == 5:
                struct.pack_into('<H', b, src, toff)
            nfix += 1
            if rf & 4 or nxt == 0xFFFF:
                break
            src = nxt

# exported entries: 1E 58 90 / 8C D8 90 -> 90 90 90
p, ordinal, npatch = ne + entoff, 1, 0
while p < ne + entoff + entlen:
    cnt, typ = exe[p], exe[p + 1]; p += 2
    if cnt == 0:
        break
    size = 0 if typ == 0 else 6 if typ == 0xFF else 3
    for k in range(cnt):
        if typ and exe[p] & 1:
            s, o = entry(ordinal)
            if not segs[s - 1][2] & 1 and img[s][o:o + 3] in (b'\x1e\x58\x90', b'\x8c\xd8\x90'):
                img[s][o:o + 2] = b'\x90\x90'; npatch += 1
        ordinal += 1; p += size

bad = 0
for i in img:
    got = (dump / ('seg%d.bin' % i)).read_bytes()
    if got != bytes(img[i]):
        diff = [k for k in range(len(got)) if got[k] != img[i][k]]
        print('segment %d: %d bytes differ, first at %#x (got %02x expected %02x)' % (i, len(diff), diff[0], got[diff[0]], img[i][diff[0]]))
        bad += 1
    else:
        print('segment %d: %5d bytes identical to the independent model' % (i, len(got)))
print('model: %d fixups, %d prologues patched, %d api entries parsed' % (nfix, npatch, len(table)))
sys.exit(1 if bad else 0)
