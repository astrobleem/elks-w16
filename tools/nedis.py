#!/usr/bin/env python3
"""nedis.py - linear 16-bit disassembly of one NE segment with relocation targets annotated (needs capstone).
Host-only analysis aid. Usage: nedis.py FILE.EXE SEGNO [START END]"""
import struct, sys
import capstone

b = open(sys.argv[1], 'rb').read(); segno = int(sys.argv[2])
u16 = lambda o: struct.unpack_from('<H', b, o)[0]
ne = struct.unpack_from('<I', b, 0x3C)[0]
nmod, segtab, modref, impn, align = u16(ne + 0x1E), u16(ne + 0x22), u16(ne + 0x28), u16(ne + 0x2A), u16(ne + 0x32) or 9
mods = []
for i in range(nmod):
    o = u16(ne + modref + 2 * i); n = b[ne + impn + o]; mods.append(b[ne + impn + o + 1:ne + impn + o + 1 + n].decode())
fo, sz, fl, mn = struct.unpack_from('<HHHH', b, ne + segtab + 8 * (segno - 1))
code = b[fo << align:(fo << align) + sz]
notes = {}
if fl & 0x100:
    p = (fo << align) + sz
    for r in range(u16(p)):
        st, rf, src, t1, t2 = struct.unpack_from('<BBHHH', b, p + 2 + 8 * r)
        k = rf & 3
        desc = '%s.%d' % (mods[t1 - 1], t2) if k == 1 else ('entry %d' % t2 if t1 == 0xFF else 'seg%d:%04x' % (t1, t2))
        while True:                         # follow the chain of non-additive fixups
            notes[src] = desc
            nxt = struct.unpack_from('<H', code, src)[0] if src + 2 <= len(code) else 0xFFFF
            if rf & 4 or nxt == 0xFFFF or nxt in notes:
                break
            src = nxt
start = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0
end = int(sys.argv[4], 16) if len(sys.argv) > 4 else sz
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
for ins in md.disasm(code[start:end], start):
    tag = ''.join('  ; ' + notes[a] for a in range(ins.address, ins.address + ins.size) if a in notes)
    print('%04x: %-22s %s %s%s' % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str, tag))
