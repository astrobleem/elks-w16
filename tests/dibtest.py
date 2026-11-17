#!/usr/bin/env python3
"""Bounded DIB parser checks; optional SOL_EXE/PBRUSH_EXE resource coverage."""
import os, pathlib, struct, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
    td = pathlib.Path(td)
    exe = td / 'dibtest'
    subprocess.run(['cc', '-DW16_HOST', '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
                    '-I'+str(root/'src'), str(root/'src/dib.c'),
                    str(root/'tests/dib_test.c'), '-o', str(exe)], check=True)
    args = [str(exe)]
    for key in ('SOL_EXE', 'PBRUSH_EXE'):
        if key not in os.environ:
            continue
        b = pathlib.Path(os.environ[key]).read_bytes()
        ne = struct.unpack_from('<I', b, 0x3c)[0]
        assert b[ne:ne+2] == b'NE'
        p = ne + struct.unpack_from('<H', b, ne+0x24)[0]
        shift = struct.unpack_from('<H', b, p)[0]; p += 2
        count = 0
        while True:
            typ = struct.unpack_from('<H', b, p)[0]
            if not typ: break
            n = struct.unpack_from('<H', b, p+2)[0]; p += 8
            for _ in range(n):
                off, size = struct.unpack_from('<HH', b, p); p += 12
                if typ != 0x8002: continue
                off <<= shift; size <<= shift
                assert off+size <= len(b)
                path = td / (key+str(count)+'.dib')
                path.write_bytes(b[off:off+size]); args.append(str(path)); count += 1
            
        assert count > 0
        print(key, count, 'bitmap resources', flush=True)
    subprocess.run(args, check=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
