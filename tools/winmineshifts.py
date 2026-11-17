#!/usr/bin/env python3
"""Experimental, partial WINMINE lowering: register immediate shifts only.

Writes a new private NE copy, never changes the input. PUSH-immediate and IMUL
remain unsupported on 8088: this is NOT a runnable 8088 conversion. Requires
ndisasm. No original application bytes are embedded in this tool.
"""
import argparse
from pathlib import Path
import struct
import subprocess
import sys
from winminecpu import scan, segment, word


def lower_shifts(code, fixups, sites):
    """Keep original addresses; append stack-free D1 shift trampolines.

    Defined flags and all non-destination registers are preserved. OF after
    multi-bit shifts is undefined on the original CPU. No temporary stack or
    scratch memory is used. Only unprefixed C1 register SHL/SHR/SAR, counts 1..31,
    are accepted. Sites must come from the known-layout disassembly, not a raw
    byte search. This is not a general reachability or indirect-target proof.
    """
    result = bytearray(code)
    seen = set()
    for pos in sites:
        if pos in seen or not 0 <= pos <= len(code) - 3:
            raise ValueError('duplicate or out-of-bounds shift site')
        seen.add(pos)
        op, modrm, count = code[pos:pos+3]
        if op != 0xc1 or modrm >> 6 != 3 or (modrm >> 3) & 7 not in (4, 5, 7):
            raise ValueError('only unprefixed register SHL/SHR/SAR supported')
        if not 1 <= count <= 31:
            raise ValueError('shift count outside supported range')
        if any(p < pos+3 and pos < p+w for p, w, _ in fixups):
            raise ValueError('shift overlaps relocation')
        if any(other != pos and abs(other-pos) < 3 for other in sites):
            raise ValueError('overlapping shift sites')
        target = len(result)
        if target + 2*count + 3 > 0x10000:
            raise ValueError('lowered code exceeds 64 KiB segment')
        result[pos:pos+3] = b'\xe9' + struct.pack('<H', (target-pos-3) & 0xffff)
        result += bytes((0xd1, modrm)) * count
        result += b'\xe9' + struct.pack('<H', (pos+3-len(result)-3) & 0xffff)
    return bytes(result)


def transform(data):
    code, fixups = segment(data)
    hits = scan(code, fixups)
    sites = [p for p, kind, _ in hits if kind == 'shift/rotate immediate']
    lowered = lower_shifts(code, fixups, sites)
    ne = struct.unpack_from('<I', data, 60)[0]
    entry = ne + word(data, ne+0x22)
    old_start = word(data, entry) << 4
    rel_start = old_start + len(code)
    rel_end = rel_start + 2 + 8*word(data, rel_start)
    if rel_end > len(data):
        raise ValueError('truncated relocation table')
    # Append the relocated segment rather than moving any existing NE resource,
    # table, or data segment. The old code copy becomes unreferenced file data.
    result = bytearray(data)
    result += bytes((-len(result)) % 16)
    start = len(result)
    if start >> 4 > 0xffff:
        raise ValueError('new code sector exceeds NE field')
    result += lowered + data[rel_start:rel_end]
    struct.pack_into('<HH', result, entry, start >> 4, len(lowered) & 0xffff)
    old_alloc = word(data, entry+6) or 0x10000
    struct.pack_into('<H', result, entry+6, max(old_alloc, len(lowered)) & 0xffff)
    return bytes(result), len(sites)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    try:
        data, count = transform(args.input.read_bytes())
        # Exclusive creation also refuses aliases of the input and other outputs.
        with args.output.open('xb') as stream:
            stream.write(data)
    except (OSError, ValueError, struct.error, subprocess.SubprocessError) as exc:
        sys.exit('winmineshifts: ' + str(exc))
    print(f'Lowered {count} immediate shifts; PUSH-immediate and IMUL remain.')
    print('Partial experimental copy only; not an 8088-ready executable.')


if __name__ == '__main__':
    main()
