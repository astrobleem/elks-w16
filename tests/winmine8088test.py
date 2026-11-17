#!/usr/bin/env python3
"""Original synthetic tests for the experimental WINMINE relayout."""
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from winmine8088 import Instruction, layout, emit, rewrite_relocs, w16


def build(*raws):
    result, pos = [], 0
    for raw in raws:
        result.append(Instruction(pos, raw))
        pos += len(raw)
    m, literals, size = layout(result)
    out, byte_map, _ = emit(result, m, literals)
    assert len(out) == size
    return result, m, literals, out, byte_map


def target(code, pos, width):
    return (pos + 1 + width + int.from_bytes(code[pos+1:pos+1+width], 'little', signed=True)) & 65535


class Relayout(unittest.TestCase):
    def test_push_every_imm8(self):
        for value in range(256):
            _, _, literals, out, _ = build(bytes((0x6a, value)), b'\xcb')
            literal = literals[0]
            self.assertEqual(out[:5], b'\x2e\xff\x36'+w16(literal))
            self.assertEqual(int.from_bytes(out[literal:literal+2], 'little'), value if value < 128 else value+65280)

    def test_push_words(self):
        for value in (0, 1, 127, 128, 0x7fff, 0x8000, 0xff00, 0xffff):
            _, _, literals, out, byte_map = build(b'\x68'+w16(value))
            self.assertEqual(out[literals[0]:], w16(value))
            self.assertEqual(byte_map, {1: literals[0], 2: literals[0]+1})

    def test_branch_relaxation(self):
        for opcode, expected_size in ((0xeb, 3), (0x74, 5), (0xe0, 7), (0xe1, 7), (0xe2, 7), (0xe3, 7)):
            raws = [bytes((opcode, 120))] + [bytes((0x6a, 1))]*60 + [b'\xcb']
            insns, m, _, out, _ = build(*raws)
            self.assertEqual(insns[0].size, expected_size)
            near = expected_size-3
            self.assertEqual(target(out, near, 2), m[122])
            if expected_size == 5:
                self.assertEqual(out[0], opcode ^ 1)
                self.assertEqual(target(out, 0, 1), 5)
            if expected_size == 7:
                self.assertEqual(target(out, 0, 1), 4)
                self.assertEqual(target(out, 2, 1), 7)

    def test_backward_near_and_short(self):
        insns, m, _, out, _ = build(b'\x6a\x01', b'\xe8\xfb\xff', b'\xeb\xf9')
        self.assertEqual(target(out, m[2], 2), 0)
        self.assertEqual(target(out, m[5], 1), 0)

    def test_bad_branch_and_opcode_rejected(self):
        for raws in ((b'\xeb\x01', b'\xb8\x00\x00'), (b'\x6b\xc0\x02',), (b'\xc1\x20\x03',), (b'\x60',)):
            with self.assertRaises(ValueError):
                build(*raws)

    def test_relocation_chain_moves_to_literals(self):
        original = b'\x68\x04\x00\x68\xff\xff\xcb'
        _, mapping, literal, out, byte_map = build(original[:3], original[3:6], original[6:])
        encoded = rewrite_relocs(out, original, [(2, 1, 1, 2, 3)], byte_map, mapping)
        self.assertEqual(struct.unpack('<BBHHH', encoded), (2, 1, literal[0], 2, 3))
        self.assertEqual(out[literal[0]:literal[0]+2], w16(literal[3]))
        self.assertEqual(out[literal[3]:literal[3]+2], b'\xff\xff')
        with self.assertRaises(ValueError):
            rewrite_relocs(out, original, [(3, 1, 1, 2, 3)], byte_map, mapping)

    def test_internal_far_target(self):
        original = b'\x6a\x00\x9a\xff\xff\x00\x00\xcb'
        _, mapping, _, out, byte_map = build(original[:2], original[2:7], original[7:])
        encoded = rewrite_relocs(out, original, [(3, 0, 3, 1, 7)], byte_map, mapping)
        self.assertEqual(struct.unpack('<BBHHH', encoded), (3, 0, mapping[2]+1, 1, mapping[7]))

    def test_imul_sequence_and_signed_arithmetic(self):
        for immediate in (0, 1, 6, 127, 128, 255):
            _, _, _, out, _ = build(bytes((0x6b, 0xd8, immediate)))
            signed = immediate if immediate < 128 else immediate-256
            self.assertEqual(out, b'\x50\x52\xbb'+w16(signed)+b'\xf7\xeb\x89\xc3\x5a\x58')
            # Small independent sequence executor: stack saves, MOV, signed F7
            # multiply, MOV, POP. Flags unaffected by saves/MOV/restores.
            for ax in (0, 1, 0x7fff, 0x8000, 0xffff):
                regs = {'ax': ax, 'dx': 0x1234, 'bx': 0x4321, 'sp': 0x200}
                stack = []
                for register in ('ax', 'dx'):
                    stack.append(regs[register]); regs['sp'] -= 2
                regs['bx'] = int.from_bytes(out[3:5], 'little', signed=True)
                product = (regs['ax'] if regs['ax'] < 32768 else regs['ax']-65536)*regs['bx']
                overflow = not -32768 <= product <= 32767
                regs['ax'], regs['dx'] = product & 65535, (product >> 16) & 65535
                regs['bx'] = regs['ax']
                for register in ('dx', 'ax'):
                    regs[register] = stack.pop(); regs['sp'] += 2
                self.assertEqual((regs['ax'], regs['dx'], regs['sp']), (ax, 0x1234, 0x200))
                self.assertEqual(regs['bx'], ((ax if ax < 32768 else ax-65536)*signed) & 65535)
                self.assertEqual(overflow, product != ((product & 65535) if product & 0x8000 == 0 else (product & 65535)-65536))


if __name__ == '__main__':
    unittest.main()
