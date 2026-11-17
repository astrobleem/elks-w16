#!/usr/bin/env python3
import pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
s=(root/'src/gdi.c').read_text();h=(root/'src/wapi.h').read_text()
def func(name):
 m=re.search(r'^(?:static )?(?:struct dc \*|u32 |void |int )'+name+r'\(',s,re.M);assert m,name
 p=s.index('{',m.start());e=p+1;n=1
 while n:n+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[m.start():e]+'\n'
code='''#include "w16.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define MAXDC 6
#define HDC_BASE 0x200
#define HOBJ_BASE 0x400
#define DC_SAVE_DEPTH 4
typedef int GR_WINDOW_ID;typedef int GR_GC_ID;
struct wnd;
'''
code+=re.search(r'struct dc \{.*?\n\};',h,re.S)[0]
code+='''
static struct dc dcs[MAXDC],dc_saved[MAXDC][DC_SAVE_DEPTH];
static u8 dc_saved_count[MAXDC];
static struct {int known,gc;} gcs[MAXDC];
#define GS(d) (&gcs[((d)->hdc-HDC_BASE)>>1])
static int nocache;
void GrDestroyGC(int g){(void)g;}
u16 bitmap_savedc(u16 h){(void)h;return 0;}
u16 bitmap_restoredc(u16 h,i16 l){(void)h;(void)l;return 0;}
u16 bitmap_rop2(u16 h,u16 m){(void)h;(void)m;return 0;}
u8 host_mem[0x110000];
u8 *host_ptr(u16 s,u16 o,u16 n){assert((u32)o+n<=65536);return host_mem+LIN(s,o);}
#define MAXOBJ 1
#define OBJ_BRUSH 2
#define BS_PATTERN 3
struct gobj {int used,type,style,width;} object={1,0,0,0},objs[1];static struct{int used;}bmps[1];
struct gobj*obj_get(u16 h){return h==0x400?&object:0;}
int bmp_index(u16 h){return h==0x900?0:-1;}
int bitmap_delete(u16 h){(void)h;return -1;}
int bitmap_object_used(u16 h){(void)h;return 0;}
#define vlog(...) ((void)0)
'''
code+=''.join(func(n) for n in ('dc_get','saved_object','g_SaveDC','g_RestoreDC','g_GetROP2','dc_release','g_DeleteObject'))
code+='''
static u32 save(u16 h){wr16(0x1000,0,h);return g_SaveDC(0x1000,0);}
static u32 restore(u16 h,i16 l){wr16(0x1000,0,l);wr16(0x1000,2,h);return g_RestoreDC(0x1000,0);}
int main(void){
 struct dc old;int i;
 dcs[0].used=1;dcs[0].hdc=HDC_BASE;dcs[0].gc=99;dcs[0].pen=0x400;dcs[0].rop2=13;
 dcs[0].brush=0x401;dcs[0].font=0x402;dcs[0].brushx=-3;dcs[0].curx=55;dcs[0].bkcolor=0x123456;
 old=dcs[0];assert(save(HDC_BASE)==1);
 dcs[0].pen=0x403;dcs[0].brushx=72;dcs[0].rop2=7;dcs[0].textcolor=333;assert(save(HDC_BASE)==2);
 wr16(0x1000,0,0x400);assert(!g_DeleteObject(0x1000,0));
 assert(saved_object(0x400));assert(!restore(HDC_BASE,0)&&!restore(HDC_BASE,-3)&&!restore(HDC_BASE,3));
 gcs[0].known=255;assert(restore(HDC_BASE,-2));assert(!memcmp(&old,&dcs[0],sizeof old));assert(!gcs[0].known);
 assert(!restore(HDC_BASE,-1));
 for(i=0;i<4;i++)assert(save(HDC_BASE)==(u32)i+1);assert(!save(HDC_BASE));
 assert(restore(HDC_BASE,2));assert(dc_saved_count[0]==1);
 dc_release(HDC_BASE);assert(!dc_saved_count[0]&&!saved_object(0x400));
 dcs[0].used=1;assert(!restore(HDC_BASE,-1));dcs[0].pen=0;
 wr16(0x1000,0,0x400);assert(g_DeleteObject(0x1000,0));assert(!object.used);
 objs[0].used=1;objs[0].type=OBJ_BRUSH;objs[0].style=BS_PATTERN;objs[0].width=0;bmps[0].used=1;
 wr16(0x1000,0,0x900);assert(!g_DeleteObject(0x1000,0)&&bmps[0].used);
 objs[0].used=0;assert(g_DeleteObject(0x1000,0)&&!bmps[0].used);
 assert(!save(HDC_BASE+1));assert(!save(0xffff));assert(!restore(0xffff,-1));
 puts("DC save/restore full state, object lifetime, limits and handle reuse passed");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
 p=pathlib.Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Wno-misleading-indentation','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
