#!/usr/bin/env python3
"""Exercise the real FrameRect bands with clipped solid/pattern raster output."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/gdi.c').read_text()
a=s.index('static int frame_band('); b=s.index('/* ---------------------------------------------------------------- bitmaps, pattern brushes, PatBlt */',a)
code=r'''
#include "w16.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define OBJ_BRUSH 2
#define BS_PATTERN 3
#define MAXBMP 1
#define GR_MODE_COPY 0
#define GS(d) (d)
typedef struct {int cols,rows;} GR_SCREEN_INFO;
struct gobj {int type,null,style,width;u32 color;} brushes[]={{2,0,0,0,3},{2,0,3,0,0},{2,1,0,0,0},{1,0,0,0,0},{2,0,3,1,0}};
static struct {int used;}bmps[]={{1}};
static struct wnd {int cw,ch;} win={8,6};
static struct dc {struct wnd*w;int wid,gc,brush;} dc={&win,1,2,999};
static struct dc*dc_get(u16 h){return h==7?&dc:0;}
static struct gobj*obj_get(u16 h){return h>=20&&h<25?&brushes[h-20]:0;}
static int bitmap_rop2(u16 h,int mode){assert(!mode);return h==8;}
static int dx,dy,scale=1,fg,mode,pixels[6][8],writes[6][8],memory_calls,clip=0;
static u8 mem[128];
u8 *host_ptr(u16 s,u16 o,u16 n){assert(s==0x1050&&o+n<=128);return mem+o;}
static void GrGetScreenInfo(GR_SCREEN_INFO*s){s->cols=8;s->rows=6;}
static int gdi_map_rect(struct dc*d,int*l,int*t,int*r,int*b){
 *l=*l*scale+dx;*r=*r*scale+dx;*t=*t*scale+dy;*b=*b*scale+dy;return 1;}
static void gs_mode(struct dc*d,int m){mode=m;}
static void gs_fg(struct dc*d,int f){fg=f;}
static int rgb_to_gr(u32 c){return c;}
static void paint(int l,int t,int w,int h,int pat){int x,y;
 assert(mode==0);assert(l>=0&&t>=0&&l+w<=8&&t+h<=6);
 for(y=t;y<t+h;y++)for(x=l;x<l+w;x++){
  if(clip&&(x<2||x>=6||y<1||y>=5))continue;
  pixels[y][x]=pat?1+((x+y)&1):fg;writes[y][x]++;
 }}
static void GrFillRect(int wid,int gc,int l,int t,int w,int h){assert(wid==1&&gc==2);paint(l,t,w,h,0);}
static int pat_fill(struct dc*d,int l,int t,int w,int h,int bi){assert(bi==0);mode=0;paint(l,t,w,h,1);return 1;}
static int bitmap_patblt(u16 h,int l,int t,int w,int ht,u32 rop,u32 c,int style,int null){
 assert(h==8&&rop==0xf00021&&c==3&&!style&&!null);memory_calls++;return 1;}
''' +s[a:b]+r'''
static u16 seg=0x1050;
static void setup(int l,int t,int r,int b,int brush,int h){
 memset(pixels,0,sizeof pixels);memset(writes,0,sizeof writes);memory_calls=0;
 wr16(seg,0,brush);wr16(seg,2,32);wr16(seg,4,seg);wr16(seg,6,h);
 wr16(seg,32,l);wr16(seg,34,t);wr16(seg,36,r);wr16(seg,38,b);}
static void expect(int l,int t,int r,int b,int pat){int x,y;
 for(y=0;y<6;y++)for(x=0;x<8;x++){
  int edge=x>=l&&x<r&&y>=t&&y<b&&(x==l||x==r-1||y==t||y==b-1);
  if(clip&&(x<2||x>=6||y<1||y>=5))edge=0;
  assert(pixels[y][x]==(edge?(pat?1+((x+y)&1):3):0));assert(writes[y][x]==edge);
 }}
int main(void){int w,h;
 for(w=1;w<=7;w++)for(h=1;h<=5;h++){
  setup(1,1,1+w,1+h,20,7);assert(u_FrameRect(seg,0));expect(1,1,1+w,1+h,0);
 }
 setup(-2,-1,5,4,20,7);assert(u_FrameRect(seg,0));expect(-2,-1,5,4,0);
 clip=1;setup(0,0,8,6,20,7);assert(u_FrameRect(seg,0));expect(0,0,8,6,0);clip=0;
 setup(1,1,7,5,21,7);assert(u_FrameRect(seg,0));expect(1,1,7,5,1);assert(dc.brush==999);
 dx=2;dy=1;setup(-1,0,4,4,20,7);assert(u_FrameRect(seg,0));expect(1,1,6,5,0);dx=dy=0;
 scale=2;setup(1,1,3,3,20,7);assert(u_FrameRect(seg,0));
 assert(pixels[2][2]==3&&pixels[3][3]==3&&pixels[4][4]==3&&pixels[5][5]==3);scale=1;
 setup(1,1,7,5,20,8);assert(u_FrameRect(seg,0)&&memory_calls==4);
 setup(1,1,7,5,22,7);assert(u_FrameRect(seg,0));expect(0,0,0,0,0);
 setup(1,1,1,5,20,7);assert(!u_FrameRect(seg,0));
 setup(7,1,1,5,20,7);assert(!u_FrameRect(seg,0));
 setup(1,1,7,5,23,7);assert(!u_FrameRect(seg,0));
 setup(1,1,7,5,24,7);assert(!u_FrameRect(seg,0));
 setup(1,1,7,5,20,99);assert(!u_FrameRect(seg,0));
 setup(1,1,7,5,20,7);wr16(seg,2,0xfffa);assert(!u_FrameRect(seg,0));
 setup(1,1,7,5,20,7);wr16(seg,2,0);wr16(seg,4,0);assert(!u_FrameRect(seg,0));
 assert(!u_FrameRect(seg,0xfffa));dc.w=0;
 setup(1,1,7,5,20,7);assert(u_FrameRect(seg,0));expect(1,1,7,5,0);
 puts("FrameRect: clipped exclusive edges, thin/empty/invalid RECTs, brush/DC state and memory dispatch passed");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-DW16_HOST','-O1','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
