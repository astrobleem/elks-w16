#!/usr/bin/env python3
"""Synthetic single-data DLLs only; no original executable bytes."""
import os,pathlib,struct,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]

def fixture(library):
    b=bytearray(512);n=64
    def w(o,v):struct.pack_into('<H',b,o,v)
    b[:2]=b'MZ';w(24,64);w(60,n);b[n:n+2]=b'NE'
    for off,val in ((4,64),(6,13),(12,0x8001 if library else 2),(14,2),
                    (16,32),(18,0 if library else 64),(22,1),(26,0 if library else 2),
                    (28,2),(30,0 if library else 1),(32,16 if library else 0),
                    (34,80),(36,96),(38,96),(40,112),(42,116),(50,4)):
        w(n+off,val)
    b[n+54]=2
    struct.pack_into('<I',b,n+44,192 if library else 0)
    # Three fixed entries: valid exported, out-of-code, unexported.
    b[n+64:n+77]=bytes([3,1,1,0,0,1,99,0,0,0,0,0,0])
    struct.pack_into('<4H',b,n+80,16,16,0 if library else 256,16)
    struct.pack_into('<4H',b,n+88,0,0,1,16)
    b[n+96:n+107]=b'\x07DLLNAME\0\0\0'
    if library:
        b[n+106:n+111]=b'\x01X\x01\0\0'
        b[192:208]=b'\x04DRAW\x01\0\x09DRAWALIAS' # replaced below
        names=b'\x04DRAW\x01\0\x09DRAWALIAS\x01\0\0'
        b[192:192+len(names)]=names;w(n+32,len(names))
        b[256:272]=bytes.fromhex('1e 58 90 55 8b ec 1e 8e d8 1f 5d cb 90 90 90 90')
    else:
        w(n+112,0)
        names=b'\x04TEST\x04DRAW';b[n+116:n+116+len(names)]=names
        b[256:272]=bytes.fromhex('90 90 90 9a ff ff 00 00 90 9a ff ff 00 00 cb 90')
        w(272,2)
        struct.pack_into('<BB3H',b,274,3,2,4,1,5)
        struct.pack_into('<BB3H',b,282,3,1,10,1,1)
    return b

with tempfile.TemporaryDirectory() as td:
    d=pathlib.Path(td)
    (d/'test.dll').write_bytes(fixture(True));(d/'test.exe').write_bytes(fixture(False))
    bad=fixture(True);struct.pack_into('<H',bad,64+32,4);(d/'bad.dll').write_bytes(bad)
    exe=d/'test'
    subprocess.run(['cc','-DW16_HOST','-O1','-g','-Wall','-Wextra','-Wno-unused-parameter',
                    '-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fsanitize=address,undefined','-fno-pie','-no-pie',
                    '-I'+str(root/'src'),str(root/'tests/library_test.c'),
                    *[str(root/'src'/f) for f in ('ne.c','mem.c','api.c','dlgtmpl.c','kernel.c','files.c')],
                    '-o',str(exe)],check=True)
    subprocess.run([str(exe),str(d/'test.dll'),str(d/'test.exe'),str(d/'bad.dll')],check=True,
                   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
