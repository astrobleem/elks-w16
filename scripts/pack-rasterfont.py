#!/usr/bin/env python3
"""Losslessly pack Nano-X's original proportional system font for near storage."""
import re
import sys
from pathlib import Path
source = Path(sys.argv[1]).read_text(encoding='latin1')
source = re.sub(r'/\*.*?\*/', '', source, flags=re.S)
def array(name):
    match = re.search(r'\b' + name + r'\[\]\s*=\s*\{([^}]+)\}', source)
    if not match:
        raise SystemExit('Unsupported Nano-X system font array: ' + name)
    return [int(v.strip(), 0) for v in match[1].split(',') if v.strip()]
rows = array('winFreeSansSerif11x13_bits')
widths = array('winFreeSansSerif11x13_width')
if len(widths) != 224 or len(rows) != 224 * 13 or not all(0 < w <= 16 for w in widths):
    raise SystemExit('Unsupported Nano-X system font dimensions')
bits = []
offsets = []
for ch, width in enumerate(widths):
    offsets.append(len(bits))
    for row in rows[ch * 13:(ch + 1) * 13]:
        if row & ((1 << (16 - width)) - 1):
            raise SystemExit('Font ink extends beyond glyph width')
        bits.extend((row >> (15 - x)) & 1 for x in range(width))
packed = bytearray((len(bits) + 7) // 8 + 2)
for pos, bit in enumerate(bits):
    packed[pos // 8] |= bit << (7 - pos % 8)
# Verify the exact runtime decoding formula against every original font row.
for ch, width in enumerate(widths):
    for y in range(13):
        pos = offsets[ch] + width * y
        at = pos // 8
        value = ((packed[at] << 16) | (packed[at+1] << 8) | packed[at+2]) >> (8-pos%8)
        assert (value & (0xffff << (16-width)) & 0xffff) == rows[ch*13+y]
def emit(typ, name, values):
    print('static const ' + typ + ' ' + name + '[] = {')
    for i in range(0, len(values), 16):
        print(' ' + ','.join(str(v) for v in values[i:i+16]) + ',')
    print('};')
print('/* Generated from Nano-X winFreeSansSerif11x13, MPL (c) 1999 darran@rimron.co.uk. */')
emit('unsigned char', 'font_width', widths)
emit('unsigned short', 'font_offset', offsets)
emit('unsigned char', 'font_packed', packed)
