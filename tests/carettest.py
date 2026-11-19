#!/usr/bin/env python3
"""Exercise real caret code against a clipped invert framebuffer."""
import pathlib,subprocess,tempfile,os,re
root=pathlib.Path(__file__).resolve().parents[1]
api=(root/'src/api.c').read_text()
# Storage annotation does not change API names or the binding contract.
api=re.sub(r'W16_RO_STRING\(("[^"]*")\)', r'\1', api)
for ordinal,name,size in [(163,'CreateCaret',8),(164,'DestroyCaret',0),(165,'SetCaretPos',4),(166,'HideCaret',2),(167,'ShowCaret',2)]:
 assert '{ U, %d, "%s", %d, 0, H(u_%s) }'%(ordinal,name,size,name) in api
source=(root/'src/caret.c').read_text().replace('#include "wapi.h"','')
code=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint16_t u16;typedef int16_t i16;typedef uint32_t u32;typedef uint8_t u8;
typedef int GR_GC_ID;
#define GR_MODE_XOR 1
#define GR_RGB(r,g,b) 0xffffff
static u16 args[4];
#define AW(n) args[(n)/2]
struct wnd {int client;} window={1};
static struct wnd*win_get(u16 h){return h==256?&window:0;}
static u32 clockms;
static u32 now_ms(void){return clockms;}
static int gc_live,points;
static unsigned char pixels[32][32];
static int GrNewGC(void){gc_live++;return 1;}
static void GrDestroyGC(int gc){assert(gc==1);gc_live--;}
static void GrSetGCMode(int gc,int mode){assert(gc==1&&mode==1);}
static void GrSetGCForeground(int gc,int color){assert(gc==1&&color==0xffffff);}
static void GrPoint(int wid,int gc,int x,int y){assert(wid==1&&gc==1);if(x>=0&&y>=0&&x<32&&y<32){pixels[y][x]^=1;points++;}}
static void GrFillRect(int wid,int gc,int x,int y,int w,int h){int i,j;for(j=0;j<h;j++)for(i=0;i<w;i++)GrPoint(wid,gc,x+i,y+j);}
'''+source+r'''
static void blank(void){int x,y;for(y=0;y<32;y++)for(x=0;x<32;x++)assert(!pixels[y][x]);}
int main(void){
 args[0]=4;args[1]=2;args[2]=0;args[3]=256;
 assert(u_CreateCaret(0,0));assert(gc_live==1);blank();
 args[0]=257;assert(!u_ShowCaret(0,0));blank();
 args[0]=0;assert(u_ShowCaret(0,0));assert(points==8);
 assert(u_ShowCaret(0,0));assert(points==8);
 assert(caret_wait(499)==1);assert(caret_wait(500)==500);blank();
 clockms=1000;assert(caret_wait(1000)==500);assert(pixels[0][0]);
 args[0]=3;args[1]=5;u_SetCaretPos(0,0);assert(!pixels[0][0]&&pixels[3][5]);
 args[0]=0;u_HideCaret(0,0);u_HideCaret(0,0);blank();u_ShowCaret(0,0);blank();u_ShowCaret(0,0);assert(pixels[3][5]);
 caret_paint(256,1);caret_paint(256,1);blank();caret_paint(256,0);blank();caret_paint(256,0);assert(pixels[3][5]);
 caret_destroy(257);assert(gc_live==1);caret_destroy(256);blank();assert(!gc_live);
 args[0]=4;args[1]=2;args[2]=1;args[3]=256;assert(u_CreateCaret(0,0));args[0]=0;u_ShowCaret(0,0);assert(pixels[0][1]&&!pixels[0][0]);u_DestroyCaret(0,0);blank();
 args[0]=4;args[1]=2;args[2]=123;args[3]=256;assert(!u_CreateCaret(0,0));assert(!gc_live);
 puts("caret: owner, invert pixels, nested hide/show, blink, position, paint and teardown passed");return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
 p=pathlib.Path(td);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
