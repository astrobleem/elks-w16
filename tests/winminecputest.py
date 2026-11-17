#!/usr/bin/env python3
"""Synthetic checks for fixup chain traversal and CPU preflight classification."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from winminecpu import chain_sites, family, segment


class WinmineCPU(unittest.TestCase):
    def test_chained(self):
        code = bytes.fromhex('0400 0000 ffff 0000')
        self.assertEqual(chain_sites(code, [(3, 1, 0, 1, 0)]), [(0, 4, 3), (4, 4, 3)])

    def test_bad_chains(self):
        for code in (b'\x00\x00', b'\x01\x00'):
            with self.assertRaises(ValueError):
                chain_sites(code, [(2, 0, 0, 1, 0)])

    def test_reject_unrelated_input(self):
        with self.assertRaises(ValueError):
            segment(bytes(100))

    def test_families(self):
        self.assertEqual(family(bytes.fromhex('2e c1')), 'shift/rotate immediate')
        for opcode, expected in ((0x68, 'PUSH immediate'), (0x69, 'IMUL immediate'),
                                 (0xc1, 'shift/rotate immediate'), (0x60, 'other suspect opcode'),
                                 (0x50, None)):
            self.assertEqual(family(bytes([opcode])), expected)


if __name__ == '__main__':
    unittest.main()
