#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[1]
s=(root/'src/user.c').read_text()
def extract(name):
 a=s.index('u32 '+name+'(');e=s.index('{',a)+1;depth=1
 while depth:
  depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
code=r'''
#include "w16.h"
#include <assert.h>
#include <stdio.h>
#define DESKTOP_HWND 1
#define MAXWIN 2
static int scr_w=640,scr_h=480;
struct wnd {int cw,ch,cx,cy,w,h,visible;u16 parent;};
static struct wnd *win_get(u16 h){(void)h;return 0;}
static void client_origin(struct wnd*w,int*x,int*y){(void)w;*x=*y=0;}
static int valid_coords(u16 s,u16 o,u16 n){return s && (u32)o+n<=65536UL;}
static u16 dc_create(struct wnd*w,int paint){assert(!w&&!paint);return 0x600;}
static u8 ram[65536];
u8 *host_ptr(u16 s,u16 o,u16 n){assert(s==0x1000&&(u32)o+n<=65536);return ram+o;}
'''
code+='\n'.join(extract(n) for n in ['u_GetDesktopHwnd','u_IsWindow','u_IsWindowVisible','u_GetClientRect','u_GetWindowRect','u_GetDC'])
code+=r'''
int main(void){
 assert(u_GetDesktopHwnd(0,0)==1);wr16(0x1000,0,1);
 assert(u_IsWindow(0x1000,0)&&u_IsWindowVisible(0x1000,0));
 assert(u_GetDC(0x1000,0)==0x600);
 wr16(0x1000,0,100);wr16(0x1000,2,0x1000);wr16(0x1000,4,1);
 u_GetWindowRect(0x1000,0);assert(rd16(0x1000,100)==0&&rd16(0x1000,102)==0&&rd16(0x1000,104)==640&&rd16(0x1000,106)==480);
 scr_w=320;scr_h=200;u_GetClientRect(0x1000,0);
 assert(rd16(0x1000,104)==320&&rd16(0x1000,106)==200);
 puts("Desktop stable handle, screen geometry, visibility and root DC passed");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(code)
 subprocess.run(['cc','-DW16_HOST','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
