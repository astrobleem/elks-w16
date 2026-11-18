#!/usr/bin/env python3
import pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
u=(root/'src/user.c').read_text(); h=(root/'src/wapi.h').read_text()
def func(name):
    m=re.search(r'^(?:static )?(?:struct wnd \*|u32 |u16 |void |int )'+name+r'\(',u,re.M)
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
#define DESKTOP_HWND 1
#define MENUH 16\n#define SCROLLBAR_SIZE 16
#define wlog(...) ((void)0)
#define vlog(...) ((void)0)
'''
pre+='\n'.join(l for l in h.splitlines() if l.startswith('#define WS_') or l.startswith('#define WM_') or l.startswith('#define MAXWIN') or l.startswith('#define MAXCLASS') or l.startswith('#define HWND_BASE'))+'\n'
for name in ('wclass','wnd'):
    pre+=re.search(r'struct '+name+r' \{.*?\n\};',h,re.S)[0]+'\n'
pre+='''struct wnd wins[MAXWIN]; static struct wclass classes[MAXCLASS];
static GR_WINDOW_ID nx_focus;
static GR_WINDOW_ID GrGetFocus(void){return nx_focus;}
static int scr_w=320,scr_h=200;
u8 host_mem[0x110000]; u16 w16_app_sp=200,w16_app_ss=0x1000;
u8 *host_ptr(u16 s,u16 o,u16 n) { assert((u32)o+n<=65536);return host_mem+LIN(s,o); }
void fzero(u16 s,u16 o,u16 n){memset(host_ptr(s,o,n),0,n);}
u16 fstrncpy_in(char*d,u16 s,u16 o,u16 n){strncpy(d,(char*)host_ptr(s,o,1),n);d[n-1]=0;return strlen(d);}
static int find_class(u16 o,u16 s){(void)o;(void)s;return 0;}
static int menu_load(struct wnd*w,const char*n,u16 id){(void)w;(void)n;(void)id;abort();}
static int menu_attach(struct wnd*w,u16 h){(void)w;(void)h;abort();}
u32 brush_color(u16 h,int *n){(void)h;*n=0;return 0;}
GR_COLOR rgb_to_gr(u32 c){return c;}
static unsigned nextid=100, frame_parent, fail_frame,fail_client,destroyed[64],ndestroy;
static int createdx,createdy,movex,movey,moves;
static unsigned maps[400],unmaps[400],sizes[400][2],msgcount;
static u16 lastmsg;static u32 lastlp;
unsigned GrNewWindowEx(unsigned p,const char*t,unsigned parent,int x,int y,int w,int h,unsigned c){(void)p;(void)t;(void)x;(void)y;(void)w;(void)h;(void)c;frame_parent=parent;createdx=x;createdy=y;return fail_frame?0:nextid++;}
unsigned GrNewWindow(unsigned p,int x,int y,int w,int h,int b,unsigned c,unsigned bc){assert(w>0&&h>0);(void)p;(void)x;(void)y;(void)w;(void)h;(void)b;(void)c;(void)bc;return fail_client?0:nextid++;}
void GrMoveWindow(unsigned w,int x,int y){(void)w;movex=x;movey=y;moves++;}
void GrDestroyWindow(unsigned w){destroyed[ndestroy++]=w;}
void GrSelectEvents(unsigned w,unsigned m){(void)w;(void)m;}
static int classcalls;void cursor_class(unsigned f,unsigned c,u16 h){(void)f;(void)c;(void)h;classcalls++;}
void cursor_attach(unsigned f,unsigned c){(void)f;(void)c;}
void GrMapWindow(unsigned w){maps[w]++;}
void GrUnmapWindow(unsigned w){unmaps[w]++;}
void GrResizeWindow(unsigned w,int x,int y){assert(x>0&&y>0);sizes[w][0]=x;sizes[w][1]=y;}
static unsigned raised,lowered;
void GrRaiseWindow(unsigned w){raised=w;}void GrLowerWindow(unsigned w){lowered=w;}
void GrSetFocus(unsigned w){(void)w;}

static u16 senthw,sentm,sentp;static u32 sentl;
u32 win_send(struct wnd*w,u16 m,u16 p,u32 l){senthw=w->hwnd;sentm=lastmsg=m;sentp=p;sentl=lastlp=l;msgcount++;return m==0x400?0xface1234UL:0;}
void scrollbar_forget(u16 w){(void)w;}\nvoid caret_destroy(u16 w){(void)w;} void menu_close_for(u16 w){(void)w;} void kill_timers(u16 w){(void)w;}
void menu_free(u16 w){(void)w;}void purge(u16 w){(void)w;}
static void filelist_clear(struct filelist*l){(void)l;}
static u32 control_message(struct wnd*w,u16 m,u16 p,u32 l){return win_send(w,m,p,l);}
static u16 capture_hwnd,focus_hwnd,keyup_hwnd,keyup_pending;
'''
code=pre+''.join(func(n) for n in ('dialog_slots_available','win_get','u_IsWindow','u_IsWindowVisible','u_GetWindowLong','u_SetWindowLong','u_SetClassWord','client_origin','top_window','win_by_nx','u_SendMessage','u_GetActiveWindow','valid_coords','convert_point','u_ClientToScreen','u_ScreenToClient','u_GetWindowRect','border_of','win_invalidate_all','u_CreateWindow','move_window','u_MoveWindow','u_SetWindowPos','set_focus','u_ShowWindow','free_controls','destroy_window'))
code+='''
static void setup(u32 style,u16 parent,int x,int y,int w,int h){
 memset(host_ptr(0x1000,0,32),0,32);wr16(0x1000,8,parent);wr16(0x1000,10,h);wr16(0x1000,12,w);wr16(0x1000,14,y);wr16(0x1000,16,x);wr32(0x1000,18,style);
}
int main(void){u16 id;int x,y;unsigned frame;
 /* Five Paintbrush windows, one 17-control Save As, then a 4-control alert. */
 assert(MAXWIN>=5+1+17+1+4);
 {int j;for(j=0;j<23;j++)wins[j].used=1;assert(dialog_slots_available(4));
  for(j=23;j<MAXWIN-4;j++)wins[j].used=1;
  assert(!dialog_slots_available(4));memset(wins,0,sizeof(wins));}
 wins[0].used=1;wins[0].hwnd=HWND_BASE;wins[0].client=77;wins[0].frame=76;wins[0].x=10;wins[0].y=20;wins[0].cx=2;wins[0].cy=30;
 strcpy(classes[0].menu,"must-not-load");
 setup(0x40800003UL,HWND_BASE,-1,117,284,15);id=u_CreateWindow(0x1000,0);assert(id==HWND_BASE+4);assert(frame_parent==77);assert(createdx==0&&createdy==117&&moves==1&&movex==-1&&movey==117);
 assert(wins[1].parent==HWND_BASE&&wins[1].w==284&&wins[1].h==15);assert(wins[1].cx==1&&wins[1].cy==1&&wins[1].ch==13);
 client_origin(&wins[1],&x,&y);assert(x==12&&y==168);assert(top_window(&wins[1])==&wins[0]);
 setup(WS_CHILD,id,3,4,10,10);assert(u_CreateWindow(0x1000,0)==HWND_BASE+8);client_origin(&wins[2],&x,&y);assert(x==15&&y==172);
 wr16(0x1000,8,id);wr16(0x1000,6,0x400);wr16(0x1000,4,0x7192);wr32(0x1000,0,0x12345678);
 assert(u_SendMessage(0x1000,0)==0xface1234UL && senthw==id && sentm==0x400 && sentp==0x7192 && sentl==0x12345678);
 wr16(0x1000,8,0xffff);u_SendMessage(0x1000,0);assert(senthw==HWND_BASE);
 wr16(0x1000,0,id);assert(u_IsWindow(0x1000,0));wins[1].visible=1;
 assert(!u_IsWindowVisible(0x1000,0));wins[0].visible=1;assert(u_IsWindowVisible(0x1000,0));
 wr16(0x1000,0,id+1);assert(!u_IsWindow(0x1000,0)&&!u_IsWindowVisible(0x1000,0));
 {u32 prev=wins[1].style;wr16(0x1000,6,id);wr16(0x1000,4,0xfff0);wr32(0x1000,0,prev|0x300000UL);
 assert(u_SetWindowLong(0x1000,0)==prev);wr16(0x1000,2,id);wr16(0x1000,0,0xfff0);
 assert(u_GetWindowLong(0x1000,0)==(prev|0x300000UL));wins[1].style=prev;}
 classes[0].used=1;classes[0].hcursor=1;wr16(0x1000,4,id);wr16(0x1000,2,(u16)-12);wr16(0x1000,0,7);
 assert(u_SetClassWord(0x1000,0)==1&&classes[0].hcursor==7&&classcalls==3);
 nx_focus=wins[2].client;assert(u_GetActiveWindow(0,0)==HWND_BASE);nx_focus=987;assert(!u_GetActiveWindow(0,0));
 wr16(0x1000,0,16);wr16(0x1000,2,0x2000);wr16(0x1000,4,id);
 u_GetWindowRect(0x1000,0);assert(rd16(0x2000,16)==11&&rd16(0x2000,18)==167);
 assert(rd16(0x2000,20)==295&&rd16(0x2000,22)==182);
 wr16(0x2000,16,(u16)-20);wr16(0x2000,18,3);
 u_ClientToScreen(0x1000,0);assert((i16)rd16(0x2000,16)==-8&&rd16(0x2000,18)==171);
 u_ScreenToClient(0x1000,0);assert((i16)rd16(0x2000,16)==-20&&rd16(0x2000,18)==3);
 wr16(0x1000,0,0xfffa);u_GetWindowRect(0x1000,0);assert(!rd16(0x2000,0xfffa));
 setup(WS_CHILD,999,0,0,10,10);assert(!u_CreateWindow(0x1000,0));
 setup(WS_CHILD|WS_CAPTION,id,0,0,10,10);assert(!u_CreateWindow(0x1000,0));
 setup(WS_CHILD,id,0,0,-1,10);assert(!u_CreateWindow(0x1000,0));
 setup(WS_CHILD,id,0,0,10,10);fail_frame=1;assert(!u_CreateWindow(0x1000,0));assert(!wins[3].used);fail_frame=0;
 fail_client=1;frame=nextid;assert(!u_CreateWindow(0x1000,0));assert(!wins[3].used&&destroyed[ndestroy-1]==frame);fail_client=0;
 capture_hwnd=wins[2].hwnd;focus_hwnd=wins[1].hwnd;destroy_window(&wins[1]);assert(!wins[1].used&&!wins[2].used&&wins[0].used);assert(!capture_hwnd&&!focus_hwnd);
 classes[0].menu[0]=0;
 setup(0,0,16,12,-32768,0);id=u_CreateWindow(0x1000,0);assert(win_get(id)->w==288&&win_get(id)->h==168);destroy_window(win_get(id));
 scr_w=640;scr_h=480;setup(0,0,16,12,-32768,0);id=u_CreateWindow(0x1000,0);assert(win_get(id)->w==624&&win_get(id)->h==448&&win_get(id)->x==16);destroy_window(win_get(id));
 setup(WS_THICKFRAME,0,-32768,0,-32768,0);id=u_CreateWindow(0x1000,0);assert(win_get(id)->x==0&&win_get(id)->w==640&&win_get(id)->cw==634&&win_get(id)->cw>615);destroy_window(win_get(id));
 setup(0,0,16,12,100,75);id=u_CreateWindow(0x1000,0);assert(win_get(id)->w==100&&win_get(id)->h==75);destroy_window(win_get(id));
 scr_w=16;scr_h=16;setup(0,0,0,0,-32768,0);id=u_CreateWindow(0x1000,0);assert(win_get(id)->w==40&&win_get(id)->h==30);destroy_window(win_get(id));

 { struct wnd *z;unsigned client,frame,n;
   setup(WS_CHILD|WS_BORDER|WS_VISIBLE,HWND_BASE,66,2,2,2);
   id=u_CreateWindow(0x1000,0);assert(id);z=win_get(id);client=z->client;frame=z->frame;
   assert(z->cw==0&&z->ch==0&&!z->invalid&&maps[client]==0&&maps[frame]==1);
   wr16(0x1000,0,1);wr16(0x1000,2,id);u_ShowWindow(0x1000,0);assert(maps[client]==0&&!z->invalid);
   wr16(0x1000,0,0);wr16(0x1000,2,102);wr16(0x1000,4,202);
   wr16(0x1000,6,7);wr16(0x1000,8,(u16)-3);wr16(0x1000,10,id);
   assert(u_MoveWindow(0x1000,0));assert(z->x==-3&&z->y==7&&z->cw==200&&z->ch==100);
   assert(maps[client]==1&&sizes[client][0]==200&&sizes[client][1]==100);
   assert(lastmsg==WM_SIZE&&lastlp==0x006400c8UL);
   n=msgcount;assert(u_MoveWindow(0x1000,0));assert(msgcount==n);
   wr16(0x1000,2,2);wr16(0x1000,4,2);assert(u_MoveWindow(0x1000,0));
   assert(z->cw==0&&z->ch==0&&unmaps[client]&&sizes[client][0]==1&&sizes[client][1]==1);
   assert(lastmsg==WM_SIZE&&!lastlp);
   wr16(0x1000,4,(u16)-1);assert(!u_MoveWindow(0x1000,0)&&z->w==2);
   wr16(0x1000,0,0x15);wr16(0x1000,2,999);wr16(0x1000,4,999);
   wr16(0x1000,6,40);wr16(0x1000,8,30);wr16(0x1000,10,0);wr16(0x1000,12,id);
   assert(u_SetWindowPos(0x1000,0)&&z->x==30&&z->y==40&&z->w==2&&z->h==2);
   wr16(0x1000,0,0x97);assert(u_SetWindowPos(0x1000,0)&&!z->visible);
   wr16(0x1000,0,0x53);assert(u_SetWindowPos(0x1000,0)&&z->visible&&raised==z->frame);
   wr16(0x1000,10,1);assert(u_SetWindowPos(0x1000,0)&&lowered==z->frame);
   wr16(0x1000,10,0xffff);assert(!u_SetWindowPos(0x1000,0));
   wr16(0x1000,0,0x20);assert(!u_SetWindowPos(0x1000,0));
   destroy_window(z);
   setup(WS_CHILD|WS_VISIBLE,HWND_BASE,0,0,0,0);id=u_CreateWindow(0x1000,0);
   assert(id);z=win_get(id);assert(!maps[z->client]&&!maps[z->frame]&&!z->invalid);destroy_window(z);
 }
 puts("Registered child hierarchy, geometry, failure and teardown checks passed");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(code)
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
