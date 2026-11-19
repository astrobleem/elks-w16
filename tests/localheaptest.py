#!/usr/bin/env python3
"""Exercise the actual local heap implementation with distinct task/DLL DS."""
import pathlib,tempfile,subprocess,os
root=pathlib.Path(__file__).resolve().parents[1]
s=(root/'src/kernel.c').read_text()
s=s[s.index('/* ---------------------------------------------------------------- local heap */'):s.index('/* ---------------------------------------------------------------- resources */')]
test=r'''
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct ne_module app={0},dll={0};
    u16 ss,h1,h2,h3,i;
    assert(!seg_alloc(64,&app.dgroup));
    assert(!seg_alloc(80,&dll.dgroup));
    assert(!seg_alloc(8,&ss));
    app.heap_start=128; app.dgroup_size=512; app.heap=384;app.loaded=1;
    dll.heap_start=224; dll.dgroup_size=1248; dll.heap=1024;dll.loaded=1;
    app.dependencies[0]=&dll;app.nmod=1;w16_mod=&app;
    lheap_init(&app);
    wr16(ss,32,app.dgroup);assert(k_GetHeapSpaces(ss,32)==0x0180017eUL);
    app.loaded=0;assert(!k_GetHeapSpaces(ss,32));app.loaded=1;
    wr16(ss,32,0xffff);assert(!k_GetHeapSpaces(ss,32)&&!k_GetHeapSpaces(ss,0xffff));
    wr16(ss,24,dll.dgroup); /* saved caller DS, ap-8 */
    wr16(ss,32,1024);wr16(ss,34,0);wr16(ss,36,0);
    assert(k_LocalInit(ss,32));
    assert(dll.local_start==224&&dll.local_end==1248);
    wr16(ss,32,20);wr16(ss,34,0x40);
    h1=k_LocalAlloc(ss,32);assert(h1==226);
    for(i=0;i<20;i++)assert(!rd8(dll.dgroup,h1+i));
    wr16(ss,24,app.dgroup);
    h2=k_LocalAlloc(ss,32);assert(h2==130);
    wr16(ss,32,app.dgroup);assert(k_GetHeapSpaces(ss,32)==0x01800168UL);
    wr16(ss,32,dll.dgroup);assert(k_GetHeapSpaces(ss,32)==0x040003e8UL);
    /* Do not report plausible free memory when an arena is corrupt. */
    wr16(dll.dgroup,246,1);assert(!k_GetHeapSpaces(ss,32));wr16(dll.dgroup,246,1002);

    wr16(ss,32,h1);assert(!k_LocalSize(ss,32));
    wr16(ss,24,dll.dgroup);assert(k_LocalSize(ss,32)==20);
    assert(k_LocalLock(ss,32)==h1);assert(!k_LocalFree(ss,32));
    wr16(ss,32,1000);h3=k_LocalAlloc(ss,32);assert(h3==226);
    wr16(ss,32,h3);assert(!k_LocalFree(ss,32));
    wr16(ss,24,app.dgroup);wr16(ss,32,h2);assert(k_LocalLock(ss,32)==h2);
    assert(!k_LocalFree(ss,32));
    wr16(ss,24,0xffff);wr16(ss,32,20);assert(!k_LocalAlloc(ss,32));
    wr16(ss,24,dll.dgroup);wr16(ss,32,1025);wr16(ss,34,0);assert(!k_LocalInit(ss,32));
    wr16(ss,32,1248);wr16(ss,34,100);wr16(ss,36,dll.dgroup);assert(!k_LocalInit(ss,32));
    /* NE minimum heap stays 1024 while a preallocated reserve is available. */
    dll.dgroup_size=1280;wr16(ss,32,1024);wr16(ss,34,0);wr16(ss,36,0);
    assert(k_LocalInit(ss,32));assert(dll.local_start==224&&dll.local_end==1280);
    /* Resize only the DLL heap, preserve handles and failure atomicity. */
    lheap_init(&dll);wr16(ss,24,dll.dgroup);
    wr16(ss,32,20);wr16(ss,34,0x40);h1=k_LocalAlloc(ss,32);
    for(i=0;i<20;i++)wr8(dll.dgroup,h1+i,0xa5);
    wr16(ss,32,0x40);wr16(ss,34,40);wr16(ss,36,h1);
    assert(k_LocalReAlloc(ss,32)==h1);
    for(i=0;i<20;i++)assert(rd8(dll.dgroup,h1+i)==0xa5);
    for(i=20;i<40;i++)assert(!rd8(dll.dgroup,h1+i));
    wr16(ss,32,0);wr16(ss,34,8);assert(k_LocalReAlloc(ss,32)==h1);
    wr16(ss,32,20);wr16(ss,34,0);h2=k_LocalAlloc(ss,32);
    assert(h2!=0 && h2!=h1);
    wr16(ss,32,0x40);wr16(ss,34,40);wr16(ss,36,h1);
    assert(!k_LocalReAlloc(ss,32)); /* live neighbor blocks growth */
    assert(rd16(dll.dgroup,h1-2)==11);
    for(i=0;i<8;i++)assert(rd8(dll.dgroup,h1+i)==0xa5);
    wr16(ss,32,h2);assert(!k_LocalFree(ss,32));
    wr16(ss,32,0x40);assert(k_LocalReAlloc(ss,32)==h1);
    wr16(ss,34,2000);assert(!k_LocalReAlloc(ss,32)); /* exhaustion */
    assert(rd16(dll.dgroup,h1-2)==43);
    wr16(ss,34,0);assert(!k_LocalReAlloc(ss,32)); /* no discard support */
    wr16(ss,32,0x80);wr16(ss,34,40);assert(!k_LocalReAlloc(ss,32));
    wr16(ss,32,0x40);wr16(ss,36,h1+2);assert(!k_LocalReAlloc(ss,32));
    wr16(ss,36,h1);wr16(ss,24,app.dgroup);assert(!k_LocalReAlloc(ss,32));
    seg_free(ss);seg_free(app.dgroup);seg_free(dll.dgroup);
    puts("Task and DLL local heap ownership, initialization, bounds and reuse passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    d=pathlib.Path(td)
    (d/'test.c').write_text("""#include "w16.h"
struct ne_module *w16_mod;
"""+s+test)
    subprocess.run(['cc','-DW16_HOST','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie',
                    '-I'+str(root/'src'),str(d/'test.c'),str(root/'src/mem.c'),'-o',str(d/'test')],check=True)
    subprocess.run([str(d/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
