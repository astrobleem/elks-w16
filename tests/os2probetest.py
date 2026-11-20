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

def fixture(folder, arena=False, deny=False):
    obj, raw = folder/'probe.o', folder/'probe.bin'
    run(['as', '--32']+(['--defsym','ARENA_PROBE=1'] if arena else [])+
        (['--defsym','DENY_SETVEC_PROBE=1'] if deny else [])+
        ['-o', str(obj), str(ROOT/'test/os2vio/probe.S')])
    run(['objcopy', '-O', 'binary', '-j', '.text', str(obj), str(raw)])
    symbols = {}
    for line in subprocess.check_output(['nm', str(obj)], text=True).splitlines():
        address, _, name = line.split()
        symbols[name] = int(address, 16)
    code = raw.read_bytes()
    data = b'O16 repository VIO probe\r\n'
    assert len(data) == 26
    # Header-relative offsets; paragraph alignment, separate code/data.
    b = bytearray(2048)
    def word(off, value): struct.pack_into('<H', b, off, value)
    b[:2] = b'MZ'; word(0x18, 64); word(0x3c, 64)
    b[64:66] = b'NE'
    for off, val in ((0xc, 0x0102), (0xe, 2), (0x12, 2048), (0x16, 1),
                     (0x1a, 2), (0x1c, 2), (0x1e, 3), (0x22, 64),
                     (0x24, 80), (0x26, 80), (0x28, 88), (0x2a, 94), (0x32, 4)):
        word(64+off, val)
    b[64+0x36] = 1
    b[144:152] = b'\x05PROBE\0\0'
    word(152, 0); word(154, 9);word(156,18)
    b[158:180] = b'\x08DOSCALLS\x08VIOCALLS\x03NLS'
    struct.pack_into('<4H', b, 128, 16, len(code), 0x100, len(code))
    struct.pack_into('<4H', b, 136, 64, len(data), 1, len(data))
    b[256:256+len(code)] = code
    pos = 256+len(code)
    records=(('vio_operand', 2, 19), ('exit_operand', 1, 5),
                                  ('version_operand', 1, 92), ('mode_operand', 1, 49),
                                  ('alloc_operand',1,34),('free_operand',1,39),
                                  ('write_operand',1,138),('close_operand',1,59))
    if arena:records+=(('dbcs_operand',3,4),('dsgrow_operand',1,38),
                       ('avail_operand',1,127),('realloc_operand',1,38),('regrow_operand',1,38))
    if deny:records+=(('setvec_operand',1,89),)
    word(pos,len(records));pos+=2
    for operand,mod,ordinal in records:
        struct.pack_into('<BBHHH', b, pos, 3, 1, symbols[operand], mod, ordinal)
        pos += 8
    assert pos < 1024
    b[1024:1024+len(data)] = data
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
                    (0x24,88),(0x26,88),(0x28,96),(0x2a,102)):
        word(off,val)
    b[216:224]=b'\x05PROBE\0\0'
    word(96,0);word(98,9);word(100,18)
    b[230:252]=b'\x08DOSCALLS\x08VIOCALLS\x03NLS'
    for i,desc in enumerate(((0x15,3559,0x0d70,3560),
                             (0x100,42,0x0d70,44),(0x104,489,0x0d09,2416))):
        struct.pack_into('<4H',b,192+8*i,*desc)
    n=struct.unpack_from('<H',original,130)[0]
    b[0x150:0x150+3559]=bytes([0x90])*3559
    b[0x254:0x254+n]=original[256:256+n]
    count=struct.unpack_from('<H',original,256+n)[0]
    rel=bytearray(original[256+n:256+n+2+8*count])
    for i in range(count):
        off=4+i*8
        struct.pack_into('<H',rel,off,struct.unpack_from('<H',rel,off)[0]+0x104)
    # Reproduce a non-additive startup-import chain headed at 1:0650.
    mode_src=struct.unpack_from('<H',rel,4+3*8)[0]
    struct.pack_into('<H',rel,4+3*8,0x650)
    struct.pack_into('<HH',b,0x150+0x650,mode_src,0xffff)
    b[0x150+3559:0x150+3559+len(rel)]=rel
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

def import_inventory_fixture(original, inventory):
    """Owned pointer records representing the Windows-local import inventory."""
    b=bytearray(original);ne=128
    struct.pack_into('<H',b,ne+0x1e,len(inventory))
    struct.pack_into('<H',b,ne+0x2a,106)
    nameoff=0;records=[]
    for mod,(name,ordinals) in enumerate(inventory.items(),1):
        struct.pack_into('<H',b,ne+96+(mod-1)*2,nameoff)
        value=bytes([len(name)])+name.encode()
        b[ne+106+nameoff:ne+106+nameoff+len(value)]=value;nameoff+=len(value)
        for ordinal in ordinals:
            src=0x700+8*len(records)
            struct.pack_into('<HH',b,0x150+src,0xffff,0xffff)
            records.append(struct.pack('<BBHHH',3,1,src,mod,ordinal))
    struct.pack_into('<H',b,0x150+3559,len(records))
    value=b''.join(records)
    assert 0x150+3559+2+len(value)<0x1000
    b[0x150+3559+2:0x150+3559+2+len(value)]=value
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
        arena_image=fixture(d,arena=True);exe.write_bytes(arena_image)
        p=subprocess.run([str(ROOT/'src/o16-host'),'-a','128','-n',str(exe)],
                         env=ENV,text=True,capture_output=True)
        assert p.returncode==0 and 'live allocations=0' in p.stderr,p.stderr
        denied_image=fixture(d,arena=True,deny=True);exe.write_bytes(denied_image)
        p=subprocess.run([str(ROOT/'src/o16-host'),'-a','128','--deny-setvec','-n',str(exe)],
                         env=ENV,text=True,capture_output=True)
        assert p.returncode==0 and 'live allocations=0' in p.stderr,p.stderr
        layout, expanded = ibm_layout_fixture(image)
        check(layout, 0)
        ansi={'DOSCALLS':[5,34,38,39,49,59,77,89,92,137,138],
              'VIOCALLS':[3,5,19], 'KBDCALLS':[4,10,11,13,22],
              'MSG':[1,2], 'NLS':[1,4]}
        checkram={'DOSCALLS':[5,34,38,39,49,77,89,92,127,138], 'NLS':[4]}
        for inventory,missing in ((ansi,11),(checkram,3)):
            exe.write_bytes(import_inventory_fixture(layout,inventory))
            p=subprocess.run([str(ROOT/'src/o16-host'),'-n',str(exe)],
                             env=ENV,text=True,capture_output=True)
            assert p.returncode==13 and p.stderr.count('unsupported import ')==missing,p.stderr
            assert 'live allocations=0' in p.stderr,p.stderr
            p=subprocess.run([str(ROOT/'src/o16-host'),'-a','128','-n',str(exe)],
                             env=ENV,text=True,capture_output=True)
            assert p.returncode==13 and p.stderr.count('unsupported import ')==missing-(1 if inventory is ansi else 2),p.stderr
            assert 'live allocations=0' in p.stderr,p.stderr
            p=subprocess.run([str(ROOT/'src/o16-host'),'-a','128','--deny-setvec','-n',str(exe)],
                             env=ENV,text=True,capture_output=True)
            if inventory is checkram:assert p.returncode==0 and 'live allocations=0' in p.stderr,p.stderr
            else:assert p.returncode==13 and p.stderr.count('unsupported import ')==9,p.stderr
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
    {
        FILE *f=tmpfile();u16 allocated,i;int pp[2];char payload[1024];
        assert(f);wr16(s,0,0);wr16(s,2,20);wr16(s,4,s);wr16(s,6,4096);
        assert(!o16_dispatch(4,s,0));allocated=rd16(s,20);assert(owned);
        assert(span(allocated,4095,1) && !span(allocated,4095,2));
        for(i=0;i<1537;i++)wr8(allocated,i,(unsigned char)(i*7+3));
        streams[1]=dup(fileno(f));assert(streams[1]>=0);
        wr16(s,0,22);wr16(s,2,s);wr16(s,4,1537);wr16(s,6,0);
        wr16(s,8,allocated);wr16(s,10,1);
        assert(!o16_dispatch(9,s,0) && rd16(s,22)==1537 && o16_pop==12);
        assert(lseek(fileno(f),0,SEEK_END)==1537);
        wr16(s,0,26);wr16(s,2,s);wr16(s,4,24);wr16(s,6,s);wr16(s,8,1);
        assert(!o16_dispatch(7,s,0) && rd16(s,24)==0 && !rd16(s,26));
        assert(o16_pop==10);
        streams[0]=dup(fileno(f));assert(streams[0]>=0);assert(lseek(streams[0],0,SEEK_SET)==0);
        wr16(s,0,22);wr16(s,2,s);wr16(s,4,1537);wr16(s,6,2048);
        wr16(s,8,allocated);wr16(s,10,0);
        assert(!o16_dispatch(8,s,0) && rd16(s,22)==1537);
        for(i=0;i<1537;i++)assert(rd8(allocated,i+2048)==(unsigned char)(i*7+3));
        assert(!o16_dispatch(8,s,0) && rd16(s,22)==0); /* EOF */
        assert(!pipe(pp));close(streams[0]);streams[0]=dup(pp[0]);
        memset(payload,'x',sizeof(payload));assert(write(pp[1],payload,sizeof(payload))==1024);
        wr16(s,4,2048);wr16(s,6,0);
        alarm(2);assert(!o16_dispatch(8,s,0));alarm(0); /* no second blocking pipe read */
        assert(rd16(s,22)==1024);
        wr16(s,0,26);wr16(s,2,s);wr16(s,4,24);wr16(s,6,s);wr16(s,8,0);
        assert(!o16_dispatch(7,s,0) && rd16(s,24)==2 && !rd16(s,26));
        wr16(s,0,22);wr16(s,2,s);wr16(s,4,1);wr16(s,6,0);
        wr16(s,8,allocated+1);wr16(s,10,0);wr16(s,22,123);
        assert(o16_dispatch(8,s,0)==487 && rd16(s,22)==123);
        streams[2]=dup(2);wr16(s,0,2);assert(!o16_dispatch(6,s,0));
        assert(fcntl(2,F_GETFD)>=0 && o16_dispatch(6,s,0)==6 && o16_pop==2);
        wr16(s,0,s);assert(o16_dispatch(5,s,0)==5); /* cannot free image */
        wr16(s,0,allocated);assert(!o16_dispatch(5,s,0));
        assert(!owned && !span(allocated,0,1) && o16_dispatch(5,s,0)==5);
        wr16(s,0,1);wr16(s,2,20);wr16(s,4,s);wr16(s,6,16);
        assert(o16_dispatch(4,s,0)==50 && !owned);
        wr16(s,0,0);host_fail_after=0;
        assert(o16_dispatch(4,s,0)==8 && !owned);host_fail_after=-1;
        wr16(s,6,0);assert(!o16_dispatch(4,s,0));allocated=rd16(s,20);
        assert(span(allocated,65535,1) && !span(allocated,65535,2));
        assert(rd8(allocated,65535)==0);owned_shutdown();assert(!owned);
        streams_shutdown();close(pp[0]);close(pp[1]);fclose(f);
        streams[1]=open(LARGE,O_RDONLY);assert(streams[1]>=0);
        wr16(s,0,22);wr16(s,2,s);wr16(s,4,1);wr16(s,6,32);
        wr16(s,8,s);wr16(s,10,1);
        assert(o16_dispatch(9,s,0)==5);streams_shutdown();
        assert(!pipe(pp));close(pp[0]);streams[1]=pp[1];
        wr16(s,0,22);wr16(s,2,s);wr16(s,4,1);wr16(s,6,32);
        wr16(s,8,s);wr16(s,10,1);
        assert(o16_dispatch(9,s,0)==109 && rd16(s,22)==0);
        assert(signal(SIGPIPE,SIG_DFL)==SIG_DFL);streams_shutdown();
        /* The normal initializer duplicates handles instead of borrowing them. */
        streams_init();assert(streams[1]!=1 && streams[2]!=2);streams_shutdown();
    }
    {
        struct ne_seg both[2];u16 code;
        assert(api_find_ord(0,89)==-1 && api_find_name(0,"DosSetVec")==-1);
        deny_setvec=1;assert(api_find_ord(0,89)==13);
        assert(!seg_alloc(2,&code));memset(both,0,sizeof(both));both[0]=seg;
        both[1].sel=code;both[1].paras=2;both[1].size=16;both[1].fileoff=1;
        image.seg=both;image.nseg=2;
        wr32(s,40,0x24681357ul);wr16(s,0,40);wr16(s,2,s);
        wr16(s,4,0);wr16(s,6,code);wr16(s,8,0);
        assert(o16_dispatch(13,s,0)==1 && o16_pop==10 && rd32(s,40)==0x24681357ul);
        assert(o16_dispatch(13,s,0)==1 && rd32(s,40)==0x24681357ul);
        wr16(s,4,16);assert(o16_dispatch(13,s,0)==487);
        wr16(s,4,0);wr16(s,6,s);assert(o16_dispatch(13,s,0)==487);
        wr16(s,6,0);assert(o16_dispatch(13,s,0)==1); /* removal is also denied */
        wr16(s,0,62);assert(o16_dispatch(13,s,0)==487);
        assert(rd32(s,40)==0x24681357ul);seg_free(code);image.seg=&seg;image.nseg=1;
        deny_setvec=0;assert(api_find_ord(0,89)==-1);
    }
    {
        FILE *f=tmpfile();u16 text,i;char out[256];int n;
        assert(f && !seg_alloc(32,&text));
        /* Register a readable owned region without borrowing the native heap. */
        struct owned_segment record;record.sel=text;record.paras=32;record.next=owned;owned=&record;
        streams[1]=dup(fileno(f));
        for(i=0;i<75;i++)wr8(text,i,'a');
        wr8(text,75,' ');for(i=76;i<86;i++)wr8(text,i,'b');
        wr16(s,0,0);wr16(s,2,text);wr16(s,4,86);wr16(s,6,1);
        assert(!o16_dispatch(14,s,0) && o16_pop==8);
        assert(lseek(fileno(f),0,SEEK_SET)==0);n=read(fileno(f),out,sizeof(out));assert(n==87);
        for(i=0;i<75;i++)assert(out[i]=='a');assert(out[75]=='\r' && out[76]=='\n');
        for(i=77;i<87;i++)assert(out[i]=='b');
        /* Explicit line breaks and whitespace remain exact. */
        assert(!ftruncate(fileno(f),0));assert(lseek(fileno(f),0,SEEK_SET)==0);
        fcopy_out(text,0,"one two\r\nthree ",15);wr16(s,4,15);
        assert(!o16_dispatch(14,s,0));assert(lseek(fileno(f),0,SEEK_SET)==0);
        assert(read(fileno(f),out,sizeof(out))==15 && !memcmp(out,"one two\r\nthree ",15));
        /* Reject unsupported forms before producing even a partial prefix. */
        assert(lseek(fileno(f),0,SEEK_END)==15);
        fcopy_out(text,0,"valid prefix ",13);for(i=13;i<94;i++)wr8(text,i,'x');wr16(s,4,94);
        assert(o16_dispatch(14,s,0)==321 && lseek(fileno(f),0,SEEK_END)==15);
        fcopy_out(text,0,"tab\there",8);wr16(s,4,8);assert(o16_dispatch(14,s,0)==321);
        wr16(s,0,510);assert(o16_dispatch(14,s,0)==487);
        wr16(s,0,0);wr16(s,4,0);assert(!o16_dispatch(14,s,0));
        wr16(s,6,7);assert(o16_dispatch(14,s,0)==6);
        streams_shutdown();fclose(f);owned=record.next;seg_free(text);
    }
    /* Exact SBCS vector, all tail bytes cleared, input and guards preserved. */
    wr16(s,30,0);wr16(s,32,0);fzero(s,40,12);wr8(s,39,123);wr8(s,50,123);
    wr16(s,0,40);wr16(s,2,s);wr16(s,4,30);wr16(s,6,s);wr16(s,8,10);
    assert(!o16_dispatch(10,s,0) && o16_pop==10);
    for(signed int j=40;j<50;j++)assert(!rd8(s,j));
    assert(rd8(s,39)==123 && rd8(s,50)==123 && !rd16(s,30) && !rd16(s,32));
    wr16(s,30,1);wr16(s,32,437);assert(!o16_dispatch(10,s,0));
    wr16(s,32,850);assert(!o16_dispatch(10,s,0));
    wr16(s,32,932);wr8(s,40,123);assert(o16_dispatch(10,s,0)==398 && rd8(s,40)==123);
    wr16(s,32,437);wr16(s,30,81);assert(o16_dispatch(10,s,0)==398);
    wr16(s,30,0);wr16(s,8,1);assert(o16_dispatch(10,s,0)==399 && !rd8(s,40));
    wr16(s,8,0);assert(o16_dispatch(10,s,0)==399);
    wr16(s,8,10);wr16(s,0,60);assert(o16_dispatch(10,s,0)==487);
    wr16(s,0,40);wr16(s,6,s+1);assert(o16_dispatch(10,s,0)==487);
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
    {
        u16 data,a,b,c,i;u32 largest;struct ne_seg local;
        host_fail_after=-1;assert(!arena_init(0x2000)); /* 128 KiB, one real backing allocation */
        assert(!seg_alloc(4,&data));memset(&local,0,sizeof(local));
        local.sel=data;local.paras=4;local.minalloc=16;local.flags=1;
        image.seg=&local;image.nseg=1;image.autodata=1;image.init_sp=32;
        assert(api_find_ord(0,38)==11 && api_find_ord(0,127)==12);
        wr16(data,0,20);wr16(data,2,data);assert(!o16_dispatch(12,data,0));
        largest=rd32(data,20);assert(largest==131072-64);
        /* CRT DS growth is stable, preserves data, and protects live SS on shrink. */
        wr8(data,40,123);wr16(data,0,data);wr16(data,2,128);
        assert(!o16_dispatch(11,data,0) && local.sel==data && local.paras==8);
        assert(rd8(data,40)==123 && !rd8(data,127));
        wr16(data,2,64);assert(o16_dispatch(11,data,0)==5 && local.paras==8);
        /* Allocate exactly the clamped query result, including 65535 -> 64 KiB. */
        wr16(data,0,0);wr16(data,2,20);wr16(data,4,data);wr16(data,6,65535);
        assert(!o16_dispatch(4,data,0));a=rd16(data,20);
        assert(span(a,65535,1));wr8(a,0,42);wr8(a,1023,77);
        wr16(data,0,0);wr16(data,2,20);wr16(data,4,data);wr16(data,6,1024);
        assert(!o16_dispatch(4,data,0));b=rd16(data,20);
        /* A blocked growth must preserve the original base, bounds and content. */
        wr16(data,0,a);wr16(data,2,0);assert(!o16_dispatch(11,data,0));
        wr16(data,2,2048);assert(!o16_dispatch(11,data,0)); /* shrink returns a gap */
        wr16(data,2,4096);assert(!o16_dispatch(11,data,0)); /* grow within that gap */
        assert(rd8(a,0)==42 && rd8(a,1023)==77);
        wr16(data,0,0);wr16(data,2,20);wr16(data,4,data);wr16(data,6,1024);
        assert(!o16_dispatch(4,data,0));c=rd16(data,20);assert(c==a+256);
        wr16(data,0,a);wr16(data,2,8192);assert(o16_dispatch(11,data,0)==8);
        assert(span(a,4095,1) && !span(a,4095,2) && rd8(a,0)==42);
        wr16(data,0,c);assert(!o16_dispatch(5,data,0));
        wr16(data,0,b);assert(!o16_dispatch(5,data,0));
        wr16(data,0,20);wr16(data,2,data);assert(!o16_dispatch(12,data,0));
        largest=rd32(data,20);assert(largest==131072-128-4096);
        /* No sum-of-holes result: fill all but a small tail, free separated blocks. */
        wr16(data,0,0);wr16(data,2,20);wr16(data,4,data);wr16(data,6,0);
        assert(!o16_dispatch(4,data,0));b=rd16(data,20);
        assert(arena_available()<65536 && arena_available()>0);
        for(i=0;i<4096;i++)assert(rd8(a,i)==(i==0?42:i==1023?77:0));
        owned_shutdown();assert(!owned);assert(arena_available()==131072-128);
        /* Separated holes: sum is 65408, largest is 32768, and exact allocation fits. */
        assert(!alloc_segment(32768,data,20,0));a=rd16(data,20);
        assert(!alloc_segment(32768,data,20,0));b=rd16(data,20);
        assert(!alloc_segment(32768,data,20,0));c=rd16(data,20);
        assert(!free_segment(b));assert(arena_available()==32768);
        assert(alloc_segment(32784,data,20,0)==8 && arena_available()==32768);
        assert(!alloc_segment(32768,data,20,0) && rd16(data,20)==b);
        owned_shutdown();assert(!owned);assert(arena_available()==131072-128);
        seg_free(data);arena_shutdown();image.seg=0;image.nseg=0;assert(!host_live);
        assert(api_find_ord(0,38)==-1 && api_find_ord(0,127)==-1);
        /* Real loader segments, expanded relocations and failure unwind in the arena. */
        assert(!arena_init(0x2000));assert(!ne_open(&image,LAYOUT));
        assert(!ne_load(&image));assert(rd16(image.dgroup,2048)==0x104);
        ne_free(&image);assert(!arena_blocks);arena_shutdown();assert(!host_live);
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
        print('OS/2 strict inventories (ANSI 11/10, CHECKRAM 3/1/0 missing (direct/arena/explicit denial)), iterated data, bounded APIs, streams and segment lifecycle passed')
