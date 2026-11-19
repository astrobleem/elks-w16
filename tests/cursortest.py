#!/usr/bin/env python3
"""Exercise actual cursor loading with bounded synthetic Win16 resources."""
import os
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'src/cursor.c').read_text().split('/* ---------------------------------------------------------------- accelerators */')[0]
source = source.replace('#include "wapi.h"', '').replace('#include "dib.h"', '')
dib_header=(root/'src/dib.h').read_text().split('#include "w16.h"',1)[1].rsplit('#endif',1)[0]
dib_source=(root/'src/dib.c').read_text().replace('#include "dib.h"','')
gdi=(root/'src/gdi.c').read_text();icon_source=gdi[gdi.index('int gdi_draw_icon('):gdi.index('/* ---------------------------------------------------------------- text extents')]
setup = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16, GR_BITMAP;
typedef uint32_t u32;
typedef unsigned GR_CURSOR_ID, GR_WINDOW_ID;
#define GR_RGB(r,g,b) (((unsigned)(r)<<16)|((unsigned)(g)<<8)|(b))
#define HCURSOR_BASE 0x500
#define MAXWIN 2
#define RT_CURSOR 1
#define RT_GROUP_CURSOR 12
static u16 args[3];
#define AW(n) args[(n)/2]
static struct { int used, ctl; unsigned frame, client; } wins[MAXWIN];
static int new_calls, fail_new, short_data, missing, resource_reads, group_reads,bad_hot,bad_header,odd_height;
static unsigned last_cursor;
static const char *guest_name;
static void wlog(const char *s, ...) { (void)s; }
static void vlog(const char *s, ...) { (void)s; }
static void res_name(u16 off,u16 seg,char *name,u16 *id) {
    if(seg) { strcpy(name, guest_name); *id=0; }
    else { *name=0; *id=off; }
}
static u8 *res_load(u16 type,const char *name,u16 id,u16 *len) {
    u8 *p;
    if(missing) return NULL;
    if(type==RT_GROUP_CURSOR) {
        group_reads++; *len=20; p=calloc(1,*len);
        /* Named aliases and distinct group ordinals can identify one image. */
        p[18]=name ? 6 : id==106 ? 6 : id;
    } else {
        assert(type==RT_CURSOR); resource_reads++;
        *len=short_data ? 2 : 180; p=calloc(1,*len);
        if(!short_data) {p[4]=bad_header?108:40;p[8]=16;p[12]=odd_height?31:32;p[16]=1;p[18]=1;p[0]=bad_hot?16:0;p[48]=p[49]=p[50]=255;}
    }
    return p;
}
static unsigned GrNewCursor(int w,int h,int hx,int hy,unsigned fg,unsigned bg,
                            const GR_BITMAP *f,const GR_BITMAP *b) {
    (void)hx;(void)hy;(void)fg;(void)bg;(void)f;(void)b;
    assert((w==16&&h==16)||(w==1&&h==1)); new_calls++;
    return fail_new ? 0 : 100+new_calls;
}
static void GrSetWindowCursor(unsigned window,unsigned cursor) { (void)window;last_cursor=cursor; }
'''
render = r'''
typedef unsigned GR_SIZE;
static unsigned gdi_gc,paint_color,draw_calls,screen[32*32];
static void gdi_usebg(int use){assert(!use);}
static void gdi_fg(unsigned c){paint_color=c;}
static void GrBitmap(unsigned wid,unsigned gc,int x,int y,unsigned w,unsigned h,const GR_BITMAP *bits){
 unsigned i,j;(void)wid;(void)gc;draw_calls++;
 for(j=0;j<h;j++)for(i=0;i<w;i++)if(bits[j*((w+15)/16)+(i>>4)]&(0x8000u>>(i&15)))screen[(y+j)*32+x+i]=paint_color;
}
'''
checks = r'''
static u16 load(u16 instance,u16 id,const char *name) {
    args[0]=id; args[1]=name ? 1 : 0; args[2]=instance; guest_name=name;
    return u_LoadCursor(0,0);
}
static void reset(void) {
    memset(cursors,0,sizeof(cursors)); memset(cur_std,0,sizeof(cur_std));
    new_calls=fail_new=short_data=missing=resource_reads=group_reads=bad_hot=bad_header=odd_height=0;
}
int main(void) {
    u16 arrow, wait, cross, handles[4], count, h;
    int i,j;
    reset();
    arrow=load(0,0x7f00,0); wait=load(0,0x7f02,0); cross=load(0,0x7f03,0);
    assert(arrow&&wait&&cross&&new_calls==2);
    for(i=0;i<4;i++) { handles[i]=load(1,6+i,0); assert(handles[i]); }
    assert(new_calls==6&&resource_reads==4);
    for(j=0;j<500;j++) {
        for(i=0;i<4;i++) assert(load(1,6+i,0)==handles[i]);
        assert(load(0,0x7f00,0)==arrow&&load(0,0x7f02,0)==wait&&load(0,0x7f03,0)==cross);
    }
    assert(load(1,106,0)==handles[0]);
    assert(load(1,0,"alias")==handles[0]);
    assert(new_calls==6&&resource_reads==4);
    cursor_census(&count); assert(count==6);
    wins[0].used=1; wins[0].frame=1; wins[0].client=2;
    args[0]=handles[2]; u_SetCursor(0,0); assert(last_cursor==cur_id(handles[2])&&last_cursor);
    h=load(1,10,0); assert(h&&h!=arrow&&new_calls==7);
    /* A full handle table must not leak either a resource or standard cursor. */
    assert(load(1,11,0)==arrow&&new_calls==7);
    assert(load(1,6,0)==handles[0]);
    reset();
    for(i=1;i<=MAXCUR;i++) assert(load(1,i,0));
    assert(!load(0,0x7f02,0)&&new_calls==MAXCUR);
    reset();
    arrow=load(0,0x7f00,0); short_data=1;
    assert(load(1,6,0)==arrow&&!new_calls); short_data=0;
    missing=1; assert(load(1,6,0)==arrow&&!new_calls); missing=0;
    fail_new=1; assert(load(1,6,0)==arrow); fail_new=0;
    h=load(1,6,0); assert(h&&h!=arrow&&new_calls==2);
    assert(load(1,6,0)==h&&new_calls==2);
    cursor_census(&count); assert(count==1);
    reset(); fail_new=1; assert(!load(0,0x7f02,0)); fail_new=0;
    assert(load(0,0x7f02,0)&&new_calls==2); cursor_census(&count); assert(count==1);
    reset(); arrow=load(0,0x7f00,0);bad_hot=1;assert(load(1,6,0)==arrow&&!new_calls);
    bad_hot=0;bad_header=1;assert(load(1,6,0)==arrow&&!new_calls);
    bad_header=0;odd_height=1;assert(load(1,6,0)==arrow&&!new_calls);
    /* Both resource DIB headers and supported depths use the same real draw
     * path. Transparent AND pixels have zero XOR, retaining their backdrop. */
    {u8 icon[160];unsigned header,depth,pal,entry,pixels,length,j;struct dib_info info;struct dib_memory memory;u32 mask;
     for(header=12;header<=40;header+=28)for(depth=1;depth<=4;depth+=3){
      memset(icon,0,sizeof(icon));entry=header==12?3:4;pal=1u<<depth;pixels=header+pal*entry;length=pixels+16;
      icon[0]=header;icon[4]=2;
      if(header==12){icon[6]=4;icon[8]=1;icon[10]=depth;}
      else {icon[8]=4;icon[12]=1;icon[14]=depth;}
      if(depth==1)icon[header+entry]=icon[header+entry+1]=icon[header+entry+2]=255;
      else icon[header+entry+1]=255;
      icon[pixels+4]=depth==1?0x80:0x10;icon[pixels+12]=0x40;
      memory.data=icon;memory.length=length;
      assert(dib_icon_open(&info,dib_read_memory,&memory,length,&mask)&&info.width==2&&info.height==2&&mask==pixels+8);
      for(j=0;j<32*32;j++)screen[j]=0xff0000;
      draw_calls=0;assert(!gdi_draw_icon(1,0,0,icon,length)&&draw_calls==2);
      assert(screen[0]==(depth==1?0xffffff:0xff00)&&screen[1]==0xff0000&&screen[32]==0&&screen[33]==0);
      draw_calls=0;assert(gdi_draw_icon(1,0,0,icon,length-1)==-1&&!draw_calls);
      if(header==40){
       icon[14]=32;assert(gdi_draw_icon(1,0,0,icon,length)==-1&&!draw_calls);icon[14]=depth;
       icon[6]=1;assert(gdi_draw_icon(1,0,0,icon,length)==-1&&!draw_calls);icon[6]=0;
       icon[8]=5;assert(gdi_draw_icon(1,0,0,icon,length)==-1&&!draw_calls);icon[8]=4;
      }
     }
     /* A wide 4-bpp XOR plane has a different stride from its 1-bpp mask. */
     memset(icon,0,sizeof(icon));icon[0]=40;icon[4]=32;icon[8]=4;icon[12]=1;icon[14]=4;
     memory.data=icon;memory.length=144;
     assert(dib_icon_open(&info,dib_read_memory,&memory,144,&mask)&&info.stride==16&&mask==136);
     assert(!dib_open(&info,dib_read_memory,&memory,144)); /* not four full XOR rows */
     draw_calls=0;assert(!gdi_draw_icon(1,0,0,icon,144)&&draw_calls==1);
     /* Reject an undeclared palette index atomically, before any request. */
     memset(icon,0,sizeof(icon));icon[0]=40;icon[4]=2;icon[8]=4;icon[12]=1;icon[14]=4;icon[32]=1;icon[44]=0xf0;
     draw_calls=0;assert(gdi_draw_icon(1,0,0,icon,60)==-1&&!draw_calls);
    }
    puts("Shared icon/cursor DIB decoding, masked drawing, cursor reuse and failure checks passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c=pathlib.Path(tmp)/'cursor.c'; exe=pathlib.Path(tmp)/'cursor'
    c.write_text(setup+dib_header+dib_source+source+render+icon_source+checks)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Wno-unused-parameter',
                    '-fsanitize=address,undefined','-fno-pie','-no-pie',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
