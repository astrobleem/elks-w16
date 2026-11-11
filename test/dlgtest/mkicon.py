#!/usr/bin/env python3
"""mkicon.py - write the DLGTEST icon: a 32x32, 16-colour Windows 3.x .ico (original artwork: a small
window with a blue caption, a red square and a green circle, transparent corners). Usage: mkicon.py OUT.ico"""
import struct, sys

W = H = 32
PAL = [(0, 0, 0), (128, 0, 0), (0, 128, 0), (128, 128, 0), (0, 0, 128), (128, 0, 128), (0, 128, 128), (192, 192, 192),
       (128, 128, 128), (255, 0, 0), (0, 255, 0), (255, 255, 0), (0, 0, 255), (255, 0, 255), (0, 255, 255), (255, 255, 255)]
pix = [[None] * W for _ in range(H)]          # None = transparent
for y in range(2, 30):
    for x in range(2, 30):
        pix[y][x] = 15                         # white window body
for x in range(2, 30):
    pix[2][x] = pix[29][x] = 0
for y in range(2, 30):
    pix[y][2] = pix[y][29] = 0
for y in range(3, 9):
    for x in range(3, 29):
        pix[y][x] = 4                          # navy caption
for y in range(12, 22):
    for x in range(6, 16):
        pix[y][x] = 9                          # red square
for y in range(12, 26):
    for x in range(16, 28):
        if (x - 21.5) ** 2 + (y - 18.5) ** 2 <= 30:
            pix[y][x] = 10                     # green circle
stride = 16                                    # 32 px * 4 bpp / 8, already a multiple of 4
xor = bytearray()
mask = bytearray()
for y in range(H - 1, -1, -1):                 # DIB rows are bottom-up
    row = bytearray()
    for x in range(0, W, 2):
        a = pix[y][x] or 0; b = pix[y][x + 1] or 0
        row.append((a << 4) | b)
    xor += row
    bits = 0
    for x in range(W):
        bits = (bits << 1) | (1 if pix[y][x] is None else 0)
    mask += struct.pack('>I', bits)
bih = struct.pack('<IiiHHIIiiII', 40, W, H * 2, 1, 4, 0, len(xor) + len(mask), 0, 0, 16, 0)
pal = b''.join(struct.pack('BBBB', b, g, r, 0) for r, g, b in PAL)
img = bih + pal + bytes(xor) + bytes(mask)
ico = struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHII', W, H, 16, 0, 1, 4, len(img), 22) + img
open(sys.argv[1], 'wb').write(ico)
print('wrote', sys.argv[1], len(ico), 'bytes')
