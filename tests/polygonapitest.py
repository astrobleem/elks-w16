#!/usr/bin/env python3
"""Exercise the real Polygon entry point, including preflight and DC dispatch."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/gdi.c').read_text()
a=s.index('/* ALTERNATE is the only supported polygon fill mode.');b=s.index('/* Rectangle(hdc',a)
code=r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "polygon.h"
typedef uint16_t u16;typedef int16_t i16;typedef uint32_t u32;
typedef u32 GR_COLOR;
typedef struct {int cols,rows;} GR_SCREEN_INFO;
struct wnd {int cw,ch;};
struct dc {struct wnd*w;int pen,brush,wid,gc,mapmode;};
struct gobj {int null,style,width;u32 color;};
static struct wnd win={16,16};static struct dc dc={&win,1,2,3,4,1};
static struct gobj objects[2]={{0,0,1,0x111111},{0,0,0,0x222222}};
static u16 args[7],points[256];static int mapped,badmap,reads,memorycalls,writes,rop=13;
static u32 screen[16][16],color;
#define AW(n) args[(n)/2]
#define OBJ_PEN 1
#define OBJ_BRUSH 2
static struct dc*dc_get(u16 h){return h==1?&dc:0;}
static struct gobj*obj_get(u16 h){return h==1||h==2?&objects[h-1]:0;}
static u16 bitmap_rop2(u16 h,u16 mode){assert(mode==0);return h==2?13:0;}
static u16 bitmap_object(u16 h,int type,int unused){return h==2?type:0;}
static u16 rd16(u16 seg,u16 off){assert(seg==123&&off<512);reads++;return points[off/2];}
static int gdi_map_point(struct dc*d,int*x,int*y){mapped++;*x+=1;*y+=2;return !badmap;}
static int gdi_map_rect(struct dc*d,int*l,int*t,int*r,int*b){return gdi_map_point(d,l,t)&&gdi_map_point(d,r,b);}
static int bitmap_polygon(u16 h,struct poly_point*p,int n,u32 pc,int np,u32 bc,int nb)
{assert(h==2&&n==4&&p[0].x==2&&p[0].y==2);memorycalls++;return 1;}
static int bitmap_flood(u16 h,struct dc*d,int x,int y,u32 c,int mode,u32 brush,void (*out)(void*,int,int,int,int),void*ctx){return 0;}
#define AL(n) ((u32)AW(n)|((u32)AW((n)+2)<<16))
static u32 rgb_to_gr(u32 c){return c;}
static void GrGetScreenInfo(GR_SCREEN_INFO*s){s->cols=s->rows=16;}
static int set_rop(struct dc*d,u32 c){color=c;return rop!=11;}
static void GrFillRect(int wid,int gc,int x,int y,int w,int h)
{int i;assert(wid==3&&gc==4&&x>=0&&x+w<=16&&y>=0&&y<16&&h==1);for(i=x;i<x+w;i++){assert(!screen[y][i]);screen[y][i]=color;writes++;}}
''' +s[a:b]+r'''
int main(void){
 u16 square[]={2,2,6,2,6,6,2,6};int x,y;
 struct poly_point box[3]={{1,1},{13,13},{6,6}};
 args[0]=1;args[1]=1;assert(g_SetPolyFillMode(0,0)==1);
 args[1]=2;assert(g_SetPolyFillMode(0,0)==1);
 args[1]=99;assert(!g_SetPolyFillMode(0,0));args[1]=1;
 args[0]=2;assert(!g_SetPolyFillMode(0,0));args[0]=0;assert(!g_SetPolyFillMode(0,0));
 assert(!g_SetPolyFillMode(0,65534));
 memcpy(points,square,sizeof square);args[0]=4;args[1]=0;args[2]=123;args[3]=1;
 assert(g_Polygon(0,0)==1&&mapped==4&&reads==8&&writes==25);
 for(y=4;y<=8;y++)for(x=3;x<=7;x++)assert(screen[y][x]==(x==3||x==7||y==4||y==8?0x111111:0x222222));
 /* PS_INSIDEFRAME width zero/one is a real cosmetic solid pen,
  * including open polylines; out-of-bounds widths are refused, not downgraded. */
 objects[0].style=6;
 for(x=0;x<=1;x++){
  int xx,yy;objects[0].width=x;memset(screen,0,sizeof screen);writes=0;
  assert(g_Polygon(0,0)&&writes==25);
  for(yy=4;yy<=8;yy++)for(xx=3;xx<=7;xx++)
   assert(screen[yy][xx]==(xx==3||xx==7||yy==4||yy==8?0x111111:0x222222));
  memset(screen,0,sizeof screen);writes=0;
  assert(g_Polyline(0,0)&&writes==12);
  assert(screen[4][3]==0x111111&&!screen[8][3]&&!screen[5][3]&&!screen[6][5]);
 }
 objects[0].width=2;memset(screen,0,sizeof screen);writes=0;
 assert(g_Polyline(0,0)&&writes==28);
 memset(screen,0,sizeof screen);writes=0;
 assert(g_Polygon(0,0)&&writes==36);
 objects[0].width=33;memset(screen,0,sizeof screen);writes=0;
 assert(!g_Polygon(0,0)&&!g_Polyline(0,0)&&!writes);
 objects[0].style=0;objects[0].width=1;
 memset(screen,0,sizeof screen);writes=0;rop=11;assert(g_Polygon(0,0)&&!writes);rop=13;
 args[3]=2;assert(g_Polygon(0,0)&&memorycalls==1);args[3]=99;assert(!g_Polygon(0,0));args[3]=1;
 badmap=1;assert(!g_Polygon(0,0)&&!writes);badmap=0;
 objects[0].width=33;assert(!g_Polygon(0,0));objects[0].width=1;
 objects[1].style=3;assert(!g_Polygon(0,0));objects[1].style=0;
 args[0]=129;assert(!g_Polygon(0,0));args[0]=1;assert(!g_Polygon(0,0));args[0]=4;
 args[1]=65530;assert(!g_Polygon(0,0));args[1]=0;args[2]=0;assert(!g_Polygon(0,0));
 args[2]=65535;args[1]=16;assert(!g_Polygon(0,0));assert(!g_Polygon(0,65530));
 assert(!writes);
 /* Inside-frame thick outlines are accepted for bounded shapes only. */
 objects[0].style=6;objects[0].width=2;objects[1].null=1;
 assert(draw_shape(1,box,0)&&writes>0);
 memset(screen,0,sizeof screen);writes=0;
 box[0].x=box[0].y=1;box[1].x=box[1].y=13;
 dc.mapmode=2;assert(!draw_shape(1,box,0)&&!writes);dc.mapmode=1;
 objects[0].style=1;assert(!draw_shape(1,box,0)&&!writes);
 objects[0].style=0;assert(!draw_shape(1,box,0)&&!writes);
 puts("Polygon API: Pascal decoding, mapped display output, memory dispatch and atomic refusals passed");return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-I'+str(root/'src'),'-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'t.c'),str(root/'src/polygon.c'),str(root/'src/oval.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
