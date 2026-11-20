"""Focused strict loader and console-call checks; no proprietary inputs."""
from pathlib import Path
import os
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ENV = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')

def run(args, **kw):
    return subprocess.run(args, check=True, **kw)

def fixture(folder):
    obj, raw = folder/'probe.o', folder/'probe.bin'
    run(['as', '--32', '-o', str(obj), str(ROOT/'test/os2vio/probe.S')])
    run(['objcopy', '-O', 'binary', '-j', '.text', str(obj), str(raw)])
    symbols = {}
    for line in subprocess.check_output(['nm', str(obj)], text=True).splitlines():
        address, _, name = line.split()
        symbols[name] = int(address, 16)
    code = raw.read_bytes()
    data = b'O16 repository VIO probe\r\n'
    assert len(data) == 26
    # Header-relative offsets; paragraph alignment, separate code/data.
    b = bytearray(512)
    def word(off, value): struct.pack_into('<H', b, off, value)
    b[:2] = b'MZ'; word(0x18, 64); word(0x3c, 64)
    b[64:66] = b'NE'
    for off, val in ((0xc, 0x0102), (0xe, 2), (0x12, 2048), (0x16, 1),
                     (0x1a, 2), (0x1c, 2), (0x1e, 2), (0x22, 64),
                     (0x24, 80), (0x26, 80), (0x28, 88), (0x2a, 92), (0x32, 4)):
        word(64+off, val)
    b[64+0x36] = 1
    b[144:152] = b'\x05PROBE\0\0'
    word(152, 0); word(154, 9)
    b[156:174] = b'\x08DOSCALLS\x08VIOCALLS'
    struct.pack_into('<4H', b, 128, 16, len(code), 0x100, len(code))
    struct.pack_into('<4H', b, 136, 24, len(data), 1, len(data))
    b[256:256+len(code)] = code
    pos = 256+len(code)
    word(pos, 2); pos += 2
    for operand, mod, ordinal in (('vio_operand', 2, 19), ('exit_operand', 1, 5)):
        struct.pack_into('<BBHHH', b, pos, 3, 1, symbols[operand], mod, ordinal)
        pos += 8
    assert pos < 384
    b[384:384+len(data)] = data
    return b

if __name__ == '__main__':
    with tempfile.TemporaryDirectory() as name:
        d = Path(name)
        image = fixture(d)
        exe = d/'probe.exe'
        def check(data, expected):
            exe.write_bytes(data)
            p = subprocess.run([str(ROOT/'src/o16-host'), '-n', str(exe)],
                               env=ENV, text=True, capture_output=True)
            assert p.returncode == expected, p.stderr
            assert 'live allocations=0' in p.stderr, p.stderr
        check(image, 0)
        for flags in (2, 0x0302, 0x010a, 0x0122, 0x0182, 0x8102):
            bad = bytearray(image); struct.pack_into('<H', bad, 64+0xc, flags)
            check(bad, 12)
        bad = bytearray(image); bad[64+0x36] = 2; check(bad, 12)
        # Same valid relocation, unsupported VIO ordinal: fail closed.
        code_len = struct.unpack_from('<H', image, 130)[0]
        bad = bytearray(image); struct.pack_into('<H', bad, 256+code_len+8, 20)
        check(bad, 13)
        exe.write_bytes(image)
        p = subprocess.run([str(ROOT/'src/w16host'), str(exe)], env=ENV,
                           text=True, capture_output=True)
        assert p.returncode == 12, p.stdout+p.stderr
        harness = d/'calls.c'
        harness.write_text(r'''
#define O16_NO_MAIN
#include "os2probe.c"
#include <assert.h>
int main(void) {
    struct ne_seg seg; u16 s; int fds[2]; char b[4];
    assert(!seg_alloc(4,&s)); memset(&seg,0,sizeof(seg));
    seg.sel=s;seg.paras=4;image.seg=&seg;image.nseg=1;
    assert(!pipe(fds));console_fd=fds[1];
    wr16(s,0,1);wr16(s,2,2);wr16(s,4,62);wr16(s,6,s);
    assert(o16_dispatch(0,s,0)==436);
    wr16(s,0,0);wr8(s,62,'O');wr8(s,63,'K');
    assert(o16_dispatch(0,s,0)==0);
    assert(read(fds[0],b,2)==2 && b[0]=='O' && b[1]=='K');
    wr16(s,2,3);assert(o16_dispatch(0,s,0)==487);
    wr16(s,2,2);wr16(s,6,s+1);assert(o16_dispatch(0,s,0)==487);
    wr16(s,2,0);assert(o16_dispatch(0,s,0)==0);
    wr16(s,0,37);wr16(s,2,1);o16_dispatch(1,s,0);
    assert(o16_done && o16_code==37);
    wr16(s,2,0);o16_dispatch(1,s,0);assert(o16_code==12);
    o16_done=0;o16_dispatch(0,s,60);assert(o16_done && o16_code==12);
    seg_free(s);close(fds[0]);close(fds[1]);assert(!host_live);
    return 0;
}
''')
        run(['gcc','-DW16_HOST','-DW16_OS2_PROBE','-g','-fsanitize=address,undefined',
             '-fno-pie','-no-pie','-I'+str(ROOT/'src'),str(harness),
             str(ROOT/'src/ne.c'),str(ROOT/'src/mem.c'),'-o',str(d/'calls')])
        run([str(d/'calls')],env=ENV)
        print('OS/2 strict import relocation, ABI arguments/bounds, console output, exit and cleanup passed')
