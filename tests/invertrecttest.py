#!/usr/bin/env python3
import pathlib,tempfile,subprocess
root=pathlib.Path(__file__).resolve().parents[1];s=(root/'src/gdi.c').read_text();a=s.index('u32 u_InvertRect(');b=s.index('\n}',a)+2
pre='''#include "w16.h"
#include <assert.h>
#include <string.h>
typedef struct{int cols,rows;}GR_SCREEN_INFO;
static struct wnd{int cw,ch;}win={4,3};
static struct dc{struct wnd *w;int gc,wid;}dc={&win,1,2};
static struct dc*dc_get(u16 h){return h==7?&dc:0;}
static void GrGetScreenInfo(GR_SCREEN_INFO *s){s->cols=4;s->rows=3;}
#define GS(d) 0
#define GR_MODE_INVERT 1
#define GR_RGB(r,g,b) 15
#define gs_mode(a,b) ((void)0)
#define gs_fg(a,b) ((void)0)
static u8 bits[3][4],mem[128];
u8 *host_ptr(u16 s,u16 o,u16 n){assert(s==0x1050&&o+n<=128);return mem+o;}
static void GrFillRect(int w,int gc,int l,int t,int dx,int dy){
 int x,y;assert(w==2&&gc==1);
 for(y=t;y<t+dy;y++)for(x=l;x<l+dx;x++){assert(y>=0&&y<3&&x>=0&&x<4);bits[y][x]^=15;}}
'''
post='''int main(void){
 u16 s=0x1050;wr16(s,0,32);wr16(s,2,s);wr16(s,4,7);
 wr16(s,32,0x8000);wr16(s,34,1);wr16(s,36,32767);wr16(s,38,32767);
 assert(u_InvertRect(s,0));assert(!bits[0][1]&&bits[1][0]==15&&bits[2][3]==15);
 assert(u_InvertRect(s,0));assert(!bits[2][3]);dc.w=0;assert(u_InvertRect(s,0));
 wr16(s,0,0xfffa);assert(!u_InvertRect(s,0));return 0;}'''
with tempfile.TemporaryDirectory() as td:
 p=pathlib.Path(td);(p/'t.c').write_text(pre+s[a:b]+post)
 subprocess.run(['cc','-DW16_HOST','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'t.c'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True,env={'ASAN_OPTIONS':'detect_leaks=0'})
