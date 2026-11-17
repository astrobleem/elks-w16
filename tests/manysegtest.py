"""Build a redistributable NE with executable code in its last segment."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

def fixture(n):
    header = 64
    segtable = 64
    names = segtable + 8*n
    code = (header + names + 16 + 15) & ~15
    b = bytearray(code + 5)
    def word(off, value):
        struct.pack_into('<H', b, off, value)
    b[:2] = b'MZ'
    word(0x18, 64)
    word(0x3c, header)
    b[header:header+2] = b'NE'
    for off, value in ((0xc, 2), (0xe, 1), (0x12, 256), (0x16, n),
                       (0x1a, 1), (0x1c, n), (0x22, segtable), (0x24, names),
                       (0x26, names), (0x32, 4)):
        word(header+off, value)
    b[header+0x36] = 2
    for i in range(n):
        last = i == n-1
        struct.pack_into('<4H', b, header+segtable+8*i,
                         code//16 if last else 0, 5 if last else 0, int(i == 0), 16)
    b[header+names:header+names+5] = b'\x04MANY'
    b[code:] = bytes.fromhex('b8 2a 4c cd 21')  # DOS exit(42), segment n
    return b

if __name__ == '__main__':
    if len(sys.argv) == 2:
        Path(sys.argv[1]).write_bytes(fixture(33))
        sys.exit()
    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        exe = d/'many.exe'
        for n in (33, 254):
            exe.write_bytes(fixture(n))
            run = subprocess.run(['src/w16host','-o', td, exe], capture_output=True, text=True)
            assert run.returncode == 0 and 'live allocations after free 0' in run.stdout, run
            assert (d/f'seg{n}.bin').read_bytes() == bytes.fromhex('b8 2a 4c cd 21') + bytes(11)
            run = subprocess.run(['src/w16host','-f',str(n-1),exe], capture_output=True, text=True)
            assert run.returncode == 14 and 'live allocations after free 0' in run.stdout, run
        print('33 and 254 segments load, last-segment contents match, partial loads clean up')
