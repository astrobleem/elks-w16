#!/usr/bin/env python3
"""Synthetic instruction and relocation tests; no Windows image required."""
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from winmineshifts import lower_shifts, transform


def target(code, pos):
    return (pos + 3 + struct.unpack_from('<h', code, pos+1)[0]) & 0xffff


class WinmineShifts(unittest.TestCase):
    def test_all_registers_and_operations(self):
        for reg in range(8):
            for ext in (4, 5, 7):
                for count in (1, 3, 4, 5, 31):
                    modrm = 0xc0 | ext*8 | reg
                    src = bytes((0xc1, modrm, count, 0xcb))
                    out = lower_shifts(src, [], [0])
                    self.assertEqual(target(out, 0), 4)
                    self.assertEqual(out[4:-3], bytes((0xd1, modrm))*count)
                    self.assertEqual(target(out, len(out)-3), 3)
                    self.assertEqual(out[3], 0xcb)

    def test_two_sites_and_unmodified_relocation(self):
        src = bytes.fromhex('c1e004 ffff c1e305 cb')
        out = lower_shifts(src, [(3, 2, 5)], [0, 5])
        self.assertEqual(out[3:5], src[3:5])
        self.assertEqual(target(out, 0), 9)
        self.assertEqual(target(out, 5), 20)
        self.assertEqual(target(out, 17), 3)
        self.assertEqual(target(out, 30), 8)

    def test_reject_unsupported(self):
        for raw in ('c1e000', 'c1e020', 'c1f001', 'c0e004', 'c12604', '2ec1e0'):
            with self.assertRaises(ValueError):
                lower_shifts(bytes.fromhex(raw), [], [0])
        with self.assertRaises(ValueError):
            lower_shifts(bytes.fromhex('c1e004'), [(1, 2, 5)], [0])
        with self.assertRaises(ValueError):
            lower_shifts(bytes.fromhex('c1e004'), [], [0, 0])
        with self.assertRaises(ValueError):
            lower_shifts(bytes.fromhex('c1e004') + bytes(65530), [], [0])

    def test_ne_preserves_tables_and_relocations(self):
        # Stub the layout discriminator/disassembler, not the NE serialization.
        data = bytearray(512)
        struct.pack_into('<I', data, 60, 64)
        struct.pack_into('<H', data, 64+0x22, 64)
        struct.pack_into('<4H', data, 128, 16, 4, 0x100, 4)
        code = bytes.fromhex('c1e004 cb')
        data[256:260] = code
        reloc = bytes.fromhex('0100 0501 0300 0100 0000')
        data[260:270] = reloc
        with patch('winmineshifts.segment', return_value=(code, [])), patch('winmineshifts.scan', return_value=[(0, 'shift/rotate immediate', [])]):
            out, count = transform(bytes(data))
        self.assertEqual(count, 1)
        self.assertEqual(out[:128], data[:128])
        self.assertEqual(out[136:512], data[136:512])
        sector, length, flags, alloc = struct.unpack_from('<4H', out, 128)
        self.assertEqual((sector, length, flags, alloc), (32, 15, 0x100, 15))
        self.assertEqual(out[512+length:], reloc)


if __name__ == '__main__':
    unittest.main()
