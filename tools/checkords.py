#!/usr/bin/env python3
"""checkords.py - verify the ordinals in src/api.c (implemented entries and diagnostic names) against an
export list taken from the Windows 3.0 modules (JSON {"byname": {NAME: ordinal}} per module).
Usage: checkords.py src/api.c EXPORTDIR   (EXPORTDIR holds exports-WINDOWS-{KERNEL,USER,GDI}.json)"""
import json, re, sys
from pathlib import Path

src = Path(sys.argv[1]).read_text(); d = Path(sys.argv[2])
mods = {'K': 'KERNEL', 'U': 'USER', 'G': 'GDI'}
exp = {m: {k.upper(): v for k, v in json.loads((d / ('exports-WINDOWS-%s.json' % m)).read_text())['byname'].items()}
       for m in mods.values()}
bad = n = 0
for m in re.finditer(r'\{\s*([KUG]),\s*(\d+),\s*"(\w+)"', src):
    mod, ordinal, name = mods[m[1]], int(m[2]), m[3]
    real = exp[mod].get(name.upper(), exp[mod].get('_' + name.upper()))
    n += 1
    if real != ordinal:
        bad += 1
        print('MISMATCH %s.%d %s: Windows 3.0 export says %s' % (mod, ordinal, name, real))
print('%d entries checked, %d mismatches' % (n, bad))
sys.exit(1 if bad else 0)
