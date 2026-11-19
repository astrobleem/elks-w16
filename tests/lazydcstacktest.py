#!/usr/bin/env python3
"""Focused optional saved-DC allocation failure, lifetime and reuse check."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];g=(root/'src/gdi.c').read_text();b=(root/'src/bitmap.c').read_text();h=(root/'src/wapi.h').read_text()
def func(src,name):
 m=re.search(r'^(?:static )?(?:struct dc \*|u32 |u16 |void |int )'+name+r'\(',src,re.M);assert m,name
 a=src.index('{',m.start());e=a+1;n=1
 while n:n+=(src[e]=='{')-(src[e]=='}');e+=1
 return src[m.start():e]+'\n'
code='''#define W16_LAZY_UI
#include "w16.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#undef AW
#define AW(n) args[(n)/2]
static u16 args[2];
#define MAXDC 6
#define HDC_BASE 0x200
#define DBASE 0xc00
#define ND 8
#define NB 16
#define DC_SAVE_DEPTH 4
#define MEMORY_SAVE_DEPTH 4
typedef unsigned GR_WINDOW_ID;typedef unsigned GR_GC_ID;
struct wnd;
'''+re.search(r'struct dc \{.*?\n\};',h,re.S)[0]+'\n'+re.search(r'struct memory_dc \{.*?\};',b,re.S)[0]+'''
static struct dc dcs[MAXDC],*dc_saved[MAXDC];static u8 dc_saved_count[MAXDC];
static struct memory_dc memory[ND],*memory_saved[ND];static u8 memory_saved_count[ND];
static struct{int gc,known;}gcs[MAXDC];static int nocache;
#define GS(d) (&gcs[((d)->hdc-HDC_BASE)>>1])
static int left=-1,live;
static void *checked_calloc(unsigned n,unsigned size){void*p;if(!left)return 0;if(left>0)left--;p=calloc(n,size);if(p)live++;return p;}
static void checked_free(void*p){if(p){assert(live>0);live--;free(p);}}
#define calloc checked_calloc
#define free checked_free
void GrDestroyGC(unsigned gc){(void)gc;}
static struct dc *dc_get(u16 id){unsigned i=(id-HDC_BASE)>>1;return id>=HDC_BASE&&!(id&1)&&i<MAXDC&&dcs[i].used?&dcs[i]:0;}
static void release_bitmap(int i){(void)i;}
'''+''.join(func(b,n) for n in ('di','bitmap_savedc','bitmap_restoredc','bitmap_deletedc','bitmap_shutdown'))+''.join(func(g,n) for n in ('saved_object','g_SaveDC','g_RestoreDC','dc_release'))+'''
static u32 save(u16 h){args[0]=h;return g_SaveDC(0,0);}
static u32 restore(u16 h,i16 n){args[0]=n;args[1]=h;return g_RestoreDC(0,0);}
int main(void){
 unsigned i;struct dc old;struct memory_dc before;
 dcs[0].used=1;dcs[0].hdc=HDC_BASE;dcs[0].pen=42;old=dcs[0];
 left=0;assert(!save(HDC_BASE)&&!dc_saved_count[0]&&!dc_saved[0]&&!memcmp(&old,&dcs[0],sizeof old));
 left=-1;assert(save(HDC_BASE)==1&&live==1);dcs[0].pen=77;assert(saved_object(42));
 for(i=1;i<4;i++)assert(save(HDC_BASE)==i+1);assert(!save(HDC_BASE));
 assert(!restore(HDC_BASE,0)&&!restore(HDC_BASE,-5)&&live==1);
 assert(restore(HDC_BASE,1)&&dcs[0].pen==42&&!live&&!dc_saved_count[0]&&!dc_saved[0]);
 left=0;assert(!save(HDC_BASE));left=-1;assert(save(HDC_BASE));dc_release(HDC_BASE);assert(!live&&!dc_saved[0]&&!dc_saved_count[0]);
 dcs[0]=old;assert(!restore(HDC_BASE,-1));assert(save(HDC_BASE)==1);dc_release(HDC_BASE);assert(!live);
 memory[0].used=1;memory[0].pen=123;before=memory[0];
 left=0;assert(!save(DBASE)&&!memory_saved_count[0]&&!memory_saved[0]&&!memcmp(&before,&memory[0],sizeof before));
 left=-1;assert(save(DBASE)==1);memory[0].pen=321;assert(save(DBASE)==2);assert(restore(DBASE,-2));assert(!memcmp(&before,&memory[0],sizeof before)&&!live&&!memory_saved[0]);
 assert(save(DBASE));assert(bitmap_deletedc(DBASE)==1&&!live&&!memory_saved_count[0]);
 memory[0]=before;assert(!restore(DBASE,-1));assert(save(DBASE)==1);bitmap_shutdown();assert(!live&&!memory_saved[0]&&!memory_saved_count[0]);bitmap_shutdown();assert(!live);
 /* Full original simultaneous capacity and four-deep stacks, not a lower limit. */
 for(i=0;i<MAXDC;i++){dcs[i]=old;dcs[i].hdc=HDC_BASE+2*i;assert(save(HDC_BASE+2*i));}
 for(i=0;i<ND;i++){memory[i]=before;assert(save(DBASE+2*i));}
 assert(live==MAXDC+ND);for(i=0;i<MAXDC;i++)dc_release(HDC_BASE+2*i);bitmap_shutdown();assert(!live);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-DW16_HOST','-I'+str(root/'src'),'-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: surface/memory SaveDC first-allocation failure, state/reference lifetime, depth, restore/delete/reuse/shutdown and full DC capacity')
