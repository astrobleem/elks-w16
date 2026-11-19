#!/usr/bin/env python3
import pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
u=(root/'src/user.c').read_text(); h=(root/'src/wapi.h').read_text()
def func(name):
    m=re.search(r'^(?:static )?(?:struct wnd \*|u32 |u16 |void |int )'+name+r'\([^;{}]*\)\s*\{',u,re.M)
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
typedef struct {unsigned wid,parent,child,sibling;} GR_WINDOW_INFO;
static GR_WINDOW_INFO native_info[700];
static void GrGetWindowInfo(unsigned id,GR_WINDOW_INFO *i){assert(id<700);*i=native_info[id];}
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
#define CLASS_MDICLIENT (-2)
#define WM_MDICREATE 0x0220
#define WM_MDIDESTROY 0x0221
#define WM_MDIACTIVATE 0x0222
#define WM_MDINEXT 0x0224
#define WM_MDIGETACTIVE 0x0229
#define SC_CLOSE 0xf060
#define MWKMOD_CTRL 1
#define MWKMOD_SHIFT 2
#define MWKMOD_ALT 4
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
''' + func('find_class').replace('    if (!seg) {','    if (!seg && !off)return 0;\n    if (!seg) {',1) + '''
static int menu_load(struct wnd*w,const char*n,u16 id){(void)w;(void)n;(void)id;abort();}
static int menu_attach(struct wnd*w,u16 h){(void)w;(void)h;abort();}
u32 brush_color(u16 h,int *n){(void)h;*n=0;return 0;}
GR_COLOR rgb_to_gr(u32 c){return c;}
static int fail_extra;
static void *extra_alloc(size_t n,size_t sz){return fail_extra?NULL:calloc(n,sz);}
#define calloc extra_alloc
static unsigned nextid=100, frame_parent, fail_frame,fail_client,destroyed[64],ndestroy;
static int createdx,createdy,movex,movey,moves;
static unsigned maps[400],unmaps[400],sizes[400][2],msgcount;
static u16 lastmsg;static u32 lastlp;
unsigned GrNewWindowEx(unsigned p,const char*t,unsigned parent,int x,int y,int w,int h,unsigned c){(void)p;(void)t;(void)x;(void)y;(void)w;(void)h;(void)c;frame_parent=parent;createdx=x;createdy=y;return fail_frame?0:nextid++;}
unsigned GrNewWindow(unsigned p,int x,int y,int w,int h,int b,unsigned c,unsigned bc){assert(w>0&&h>0);(void)p;(void)x;(void)y;(void)w;(void)h;(void)b;(void)c;(void)bc;return fail_client?0:nextid++;}
void GrMoveWindow(unsigned w,int x,int y){(void)w;movex=x;movey=y;moves++;}
static void native_unlink(unsigned w){unsigned p=native_info[w].parent,*n;
 if(!p)return;
 n=&native_info[p].child;while(*n&&*n!=w)n=&native_info[*n].sibling;
 if(*n==w)*n=native_info[w].sibling;}
void GrDestroyWindow(unsigned w){native_unlink(w);destroyed[ndestroy++]=w;}
void GrSelectEvents(unsigned w,unsigned m){(void)w;(void)m;}
static int classcalls;void cursor_class(unsigned f,unsigned c,u16 h){(void)f;(void)c;(void)h;classcalls++;}
void cursor_attach(unsigned f,unsigned c){(void)f;(void)c;}
void GrMapWindow(unsigned w){maps[w]++;}
void GrUnmapWindow(unsigned w){unmaps[w]++;}
void GrResizeWindow(unsigned w,int x,int y){assert(x>0&&y>0);sizes[w][0]=x;sizes[w][1]=y;}
static unsigned raised,lowered;
void GrRaiseWindow(unsigned w){unsigned p=native_info[w].parent;raised=w;
 if(p){native_unlink(w);native_info[w].sibling=native_info[p].child;native_info[p].child=w;}}void GrLowerWindow(unsigned w){unsigned p=native_info[w].parent,*n;lowered=w;
 if(p){native_unlink(w);n=&native_info[p].child;while(*n)n=&native_info[*n].sibling;*n=w;native_info[w].sibling=0;}}
void GrSetFocus(unsigned w){nx_focus=w;}

static u32 mdi_message(struct wnd*,u16,u16,u32);
static int valid_coords(u16,u16,u16);
u32 u_DefWindowProc(u16,u16);
struct dc { int paint; struct wnd *w; };
static struct dc *dc_get(u16 h){(void)h;return NULL;}
static void gdi_fill(unsigned w,unsigned c,int x,int y,int wd,int ht){(void)w;(void)c;(void)x;(void)y;(void)wd;(void)ht;}
static u16 key_mods,mdi_serial;
static int mdi_default_commands=1;
u32 u_DefMDIChildProc(u16,u16);
static struct wnd *mdi_child(struct wnd*);
static int mdi_activate(struct wnd*,u16);
static int mdi_eligible(struct wnd*,struct wnd*);
static void mdi_release(struct wnd*);
static u16 set_focus(struct wnd*);
static u16 activation_serial;
static u16 reenter_on,reenter_to;static unsigned activation_events;
u32 u_SetActiveWindow(u16,u16);
static int modal_block;
static int blocked(struct wnd*w){(void)w;return modal_block;}
static unsigned create_fail,extra_creates,extra_destroys;
static u16 senthw,sentm,sentp;static u32 sentl,createarg,mdi_pair;
static u16 mdi_deactivated,mdi_activated,mdi_reenter_on,mdi_reenter_to;
u32 win_send(struct wnd*w,u16 m,u16 p,u32 l){
if(m==6||m==0x86)activation_events++;
if(m==WM_CREATE)createarg=l;
if(m==WM_MDIACTIVATE){mdi_pair=l;if(p)mdi_activated=w->hwnd;else mdi_deactivated=w->hwnd;
 if(!p&&w->hwnd==mdi_reenter_on){mdi_reenter_on=0;mdi_activate(&wins[(w->parent-HWND_BASE)/4],mdi_reenter_to);}}
if(m==6&&!p&&w->hwnd==reenter_on){reenter_on=0;wr16(0x1000,0,reenter_to);u_SetActiveWindow(0x1000,0);}
if(w->cls==CLASS_MDICLIENT)return mdi_message(w,m,p,l);
if(w->extra&&m==WM_CREATE){assert(w->extra[0]==0&&w->extra[1]==0);w->extra[0]=0x34;w->extra[1]=0x12;extra_creates++;}
if(w->extra&&m==WM_NCDESTROY){assert(w->extra[0]);extra_destroys++;}
if(create_fail&&m==WM_CREATE)return 0xffffffffUL;
if(mdi_default_commands && (m==WM_CLOSE||m==WM_SYSCOMMAND) && w->parent && wins[(w->parent-HWND_BASE)/4].cls==CLASS_MDICLIENT) {
 wr32(0x1000,0x400,l);wr16(0x1000,0x404,p);wr16(0x1000,0x406,m);wr16(0x1000,0x408,w->hwnd);return u_DefMDIChildProc(0x1000,0x400);}
senthw=w->hwnd;sentm=lastmsg=m;sentp=p;sentl=lastlp=l;msgcount++;return m==0x400?0xface1234UL:0;}
void scrollbar_forget(u16 w){(void)w;}\nvoid caret_destroy(u16 w){(void)w;} void menu_close_for(u16 w){(void)w;} void kill_timers(u16 w){(void)w;}
void menu_free(u16 w){(void)w;}void purge(u16 w){(void)w;}
static void filelist_clear(struct filelist*l){(void)l;}
static u32 control_message(struct wnd*w,u16 m,u16 p,u32 l){return win_send(w,m,p,l);}
static u16 capture_hwnd,focus_hwnd,keyup_hwnd,keyup_pending;
'''
code=pre+''.join(func(n) for n in ('dialog_slots_available','win_get','u_IsWindow','u_IsWindowVisible','u_GetParent','window_relative','u_GetWindow','window_word','u_GetWindowWord','u_SetWindowWord','u_GetWindowLong','u_SetWindowLong','u_SetClassWord','client_origin','top_window','win_by_nx','window_message','u_SendMessage','u_GetActiveWindow','valid_coords','convert_point','u_ClientToScreen','u_ScreenToClient','u_GetWindowRect','border_of','win_invalidate_all','u_CreateWindow','move_window','u_MoveWindow','u_SetWindowPos','set_focus','mdi_child','mdi_eligible','mdi_successor','mdi_activate','mdi_release','u_SetFocus','u_SetActiveWindow','u_ShowWindow','free_controls','destroy_window','mdi_message','u_DefWindowProc','u_DefMDIChildProc','u_TranslateMDISysAccel'))
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
 classes[0].cbwndextra=3;
 strcpy(classes[0].menu,"must-not-load");
 setup(0x40800003UL,HWND_BASE,-1,117,284,15);id=u_CreateWindow(0x1000,0);assert(id==HWND_BASE+4);assert(frame_parent==77);assert(createdx==0&&createdy==117&&moves==1&&movex==-1&&movey==117);
 assert(wins[1].parent==HWND_BASE&&wins[1].w==284&&wins[1].h==15);assert(wins[1].cx==1&&wins[1].cy==1&&wins[1].ch==13);
 client_origin(&wins[1],&x,&y);assert(x==12&&y==168);assert(top_window(&wins[1])==&wins[0]);
 setup(WS_CHILD,id,3,4,10,10);assert(u_CreateWindow(0x1000,0)==HWND_BASE+8);client_origin(&wins[2],&x,&y);assert(x==15&&y==172);
 /* Group state survives WM_CREATE, is private to each child, and permits odd offsets. */
 wr16(0x1000,2,id);wr16(0x1000,0,0);assert(u_GetWindowWord(0x1000,0)==0x1234);
 wr16(0x1000,4,id);wr16(0x1000,2,1);wr16(0x1000,0,0xabcd);assert(u_SetWindowWord(0x1000,0)==0x12);
 wr16(0x1000,2,id);wr16(0x1000,0,1);assert(u_GetWindowWord(0x1000,0)==0xabcd);
 wr16(0x1000,2,HWND_BASE+8);wr16(0x1000,0,0);assert(u_GetWindowWord(0x1000,0)==0x1234);
 wr16(0x1000,4,id);wr16(0x1000,2,2);wr16(0x1000,0,0xffff);assert(!u_SetWindowWord(0x1000,0));assert(wins[1].extra[2]==0xab);
 wr16(0x1000,2,(u16)-12);wr16(0x1000,0,19);assert(!u_SetWindowWord(0x1000,0));assert(wins[1].id==19);
 wr16(0x1000,2,id);wr16(0x1000,0,(u16)-12);assert(u_GetWindowWord(0x1000,0)==19);
 wr16(0x1000,0,(u16)-8);assert(u_GetWindowWord(0x1000,0)==HWND_BASE);
 assert(!u_GetWindowWord(0x1000,0xffff)&&!u_SetWindowWord(0x1000,0xffff));
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
 setup(WS_CHILD,id,0,0,10,10);fail_extra=1;frame=nextid;
 assert(!u_CreateWindow(0x1000,0)&&nextid==frame&&!wins[3].used&&!wins[3].extra);fail_extra=0;
 setup(WS_CHILD,id,0,0,10,10);fail_frame=1;assert(!u_CreateWindow(0x1000,0));assert(!wins[3].used);fail_frame=0;assert(!wins[3].extra);
 fail_client=1;frame=nextid;assert(!u_CreateWindow(0x1000,0));assert(!wins[3].used&&destroyed[ndestroy-1]==frame);fail_client=0;assert(!wins[3].extra);
 create_fail=1;setup(WS_CHILD,id,0,0,10,10);assert(!u_CreateWindow(0x1000,0)&&!wins[3].used&&!wins[3].extra);create_fail=0;
 capture_hwnd=wins[2].hwnd;focus_hwnd=wins[1].hwnd;destroy_window(&wins[1]);assert(!wins[1].used&&!wins[2].used&&wins[0].used);assert(!capture_hwnd&&!focus_hwnd);assert(!wins[1].extra&&!wins[2].extra&&extra_destroys==2);
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
 /* A real built-in MDI client and a decorated registered group child. */
 {u16 mdi,group,second,third,pane;struct wnd *c,*g,*g2,*g3;
  strcpy((char*)host_ptr(0x2000,0,16),"mDiClIeNt");
  wr16(0x2000,32,0);wr16(0x2000,34,1000);
  setup(WS_CHILD|WS_VISIBLE,HWND_BASE,0,0,280,150);
  wr16(0x1000,26,0);wr16(0x1000,28,0x2000);wr32(0x1000,0,0x20000020UL);
  mdi=u_CreateWindow(0x1000,0);assert(mdi);c=win_get(mdi);
  assert(c->cls==CLASS_MDICLIENT&&c->parent==HWND_BASE&&frame_parent==77&&c->defid==1000&&!c->extra);
  fzero(0x2000,64,26);wr32(0x2000,64,0xc000);wr16(0x2000,72,0x1234);
  wr16(0x2000,74,5);wr16(0x2000,76,6);wr16(0x2000,78,180);wr16(0x2000,80,100);
  wr32(0x2000,86,0xbeef);
  group=win_send(c,WM_MDICREATE,0,0x20000040UL);assert(group);g=win_get(group);
  assert(g->parent==mdi&&frame_parent==c->client&&g->id==1000&&g->visible);
  assert((g->style&WS_CAPTION)==WS_CAPTION&&g->x==5&&g->y==6&&g->cw==174);
  assert(rd32(w16_app_ss,(u16)createarg)==0x20000040UL); /* CREATESTRUCT.lpCreateParams */
  assert(g->extra[0]==0x34&&g->extra[1]==0x12);
  assert(win_send(c,WM_MDIGETACTIVE,0,0)==group&&mdi_activated==group);
  second=win_send(c,WM_MDICREATE,0,0x20000040UL);assert(second&&second!=group);g2=win_get(second);
  assert(c->mdi_active==second);
  native_info[c->client].wid=c->client;native_info[c->client].child=g2->frame;
  native_info[g2->frame].wid=g2->frame;native_info[g2->frame].parent=c->client;native_info[g2->frame].sibling=g->frame;
  native_info[g->frame].wid=g->frame;native_info[g->frame].parent=c->client;
  third=win_send(c,WM_MDICREATE,0,0x20000040UL);assert(third);g3=win_get(third);
  native_info[g3->frame].wid=g3->frame;native_info[g3->frame].parent=c->client;
  native_info[g3->frame].sibling=native_info[c->client].child;native_info[c->client].child=g3->frame;
  win_send(c,WM_MDIACTIVATE,group,0);
  win_send(c,WM_MDINEXT,group,0);assert(c->mdi_active==third);
  win_send(c,WM_MDINEXT,third,0);assert(c->mdi_active==second);
  win_send(c,WM_MDINEXT,second,0);assert(c->mdi_active==group);
  win_send(c,WM_MDINEXT,group,1);assert(c->mdi_active==second);
  win_send(c,WM_MDINEXT,second,1);assert(c->mdi_active==third);
  win_send(c,WM_MDINEXT,third,1);assert(c->mdi_active==group);
  win_send(c,WM_MDIDESTROY,third,0);assert(!win_get(third)&&c->mdi_active==group);
  win_send(c,WM_MDIACTIVATE,second,0);
  win_send(c,WM_MDIACTIVATE,group,0);
  assert(c->mdi_active==group&&mdi_deactivated==second&&mdi_activated==group&&mdi_pair==(((u32)second<<16)|group));
  win_send(c,WM_MDINEXT,group,0);assert(c->mdi_active==second);
  win_send(c,WM_MDINEXT,second,1);assert(c->mdi_active==group);
  win_send(c,WM_MDIACTIVATE,HWND_BASE,0);assert(c->mdi_active==group);
  win_send(c,WM_MDINEXT,HWND_BASE,0);assert(c->mdi_active==group);
  wr16(0x1000,0,0);wr16(0x1000,2,group);u_ShowWindow(0x1000,0);
  assert(c->mdi_active==second&&!g->visible);
  wr16(0x1000,0,8);u_ShowWindow(0x1000,0);assert(g->visible&&c->mdi_active==second);
  setup(WS_CHILD|WS_VISIBLE,group,1,1,20,20);pane=u_CreateWindow(0x1000,0);assert(pane&&c->mdi_active==second);
  nx_focus=g2->client;focus_hwnd=second;wr16(0x1000,0,pane);u_SetFocus(0x1000,0);
  assert(c->mdi_active==group&&nx_focus==win_get(pane)->client&&senthw==pane&&sentm==WM_SETFOCUS&&sentp==group);
  wr16(0x1000,0,1);wr16(0x1000,2,group);u_ShowWindow(0x1000,0);assert(c->mdi_active==group);
  wr16(0x1000,0,0x97);wr16(0x1000,12,group);u_SetWindowPos(0x1000,0);assert(!g->visible&&c->mdi_active==second);
  wr16(0x1000,0,0x57);u_SetWindowPos(0x1000,0);assert(g->visible&&c->mdi_active==second);
  wr16(0x1000,6,group);wr16(0x1000,4,(u16)-16);wr32(0x1000,0,g->style|WS_DISABLED);u_SetWindowLong(0x1000,0);
  win_send(c,WM_MDIACTIVATE,group,0);assert(c->mdi_active==second);g->style&=~WS_DISABLED;
  nx_focus=77;focus_hwnd=HWND_BASE;wr16(0x1000,0,group);u_SetFocus(0x1000,0);
  assert(c->mdi_active==group&&nx_focus==g->client);
  wr16(0x1000,6,group);wr16(0x1000,4,(u16)-16);wr32(0x1000,0,g->style|WS_DISABLED);u_SetWindowLong(0x1000,0);
  assert(c->mdi_active==second);g->style&=~WS_DISABLED;wr16(0x1000,0,group);u_SetFocus(0x1000,0);assert(c->mdi_active==group);
  mdi_reenter_on=group;mdi_reenter_to=group;win_send(c,WM_MDIACTIVATE,second,0);assert(c->mdi_active==group);
  win_send(c,WM_MDIDESTROY,HWND_BASE,0);assert(wins[0].used&&g->used);
  /* Use frame-addressed MSGs, as Progman's main message loop does. */
  wr16(0x1000,0,0x300);wr16(0x1000,2,0x2000);wr16(0x1000,4,mdi);
  wr16(0x2000,0x300,HWND_BASE);wr16(0x2000,0x302,WM_KEYDOWN);wr16(0x2000,0x304,9);
  key_mods=0;assert(!u_TranslateMDISysAccel(0x1000,0));
  key_mods=MWKMOD_CTRL|MWKMOD_ALT;assert(!u_TranslateMDISysAccel(0x1000,0));
  key_mods=MWKMOD_CTRL;assert(u_TranslateMDISysAccel(0x1000,0)&&c->mdi_active==second);
  key_mods|=MWKMOD_SHIFT;wr16(0x2000,0x304,0x75);assert(u_TranslateMDISysAccel(0x1000,0)&&c->mdi_active==group);
  wr16(0x2000,0x304,0x76);assert(!u_TranslateMDISysAccel(0x1000,0));
  wr16(0x2000,0x304,0x73);wr16(0x2000,0x302,WM_KEYUP);assert(!u_TranslateMDISysAccel(0x1000,0));
  wr16(0x2000,0x302,WM_SYSKEYDOWN);key_mods=MWKMOD_CTRL;mdi_default_commands=0;
  assert(u_TranslateMDISysAccel(0x1000,0)&&win_get(group)&&c->mdi_active==group); /* application veto */
  mdi_default_commands=1;assert(u_TranslateMDISysAccel(0x1000,0)&&!win_get(group)&&c->mdi_active==second&&!g->extra);
  assert(!u_TranslateMDISysAccel(0x1000,0xffff));
  wr32(0x1000,0,0);wr16(0x1000,4,0);wr16(0x1000,6,WM_CLOSE);
  wr16(0x1000,8,second);u_DefMDIChildProc(0x1000,0);
  assert(!win_get(second)&&!c->mdi_active&&win_get(mdi)&&wins[0].used);
  destroy_window(c);assert(!win_get(mdi)&&wins[0].used);
 }
 /* Native sibling order deliberately differs from allocation order and
  * contains more foreign windows than the W16 table has slots. */
 {u16 popup,owned,child,other=HWND_BASE+16;unsigned pf,cf;int j;
  setup(WS_POPUP|WS_VISIBLE,HWND_BASE,1,1,100,60);popup=u_CreateWindow(0x1000,0);assert(popup);pf=win_get(popup)->frame;
  setup(0,HWND_BASE,1,1,100,60);owned=u_CreateWindow(0x1000,0);assert(owned);
  setup(WS_CHILD|WS_VISIBLE,HWND_BASE,1,1,10,10);child=u_CreateWindow(0x1000,0);assert(child);cf=win_get(child)->frame;
  wins[4]=wins[0];wins[4].hwnd=other;wins[4].frame=500;wins[4].client=501;wins[4].visible=1;
  native_info[1].wid=1;native_info[1].child=2;
  for(j=2;j<46;j++){native_info[j].wid=j;native_info[j].sibling=j+1;}
  native_info[45].sibling=500;native_info[500].wid=500;native_info[500].sibling=76;
  native_info[76].wid=76;native_info[76].sibling=pf;native_info[pf].wid=pf;
  native_info[77].wid=77;native_info[77].child=600;native_info[600].wid=600;native_info[600].sibling=cf;native_info[cf].wid=cf;
  wr16(0x1000,2,HWND_BASE);wr16(0x1000,0,0);assert(u_GetWindow(0x1000,0)==other);
  wr16(0x1000,0,1);assert(u_GetWindow(0x1000,0)==popup);
  wr16(0x1000,0,2);assert(u_GetWindow(0x1000,0)==popup);
  wr16(0x1000,0,3);assert(u_GetWindow(0x1000,0)==other);
  wr16(0x1000,2,popup);wr16(0x1000,0,2);assert(!u_GetWindow(0x1000,0));
  wr16(0x1000,0,4);assert(u_GetWindow(0x1000,0)==HWND_BASE);
  wr16(0x1000,2,HWND_BASE);wr16(0x1000,0,5);assert(u_GetWindow(0x1000,0)==child);
  wr16(0x1000,2,DESKTOP_HWND);assert(u_GetWindow(0x1000,0)==other);
  wr16(0x1000,0,popup);assert(u_GetParent(0x1000,0)==HWND_BASE);
  wr16(0x1000,0,owned);assert(!u_GetParent(0x1000,0));
  wr16(0x1000,2,owned);wr16(0x1000,0,4);assert(u_GetWindow(0x1000,0)==HWND_BASE);
  wr16(0x1000,0,child);assert(u_GetParent(0x1000,0)==HWND_BASE);
  assert(!u_GetWindow(0x1000,0xffff)&&!u_GetParent(0x1000,0xffff));
  nx_focus=77;focus_hwnd=HWND_BASE;activation_events=0;
  wr16(0x1000,0,other);assert(u_SetActiveWindow(0x1000,0)==HWND_BASE);
  assert(u_GetActiveWindow(0,0)==other&&raised==500&&activation_events==4);
  wr16(0x1000,0,child);assert(!u_SetActiveWindow(0x1000,0)&&u_GetActiveWindow(0,0)==other);
  win_get(popup)->style|=WS_DISABLED;wr16(0x1000,0,popup);assert(!u_SetActiveWindow(0x1000,0));win_get(popup)->style&=~WS_DISABLED;
  modal_block=1;wr16(0x1000,0,HWND_BASE);assert(!u_SetActiveWindow(0x1000,0));modal_block=0;
  nx_focus=650;focus_hwnd=0;assert(!u_SetActiveWindow(0x1000,0)&&u_GetActiveWindow(0,0)==HWND_BASE);
  reenter_on=HWND_BASE;reenter_to=popup;wr16(0x1000,0,other);
  assert(u_SetActiveWindow(0x1000,0)==HWND_BASE&&u_GetActiveWindow(0,0)==popup);
  wr16(0x1000,0,0);assert(u_SetActiveWindow(0x1000,0)==popup&&!u_GetActiveWindow(0,0));
  wins[5]=wins[4];wins[5].hwnd=HWND_BASE+20;wins[5].owner=HWND_BASE;wins[5].isdialog=1;wins[5].frame=510;
  destroy_window(&wins[0]);assert(wins[5].used&&wins[5].ended);destroy_window(&wins[5]);
  assert(!win_get(popup)&&!win_get(owned)&&!win_get(child)&&win_get(other));destroy_window(win_get(other));
 }
 puts("Window tree, native order, ownership, activation and MDI lifecycle checks passed");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(code)
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
