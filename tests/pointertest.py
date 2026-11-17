#!/usr/bin/env python3
"""Real pointer API and event clamping with a live-position backend double."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
code=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint16_t u16;typedef uint32_t u32;typedef int16_t i16;
typedef struct{int cols,rows,xpos,ypos;}GR_SCREEN_INFO;
typedef struct{int x,y,rootx,rooty,wid,buttons;}Mouse;
typedef struct{int type;Mouse button,mouse;}GR_EVENT;
#define GR_EVENT_TYPE_BUTTON_DOWN 1
#define GR_EVENT_TYPE_BUTTON_UP 2
#define GR_EVENT_TYPE_MOUSE_POSITION 3
static GR_SCREEN_INFO screen={640,480,300,200};static int warps;
static u16 args[2],ram[32768];
#define AW(n) args[(n)/2]
static u16 rd16(u16 s,u16 o){assert(s==1&&!(o&1));return ram[o/2];}
static void wr16(u16 s,u16 o,u16 v){assert(s==1&&!(o&1));ram[o/2]=v;}
static void GrGetScreenInfo(GR_SCREEN_INFO*s){*s=screen;}
static void GrMoveCursor(int x,int y){screen.xpos=x;screen.ypos=y;warps++;}
'''
code+=(root/'src/pointer.c').read_text().replace('#include "wapi.h"','')
code+=r'''
int main(void){
 GR_EVENT e={0};int n;
 args[1]=1;ram[0]=100;ram[1]=50;ram[2]=200;ram[3]=150;
 assert(u_ClipCursor(0,0));assert(screen.xpos==199&&screen.ypos==149&&warps==1);
 args[1]=400;args[0]=1;assert(u_SetCursorPos(0,0));assert(screen.xpos==199&&screen.ypos==50);
 args[1]=1;args[0]=8;assert(u_GetCursorPos(0,0));assert(ram[4]==199&&ram[5]==50);
 e.type=GR_EVENT_TYPE_MOUSE_POSITION;e.mouse.rootx=50;e.mouse.rooty=400;e.mouse.x=40;e.mouse.y=380;e.mouse.wid=7;e.mouse.buttons=1;
 pointer_event(&e);assert(e.mouse.rootx==100&&e.mouse.rooty==149&&e.mouse.x==90&&e.mouse.y==129);
 assert(e.mouse.wid==7&&e.mouse.buttons==1&&screen.xpos==100&&screen.ypos==149);
 e.type=GR_EVENT_TYPE_BUTTON_UP;e.button.rootx=205;e.button.rooty=45;e.button.x=105;e.button.y=5;e.button.wid=9;
 pointer_event(&e);assert(e.button.rootx==199&&e.button.rooty==50&&e.button.x==99&&e.button.y==10&&e.button.wid==9);
 n=warps;pointer_event(&e);assert(warps==n);
 args[0]=0;ram[2]=99;assert(!u_ClipCursor(0,0));assert(clip_on&&clip_right==200);
 args[0]=65530;assert(!u_ClipCursor(0,0));assert(clip_on);
 args[0]=65534;assert(!u_GetCursorPos(0,0));
 args[0]=args[1]=0;assert(u_ClipCursor(0,0));assert(!clip_on);
 args[0]=600;args[1]=700;assert(u_SetCursorPos(0,0));assert(screen.xpos==639&&screen.ypos==479);
 args[1]=1;args[0]=0;ram[0]=(u16)-10;ram[1]=(u16)-5;ram[2]=800;ram[3]=900;
 assert(u_ClipCursor(0,0));assert(clip_left==0&&clip_top==0&&clip_right==640&&clip_bottom==480);
 puts("ClipCursor/Get/SetCursorPos: exclusive bounds, backend warps, events, reset and invalid spans passed");return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
