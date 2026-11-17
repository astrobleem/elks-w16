#!/usr/bin/env python3
from pathlib import Path
import re,tempfile,subprocess,os
root=Path(__file__).resolve().parents[1];h=(root/'src/wapi.h').read_text()
code=r"""
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef int16_t i16;
typedef int GR_WINDOW_ID,GR_GC_ID,GR_COLOR;
#define GR_RGB(r,g,b) ((r)|((g)<<8)|((b)<<16))
int gdi_gc;
"""
code+='\n'.join(l for l in h.splitlines() if l.startswith('#define WS_') or l.startswith('#define WM_'))+'\n'
code+=re.search(r'struct wnd \{.*?\n\};',h,re.S)[0]
code+=r"""
static struct wnd w;
static u16 codes[30],msgs[30];static u32 vals[30];static int n,draws;
struct wnd*win_get(u16 h){return h==w.hwnd&&w.used?&w:0;}
void win_client_origin(struct wnd*p,int*x,int*y){*x=p->x+p->cx;*y=p->y+p->cy;}
void w16_post(u16 hwnd,u16 msg,u16 code,u32 val){assert(hwnd==w.hwnd&&n<30);msgs[n]=msg;codes[n]=code;vals[n++]=val;}
void gdi_fill(int id,int color,int x,int y,int wid,int ht){
 int xx,yy;(void)color;assert(id==w.frame&&wid>0&&ht>0);
 for(yy=y;yy<y+ht;yy++)for(xx=x;xx<x+wid;xx++){
  assert(xx>=0&&yy>=0&&xx<w.w&&yy<w.h);
  assert(xx>=w.cx+w.cw||yy>=w.cy+w.ch);draws++;
 }
}
void gdi_fg(int c){(void)c;}
void GrRect(int id,int gc,int x,int y,int wi,int ht){(void)id;(void)gc;(void)x;(void)y;(void)wi;(void)ht;}
void GrLine(int id,int gc,int x,int y,int xx,int yy){(void)id;(void)gc;(void)x;(void)y;(void)xx;(void)yy;}
"""
code+=(root/'src/scrollbar.c').read_text().replace('#include "wapi.h"','')
code+=r"""
int main(void){
 w.used=1;w.hwnd=0x100;w.frame=7;w.x=10;w.y=20;w.cx=w.cy=1;
 w.cw=w.ch=100;w.w=w.h=118;w.style=WS_HSCROLL|WS_VSCROLL;
 w.scroll_max[0]=w.scroll_max[1]=100;
 scrollbar_draw(&w);assert(draws>0);
 assert(scrollbar_input(&w,1,115,25)&&codes[0]==0&&msgs[0]==WM_VSCROLL&&codes[1]==8);
 n=0;assert(scrollbar_input(&w,1,115,115)&&codes[0]==1);
 n=0;assert(scrollbar_input(&w,1,115,70)&&codes[0]==3);
 n=0;assert(!scrollbar_input(&w,1,40,50)&&n==0);
 assert(scrollbar_input(&w,1,115,40)&&n==0);
 assert(scrollbar_input(&w,0,115,92)&&codes[0]==5&&vals[0]==100);
 assert(scrollbar_input(&w,2,115,92)&&codes[1]==4&&vals[1]==100&&codes[2]==8);
 assert(w.scroll_pos[1]==0); /* app, not UI, owns the scroll position */
 n=0;w.scroll_pos[1]=100;
 assert(scrollbar_input(&w,1,115,50)&&codes[0]==2);
 n=0;w.scroll_max[0]=0;assert(scrollbar_input(&w,1,15,125)&&n==0);
 scrollbar_forget(w.hwnd);
 puts("Scrollbar non-client paint, line/page clicks and thumb notifications passed");return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-fsanitize=address,undefined','-no-pie',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
