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
    b = bytearray(1024)
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
    struct.pack_into('<4H', b, 136, 32, len(data), 1, len(data))
    b[256:256+len(code)] = code
    pos = 256+len(code)
    word(pos, 4); pos += 2
    for operand, mod, ordinal in (('vio_operand', 2, 19), ('exit_operand', 1, 5),
                                  ('version_operand', 1, 92), ('mode_operand', 1, 49)):
        struct.pack_into('<BBHHH', b, pos, 3, 1, symbols[operand], mod, ordinal)
        pos += 8
    assert pos < 512
    b[512:512+len(data)] = data
    return b

def ibm_layout_fixture(original):
    """Owned bytes; only segment geometry/flags match the observed IBM file."""
    b=bytearray(4660)
    b[:256]=original[:256]
    # Move NE from 0x40 to IBM's observed 0x80, keeping header-relative tables.
    b[128:320]=original[64:256]
    struct.pack_into('<I',b,0x3c,128)
    def word(off,value):struct.pack_into('<H',b,128+off,value)
    for off,val in ((0xc,0x0202),(0xe,3),(0x10,5120),(0x12,5120),
                    (0x14,0x104),(0x16,1),(0x1a,3),(0x1c,3),
                    (0x24,88),(0x26,88),(0x28,96),(0x2a,100)):
        word(off,val)
    b[216:224]=b'\x05PROBE\0\0'
    word(96,0);word(98,9)
    b[228:246]=b'\x08DOSCALLS\x08VIOCALLS'
    for i,desc in enumerate(((0x15,3559,0x0d70,3560),
                             (0x100,42,0x0d70,44),(0x104,489,0x0d09,2416))):
        struct.pack_into('<4H',b,192+8*i,*desc)
    n=struct.unpack_from('<H',original,130)[0]
    b[0x150:0x150+3559]=bytes([0x90])*3559
    b[0x254:0x254+n]=original[256:256+n]
    rel=bytearray(original[256+n:256+n+34])
    for i in range(4):
        off=4+i*8
        struct.pack_into('<H',rel,off,struct.unpack_from('<H',rel,off)[0]+0x104)
    # Reproduce a non-additive startup-import chain headed at 1:0650.
    mode_src=struct.unpack_from('<H',rel,4+3*8)[0]
    struct.pack_into('<H',rel,4+3*8,0x650)
    struct.pack_into('<HH',b,0x150+0x650,mode_src,0xffff)
    b[0x150+3559:0x150+3559+34]=rel
    b[0x1000:0x1000+42]=bytes([0x90])*42
    struct.pack_into('<H',b,0x1000+42,0)
    data=bytearray((i*7+3)%256 for i in range(480))
    data[:26]=b'O16 repository VIO probe\r\n'
    encoded=struct.pack('<HH',1,480)+data+struct.pack('<HHB',1936,1,255)
    assert len(encoded)==489
    b[0x1040:0x1040+489]=encoded
    # Source offset 2048 is beyond ENCODED size but inside expanded bytes.
    struct.pack_into('<HBBHHH',b,0x1040+489,1,3,0,2048,1,0x104)
    return b,bytes(data)+bytes([255])*1936

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
        layout, expanded = ibm_layout_fixture(image)
        check(layout, 0)
        # A literal crossing scratch chunks, repeated more than once.
        large=bytearray(layout)
        payload=bytes((i*11+7)%256 for i in range(1500))
        encoded=struct.pack('<HH',2,1500)+payload
        large[0x1040:]=encoded+bytes(2) # zero relocation count at encoded end
        struct.pack_into('<H',large,128+64+16+2,len(encoded))
        struct.pack_into('<H',large,128+64+16+6,3000)
        check(large,0)
        (d/'large.exe').write_bytes(large)
        # The same file remains OS/2-only in the normal W16 build.
        for encoded,expected in ((b'\x01\0\x01',11),
                                 (struct.pack('<HH',0,1)+b'x',11),
                                 (struct.pack('<HH',1,0),11),
                                 (struct.pack('<HH',1,2)+b'x',11),
                                 (struct.pack('<HHB',65535,65535,1),11),
                                 (struct.pack('<HH',2417,1)+b'x',14),
                                 (struct.pack('<HH',65535,2)+b'xy',14)):
            bad=bytearray(layout)
            struct.pack_into('<H',bad,128+64+16+2,len(encoded))
            bad[0x1040:0x1040+len(encoded)]=encoded
            check(bad,expected)
        bad=bytearray(layout)
        struct.pack_into('<H',bad,128+64+16+4,0x0d08) # iterated CODE unsupported
        check(bad,12)
        bad=bytearray(layout)
        struct.pack_into('<H',bad,0x1040+489+4,2414) # four-byte fixup crosses extent
        check(bad,15)
        # A truncated encoded extent/relocation table must never read padding.
        check(layout[:0x1040+488],10)
        # C harness below inspects all expanded bytes and the relocated pointer.
        (d/'layout.exe').write_bytes(layout)
        (d/'expanded.bin').write_bytes(expanded)
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
    extern int host_fail_after;
    assert(api_find_ord(0,92)==2 && api_find_ord(0,49)==3);
    assert(api_find_name(0,"DosGetVersion")==2);
    assert(api_find_name(0,"DosGetMachineMode")==3);
    assert(api_find_name(0,"DosGetEnv")==-1 && api_find_name(0,"DosGetInfoSeg")==-1);
    assert(!seg_alloc(4,&s)); memset(&seg,0,sizeof(seg));
    seg.sel=s;seg.paras=4;seg.flags=1;image.seg=&seg;image.nseg=1;
    assert(!pipe(fds));console_fd=fds[1];
    wr16(s,0,1);wr16(s,2,2);wr16(s,4,62);wr16(s,6,s);
    assert(o16_dispatch(0,s,0)==436);
    wr16(s,0,0);wr8(s,62,'O');wr8(s,63,'K');
    assert(o16_dispatch(0,s,0)==0);
    assert(read(fds[0],b,2)==2 && b[0]=='O' && b[1]=='K');
    wr16(s,2,3);assert(o16_dispatch(0,s,0)==487);
    wr16(s,2,2);wr16(s,6,s+1);assert(o16_dispatch(0,s,0)==487);
    wr16(s,2,0);assert(o16_dispatch(0,s,0)==0);
    wr16(s,0,62);wr16(s,2,s);assert(!o16_dispatch(2,s,0));
    assert(rd16(s,62)==0x0a1e);
    wr16(s,0,63);wr8(s,62,123);assert(!o16_dispatch(3,s,0));
    assert(rd8(s,63)==0 && rd8(s,62)==123);
    assert(o16_dispatch(2,s,0)==487); /* word crosses allocation boundary */
    wr16(s,0,65535);assert(o16_dispatch(3,s,0)==487);
    wr16(s,0,62);seg.flags=0;assert(o16_dispatch(2,s,0)==487);seg.flags=1;
    wr16(s,2,s+1);assert(o16_dispatch(3,s,0)==487);
    wr16(s,0,37);wr16(s,2,1);o16_dispatch(1,s,0);
    assert(o16_done && o16_code==37);
    wr16(s,2,0);o16_dispatch(1,s,0);assert(o16_code==12);
    o16_done=0;o16_dispatch(0,s,60);assert(o16_done && o16_code==12);
    seg_free(s);close(fds[0]);close(fds[1]);assert(!host_live);
    {
        FILE *f;unsigned char expected[2416];u16 i,ds;
        assert(!ne_open(&image,LAYOUT));assert(!ne_check_imports(&image));
        assert(!ne_load(&image));ds=image.dgroup;
        assert(image.seg[2].size==489 && image.seg[2].expanded==2416);
        f=fopen(EXPANDED,"rb");assert(f);assert(fread(expected,1,2416,f)==2416);fclose(f);
        for(i=0;i<2416;i++)if(i<2048 || i>=2052)assert(rd8(ds,i)==expected[i]);
        assert(rd16(ds,2048)==0x104 && rd16(ds,2050)==image.seg[0].sel);
        assert(rd16(image.seg[0].sel,0x650)==48);
        assert(rd16(image.seg[0].sel,0x652)==0xf000);
        assert(rd8(ds,2416)==0);ne_free(&image);assert(!host_live);
        assert(!ne_open(&image,LARGE));assert(!ne_load(&image));ds=image.dgroup;
        for(i=0;i<3000;i++)assert(rd8(ds,i)==(unsigned char)(((i%1500)*11+7)%256));
        ne_free(&image);assert(!host_live);
        host_fail_after=1;assert(!ne_open(&image,LAYOUT));
        assert(ne_load(&image)==14);ne_free(&image);assert(!host_live);
    }
    return 0;
}
''')
        run(['gcc', '-DLAYOUT=\"'+str(d/'layout.exe')+'\"',
             '-DEXPANDED=\"'+str(d/'expanded.bin')+'\"',
             '-DLARGE=\"'+str(d/'large.exe')+'\"','-DW16_HOST','-DW16_OS2_PROBE','-g','-fsanitize=address,undefined',
             '-fno-pie','-no-pie','-I'+str(ROOT/'src'),str(harness),
             str(ROOT/'src/ne.c'),str(ROOT/'src/mem.c'),'-o',str(d/'calls')])
        run([str(d/'calls')],env=ENV)
        print('OS/2 iterated bytes/chunks, relocation chains/extents, startup queries, failure cleanup and strict imports passed')
