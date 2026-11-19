#!/usr/bin/env python3
import pathlib,subprocess,tempfile,os,re
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
    source=(root/'src/kernel.c').read_text()
    def func(name):
        m=re.search(r'^(?:static )?(?:int|u32) '+name+r'\(',source,re.M)
        start=source.index('{',m.start());end=start+1;level=1
        while level:
            level+=(source[end]=='{')-(source[end]=='}');end+=1
        return source[m.start():end]
    integration='#include "w16.h"\n#include <assert.h>\n#define MAXRSRC 8\nstatic struct {u8 used;u32 off;u16 len,seg;} rsrc[MAXRSRC];\n'
    integration+='\n'.join(func(n) for n in ('k_DirectResAlloc','rsrc_by_seg','resource_view','k_GlobalLock','k_GlobalUnlock','k_GlobalSize','k_FreeResource'))
    integration+="""
void test_resource(u16 stack) {
 u16 seg; u32 ptr;
 assert(!seg_alloc(2,&seg));rsrc[0].used=1;rsrc[0].seg=seg;rsrc[0].len=21;
 wr16(stack,0,seg);assert(k_GlobalLock(stack,0)==((u32)seg<<16));
 assert(k_GlobalSize(stack,0)==32);assert(!k_GlobalUnlock(stack,0));
 assert(!global_pointer(seg,&ptr));assert(!k_FreeResource(stack,0));
 assert(!rsrc[0].used);
}
"""
    source=(root/'src/user.c').read_text()
    integration+='\n#include <stdlib.h>\n'+func('u_LoadIconHandler')
    integration+='\n'+source[source.index('/* Resource images share'):source.index('/* GetIconId(')]
    integration+='\n'+func('u_CopyIcon')+'\n'+func('u_DestroyIcon')
    integration+=r"""
#include <string.h>
u32 u_DumpIcon(u16,u16),k_GlobalFree(u16,u16),k_GlobalReAlloc(u16,u16),u_DestroyCursor(u16,u16);
static u8 *load_icon(const char *name,u16 id,u16 *len,u16 *resource) {
 u8 *d;unsigned i;(void)name;assert(id==10||id==11);*len=744;*resource=9;
 d=calloc(1,*len);assert(d);d[0]=40;d[4]=32;d[8]=64;d[12]=1;d[14]=4;
 for(i=0;i<16;i++){
  u8 v=i&8?255:128;
  d[40+4*i]=i&1?v:0;d[41+4*i]=i&2?v:0;d[42+4*i]=i&4?v:0;
  if(i==7)d[40+4*i]=d[41+4*i]=d[42+4*i]=192;
  if(i==8)d[40+4*i]=d[41+4*i]=d[42+4*i]=128;
 }
 d[104+31*16]=0x1f;d[616+31*4]=0x80;return d;
}
void test_resource_icon(u16 stack) {
 struct ne_module app;u16 h,copy,out,len;u32 ptr,other;u8 *dib;
 memset(&app,0,sizeof(app));app.dgroup=0x1234;app.loaded=1;w16_mod=&app;
 assert(!seg_alloc(8,&out));
 wr16(stack,0,10);wr16(stack,2,0);wr16(stack,4,app.dgroup);
 h=u_LoadIcon(stack,0);assert(h&&h!=0x05a0&&global_image(h,0));
 wr16(stack,0,11);assert(u_LoadIcon(stack,0)==h);
 wr16(stack,0,h);ptr=k_GlobalLock(stack,0);assert(ptr);
 assert(rd16(ptr>>16,0)==16&&rd16(ptr>>16,2)==16);
 assert(rd16(ptr>>16,4)==32&&rd16(ptr>>16,6)==32&&rd16(ptr>>16,8)==4);
 assert(rd8(ptr>>16,10)==1&&rd8(ptr>>16,11)==4);
 wr16(stack,0,0);wr16(stack,2,out);wr16(stack,4,4);wr16(stack,6,out);
 wr16(stack,8,8);wr16(stack,10,out);wr16(stack,12,0);wr16(stack,14,ptr>>16);
 assert(u_DumpIcon(stack,0)==0x02000080UL&&rd16(out,8)==12);
 assert(rd32(out,4)==ptr+12&&rd32(out,0)==ptr+140);
 assert(rd8(ptr>>16,12)==0x80&&rd8(ptr>>16,140)==0x1f);
 wr16(stack,0,h);assert(k_GlobalFree(stack,0)==h);
 wr16(stack,0,0);wr32(stack,2,1024);wr16(stack,6,h);assert(!k_GlobalReAlloc(stack,0));
 wr16(stack,0,h);wr16(stack,2,app.dgroup);copy=u_CopyIcon(stack,0);assert(copy&&copy!=h);
 assert(global_pointer(copy,&other)&&other!=ptr&&rd8(other>>16,140)==0x1f);global_unlock(copy);
 wr8(ptr>>16,140,0xa0);dib=image_snapshot(h,&len);assert(dib&&len==744&&dib[104+31*16]==0xa0);free(dib);
 wr16(stack,0,h);assert(!k_GlobalUnlock(stack,0));assert(!u_DestroyIcon(stack,0));assert(!u_DestroyCursor(stack,0));
 assert(global_pointer(h,&ptr)&&ptr);global_unlock(h);
 wr16(stack,0,copy);assert(u_DestroyIcon(stack,0)&&!global_pointer(copy,&other));
 wr16(stack,0,0x05a0);assert(!k_GlobalLock(stack,0));
 icon_shutdown();assert(!global_pointer(h,&ptr));seg_free(out);w16_mod=0;
}
"""
    local=pathlib.Path(td)/'resources.c';local.write_text(integration)
    exe=str(pathlib.Path(td)/'globaltest')
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
                    '-I'+str(root/'src'),str(root/'src/global.c'),str(root/'src/icon.c'),str(root/'src/dib.c'),str(root/'src/mem.c'),
                    str(root/'tests/global_test.c'),str(local),'-o',exe],check=True)
    subprocess.run([exe],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
