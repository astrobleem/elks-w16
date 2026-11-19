#!/usr/bin/env python3
"""Exercise the real single-line Win16 edit dispatcher and Pascal API wrappers."""
import pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
u=(root/'src/user.c').read_text();h=(root/'src/wapi.h').read_text()
def func(name):
    m=re.search(r'^(?:static )?(?:struct wnd \*|u32 |u16 |void |int )'+name+r'\([^;]*?\)\n\{',u,re.M)
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
#define CLASS_MDICLIENT (-2)
#define WS_MINIMIZE 0x20000000UL
'''
pre+='\n'.join(l for l in h.splitlines() if l.startswith(('#define WM_','#define MAXCLASS','#define MAXWIN','#define HWND_BASE','#define CTL_')))+'\n'
pre+=re.search(r'struct wnd \{.*?\n\};',h,re.S)[0]+'\n'
pre+='''struct wnd wins[MAXWIN];static unsigned key_mods,paints,notifications;
u8 host_mem[0x110000];
u8 *host_ptr(u16 s,u16 o,u16 n){assert((u32)o+n<=65536UL);assert(LIN(s,o)+n<=0x100000UL);return host_mem+LIN(s,o);}
void fcopy_out(u16 s,u16 o,const void *p,u16 n){memcpy(host_ptr(s,o,n),p,n);}
u16 fstrncpy_in(char*d,u16 s,u16 o,u16 n){strncpy(d,(char*)host_ptr(s,o,n),n);d[n-1]=0;return strlen(d);}
static u32 list_message(struct wnd*w,u16 m,u16 p,u32 l){(void)w;(void)m;(void)p;(void)l;return 0;}
static void paint_control(struct wnd*w){(void)w;paints++;}
static u32 win_send(struct wnd*w,u16 m,u16 p,u32 l){if(m==WM_SETFOCUS||m==0x0008){assert(w==wins);return 0;}assert(w==wins);assert(m==WM_COMMAND&&p==106&&l==0x3000104UL);notifications++;return 0;}
'''
pre+=re.search(r'struct wclass \{.*?\n\};',h,re.S)[0]+'\n'
pre+='''static struct wclass classes[MAXCLASS];static u16 focus_hwnd;static int raised,focused,modal;
#define WS_DISABLED 0x08000000UL
static void GrRaiseWindow(int w){raised=w;}
static void GrSetFocus(int w){focused=w;}
static int blocked(struct wnd*w){(void)w;return modal;}
static int mdi_activate(struct wnd*w,u16 h){(void)w;(void)h;assert(0);return 0;}
'''
code=pre+''.join(func(n) for n in ('win_get','find_class','window_name_matches','u_FindWindow','mdi_child','set_focus','u_BringWindowToTop'))
code+='''int main(void){
 u16 st=0x1000,str=0x2000;struct wnd*w=&wins[0];
 classes[0].used=1;strcpy(classes[0].name,"Mine");
 w->used=1;w->hwnd=HWND_BASE;w->visible=1;w->frame=10;w->client=11;strcpy(w->title,"Game");
 wins[1]=*w;wins[1].hwnd=HWND_BASE+4;wins[1].parent=HWND_BASE;
 memset(host_mem,0,sizeof host_mem);
 strcpy((char*)host_ptr(str,0,5),"mine");strcpy((char*)host_ptr(str,10,5),"gAmE");
 wr16(st,0,10);wr16(st,2,str);wr16(st,4,0);wr16(st,6,str);
 assert(u_FindWindow(st,0)==HWND_BASE);
 wr16(st,6,0);wr16(st,4,0xc000);assert(u_FindWindow(st,0)==HWND_BASE);
 wr16(st,4,0xc001);assert(!u_FindWindow(st,0));
 wr16(st,4,0);wr16(st,0,0);wr16(st,2,0);assert(u_FindWindow(st,0)==HWND_BASE);
 w->used=0;assert(!u_FindWindow(st,0));w->used=1;
 wr16(st,6,str);wr16(st,4,65535);wr8(str,65535,'M');assert(!u_FindWindow(st,0));
 wr16(st,0,HWND_BASE);assert(u_BringWindowToTop(st,0)==1&&raised==10&&focused==11&&focus_hwnd==HWND_BASE);
 focused=0;modal=1;assert(u_BringWindowToTop(st,0)==1&&focused==0);modal=0;
 w->visible=0;assert(u_BringWindowToTop(st,0)==1&&focused==0);w->visible=1;
 w->style=WS_DISABLED;assert(u_BringWindowToTop(st,0)==1&&focused==0);
 wr16(st,0,0xffff);assert(!u_BringWindowToTop(st,0));
 puts("FindWindow exact name/atom/wildcard bounds and BringWindowToTop focus guards passed");return 0;
}'''
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(code)
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
