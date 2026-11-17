#!/usr/bin/env python3
"""Exercise the real single-line Win16 edit dispatcher and Pascal API wrappers."""
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
static void paint_control(struct wnd*w){(void)w;paints++;}
static u32 win_send(struct wnd*w,u16 m,u16 p,u32 l){assert(w==wins);assert(m==WM_COMMAND&&p==106&&l==0x3000104UL);notifications++;return 0;}
'''
code=pre+''.join(func(n) for n in ('win_get','find_ctl','control_message','u_GetDlgItemText','u_SendDlgItemMessage'))
code+='''int main(void){struct wnd*w=&wins[1];unsigned i;
 wins[0].used=1;wins[0].hwnd=HWND_BASE;
 w->used=1;w->ctl=CTL_EDIT;w->hwnd=HWND_BASE+4;w->parent=HWND_BASE;w->id=106;w->edit_limit=99;w->visible=1;
 strcpy((char*)host_ptr(0x2000,0,20),"C:\\\\OLD.BMP");
 assert(control_message(w,0xc,0,0x20000000UL)==1);
 assert(control_message(w,0xe,0,0)==10);
 /* Win16 EM_SETSEL has endpoints in lParam, unlike Win32. */
 control_message(w,0x401,0,0xffff0000UL);
 assert(control_message(w,0x400,0,0)==0x000a0000UL);
 for(i=0;i<9;i++)control_message(w,WM_CHAR,"COLOR.BMP"[i],0);
 assert(!strcmp(w->title,"COLOR.BMP"));assert(notifications==9);
 control_message(w,WM_KEYDOWN,0x24,0);control_message(w,WM_KEYDOWN,0x2e,0);
 assert(!strcmp(w->title,"OLOR.BMP"));control_message(w,WM_CHAR,'C',0);
 control_message(w,WM_KEYDOWN,0x23,0);control_message(w,WM_KEYDOWN,0x2e,0);assert(!strcmp(w->title,"COLOR.BMP"));control_message(w,WM_CHAR,8,0);
 assert(!strcmp(w->title,"COLOR.BM"));control_message(w,WM_CHAR,'P',0);
 key_mods=1;control_message(w,WM_KEYDOWN,0x25,0);control_message(w,WM_CHAR,'X',0);key_mods=0;
 assert(!strcmp(w->title,"COLOR.BMX"));
 control_message(w,0x401,0,0xffff0000UL);control_message(w,WM_CHAR,'Z',0);
 assert(!strcmp(w->title,"Z"));control_message(w,0x415,2,0);
 control_message(w,WM_CHAR,'Z',0);control_message(w,WM_CHAR,'Z',0);assert(!strcmp(w->title,"ZZ"));
 assert(control_message(w,0xd,2,0x20000020UL)==1);assert(!strcmp((char*)host_ptr(0x2000,32,2),"Z"));
 assert(control_message(w,0xd,0,0)==0);assert(control_message(w,0xd,2,0x2000ffffUL)==0);
 assert(control_message(w,0xc,0,0xfffffff0UL)==0);
 wr16(0x1000,0,10);wr16(0x1000,2,32);wr16(0x1000,4,0x2000);wr16(0x1000,6,106);wr16(0x1000,8,HWND_BASE);
 assert(u_GetDlgItemText(0x1000,0)==2);assert(!strcmp((char*)host_ptr(0x2000,32,3),"ZZ"));
 wr32(0x1000,0,0xffff0000UL);wr16(0x1000,4,0);wr16(0x1000,6,0x401);wr16(0x1000,8,106);wr16(0x1000,10,HWND_BASE);
 u_SendDlgItemMessage(0x1000,0);assert(control_message(w,0x400,0,0)==0x20000UL);
 wr16(0x1000,8,999);assert(!u_SendDlgItemMessage(0x1000,0));
 /* Filling storage and replacing a selection must remain bounded. */
 control_message(w,0x415,0,0);control_message(w,WM_CHAR,'A',0);
 for(i=0;i<200;i++)control_message(w,WM_CHAR,'B',0);
 assert(strlen(w->title)==99);control_message(w,0x401,0,0xffff0000UL);control_message(w,WM_CHAR,'X',0);
 assert(!strcmp(w->title,"X"));assert(paints>0);
 puts("Win16 filename edit selection, input, bounds and API ABI checks passed");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(code)
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
