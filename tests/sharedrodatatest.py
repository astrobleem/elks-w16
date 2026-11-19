#!/usr/bin/env python3
"""Focused optional native shared-constant relocation and logging checks."""
import struct as s
from pathlib import Path
p=Path('src/w16-ro.elf');b=p.read_bytes();off=s.unpack_from('<I',b,32)[0];esz,n,st=s.unpack_from('<HHH',b,46);hs=[s.unpack_from('<10I',b,off+i*esz) for i in range(n)];t=hs[st];names=b[t[4]:t[4]+t[5]]
name=lambda h:names[h[0]:names.index(0,h[0])].decode()
sections={name(h):h for h in hs}
keys=['.text','.fartext','.bitmap','.data','.sharedconst'];groups=[sections[k] for k in keys];bases=[h[3] for h in groups];original=[bytearray(b[h[4]:h[4]+h[5]]) for h in groups]
r=sections['.rel.dyn'];expected={}
for off in range(r[4],r[4]+r[5],8):
 address,info=s.unpack_from('<II',b,off);assert info==48
 i=next(i for i,h in enumerate(groups) if h[3]<=address< h[3]+h[5]-1);at=address-bases[i];target=s.unpack_from('<H',original[i],at)[0]*16
 expected[i,at]=bases.index(target)
assert not any(target==3 for target in expected.values()),'absolute DATA selector would break fork sharing'
for sym in hs:
 if sym[1]!=2:continue
 strings=hs[sym[6]];pool=b[strings[4]:strings[4]+strings[5]]
 for pos in range(sym[4],sym[4]+sym[5],16):
  nm,value,size,info,other,index=s.unpack_from('<IIIBBH',b,pos)
  if pool[nm:pool.index(0,nm)]==b'w16_shared_safe':safe_offset=value-bases[3]
image=Path('src/w16-ro.ne').read_bytes();ne=s.unpack_from('<I',image,60)[0];assert s.unpack_from('<H',image,ne+28)[0]==5
st=s.unpack_from('<H',image,ne+34)[0];shift=s.unpack_from('<H',image,ne+50)[0];seen={}
for i,h in enumerate(groups):
 sector,n,flags,minimum=s.unpack_from('<4H',image,ne+st+8*i);assert n==h[5] and minimum==h[5]+(sections['.bss'][5] if i==3 else 0)
 assert bool(flags&1)==(i==3)
 offset=sector<<shift;actual=bytearray(image[offset:offset+n]);want=original[i][:]
 if i==3:s.pack_into('<H',want,safe_offset,1)
 if flags&0x100:
  count=s.unpack_from('<H',image,offset+n)[0]
  for j in range(count):
   kind,rf,at,target,reserved,to=s.unpack_from('<BBHBBH',image,offset+n+2+j*8)
   assert (kind,rf,reserved,to)==(2,0,0,0) and s.unpack_from('<H',actual,at)[0]==65535
   assert (i,at) not in seen;seen[i,at]=target-1
   # Apply real loader selector fixup and independently check its ELF target.
   selector=[0x1111,0x2222,0x3333,0x4444,0x5555][target-1]
   s.pack_into('<H',actual,at,selector);s.pack_into('<H',want,at,selector)
 assert actual==want,keys[i]+' extent/content mismatch'
assert seen==expected
print('PASS: five segments,',len(seen),'ELF/NE fixups preserved;',sum(i==4 for i,at in seen),'shared-bank pointers; no absolute DATA selector; segment contents agree')

# Exercise the exact formatting adapter with ordinary address-space pointers
# on the host. Native far addressing/selector preservation is checked above;
# this check covers varargs, near %s arguments and existing output truncation.
import os, re, subprocess, tempfile
mem=Path('src/mem.c').read_text()
def function(name):
 return re.search(r'void '+name+r'\(.*?\n}\n',mem,re.S)[0]
code = r"""
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define __far
static char logbuf[256], capture[2][256];
static unsigned emitted;
static void emit(void) { assert(emitted<2); strcpy(capture[emitted++],logbuf); }
""" + function('wlog') + function('wlog_far') + r"""
static void equal(void) { assert(emitted==2); assert(!strcmp(capture[0],capture[1])); emitted=0; }
int main(void) {
 char large[400], format[256];
 wlog("str=%s i=%d u=%u x=%04x l=%lu %%\n", "near argument", -17, 23u, 0x12u, 123456ul);
 wlog_far("str=%s i=%d u=%u x=%04x l=%lu %%\n", "near argument", -17, 23u, 0x12u, 123456ul); equal();
 memset(large,'a',399); large[399]=0;
 wlog("%s",large); wlog_far("%s",large); assert(strlen(capture[0])==255); equal();
 memset(format,'z',255); format[255]=0;
 wlog(format); wlog_far(format); assert(strlen(capture[0])==255); equal();
 wlog(""); wlog_far(""); equal();
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
 source=Path(tmp)/'logging.c';binary=Path(tmp)/'logging';source.write_text(code)
 subprocess.run(['cc','-fsanitize=address,undefined','-fno-pie','-no-pie',str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True,env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0"))
print('PASS: far-format adapter preserves varargs, near strings, empty/maximal formats and existing 255-byte output limit (host check)')
