#!/usr/bin/env python3
"""Exercise real Win16 integer dialog APIs and 16-bit range checks."""
import pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
u=(root/'src/user.c').read_text();h=(root/'src/wapi.h').read_text()
def func(name):
    m=re.search(r'^(?:static )?(?:struct wnd \*|u32 |void |int )'+name+r'\([^;]*?\)\n\{',u,re.M)
    assert m,name
    start=u.index('{',m.start());level=1;p=start+1
    while level:
        level+=(u[p]=='{')-(u[p]=='}');p+=1
    return u[m.start():p]+'\n'
pre='''#include "w16.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef unsigned GR_WINDOW_ID;
#define MWKMOD_SHIFT 1
'''
pre+='\n'.join(l for l in h.splitlines() if l.startswith(('#define WM_','#define MAXWIN','#define HWND_BASE','#define CTL_')))+'\n'
pre+=re.search(r'struct wnd \{.*?\n\};',h,re.S)[0]+'\n'
pre+='''struct wnd wins[MAXWIN];static unsigned key_mods,paints,notifications;
u8 host_mem[0x110000];
u8 *host_ptr(u16 s,u16 o,u16 n){assert((u32)o+n<=65536UL);assert(LIN(s,o)+n<=0x100000UL);return host_mem+LIN(s,o);}
void fcopy_out(u16 s,u16 o,const void *p,u16 n){memcpy(host_ptr(s,o,n),p,n);}
u16 fstrncpy_in(char*d,u16 s,u16 o,u16 n){strncpy(d,(char*)host_ptr(s,o,n),n);d[n-1]=0;return strlen(d);}
static u32 list_message(struct wnd*w,u16 m,u16 p,u32 l){(void)w;(void)m;(void)p;(void)l;return 0;}
static void paint_control(struct wnd*w){(void)w;paints++;}
static u32 win_send(struct wnd*w,u16 m,u16 p,u32 l){assert(w==wins);assert(m==WM_COMMAND&&p==106&&l==0x3000104UL);notifications++;return 0;}
'''
code=pre+''.join(func(n) for n in ('win_get','find_ctl','u_SetDlgItemInt','u_GetDlgItemInt'))
code+='''int main(void){
 struct wnd*w=&wins[1];u16 st=0x1000,out=0x2000;
 w->used=1;w->ctl=CTL_EDIT;w->hwnd=HWND_BASE+4;w->parent=HWND_BASE;w->id=106;w->visible=1;
 wr16(st,0,1);wr16(st,2,0x8000);wr16(st,4,106);wr16(st,6,HWND_BASE);
 assert(u_SetDlgItemInt(st,0)==1&&!strcmp(w->title,"-32768")&&paints==1);
 wr16(st,0,0);wr16(st,2,65535);assert(u_SetDlgItemInt(st,0)&&!strcmp(w->title,"65535"));
 wr16(st,0,0);wr16(st,2,0);wr16(st,4,out);wr16(st,6,106);wr16(st,8,HWND_BASE);
 assert(u_GetDlgItemInt(st,0)==65535&&rd16(out,0)==1);
 strcpy(w->title,"65536");assert(!u_GetDlgItemInt(st,0)&&!rd16(out,0));
 wr16(st,0,1);strcpy(w->title,"-32768");assert(u_GetDlgItemInt(st,0)==32768&&rd16(out,0));
 strcpy(w->title,"32768");assert(!u_GetDlgItemInt(st,0)&&!rd16(out,0));
 strcpy(w->title,"-32769");assert(!u_GetDlgItemInt(st,0)&&!rd16(out,0));
 strcpy(w->title,"  +42x");assert(u_GetDlgItemInt(st,0)==42&&rd16(out,0));
 strcpy(w->title,"0");assert(!u_GetDlgItemInt(st,0)&&rd16(out,0));
 strcpy(w->title,"-");assert(!u_GetDlgItemInt(st,0)&&!rd16(out,0));
 wr16(st,0,0);strcpy(w->title,"-1");assert(!u_GetDlgItemInt(st,0)&&!rd16(out,0));
 wr16(st,4,0);strcpy(w->title,"17");assert(u_GetDlgItemInt(st,0)==17);
 wr16(st,4,out);wr16(st,2,65535);assert(!u_GetDlgItemInt(st,0));
 wr16(st,2,0);wr16(st,6,999);assert(!u_GetDlgItemInt(st,0)&&!rd16(out,0));
 puts("Win16 dialog integer bounds, sign, translated BOOL and Pascal ABI passed");return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(code)
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
