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
void caret_destroy(u16 w){(void)w;} void menu_close_for(u16 w){(void)w;} void kill_timers(u16 w){(void)w;}
void menu_free(u16 w){(void)w;}void purge(u16 w){(void)w;}
static u16 capture_hwnd,focus_hwnd,keyup_hwnd,keyup_pending;
'''
pre+='static void placement_moved(struct wnd*w){(void)w;}\n'
pre+='void scrollbar_draw(struct wnd*w){(void)w;}\n'
code=pre+''.join(func(n) for n in ('win_get','scroll_window','scroll_redraw','u_SetScrollRange','u_SetScrollPos','u_GetScrollPos','scroll_word','u_GetScrollRange','move_children','u_ScrollWindow'))
code+="""
static void range(u16 h,u16 bar,i16 min,i16 max){
 wr16(0x1000,0,1);wr16(0x1000,2,max);wr16(0x1000,4,min);wr16(0x1000,6,bar);wr16(0x1000,8,h);u_SetScrollRange(0x1000,0);
}
static u32 pos(u16 h,u16 bar,i16 p){
 wr16(0x1000,0,0);wr16(0x1000,2,p);wr16(0x1000,4,bar);wr16(0x1000,6,h);return u_SetScrollPos(0x1000,0);
}
int main(void){
 u16 h=HWND_BASE;
 wins[0].used=1;wins[0].hwnd=h;
 assert(pos(h,0,55)==0);assert(wins[0].scroll_pos[0]==55);
 range(h,0,-10,10);assert(wins[0].scroll_pos[0]==10);
 assert(pos(h,0,-20)==10);assert(wins[0].scroll_pos[0]==-10);
 range(h,1,2,300);assert(wins[0].scroll_pos[1]==2);assert(wins[0].scroll_pos[0]==-10);
 assert(pos(h,1,800)==2);assert(wins[0].scroll_pos[1]==300);
 range(h,1,20,10);assert(wins[0].scroll_max[1]==300);
 range(h,1,-32768,32767);assert(wins[0].scroll_max[1]==300);
 range(h,1,5,5);assert(wins[0].scroll_pos[1]==5);
 assert(!pos(h,2,60));assert(!pos(999,0,5));
 wr16(0x1000,0,0);wr16(0x1000,2,h);assert((i16)u_GetScrollPos(0x1000,0)==-10);
 wr16(0x1000,0,4);wr16(0x1000,2,0x2000);wr16(0x1000,4,0);wr16(0x1000,6,0x2000);wr16(0x1000,8,0);wr16(0x1000,10,h);
 u_GetScrollRange(0x1000,0);assert((i16)rd16(0x2000,0)==-10 && rd16(0x2000,4)==10);
 wr16(0x1000,0,0xffff);wr16(0x2000,0,77);u_GetScrollRange(0x1000,0);assert(rd16(0x2000,0)==77);
 memset(&wins[0],0,sizeof(wins[0]));wins[0].used=1;wins[0].hwnd=h;
 assert(!pos(h,0,1));assert(wins[0].scroll_min[0]==0&&wins[0].scroll_max[0]==100);
 assert(!u_GetScrollPos(0x1000,0xfffd));assert(!u_SetScrollRange(0x1000,0xfff7));assert(!u_GetScrollRange(0x1000,0xfff5));
 /* ScrollWindow invalidates the real client, and moves children only for null rectangle. */
 wins[0].cw=100;wins[0].ch=80;
 wins[1].used=1;wins[1].hwnd=h+4;wins[1].parent=h;wins[1].x=10;wins[1].y=20;wins[1].frame=8;
 memset(host_ptr(0x1000,0,14),0,14);wr16(0x1000,8,(u16)-3);wr16(0x1000,10,4);wr16(0x1000,12,h);
 u_ScrollWindow(0x1000,0);assert(wins[0].invalid&&wins[1].x==14&&wins[1].y==17);
 wins[0].invalid=0;wr16(0x1000,4,100);wr16(0x1000,6,0x1000);
 u_ScrollWindow(0x1000,0);assert(wins[0].invalid&&wins[1].x==14);
 wins[0].invalid=0;wr16(0x1000,4,0xfffa);
 u_ScrollWindow(0x1000,0);assert(!wins[0].invalid);
 puts("Scrollbar signed ranges, independent axes, bounds, reset and Pascal ABI passed");return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
 p=pathlib.Path(td);(p/'test.c').write_text(code)
 subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
