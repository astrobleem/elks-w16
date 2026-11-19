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
    local=pathlib.Path(td)/'resources.c';local.write_text(integration)
    exe=str(pathlib.Path(td)/'globaltest')
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
                    '-I'+str(root/'src'),str(root/'src/global.c'),str(root/'src/icon.c'),str(root/'src/dib.c'),str(root/'src/mem.c'),
                    str(root/'tests/global_test.c'),str(local),'-o',exe],check=True)
    subprocess.run([exe],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
