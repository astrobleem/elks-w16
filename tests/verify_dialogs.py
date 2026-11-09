#!/usr/bin/env python3
"""verify_dialogs.py - host-only cross-check of the loader's resource lookup and dialog-template parser.

Independently walks the NE resource table (and any Windows 3.0 NAMETABLE) in Python, then asks w16host
to find every resource by ordinal and by name and to decode every DIALOG, and compares the results.
Usage: verify_dialogs.py W16HOST PROGRAM.EXE"""
import re, struct, subprocess, sys

host, exe = sys.argv[1], sys.argv[2]
b = open(exe, 'rb').read()
u16 = lambda o: struct.unpack_from('<H', b, o)[0]
ne = struct.unpack_from('<I', b, 0x3C)[0]
rsrc, resn = ne + u16(ne + 0x24), ne + u16(ne + 0x26)
fails = checks = 0


def run(args):
    return subprocess.run([host] + args + [exe], capture_output=True, encoding='latin1').stdout


def expect(cond, what):
    global fails, checks
    checks += 1
    if not cond:
        fails += 1
        print('FAIL', what)


def cstr(d, p):
    if d[p] == 0xFF:
        return '#%d' % struct.unpack_from('<H', d, p + 1)[0], p + 3
    e = d.index(b'\0', p)
    return d[p:e].decode('latin1'), e + 1


def decode_dialog(d):
    style, n, x, y, cx, cy = struct.unpack_from('<IBhhhh', d, 0)
    p = 13
    menu, p = cstr(d, p); cls, p = cstr(d, p); cap, p = cstr(d, p)
    pt, face = 0, ''
    if style & 0x40:
        pt = struct.unpack_from('<H', d, p)[0]; face, p = cstr(d, p + 2)
    items = []
    for i in range(n):
        ix, iy, icx, icy, iid, ist = struct.unpack_from('<hhhhHI', d, p); p += 14
        if d[p] & 0x80:
            c, cn = d[p], ''; p += 1
        else:
            c = 0; cn, p = cstr(d, p)
        t, p = cstr(d, p)
        p += 1 + d[p]
        items.append((iid, c, cn, ist, ix, iy, icx, icy, t))
    return (style, x, y, cx, cy, menu, cls, cap, pt, face, items)


resources = []                              # (type, id or name, file offset, length)
if rsrc != resn:
    shift = u16(rsrc); p = rsrc + 2
    while u16(p):
        t, cnt = u16(p), u16(p + 2); p += 8
        for i in range(cnt):
            off, ln, fl, rid = struct.unpack_from('<HHHH', b, p); p += 12
            key = ('#%d' % (rid & 0x7FFF)) if rid & 0x8000 else b[rsrc + rid + 1:rsrc + rid + 1 + b[rsrc + rid]].decode('latin1')
            if t & 0x8000:
                resources.append((t & 0x7FFF, key, off << shift, ln << shift))
names = []                                  # NAMETABLE: (type, name, ordinal key)
for t, key, off, ln in resources:
    if t == 15:
        q = off
        while q + 8 <= off + ln and u16(q) >= 8:
            sz, nt, ordv = u16(q), u16(q + 2) & 0x7FFF, u16(q + 4)
            nm = b[q + 7:b.index(b'\0', q + 7)].decode('latin1')
            names.append((nt, nm, '#%d' % (ordv & 0x7FFF)))
            q += sz
for t, key, off, ln in resources:
    out = run(['-R', str(t), key])
    expect('at %d length %d' % (off, ln) in out, 'resource %d %s: expected %d/%d, got %r' % (t, key, off, ln, out.strip()))
    if not key.startswith('#'):
        out = run(['-R', str(t), key.lower()])
        expect('at %d length %d' % (off, ln) in out, 'case-insensitive lookup of %s' % key)
for t, nm, ordkey in names:
    want = [r for r in resources if r[0] == t and r[1] == ordkey][0]
    out = run(['-R', str(t), nm.lower()])
    expect('at %d length %d' % (want[2], want[3]) in out, 'NAMETABLE name %s for type %d' % (nm, t))
for t, key, off, ln in resources:
    if t != 5:
        continue
    style, x, y, cx, cy, menu, cls, cap, pt, face, items = decode_dialog(b[off:off + ln])
    out = run(['-D', key]).splitlines()
    head = "dialog style %08x at %d,%d %dx%d menu '%s' class '%s' caption '%s' font %d '%s', %d items" % (
        style, x, y, cx, cy, menu, cls, cap, pt, face, len(items))
    expect(out and out[0] == head, 'dialog %s header: %r vs %r' % (key, out[:1], head))
    for k, it in enumerate(items):
        iid, c, cn, ist, ix, iy, icx, icy, t2 = it
        line = "item id %d class %02x '%s' style %08x at %d,%d %dx%d text '%s'" % (iid, c, cn[:15], ist, ix, iy, icx, icy, t2[:47])
        expect(len(out) > k + 1 and out[k + 1] == line, 'dialog %s item %d: %r vs %r' % (key, k, out[k + 1] if len(out) > k + 1 else None, line))
out = run(['-R', '5', 'NoSuchResource'])
expect('not found' in out, 'missing resource reported')
print('%s: %d resources, %d NAMETABLE names, %d checks, %d failures' % (exe.split('/')[-1], len(resources), len(names), checks, fails))
sys.exit(1 if fails else 0)
