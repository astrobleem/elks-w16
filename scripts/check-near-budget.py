#!/usr/bin/env python3
"""Reject executables the ELKS loader cannot fit with heap, stack and argv."""
import struct
import sys
from pathlib import Path
header = Path(sys.argv[1]).read_bytes()[:64]
if len(header) != 64 or header[:4] != bytes((1, 3, 0x30, 4)) or header[6] != 1:
    raise SystemExit('Unsupported ELKS executable header')
data, bss = struct.unpack_from('<II', header, 12)
heap, stack = struct.unpack_from('<HH', header, 24)
# Preserve at least 1 KiB for normal argv/environment, beyond the explicit stack.
if not heap or not stack or data + bss + heap + stack + 1024 > 0xfff0:
    raise SystemExit('W16 near-data budget exceeds 64 KiB including heap, stack and 1 KiB argv/environment')
