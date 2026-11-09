#!/usr/bin/env python3
"""nedump.py - list the parts of a Win16 NE executable that a loader has to handle.
Host-only tool. Usage: nedump.py FILE.EXE [--relocs]"""
import struct, sys

SRC = {0: 'LOBYTE', 2: 'SEGMENT', 3: 'FARADDR', 5: 'OFFSET', 11: 'PTR48', 13: 'OFFSET32'}
TGT = {0: 'INTERNALREF', 1: 'IMPORTORDINAL', 2: 'IMPORTNAME', 3: 'OSFIXUP'}


def main():
    path = sys.argv[1]; show = '--relocs' in sys.argv
    b = open(path, 'rb').read()
    if b[:2] != b'MZ': sys.exit('not MZ')
    ne = struct.unpack_from('<H', b, 0x3c)[0]
    if b[ne:ne + 2] != b'NE': sys.exit('no NE header')
    (_, lver, lrev, entoff, entlen, crc, flags, autods, heap, stack, ip, cs, sp, ss, nseg, nmod, nrnlen,
     segoff, rsrcoff, resnoff, modoff, impoff, nrnoff, nmov, align, nres, os_, flags2, gangoff, ganglen,
     swaparea, expver) = struct.unpack_from('<2sBBHHIHHHHHHHHHHHHHHHHIHHHBBHHHH', b, ne)
    print('NE at %#x linker %d.%d flags %#06x autodata %d heap %d stack %d CS:IP %d:%#06x SS:SP %d:%#06x' % (
        ne, lver, lrev, flags, autods, heap, stack, cs, ip, ss, sp))
    print('segments %d modules %d align shift %d target os %d flags2 %#04x expected windows %d.%02d nonres@%#x' % (
        nseg, nmod, align, os_, flags2, expver >> 8, expver & 0xff, nrnoff))
    mods = []
    for i in range(nmod):
        off = struct.unpack_from('<H', b, ne + modoff + 2 * i)[0]
        n = b[ne + impoff + off]; mods.append(b[ne + impoff + off + 1: ne + impoff + off + 1 + n].decode())
    print('imported modules:', mods)
    resn = []
    p = ne + resnoff
    while b[p]:
        n = b[p]; nm = b[p + 1:p + 1 + n].decode(); o = struct.unpack_from('<H', b, p + 1 + n)[0]; resn.append((nm, o)); p += 3 + n
    print('resident names:', resn)
    # entry table
    p = ne + entoff; end = p + entlen; ordn = 1; ents = []
    while p < end:
        cnt, ind = b[p], b[p + 1]; p += 2
        if cnt == 0: break
        for _ in range(cnt):
            if ind == 0: ordn += 1; continue
            if ind == 0xff:
                fl, _int3f, seg, off = struct.unpack_from('<BHBH', b, p); p += 6
            else:
                fl, off = struct.unpack_from('<BH', b, p); seg = ind; p += 3
            ents.append((ordn, seg, off, fl)); ordn += 1
    print('entries:', ['@%d seg%d:%#06x fl%#x' % e for e in ents])
    imports = {}
    for i in range(nseg):
        so, sz, sf, mn = struct.unpack_from('<HHHH', b, ne + segoff + 8 * i)
        fo = so << align
        print('seg %d: file %#06x size %5d min %5d flags %#06x %s%s%s' % (
            i + 1, fo, sz, mn, sf, 'DATA' if sf & 1 else 'CODE', ' RELOC' if sf & 0x100 else '', ' ITER' if sf & 8 else ''))
        if sf & 0x100:
            rp = fo + (sz or 0x10000); nr = struct.unpack_from('<H', b, rp)[0]; rp += 2
            for r in range(nr):
                st, fl, src, a1, a2 = struct.unpack_from('<BBHHH', b, rp + 8 * r)
                t = fl & 3
                if t == 1: desc = '%s.%d' % (mods[a1 - 1], a2); imports.setdefault(desc, []).append(SRC.get(st, st))
                elif t == 2:
                    n = b[ne + impoff + a2]; desc = '%s.%s' % (mods[a1 - 1], b[ne + impoff + a2 + 1:ne + impoff + a2 + 1 + n].decode())
                    imports.setdefault(desc, []).append(SRC.get(st, st))
                elif t == 0: desc = 'seg%d:%#06x' % (a1, a2) if a1 != 0xff else 'movable entry %d' % a2
                else: desc = 'osfixup %d' % a1
                if show: print('   %-8s %-13s%s at %#06x -> %s' % (SRC.get(st, st), TGT[t], ' ADDITIVE' if fl & 4 else '', src, desc))
            print('   %d relocation records' % nr)
    print('imports used (%d):' % len(imports))
    for k in sorted(imports): print('  ', k, sorted(set(imports[k])))
    if '--resources' in sys.argv and rsrcoff != resnoff:
        dump_resources(b, ne + rsrcoff)


RT = {1: 'CURSOR', 2: 'BITMAP', 3: 'ICON', 4: 'MENU', 5: 'DIALOG', 6: 'STRING', 7: 'FONTDIR', 8: 'FONT',
      9: 'ACCELERATOR', 10: 'RCDATA', 12: 'GROUP_CURSOR', 14: 'GROUP_ICON', 15: 'NAMETABLE', 16: 'VERSION'}
CLS = {0x80: 'BUTTON', 0x81: 'EDIT', 0x82: 'STATIC', 0x83: 'LISTBOX', 0x84: 'SCROLLBAR', 0x85: 'COMBOBOX'}


def rname(b, base, v):
    if v & 0x8000:
        return '#%d' % (v & 0x7FFF)
    n = b[base + v]
    return b[base + v + 1:base + v + 1 + n].decode('latin1')


def cstr(b, p):
    """Win16 resource string: 0xFF + ordinal, or NUL-terminated text."""
    if b[p] == 0xFF:
        return '#%d' % struct.unpack_from('<H', b, p + 1)[0], p + 3
    e = b.index(b'\0', p)
    return b[p:e].decode('latin1'), e + 1


def dump_dialog(d):
    style, n, x, y, cx, cy = struct.unpack_from('<IBHHHH', d, 0)
    p = 13
    menu, p = cstr(d, p); cls, p = cstr(d, p); cap, p = cstr(d, p)
    font = ''
    if style & 0x40:
        pt = struct.unpack_from('<H', d, p)[0]; face, p = cstr(d, p + 2); font = ' font %d %s' % (pt, face)
    print('      dialog style %08x at %d,%d %dx%d menu %r class %r caption %r%s, %d items' % (style, x, y, cx, cy, menu, cls, cap, font, n))
    for i in range(n):
        ix, iy, icx, icy, iid, ist = struct.unpack_from('<HHHHHI', d, p); p += 14
        if d[p] & 0x80:
            c = CLS.get(d[p], '%#x' % d[p]); p += 1
        else:
            c, p = cstr(d, p)
        t, p = cstr(d, p)
        extra = d[p]; p += 1 + extra
        print('        item id %5d %-8s style %08x at %d,%d %dx%d text %r' % (iid, c, ist, ix, iy, icx, icy, t))


def dump_resources(b, base):
    shift = struct.unpack_from('<H', b, base)[0]; p = base + 2
    print('resources (align shift %d):' % shift)
    while True:
        t, cnt = struct.unpack_from('<HH', b, p)
        if t == 0:
            break
        tname = RT.get(t & 0x7FFF, '#%d' % (t & 0x7FFF)) if t & 0x8000 else rname(b, base, t)
        p += 8
        for i in range(cnt):
            off, ln, fl, rid = struct.unpack_from('<HHHH', b, p); p += 12
            data = b[off << shift:(off << shift) + (ln << shift)]
            print('   %-12s %-10s file %#07x len %5d flags %#06x' % (tname, rname(b, base, rid), off << shift, ln << shift, fl))
            if tname == 'DIALOG':
                dump_dialog(data)
            elif tname == 'GROUP_ICON':
                n = struct.unpack_from('<H', data, 4)[0]
                for k in range(n):
                    w, h, cc, _, planes, bits, size, iid = struct.unpack_from('<BBBBHHIH', data, 6 + 14 * k)
                    print('        icon %dx%d colours %d bits %d -> ICON #%d' % (w, h, cc, bits * planes, iid))


if __name__ == '__main__':
    main()
