#!/usr/bin/env python3
"""w16census.py - what would it take for w16 to run this Win16 program? (host-only analysis)

For each NE file: header flags, segments and the automatic data segment size, resources by type,
every imported entry point with its Windows 3.0 name and whether w16 provides it, and (optionally)
the number of 80186+ instruction sites in reached code from 86TO88's tools/wep/analyze_ne.py.
Usage: w16census.py --api src/api.c --exports DIR [--analyzer analyze_ne.py] FILE.EXE...
Prints a summary table and, per file, the missing imports."""
import argparse, json, re, struct, subprocess, sys, tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--api', required=True); p.add_argument('--exports', required=True); p.add_argument('--analyzer')
p.add_argument('--detail', action='store_true'); p.add_argument('files', nargs='+')
a = p.parse_args()
src = Path(a.api).read_text()
provided = {(dict(K='KERNEL', U='USER', G='GDI')[m[1]], int(m[2])) for m in re.finditer(r'\{\s*([KUG]),\s*(\d+),\s*"\w+"', src[src.index('api_table[]'):src.index('api_count')])}
names = {}
for f in Path(a.exports).glob('exports-WINDOWS-*.json'):
    mod = f.stem.split('-')[-1]
    for n, o in json.loads(f.read_text())['byname'].items():
        names[(mod, o)] = n
u16 = lambda b, o: struct.unpack_from('<H', b, o)[0]
RT = {1: 'CURSOR', 2: 'BITMAP', 3: 'ICON', 4: 'MENU', 5: 'DIALOG', 6: 'STRING', 7: 'FONTDIR', 8: 'FONT',
      9: 'ACCEL', 10: 'RCDATA', 12: 'GROUP_CURSOR', 14: 'GROUP_ICON', 16: 'VERSION'}


def census(path):
    b = Path(path).read_bytes()
    ne = struct.unpack_from('<I', b, 0x3C)[0]
    flags, autod, heap, stack = u16(b, ne + 0xC), u16(b, ne + 0xE), u16(b, ne + 0x10), u16(b, ne + 0x12)
    nseg, nmod, segtab, rsrc, resn, modref, impn = (u16(b, ne + x) for x in (0x1C, 0x1E, 0x22, 0x24, 0x26, 0x28, 0x2A))
    align = u16(b, ne + 0x32) or 9
    segs = [struct.unpack_from('<HHHH', b, ne + segtab + 8 * i) for i in range(nseg)]
    mods = []
    for i in range(nmod):
        o = u16(b, ne + modref + 2 * i); n = b[ne + impn + o]
        mods.append(b[ne + impn + o + 1:ne + impn + o + 1 + n].decode().upper())
    imports = {}
    other = set()
    for i, (fo, sz, fl, mn) in enumerate(segs):
        if not fl & 0x100 or not fo:
            continue
        pp = (fo << align) + (sz or 0x10000)
        for r in range(u16(b, pp)):
            st, rf, srcoff, t1, t2 = struct.unpack_from('<BBHHH', b, pp + 2 + 8 * r)
            if rf & 3 == 1:
                imports[(mods[t1 - 1], t2)] = imports.get((mods[t1 - 1], t2), 0) + 1
            elif rf & 3 == 2:
                n = b[ne + impn + t2]
                imports[(mods[t1 - 1], b[ne + impn + t2 + 1:ne + impn + t2 + 1 + n].decode())] = 1
            elif rf & 3 == 3:
                other.add('OSFIXUP')
    res = {}
    if rsrc != resn:
        q = ne + rsrc + 2
        while u16(b, q):
            t, cnt = u16(b, q), u16(b, q + 2)
            res[RT.get(t & 0x7FFF, 'named' if not t & 0x8000 else str(t & 0x7FFF))] = cnt
            q += 8 + 12 * cnt
    data = segs[autod - 1][3] or 0x10000 if autod else 0
    dgroup = (data + heap + stack) if autod else 0
    sites = None
    if a.analyzer:
        with tempfile.NamedTemporaryFile(suffix='.json', delete=False) as t:
            pass
        r = subprocess.run([sys.executable, a.analyzer, path, '--json', t.name], capture_output=True, text=True)
        try:
            j = json.loads(Path(t.name).read_text())
            sites = sum(len(s.get('sites', [])) + len(s.get('unsup', [])) for s in j['segments'])
        except Exception:
            sites = 'n/a'
    missing = sorted(k for k in imports if k not in provided)
    return dict(name=Path(path).name, flags=flags, nseg=nseg,
                code=sum(s[1] for s in segs if not s[2] & 1), data=sum(s[1] for s in segs if s[2] & 1),
                dgroup=dgroup, mods=mods, nimp=len(imports), nmissing=len(missing), missing=missing,
                res=res, other=sorted(other), sites=sites,
                notes=[x for x, bit in (('PROTMODE-only', 8), ('286', 0x20), ('386', 0x40), ('8087', 0x80), ('selfload', 0x800), ('DLL', 0x8000)) if flags & bit])


rows = [census(f) for f in a.files]
print('%-13s %4s %6s %6s %6s %-28s %5s %5s %6s %s' % ('program', 'segs', 'code', 'data', 'dgroup', 'modules', 'imps', 'miss', '186+', 'resources / notes'))
for r in rows:
    print('%-13s %4d %6d %6d %6d %-28s %5d %5d %6s %s %s' % (r['name'], r['nseg'], r['code'], r['data'], r['dgroup'],
          ','.join(r['mods'])[:28], r['nimp'], r['nmissing'], r['sites'] if r['sites'] is not None else '-',
          ' '.join('%s:%d' % kv for kv in r['res'].items()), ' '.join(r['notes'] + r['other'])))
if a.detail:
    for r in rows:
        print('\n%s missing (%d of %d):' % (r['name'], r['nmissing'], r['nimp']))
        print('  ' + ', '.join('%s.%s' % (m, names.get((m, o), o) if isinstance(o, int) else o) for m, o in r['missing']))
