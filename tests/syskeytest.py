#!/usr/bin/env python3
"""Actual USER key-event, translation and default paths with a focused MDI child."""
from pathlib import Path
import re,subprocess,tempfile,os
root=Path(__file__).resolve().parents[1];u=(root/'src/user.c').read_text()
def fn(name):
 m=re.search(r'^(?:static )?(?:u32|u16|void) '+name+r'\([^;]*?\)\n\{',u,re.M);assert m,name
 a=u.index('{',m.start());b=a+1;n=1
 while n:n+=(u[b]=='{')-(u[b]=='}');b+=1
 return u[m.start():b]+'\n'
pre=r"""
#include "w16.h"
typedef unsigned GR_KEY;
typedef struct {int type;struct{unsigned wid,ch,modifiers,scancode;}keystroke;} GR_EVENT;
struct wnd{u16 hwnd,parent,hmenu;u32 style;};
#define MWKMOD_ALT 1
#define GR_EVENT_TYPE_KEY_DOWN 1
#define GR_EVENT_TYPE_KEY_UP 2
enum{MWKEY_LEFT=256,MWKEY_UP,MWKEY_RIGHT,MWKEY_DOWN,MWKEY_DELETE,MWKEY_INSERT,MWKEY_HOME,MWKEY_END,MWKEY_PAGEUP,MWKEY_PAGEDOWN,MWKEY_F1=300,MWKEY_F10=309,MWKEY_F12=311};
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#define QSIZE 24
struct qmsg {u16 hwnd,msg,wp;u32 lp;i16 x,y;u32 time;u8 ch;};
static struct qmsg queue[QSIZE];static int qhead,qcount,kcnext,keyup_pending,saw_real_keyup;
static struct{u32 time;u8 ch;}keychars[8];static u32 msgclock,keyup_lp;
static u16 keyup_hwnd,keyup_vk,f10_default,key_mods;
static u16 args[2];static u8 ram[256];
#undef AW
#define AW(n) args[(n)/2]
u8 *host_ptr(u16 s,u16 o,u16 n){assert(s==1&&(unsigned)o+n<=sizeof(ram));return ram+o;}
static struct wnd nodes[3];static int menus,systems,closes,intercept,callbacks;static u16 lastchar;
int opt_verbose;
void wlog(const char *f,...){(void)f;}
struct wnd *win_get(u16 h){return h>=1&&h<=3?nodes+h-1:0;}
static struct wnd *win_by_nx(unsigned h,int *client){*client=1;return win_get(h);}
static int blocked(struct wnd*w){(void)w;return 0;}
static int post(u16 h,u16 m,u16 p,u32 l,int x,int y){struct qmsg*q;assert(qcount<QSIZE);q=&queue[(qhead+qcount++)%QSIZE];memset(q,0,sizeof(*q));q->hwnd=h;q->msg=m;q->wp=p;q->lp=l;q->x=x;q->y=y;q->time=++msgclock;return 1;}
int menu_key(struct wnd*w,GR_KEY ch,u32 mods){assert(w==nodes&&w->hmenu==9);assert((mods&MWKMOD_ALT)||ch==MWKEY_F10);menus++;lastchar=ch;return 1;}
int menu_sys_open(struct wnd*w,int keyboard,u32 hilite){assert(w==nodes&&keyboard&&!hilite);systems++;return 1;}
static u32 default_system_key(struct wnd*,u16,u16,u32);
u32 win_send(struct wnd*w,u16 msg,u16 key,u32 lp){callbacks++;if(intercept&&w==nodes+2)return 77;if(msg==WM_SYSCOMMAND){assert(w==nodes&&key==SC_CLOSE);closes++;return 1;}return default_system_key(w,msg,key,lp);}
"""
keys=u[u.index('    case GR_EVENT_TYPE_KEY_DOWN: {'):u.index('    case GR_EVENT_TYPE_BUTTON_DOWN:',u.index('    case GR_EVENT_TYPE_KEY_DOWN: {'))]
pre='\n'.join(x for x in (root/'src/wapi.h').read_text().splitlines() if x.startswith(('#define WM_','#define WS_','#define SC_')))+'\n'+pre
code=pre+fn('vk_from_key')+fn('dequeue')+fn('u_TranslateMessage')+fn('default_system_key')
code+='static void event(GR_EVENT *ev){struct wnd*w;int isclient;switch(ev->type){'+keys+'}}\n'
code+=r"""
static void key(int type,int ch,unsigned mods){GR_EVENT e;memset(&e,0,sizeof(e));e.type=type;e.keystroke.wid=3;e.keystroke.ch=ch;e.keystroke.modifiers=mods;e.keystroke.scancode=17;event(&e);}
static void translate(struct qmsg*m){wr16(1,64,m->hwnd);wr16(1,66,m->msg);wr16(1,68,m->wp);wr32(1,70,m->lp);wr32(1,74,m->time);args[0]=64;args[1]=1;assert(u_TranslateMessage(0,0));}
int main(void){struct qmsg m,c;
 nodes[0].hwnd=1;nodes[0].hmenu=9;nodes[1].hwnd=2;nodes[1].style=WS_CHILD;nodes[1].parent=1;nodes[2].hwnd=3;nodes[2].style=WS_CHILD;nodes[2].parent=2;
 key(GR_EVENT_TYPE_KEY_DOWN,'w',MWKMOD_ALT);dequeue(&m);assert(m.hwnd==3&&m.msg==WM_SYSKEYDOWN&&m.wp=='W'&&(m.lp&0x20000000UL)&&!menus);
 translate(&m);dequeue(&c);assert(c.msg==WM_SYSCHAR&&c.wp=='w'&&c.lp==m.lp);
 assert(win_send(nodes+2,c.msg,c.wp,c.lp)&&menus==1&&lastchar=='w'&&callbacks==3);
 intercept=1;assert(win_send(nodes+2,c.msg,c.wp,c.lp)==77&&menus==1);intercept=0;
 key(GR_EVENT_TYPE_KEY_UP,'w',0);dequeue(&m);assert(m.msg==WM_SYSKEYUP&&(m.lp&0xe0000000UL)==0xe0000000UL);
 key(GR_EVENT_TYPE_KEY_DOWN,'a',0);dequeue(&m);assert(m.msg==WM_KEYDOWN&&!(m.lp&0x20000000UL));translate(&m);dequeue(&c);assert(c.msg==WM_CHAR&&c.wp=='a');
 key(GR_EVENT_TYPE_KEY_UP,'a',0);dequeue(&m);assert(m.msg==WM_KEYUP&&!(m.lp&0x20000000UL));
 saw_real_keyup=0;key(GR_EVENT_TYPE_KEY_DOWN,'f',MWKMOD_ALT);dequeue(&m);key(GR_EVENT_TYPE_KEY_DOWN,'a',0);dequeue(&m);assert(m.msg==WM_SYSKEYUP&&(m.lp&0x20000000UL));dequeue(&m);assert(m.msg==WM_KEYDOWN);keyup_pending=0;
 assert(win_send(nodes+2,WM_SYSCHAR,' ',0x20000001UL)&&systems==1);
 win_send(nodes+2,WM_SYSKEYDOWN,0x73,0x20000001UL);assert(closes==1);
 win_send(nodes+2,WM_SYSKEYDOWN,0x73,1);assert(closes==1);
 win_send(nodes+2,WM_SYSKEYUP,0x79,0xc0000001UL);assert(menus==1); /* handled down never armed default */
 win_send(nodes+2,WM_SYSKEYDOWN,0x79,1);assert(menus==1);win_send(nodes+2,WM_SYSKEYUP,0x79,0xc0000001UL);assert(menus==2&&lastchar==MWKEY_F10);
 puts("USER system keys: context, real/synthetic releases, translation, MDI default routing, interception and normal characters passed");
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(code)
 subprocess.run(['cc','-DW16_HOST','-Wall','-Wextra','-Wno-unused-parameter','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
