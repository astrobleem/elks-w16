#!/usr/bin/env python3
"""Convert the optional linked W16 ELF layout to fixed native ELKS OS/2 NE."""
import struct as s,sys
from pathlib import Path
def need(ok,msg):
 if not ok:raise SystemExit(msg)
b=Path(sys.argv[1]).read_bytes();need(b[:7]==b'\x7fELF\x01\x01\x01','not ELF32 little endian')
need(s.unpack_from('<HH',b,16)==(2,3),'expected linked i386 ELF')
entry=s.unpack_from('<I',b,24)[0];off=s.unpack_from('<I',b,32)[0];esz,n,strings=s.unpack_from('<HHH',b,46);need(esz==40,'bad section table')
sections=[s.unpack_from('<10I',b,off+i*40) for i in range(n)];strh=sections[strings];names=b[strh[4]:strh[4]+strh[5]]
def name(h):return names[h[0]:names.index(0,h[0])].decode()
def contents(h):
 need(h[4]+h[5]<=len(b),'section exceeds ELF file')
 return bytes(h[5]) if h[1]==8 else b[h[4]:h[4]+h[5]]
sec={name(h):h for h in sections};required={'.text','.fartext','.bitmap','.data','.bss','.rel.dyn'};allowed=required|{'.sharedconst'}
for h in sections:
 need(not(h[2]&2 and h[5]) or name(h) in allowed,'unhandled allocated section '+name(h))
for nm in required:need(sum(name(h)==nm for h in sections)==1,'missing or duplicate '+nm)
need(sum(name(h)=='.sharedconst' for h in sections)<=1,'duplicate shared constant section')
for h in sections:need(h[1] not in (4,9) or not h[5] or name(h)=='.rel.dyn','unhandled relocation section '+name(h))
code=[sec[x] for x in ['.text','.fartext','.bitmap']];d=sec['.data'];z=sec['.bss']
need(d[3]+d[5]==z[3],'non-contiguous near data/BSS')
need(not any(contents(z)),'nonzero BSS requires explicit handling')
ro=sec.get('.sharedconst');groups=code+[d]+([ro] if ro and ro[5] else [])
need(len(groups)<=5,'ELKS native segment limit exceeded')
if ro:need(not(ro[2]&1),'shared constants must not be writable')
blobs=[bytearray(contents(h)) for h in groups];bases=[h[3] for h in groups]
minimum=[len(x) for x in blobs];minimum[3]+=z[5];heap=0x2800;stack=0x1800
for i,m in enumerate(minimum):need(0<m<=0xfff0 and bases[i]%16==0,'invalid segment extent')
for i in range(len(groups)):
 for j in range(i):need(bases[i]+minimum[i]<=bases[j] or bases[j]+minimum[j]<=bases[i],'overlapping segment extents')
need(minimum[3]+heap+stack+1024<=0xfff0,'near data/heap/stack/argv budget exceeded')
need(code[0][3]<=entry<code[0][3]+code[0][5],'entry outside first code group')
data_segment_references=0
fix=[[] for _ in blobs];seen=set();r=sec['.rel.dyn'];need(r[1]==9 and r[9]==8 and r[5]%8==0,'bad relocation section')
for at in range(r[4],r[4]+r[5],8):
 address,info=s.unpack_from('<II',b,at);need(info==48,'unsupported dynamic relocation '+str(info))
 places=[i for i,base in enumerate(bases) if base<=address and address+2<=base+len(blobs[i])]
 need(len(places)==1 and address not in seen,'stray or duplicate relocation');seen.add(address)
 i=places[0];place=address-bases[i];target=s.unpack_from('<H',blobs[i],place)[0]*16
 need(target in bases,'segment relocation has unknown target base')
 if target==bases[3]:data_segment_references+=1
 fix[i].append(s.pack('<BBHBBH',2,0,place,bases.index(target)+1,0,0))
 s.pack_into('<H',blobs[i],place,0xffff)
# Sharing native text is enabled only when no absolute DS selector occurs.
# Keep every normal relocation above; incompatible images still run privately.
for sym in sections:
 if sym[1]!=2:continue
 strings=sections[sym[6]];pool=contents(strings)
 need(sym[9]==16 and sym[5]%16==0,'bad symbol table')
 for pos in range(sym[4],sym[4]+sym[5],16):
  nm,value,size,info,other,index=s.unpack_from('<IIIBBH',b,pos)
  need(nm<len(pool),'bad symbol name')
  if pool[nm:pool.index(0,nm)]!=b'w16_shared_safe':continue
  need(size==2 and d[3]<=value and value+2<=d[3]+d[5],'shared capability outside initialized data')
  s.pack_into('<H',blobs[3],value-d[3],int(data_segment_references==0))
image=bytearray(256);image[:2]=b'MZ';s.pack_into('<H',image,8,4);s.pack_into('<H',image,16,256);s.pack_into('<H',image,24,64);s.pack_into('<I',image,60,128);image[64:69]=b'\xb8\x01\x4c\xcd\x21'
count=len(groups);resource=64+8*count;resident=resource+4;imports=resident+10
h=bytearray(64);h[:4]=b'NE\x01\x00';s.pack_into('<HH',h,4,imports+2,2);h[12]=2
s.pack_into('<HHHHHHHH',h,14,4,heap,stack,entry-bases[0],1,minimum[3]+heap+stack,4,count)
s.pack_into('<HHHHH',h,34,64,resource,resident,imports,imports);s.pack_into('<H',h,50,4);h[54]=1;image[128:192]=h;image[128+resident:128+resident+10]=b'\x06W16NE3\x00\x00\x00'
for i,(blob,rs) in enumerate(zip(blobs,fix)):
 while len(image)%16:image.append(0)
 need(len(image)//16<=65535 and len(rs)<=65535,'NE file/relocation extent exceeded')
 s.pack_into('<HHHH',image,192+8*i,len(image)//16,len(blob),0x40|(i==3)|(0x100 if rs else 0),minimum[i])
 image+=blob
 if rs:image+=s.pack('<H',len(rs))+b''.join(rs)
s.pack_into('<HH',image,2,len(image)%512,(len(image)+511)//512)
out=Path(sys.argv[2]);out.write_bytes(image);out.chmod(0o755)
print('NE code:',minimum[:3],'data/BSS:',len(blobs[3]),z[5],'fixups:',[len(x) for x in fix],'heap/stack:',heap,stack,'shared constants:',minimum[4:])
