#!/usr/bin/env python3
"""Read-only 8088 CPU preflight for the known WINMINE NE layout.

Usage: python3 tools/winminecpu.py WINMINE.EXE [--sites]
Requires NASM's ndisasm on PATH. Does not patch, launch, or certify an image.
"""
import argparse
from collections import Counter
from pathlib import Path
import re
import struct
import subprocess
import sys


def word(data, offset):
    return struct.unpack_from('<H', data, offset)[0]


def chain_sites(code, records):
    """Return (source offset, width, type) including every chained fixup."""
    result = []
    for typ, flags, pos, _, _ in records:
        if typ not in (2, 3, 5) or flags not in (0, 1):
            raise ValueError('unexpected fixup form for this profile')
        width = 4 if typ == 3 else 2
        seen = set()
        while True:
            if pos in seen or pos + width > len(code):
                raise ValueError('cyclic or out-of-bounds NE relocation chain')
            seen.add(pos)
            result.append((pos, width, typ))
            pos = word(code, pos)
            if pos == 0xffff:
                break
    return result


def segment(data):
    if data[:2] != b'MZ':
        raise ValueError('not an MZ image')
    ne, = struct.unpack_from('<I', data, 60)
    if data[ne:ne+2] != b'NE':
        raise ValueError('not an NE image')
    if (word(data, ne+0x1c), word(data, ne+0x14), word(data, ne+0x16)) != (2, 0x2563, 1):
        raise ValueError('not the WINMINE layout this sweep understands')
    sector, length, flags, _ = struct.unpack_from('<4H', data, ne + word(data, ne+0x22))
    align = word(data, ne+0x32)
    if align != 4 or length != 0x2bd0 or flags & 0x101 != 0x100:
        raise ValueError('unexpected WINMINE code segment layout')
    start = sector << align
    end = start + length
    count = word(data, end)
    rows = [struct.unpack_from('<BBHHH', data, end+2+8*i) for i in range(count)]
    code = data[start:end]
    fixups = chain_sites(code, rows)
    if (0x2561, 2, 5) not in fixups:
        raise ValueError('expected relocated data word in code is absent')
    return code, fixups


def family(raw):
    # Segment overrides and LOCK/REP do not hide the following opcode.
    for op in raw:
        if op not in (0x26, 0x2e, 0x36, 0x3e, 0xf0, 0xf2, 0xf3):
            break
    else:
        return None
    if op in (0x68, 0x6a):
        return 'PUSH immediate'
    if op in (0xc0, 0xc1):
        return 'shift/rotate immediate'
    if op in (0x69, 0x6b):
        return 'IMUL immediate'
    if op in (*range(0x60, 0x70), 0xc8, 0xc9, 0x0f):
        return 'other suspect opcode'
    return None


def scan(code, fixups):
    hits = []
    # Exclude the known non-instruction OFFSET16 word in segment 1.
    for a, b in ((0, 0x2561), (0x2563, len(code))):
        lines = subprocess.check_output(['ndisasm', '-b', '16', '-o', str(a), '-'],
                                        input=code[a:b]).decode('ascii').splitlines()
        pos = a
        for line in lines:
            m = re.fullmatch(r'([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+(.*)', line)
            if not m or int(m[1], 16) != pos:
                raise ValueError('unexpected disassembler output or discontinuous offsets')
            size = len(bytes.fromhex(m[2]))
            raw = code[pos:pos+size]
            if bytes.fromhex(m[2]) != raw:
                raise ValueError('disassembler bytes do not match input')
            kind = family(raw)
            if kind:
                overlaps = [(p, w, t) for p, w, t in fixups if p < pos+size and pos < p+w]
                hits.append((pos, kind, overlaps))
            pos += size
        if pos != b:
            raise ValueError('disassembly stopped early')
    return hits


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=Path)
    ap.add_argument('--sites', action='store_true', help='also list each code offset')
    args = ap.parse_args()
    try:
        hits = scan(*segment(args.exe.read_bytes()))
    except (OSError, struct.error, ValueError, subprocess.SubprocessError) as exc:
        sys.exit('winminecpu: ' + str(exc))
    for kind, n in sorted(Counter(h[1] for h in hits).items()):
        print(f'{kind}: {n}')
    print(f'{len(hits)} candidate sites; {sum(bool(x[2]) for x in hits)} overlap NE fixups')
    for pos, kind, refs in hits:
        if args.sites or refs:
            suffix = ''.join(f' [fixup type {t} at 1:{p:04X}, width {w}]' for p, w, t in refs)
            print(f'1:{pos:04X} {kind}{suffix}')
    print('Static linear sweep of the known layout, not a reachability or execution test.')


if __name__ == '__main__':
    main()
