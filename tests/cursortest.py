#!/usr/bin/env python3
"""Exercise actual cursor loading with bounded synthetic Win16 resources."""
import os
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'src/cursor.c').read_text().split('/* ---------------------------------------------------------------- accelerators */')[0]
source = source.replace('#include "wapi.h"', '')
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
static int new_calls, fail_new, short_data, missing, resource_reads, group_reads;
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
        if(!short_data) { p[8]=16; p[12]=32; p[18]=1; }
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
checks = r'''
static u16 load(u16 instance,u16 id,const char *name) {
    args[0]=id; args[1]=name ? 1 : 0; args[2]=instance; guest_name=name;
    return u_LoadCursor(0,0);
}
static void reset(void) {
    memset(cursors,0,sizeof(cursors)); memset(cur_std,0,sizeof(cur_std));
    new_calls=fail_new=short_data=missing=resource_reads=group_reads=0;
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
    puts("Cursor resource reuse, aliases, tool cycling, capacity and failure rollback passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c=pathlib.Path(tmp)/'cursor.c'; exe=pathlib.Path(tmp)/'cursor'
    c.write_text(setup+source+checks)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Wno-unused-parameter',
                    '-fsanitize=address,undefined','-fno-pie','-no-pie',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
