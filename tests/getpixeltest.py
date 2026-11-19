#!/usr/bin/env python3
import pathlib,subprocess,tempfile,os,re
root=pathlib.Path(__file__).resolve().parents[1]
g=(root/'src/gdi.c').read_text();g=g[g.index('u32 g_GetPixel('):]
start=g.index('{');level=1;end=start+1
while level:
    level+=(g[end]=='{')-(g[end]=='}');end+=1
g=g[:end]+'\n'
extra=(root/'src/gdi.c').read_text();extra=extra[extra.index('u32 g_PtVisible('):]
start=extra.index('{');level=1;end=start+1
while level:
    level+=(extra[end]=='{')-(extra[end]=='}');end+=1
g+=extra[:end]+'\n'
pre='''#include "w16.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int opt_trace;
void wlog(const char *fmt,...) {(void)fmt;}
typedef unsigned char GR_PIXELVAL;
typedef struct {unsigned gcid;int mode,region;} GR_GC_INFO;
typedef struct {unsigned char r,g,b;} RGB;
typedef struct {int count;RGB palette[256];} GR_PALETTE;
struct wnd{int cw,ch;};struct dc{struct wnd*w;unsigned gc,wid;};
static struct wnd w={100,80};static struct dc d={&w,4,5};
static int cap=0x1000,region,readcalls,palcalls,pixel=12,palcount=16;
u8 w16_scratch[W16_SCRATCH],host_mem[0x110000];
u8*host_ptr(u16 s,u16 o,u16 n){assert((u32)o+n<=65536);return host_mem+LIN(s,o);}
static u32 bitmap_getpixel(u16 h,i16 x,i16 y){(void)h;(void)x;(void)y;return 0xffffffffUL;}
struct dc*dc_get(u16 h){return h==0x200?&d:0;}
void GrGetGCInfo(unsigned gc,GR_GC_INFO*i){i->gcid=gc;i->mode=cap;i->region=region;}
void GrReadArea(unsigned wid,int x,int y,int width,int height,GR_PIXELVAL*p){assert(wid==5&&x==17&&y==5&&width==1&&height==1);readcalls++;*p=pixel;}
void GrGetSystemPalette(GR_PALETTE*p){int i;palcalls++;p->count=palcount;for(i=0;i<16;i++){p->palette[i].r=i;p->palette[i].g=2*i;p->palette[i].b=3*i;}}
'''
test='''int main(void){int before;
wr16(0x1000,0,5);wr16(0x1000,2,17);wr16(0x1000,4,0x200);
assert(g_GetPixel(0x1000,0)==0x24180c);assert(readcalls==1&&palcalls==1);assert(g_PtVisible(0x1000,0)==1);
palcount=2;pixel=1;assert(g_GetPixel(0x1000,0)==0x030201);pixel=2;assert(g_GetPixel(0x1000,0)==0xffffffffUL);palcount=16;pixel=12;
before=readcalls;cap=0;assert(g_GetPixel(0x1000,0)==0xffffffffUL);assert(readcalls==before);assert(!g_PtVisible(0x1000,0));assert(readcalls==before);cap=0x1000;
region=1;assert(g_GetPixel(0x1000,0)==0xffffffffUL);assert(readcalls==before);assert(!g_PtVisible(0x1000,0));assert(readcalls==before);region=0;
wr16(0x1000,2,100);assert(g_GetPixel(0x1000,0)==0xffffffffUL);assert(readcalls==before);
wr16(0x1000,2,0xffff);assert(g_GetPixel(0x1000,0)==0xffffffffUL);assert(readcalls==before);wr16(0x1000,2,17);
pixel=255;assert(g_GetPixel(0x1000,0)==0xffffffffUL);assert(!g_PtVisible(0x1000,0));pixel=12;palcount=0;assert(g_GetPixel(0x1000,0)==0xffffffffUL);
assert(g_GetPixel(0x1000,0xfffb)==0xffffffffUL);
puts("GetPixel palette RGB, oldserver refusal, clipping and bounds checks passed");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(pre+g+test)
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
