#!/usr/bin/env python3
"""Experimental private-copy WINMINE 8088 relayout, not a general NE converter.

All 455 statically identified 186 instructions are lowered. Direct branches,
NE entries/relocations, the known CS data slot and three CRT code pointers are
remapped. Unknown indirect code pointers, interrupt timing and actual gameplay
are NOT proved by this transformation. Use only for controlled runtime tests.
No original executable bytes are embedded; input must be supplied privately.
"""
import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import struct
import subprocess
import sys
from winminecpu import segment, word, family


def w16(n):
    return struct.pack('<H', n & 0xffff)


@dataclass
class Instruction:
    pos: int
    raw: bytes
    text: str = ''
    size: int = 0
    kind: str = 'copy'
    target: int = 0


def decode(code):
    result = []
    # Preserve the previously mapped opaque zero spans without treating them
    # as executable instructions or claiming they cannot be indirectly reached.
    gaps = [(0, 0x10), (0x255b, 0x2560), (0x2561, 0x2563),
            (0x27c3, 0x27c4), (0x29df, 0x29e0), (0x2a39, 0x2a3a),
            (0x2ab7, 0x2ab8), (0x2abb, 0x2abc)]
    spans, a = [], 0
    for begin, end in gaps:
        if a < begin:
            spans.append((a, begin, False))
        spans.append((begin, end, True))
        a = end
    spans.append((a, len(code), False))
    for a, b, opaque in spans:
        if opaque:
            if a != 0x2561 and any(code[a:b]):
                raise ValueError('opaque gap no longer zero-filled')
            for pos in range(a, b):
                result.append(Instruction(pos, code[pos:pos+1], 'data'))
            continue
        output = subprocess.check_output(['ndisasm', '-b', '16', '-o', str(a), '-'], input=code[a:b]).decode('ascii')
        pos = a
        for line in output.splitlines():
            m = re.fullmatch(r'([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+(.*)', line)
            if not m or int(m[1], 16) != pos:
                raise ValueError('disassembly discontinuity')
            raw = bytes.fromhex(m[2])
            if raw != code[pos:pos+len(raw)]:
                raise ValueError('disassembly byte mismatch')
            result.append(Instruction(pos, raw, m[3]))
            pos += len(raw)
        if pos != b:
            raise ValueError('truncated disassembly')
    return result


def prepare(insns):
    for ins in insns:
        raw = ins.raw
        ins.size = len(raw)
        op = raw[0]
        if ins.text == 'data':
            continue
        if op in (0x68, 0x6a):
            if len(raw) != (3 if op == 0x68 else 2):
                raise ValueError('unsupported PUSH encoding')
            ins.kind, ins.size = 'push', 5
        elif op == 0xc1:
            if len(raw) != 3 or raw[1] >> 6 != 3 or (raw[1] >> 3) & 7 not in (4, 5, 7) or not 1 <= raw[2] <= 31:
                raise ValueError('unsupported shift encoding')
            ins.kind, ins.size = 'shift', 2*raw[2]
        elif op == 0x6b:
            # IMUL BX,AX,imm8. Preserve AX/DX with ordinary stack spills.
            if len(raw) != 3 or raw[1] != 0xd8:
                raise ValueError('unsupported IMUL encoding')
            ins.kind, ins.size = 'imul', 11
        elif op in (0xe8, 0xe9, 0xeb, *range(0x70, 0x80), *range(0xe0, 0xe4)):
            width = 2 if op in (0xe8, 0xe9) else 1
            if len(raw) != width+1:
                raise ValueError('unsupported branch encoding')
            disp = int.from_bytes(raw[1:], 'little', signed=True)
            ins.kind, ins.target = 'branch', (ins.pos + len(raw) + disp) & 0xffff
        elif family(raw):
            raise ValueError('unhandled post-8088 opcode')


def layout(insns):
    prepare(insns)
    while True:
        mapping, pos = {}, 0
        for ins in insns:
            mapping[ins.pos] = pos
            pos += ins.size
        changed = False
        for ins in insns:
            if ins.kind != 'branch':
                continue
            if ins.target not in mapping:
                raise ValueError(f'branch target {ins.target:04x} is not an instruction boundary')
            delta = mapping[ins.target] - (mapping[ins.pos] + ins.size)
            if len(ins.raw) == 2 and ins.size == 2 and not -128 <= delta <= 127:
                op = ins.raw[0]
                ins.size = 3 if op == 0xeb else 5 if 0x70 <= op <= 0x7f else 7
                changed = True
        if not changed:
            break
    literals = {}
    for ins in insns:
        if ins.kind == 'push':
            literals[ins.pos] = pos
            pos += 2
    if pos > 0xffff:
        raise ValueError('expanded code exceeds supported segment size')
    return mapping, literals, pos


def emit(insns, mapping, literals):
    out = bytearray()
    byte_map = {}
    cs_refs = 0
    for ins in insns:
        pos, raw = len(out), ins.raw
        if pos != mapping[ins.pos]:
            raise ValueError('inconsistent instruction layout')
        if ins.kind == 'push':
            new = b'\x2e\xff\x36' + w16(literals[ins.pos])
            # NE fixups in immediate PUSH migrate to literal pool words.
            byte_map[ins.pos+1] = literals[ins.pos]
            if len(raw) == 3:
                byte_map[ins.pos+2] = literals[ins.pos]+1
        elif ins.kind == 'shift':
            new = bytes((0xd1, raw[1])) * raw[2]
        elif ins.kind == 'imul':
            immediate = int.from_bytes(raw[2:], 'little', signed=True)
            new = b'\x50\x52\xbb' + w16(immediate) + b'\xf7\xeb\x89\xc3\x5a\x58'
        elif ins.kind == 'branch':
            op, dest = raw[0], mapping[ins.target]
            if ins.size == 2:
                new = bytes((op, (dest-pos-2) & 255))
            elif op in (0xe8, 0xe9, 0xeb):
                new = bytes((0xe8 if op == 0xe8 else 0xe9,)) + w16(dest-pos-3)
            elif 0x70 <= op <= 0x7f:
                new = bytes((op ^ 1, 3, 0xe9)) + w16(dest-pos-5)
            else:
                # LOOPcc/JCXZ retain their original flags/CX semantics.
                new = bytes((op, 2, 0xeb, 3, 0xe9)) + w16(dest-pos-7)
        else:
            new = raw
            for offset in range(len(raw)):
                byte_map[ins.pos+offset] = pos+offset
            if raw[:3] == bytes((0x2e, 0xf7, 0x06)) and len(raw) == 7 and word(raw, 3) == 0x2561:
                new = raw[:3] + w16(mapping[0x2561]) + raw[5:]
                cs_refs += 1
            elif 'cs:' in ins.text:
                raise ValueError('unrecognized CS-relative reference')
        if len(new) != ins.size:
            raise ValueError('emission size mismatch')
        out += new
    for ins in insns:
        if ins.kind == 'push':
            value = word(ins.raw, 1) if len(ins.raw) == 3 else int.from_bytes(ins.raw[1:], 'little', signed=True)
            if len(out) != literals[ins.pos]:
                raise ValueError('literal layout mismatch')
            out += w16(value)
    return out, byte_map, cs_refs


def rewrite_relocs(code, old_code, rows, byte_map, mapping):
    new_rows = []
    for typ, flags, source, a, b in rows:
        if typ not in (2, 3, 5) or flags not in (0, 1):
            raise ValueError('unsupported relocation form')
        sources, seen = [], set()
        while True:
            if source in seen or source+2 > len(old_code):
                raise ValueError('bad relocation chain')
            seen.add(source)
            if source not in byte_map:
                raise ValueError('relocation source lost during expansion')
            mapped = byte_map[source]
            width = 4 if typ == 3 else 2
            if any(byte_map.get(source+i) != mapped+i for i in range(width)):
                raise ValueError('relocation field split during expansion')
            sources.append(mapped)
            source = word(old_code, source)
            if source == 0xffff:
                break
        for i, mapped in enumerate(sources):
            code[mapped:mapped+2] = w16(sources[i+1] if i+1 < len(sources) else 0xffff)
        if flags == 0 and a == 1:
            if b not in mapping:
                raise ValueError('internal code target not mapped')
            b = mapping[b]
        new_rows.append((typ, flags, sources[0], a, b))
    return b''.join(struct.pack('<BBHHH', *r) for r in new_rows)


def transform(data):
    old_code, _ = segment(data)
    insns = decode(old_code)
    mapping, literals, size = layout(insns)
    code, byte_map, cs_refs = emit(insns, mapping, literals)
    if cs_refs != 6:
        raise ValueError('expected six known CS references')
    ne = struct.unpack_from('<I', data, 60)[0]
    table = ne + word(data, ne+0x22)
    result = bytearray(data)
    for segno in (1, 2):
        entry = table + (segno-1)*8
        start = word(data, entry) << 4
        length = word(data, entry+2)
        old = data[start:start+length]
        count = word(data, start+length)
        rows = [struct.unpack_from('<BBHHH', data, start+length+2+8*i) for i in range(count)]
        new = code if segno == 1 else bytearray(old)
        smap = byte_map if segno == 1 else {i: i for i in range(length)}
        relocs = rewrite_relocs(new, old, rows, smap, mapping)
        if segno == 2:
            for offset in (0x168, 0x16a, 0x16c):
                if word(old, offset) != 0x2560:
                    raise ValueError('unexpected CRT code pointer')
                new[offset:offset+2] = w16(mapping[0x2560])
        result += bytes((-len(result)) % 16)
        new_start = len(result)
        if new_start >> 4 > 0xffff:
            raise ValueError('file exceeds NE sector range')
        result += new + w16(count) + relocs
        struct.pack_into('<HH', result, entry, new_start >> 4, len(new))
        allocation = word(data, entry+6) or 0x10000
        struct.pack_into('<H', result, entry+6, max(allocation, len(new)) & 0xffff)
    # Map executable entry points; no resource/file offsets move.
    struct.pack_into('<H', result, ne+0x14, mapping[word(data, ne+0x14)])
    pos, end = ne+word(data, ne+4), ne+word(data, ne+4)+word(data, ne+6)
    while pos < end:
        count, seg = data[pos:pos+2]
        pos += 2
        if not count:
            break
        for _ in range(count):
            if seg == 0:
                continue
            if seg == 0xff:
                actual, offpos, advance = data[pos+3], pos+4, 6
            else:
                actual, offpos, advance = seg, pos+1, 3
            if actual == 1:
                struct.pack_into('<H', result, offpos, mapping[word(data, offpos)])
            pos += advance
    return bytes(result), {'push': len(literals), 'shift': sum(i.kind == 'shift' for i in insns),
                           'imul': sum(i.kind == 'imul' for i in insns), 'code_bytes': size,
                           'widened_branches': sum(i.kind == 'branch' and i.size > len(i.raw) for i in insns)}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    try:
        out, counts = transform(args.input.read_bytes())
        with args.output.open('xb') as stream:
            stream.write(out)
    except (OSError, ValueError, KeyError, struct.error, subprocess.SubprocessError) as exc:
        sys.exit('winmine8088: ' + str(exc))
    print(counts)
    print('Experimental relayout only. Runtime correctness and gameplay remain unverified.')


if __name__ == '__main__':
    main()
