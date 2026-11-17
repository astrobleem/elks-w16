#!/usr/bin/env python3
import pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
u=(root/'src/user.c').read_text(); h=(root/'src/wapi.h').read_text()
def func(name):
    m=re.search(r'^(?:static )?(?:struct wnd \*|u32 |void |int )'+name+r'\(',u,re.M)
    assert m,name
    start=u.index('{',m.start()); level=1; p=start+1
    while level:
        level+=(u[p]=='{')-(u[p]=='}');p+=1
    return u[m.start():p]+'\n'
pre='''#include "w16.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef unsigned GR_WINDOW_ID, GR_COLOR;
#define GR_RGB(a,b,c) 0
#define GR_ROOT_WINDOW_ID 1
#define GR_WM_PROPS_NODECORATE 1
#define GR_WM_PROPS_NOMOVE 2
#define GR_EVENT_MASK_EXPOSURE 1
#define GR_EVENT_MASK_BUTTON_DOWN 2
#define GR_EVENT_MASK_CLOSE_REQ 4
#define GR_EVENT_MASK_KEY_DOWN 8
#define GR_EVENT_MASK_KEY_UP 16
#define GR_EVENT_MASK_BUTTON_UP 32
#define GR_EVENT_MASK_MOUSE_POSITION 64
#define CAPH 14
#define MENUH 16
#define wlog(...) ((void)0)
#define vlog(...) ((void)0)
'''
pre+='\n'.join(l for l in h.splitlines() if l.startswith('#define WS_') or l.startswith('#define WM_') or l.startswith('#define MAXWIN') or l.startswith('#define MAXCLASS') or l.startswith('#define HWND_BASE'))+'\n'
for name in ('wclass','wnd'):
    pre+=re.search(r'struct '+name+r' \{.*?\n\};',h,re.S)[0]+'\n'
pre+='''struct wnd wins[MAXWIN]; static struct wclass classes[MAXCLASS];
static int scr_w=320,scr_h=200;
u8 host_mem[0x110000]; u16 w16_app_sp=200,w16_app_ss=0x1000;
u8 *host_ptr(u16 s,u16 o,u16 n) { assert((u32)o+n<=65536);return host_mem+LIN(s,o); }
void fzero(u16 s,u16 o,u16 n){memset(host_ptr(s,o,n),0,n);}
u16 fstrncpy_in(char*d,u16 s,u16 o,u16 n){strncpy(d,(char*)host_ptr(s,o,1),n);d[n-1]=0;return strlen(d);}
static int find_class(u16 o,u16 s){(void)o;(void)s;return 0;}
static int menu_load(struct wnd*w,const char*n,u16 id){(void)w;(void)n;(void)id;abort();}
u32 brush_color(u16 h,int *n){(void)h;*n=0;return 0;}
GR_COLOR rgb_to_gr(u32 c){return c;}
static unsigned nextid=100, frame_parent, fail_frame,fail_client,destroyed[64],ndestroy;
static int createdx,createdy,movex,movey,moves;
unsigned GrNewWindowEx(unsigned p,const char*t,unsigned parent,int x,int y,int w,int h,unsigned c){(void)p;(void)t;(void)x;(void)y;(void)w;(void)h;(void)c;frame_parent=parent;createdx=x;createdy=y;return fail_frame?0:nextid++;}
unsigned GrNewWindow(unsigned p,int x,int y,int w,int h,int b,unsigned c,unsigned bc){(void)p;(void)x;(void)y;(void)w;(void)h;(void)b;(void)c;(void)bc;return fail_client?0:nextid++;}
void GrMoveWindow(unsigned w,int x,int y){(void)w;movex=x;movey=y;moves++;}
void GrDestroyWindow(unsigned w){destroyed[ndestroy++]=w;}
void GrSelectEvents(unsigned w,unsigned m){(void)w;(void)m;}
void cursor_attach(unsigned f,unsigned c){(void)f;(void)c;}
void GrMapWindow(unsigned w){(void)w;}
void win_invalidate_all(struct wnd*w,int e){w->invalid=1;w->erase=e;}
u32 win_send(struct wnd*w,u16 m,u16 p,u32 l){(void)w;(void)m;(void)p;(void)l;return 0;}
void menu_close_for(u16 w){(void)w;} void kill_timers(u16 w){(void)w;}
void menu_free(u16 w){(void)w;}void purge(u16 w){(void)w;}
static u16 capture_hwnd,focus_hwnd,keyup_hwnd,keyup_pending;
'''
code=pre+''.join(func(n) for n in ('win_get','client_origin','top_window','border_of','u_CreateWindow','free_controls','destroy_window'))
code+='''
static void setup(u32 style,u16 parent,int x,int y,int w,int h){
 memset(host_ptr(0x1000,0,32),0,32);wr16(0x1000,8,parent);wr16(0x1000,10,h);wr16(0x1000,12,w);wr16(0x1000,14,y);wr16(0x1000,16,x);wr32(0x1000,18,style);
}
int main(void){u16 id;int x,y;unsigned frame;
 wins[0].used=1;wins[0].hwnd=HWND_BASE;wins[0].client=77;wins[0].frame=76;wins[0].x=10;wins[0].y=20;wins[0].cx=2;wins[0].cy=30;
 strcpy(classes[0].menu,"must-not-load");
 setup(0x40800003UL,HWND_BASE,-1,117,284,15);id=u_CreateWindow(0x1000,0);assert(id==HWND_BASE+4);assert(frame_parent==77);assert(createdx==0&&createdy==117&&moves==1&&movex==-1&&movey==117);
 assert(wins[1].parent==HWND_BASE&&wins[1].w==284&&wins[1].h==15);assert(wins[1].cx==1&&wins[1].cy==1&&wins[1].ch==13);
 client_origin(&wins[1],&x,&y);assert(x==12&&y==168);assert(top_window(&wins[1])==&wins[0]);
 setup(WS_CHILD,id,3,4,10,10);assert(u_CreateWindow(0x1000,0)==HWND_BASE+8);client_origin(&wins[2],&x,&y);assert(x==15&&y==172);
 setup(WS_CHILD,999,0,0,10,10);assert(!u_CreateWindow(0x1000,0));
 setup(WS_CHILD|WS_CAPTION,id,0,0,10,10);assert(!u_CreateWindow(0x1000,0));
 setup(WS_CHILD,id,0,0,0,10);assert(!u_CreateWindow(0x1000,0));
 setup(WS_CHILD,id,0,0,10,10);fail_frame=1;assert(!u_CreateWindow(0x1000,0));assert(!wins[3].used);fail_frame=0;
 fail_client=1;frame=nextid;assert(!u_CreateWindow(0x1000,0));assert(!wins[3].used&&destroyed[ndestroy-1]==frame);fail_client=0;
 capture_hwnd=wins[2].hwnd;focus_hwnd=wins[1].hwnd;destroy_window(&wins[1]);assert(!wins[1].used&&!wins[2].used&&wins[0].used);assert(!capture_hwnd&&!focus_hwnd);
 classes[0].menu[0]=0;
 setup(0,0,16,12,-32768,0);id=u_CreateWindow(0x1000,0);assert(win_get(id)->w==288&&win_get(id)->h==168);destroy_window(win_get(id));
 scr_w=640;scr_h=480;setup(0,0,16,12,-32768,0);id=u_CreateWindow(0x1000,0);assert(win_get(id)->w==624&&win_get(id)->h==448&&win_get(id)->x==16);destroy_window(win_get(id));
 setup(WS_THICKFRAME,0,-32768,0,-32768,0);id=u_CreateWindow(0x1000,0);assert(win_get(id)->x==0&&win_get(id)->w==640&&win_get(id)->cw==634&&win_get(id)->cw>615);destroy_window(win_get(id));
 setup(0,0,16,12,100,75);id=u_CreateWindow(0x1000,0);assert(win_get(id)->w==100&&win_get(id)->h==75);destroy_window(win_get(id));
 scr_w=16;scr_h=16;setup(0,0,0,0,-32768,0);id=u_CreateWindow(0x1000,0);assert(win_get(id)->w==40&&win_get(id)->h==30);destroy_window(win_get(id));
 puts("Registered child hierarchy, geometry, failure and teardown checks passed");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(code)
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
