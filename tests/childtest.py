#!/usr/bin/env python3
import pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
u=(root/'src/user.c').read_text(); h=(root/'src/wapi.h').read_text()
def func(name):
    m=re.search(r'^(?:static )?(?:struct wnd \*|struct placement \*|u32 |u16 |void |int )'+name+r'\([^;{}]*\)\s*\{',u,re.M)
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
typedef unsigned GR_WINDOW_ID, GR_COLOR, GR_CURSOR_ID;
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
pre+='\n'.join(l for l in h.splitlines() if l.startswith('#define ICON_') or l.startswith('#define WS_') or l.startswith('#define WM_') or l.startswith('#define MAXWIN') or l.startswith('#define MAXCLASS') or l.startswith('#define HWND_BASE'))+'\n'
for name in ('wclass','wnd'):
    pre+=re.search(r'struct '+name+r' \{.*?\n\};',h,re.S)[0]+'\n'
pre+=re.search(r'struct placement \{.*?\n\};',u,re.S)[0]+'\n'
pre+=re.search(r'struct drag_event \{.*?\};',h,re.S)[0]+'\n'+re.search(r'struct drag_cursor \{.*?\};',h,re.S)[0]+'\n'
pre+=re.search(r'struct qmsg \{.*?\};',u,re.S)[0]+'\n'
pre+='''static struct placement *placements;
static struct placement *placement_get(struct wnd*,int);
static void placement_moved(struct wnd*);
static void placement_forget(struct wnd*);
static int show_window(struct wnd*,u16);
struct wnd wins[MAXWIN]; static struct wclass classes[MAXCLASS];
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
static unsigned frame_clears;
void GrClearArea(unsigned w,int x,int y,int wd,int ht,int expose){(void)w;(void)x;(void)y;assert(wd>=0&&ht>=0&&expose);frame_clears++;}
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
static int mdi_default_commands=1,query_open=1;
static u16 destroy_on_size;static void destroy_window(struct wnd*);
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
static u16 sysmodal_hwnd,modal_hwnd,modal_owner;
static int actual_blocked(struct wnd*);
static int blocked(struct wnd*w){return modal_block||actual_blocked(w);}
static unsigned create_fail,extra_creates,extra_destroys;
static u16 senthw,sentm,sentp;static u32 sentl,createarg,mdi_pair,mdi_syslp;
static u16 mdi_deactivated,mdi_activated,mdi_reenter_on,mdi_reenter_to;
static u32 drag_message(struct wnd*,u16,u16,u32);
u32 win_send(struct wnd*w,u16 m,u16 p,u32 l){
if(m>=0x22a&&m<=0x22f)return drag_message(w,m,p,l);
if(m==WM_QUERYOPEN)return query_open;
if(m==WM_SIZE&&w->hwnd==destroy_on_size){destroy_on_size=0;destroy_window(w);return 0;}
if(m==WM_SYSCOMMAND)mdi_syslp=l;
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
static void scrollbar_draw(struct wnd*w){(void)w;}
void scrollbar_forget(u16 w){(void)w;}\nvoid caret_destroy(u16 w){(void)w;} void menu_close_for(u16 w){(void)w;} void kill_timers(u16 w){(void)w;}
void menu_free(u16 w){(void)w;}void purge(u16 w){(void)w;}
static void filelist_clear(struct filelist*l){(void)l;}
static u32 control_message(struct wnd*w,u16 m,u16 p,u32 l){return win_send(w,m,p,l);}
static u16 capture_hwnd,focus_hwnd,keyup_hwnd,keyup_pending;
'''
pre+='''
u8 w16_scratch[W16_SCRATCH];
static u16 allocation_next=0x4000;static int allocation_live,fail_global;
int seg_alloc(u16 n,u16*s){if(fail_global)return -1;*s=allocation_next;allocation_next+=n+1;allocation_live++;return 0;}
void seg_free(u16 s){(void)s;assert(allocation_live>0);allocation_live--;}
void fcopy_in(void*p,u16 s,u16 o,u16 n){memcpy(p,host_ptr(s,o,n),n);}
void fcopy_out(u16 s,u16 o,const void*p,u16 n){memcpy(host_ptr(s,o,n),p,n);}
static struct qmsg input_messages[12];static int input_n,input_at,dispatched;
static int quit_pending;static u16 quit_code;
static int next_msg(struct qmsg*m,int block){assert(block&&input_at<input_n);*m=input_messages[input_at++];return 1;}
static u32 dispatch(u16 h,u16 m,u16 p,u32 l){(void)h;(void)m;(void)p;(void)l;dispatched++;return 0;}
static int cursor_active,cursor_failed,cursor_choices,bells;
int cursor_drag_start(struct drag_cursor*c,u16 h){(void)c;(void)h;if(cursor_failed)return 0;assert(!cursor_active);cursor_active=1;return 1;}
void cursor_drag_choose(struct drag_cursor*c,u16 reply){(void)c;(void)reply;assert(cursor_active);cursor_choices++;}
void cursor_drag_end(struct drag_cursor*c){(void)c;assert(cursor_active);cursor_active=0;}
static const char *beep_setting;
const char *prof_path(void){return "win.ini";}
int prof_get(const char*path,const char*app,const char*key,char*out,int n){assert(!strcmp(path,"win.ini")&&!strcmp(app,"windows")&&!strcmp(key,"Beep"));if(!beep_setting)return -1;snprintf(out,n,"%s",beep_setting);return strlen(out);}
void GrBell(void){bells++;}
'''
code=pre+''.join(func(n) for n in ('dialog_slots_available','win_get','u_IsWindow','u_IsWindowVisible','u_GetParent','window_relative','u_GetWindow','window_word','u_GetWindowWord','u_SetWindowWord','u_GetWindowLong','u_SetWindowLong','u_SetClassWord','client_origin','win_client_origin','top_window','win_by_nx','user_input_blocked','user_window_relative','u_SetSysModalWindow','u_GetSysModalWindow','user_drag_capture','user_drag_alive','user_drag_release','user_drag_next','window_message','u_SendMessage','u_GetActiveWindow','valid_coords','convert_point','u_ClientToScreen','u_ScreenToClient','u_GetWindowRect','border_of','win_invalidate_all','move_children','scroll_redraw','u_CreateWindow','move_window','show_bars','u_ShowScrollBar','calc_child_scroll','u_CalcChildScroll','scroll_children','u_ScrollChildren','u_MoveWindow','u_SetWindowPos','set_focus','mdi_child','mdi_eligible','mdi_successor','mdi_activate','mdi_release','u_SetFocus','u_SetActiveWindow','placement_get','placement_moved','placement_forget','icon_position','show_window','u_ShowWindow','u_IsIconic','u_GetInternalWindowPos','u_SetInternalWindowPos','u_ArrangeIconicWindows','free_controls','destroy_window','mdi_message','u_DefWindowProc','u_DefFrameProc','u_DefMDIChildProc','u_TranslateMDISysAccel'))
code+=func('blocked').replace('blocked(', 'actual_blocked(',1)
code+=(root/'src/drag.c').read_text().replace('#include \"wapi.h\"','')
code+='''
static void setup(u32 style,u16 parent,int x,int y,int w,int h){
 memset(host_ptr(0x1000,0,32),0,32);wr16(0x1000,8,parent);wr16(0x1000,10,h);wr16(0x1000,12,w);wr16(0x1000,14,y);wr16(0x1000,16,x);wr32(0x1000,18,style);
}

static int drag_mode,queries,selects,deselects,dragmoves,drops,loops;
static u32 drag_message(struct wnd*w,u16 msg,u16 wp,u32 lp)
{
 u16 seg=lp>>16,off=(u16)lp;
 assert(rd16(seg,off)==HWND_BASE+4&&rd16(seg,off+4)==1);
 assert(rd16(seg,off+6)==0x1234&&rd16(seg,off+8)==0x5678);
 if(msg==WM_QUERYDROPOBJECT){
  queries++;if(drag_mode==1)return 0;
  if(drag_mode==2){capture_hwnd=0;return 1;}
  if(drag_mode==3){destroy_window(win_get(HWND_BASE+4));return 1;}
  if(drag_mode==4)assert(!u_DragObject(0x1000,0));
  assert(rd16(seg,off+2)==w->hwnd);
  wr32(seg,off+14,w->hwnd);return 1;
 }
 if(msg==WM_DRAGLOOP){assert(w->hwnd==HWND_BASE+4&&cursor_active);loops++;return 0;}
 if(msg==WM_DRAGSELECT){assert(rd16(seg,off+2)==w->hwnd);if(wp)selects++;else deselects++;return 0;}
 if(msg==WM_DRAGMOVE){assert(rd16(seg,off+2)==w->hwnd);dragmoves++;return 0;}
 assert(msg==WM_DROPOBJECT&&wp==HWND_BASE+4&&!capture_hwnd&&!cursor_active);
 assert(rd16(seg,off+2)==w->hwnd&&rd32(seg,off+14)==w->hwnd);drops++;return 0x12340002UL;
}
static void drag_fixture(void)
{
 int i;assert(!placements&&!allocation_live);memset(wins,0,sizeof(wins));memset(native_info,0,sizeof(native_info));
 sysmodal_hwnd=modal_hwnd=modal_owner=capture_hwnd=0;modal_block=0;
 queries=selects=deselects=dragmoves=drops=loops=0;drag_mode=0;input_at=0;input_n=5;dispatched=0;quit_pending=0;
 for(i=0;i<4;i++){
  struct wnd*w=&wins[i];w->used=w->visible=1;w->hwnd=HWND_BASE+4*i;w->frame=100+10*i;w->client=101+10*i;
  w->cx=w->cy=1;w->w=80;w->h=60;w->cw=78;w->ch=58;w->x=i==1?5:i==2?100:200;w->y=i==1?5:20;w->parent=i?HWND_BASE:0;
  native_info[w->frame].wid=w->frame;native_info[w->frame].parent=i?101:1;native_info[w->client].wid=w->client;
 }
 wins[0].x=wins[0].y=0;wins[0].w=302;wins[0].h=182;wins[0].cw=300;wins[0].ch=180;
 native_info[1].wid=1;native_info[1].child=100;native_info[101].child=130;native_info[130].sibling=120;native_info[120].sibling=110;
 memset(input_messages,0,sizeof(input_messages));input_messages[0].msg=WM_TIMER;
 for(i=1;i<5;i++){input_messages[i].msg=i==4?0x202:0x200;input_messages[i].x=i<=2?120:220;input_messages[i].y=40;}
 wr16(0x1000,10,HWND_BASE);wr16(0x1000,8,HWND_BASE+4);wr16(0x1000,6,1);wr16(0x1000,4,0x5678);wr16(0x1000,2,0x1234);wr16(0x1000,0,0x500);
}
static void drag_protocol(void)
{
 drag_fixture();drag_mode=4;assert(u_DragObject(0x1000,0)==0x12340002UL);
 assert(queries==4&&loops==4&&selects==2&&deselects==1&&dragmoves==2&&drops==1&&dispatched==1&&!allocation_live&&!cursor_active&&!capture_hwnd);
 drag_fixture();drag_mode=1;assert(!u_DragObject(0x1000,0)&&!drops&&!selects&&!allocation_live&&!cursor_active);
 drag_fixture();drag_mode=2;assert(!u_DragObject(0x1000,0)&&!drops&&!allocation_live&&!cursor_active);
 drag_fixture();drag_mode=3;assert(!u_DragObject(0x1000,0)&&!drops&&!allocation_live&&!cursor_active&&!win_get(HWND_BASE+4));
 drag_fixture();input_n=1;input_messages[0].msg=WM_QUIT;input_messages[0].wp=17;
 assert(!u_DragObject(0x1000,0)&&quit_pending&&quit_code==17&&!allocation_live&&!cursor_active);
 drag_fixture();input_n=2;input_messages[0]=input_messages[1];input_messages[1].msg=WM_KEYDOWN;input_messages[1].wp=27;
 assert(!u_DragObject(0x1000,0)&&selects==1&&deselects==1&&!drops&&!allocation_live&&!cursor_active);
 drag_fixture();fail_global=1;assert(!u_DragObject(0x1000,0)&&!allocation_live&&!cursor_active);fail_global=0;
 cursor_failed=1;assert(!u_DragObject(0x1000,0)&&!allocation_live&&!cursor_active);cursor_failed=0;
 assert(!u_DragObject(0x1000,0xffff));
 /* A system-modal target admits its children/owned popups, blocks siblings,
  * and revokes capture outside its tree. Invalid handles do not replace it. */
 capture_hwnd=HWND_BASE+4;wr16(0x1000,0,HWND_BASE+8);assert(!u_SetSysModalWindow(0x1000,0));
 assert(u_GetSysModalWindow(0,0)==HWND_BASE+8&&!capture_hwnd&&blocked(&wins[1])&&!blocked(&wins[2]));
 wins[4]=wins[2];wins[4].hwnd=HWND_BASE+16;wins[4].parent=HWND_BASE+8;assert(!blocked(&wins[4]));
 wins[5]=wins[2];wins[5].hwnd=HWND_BASE+20;wins[5].parent=0;wins[5].owner=HWND_BASE+8;assert(!blocked(&wins[5]));
 wins[2].style|=WS_DISABLED;assert(blocked(&wins[4])&&!blocked(&wins[5]));wins[2].style&=~WS_DISABLED;
 wr16(0x1000,0,0xffff);assert(!u_SetSysModalWindow(0x1000,0)&&u_GetSysModalWindow(0,0)==HWND_BASE+8);
 wr16(0x1000,0,0);assert(u_SetSysModalWindow(0x1000,0)==HWND_BASE+8&&!u_GetSysModalWindow(0,0));
 wr16(0x1000,0,HWND_BASE+8);u_SetSysModalWindow(0x1000,0);wins[4].used=wins[5].used=0;destroy_window(&wins[2]);assert(!u_GetSysModalWindow(0,0));
 bells=0;beep_setting=0;wr16(0x1000,0,0xffff);u_MessageBeep(0x1000,0);assert(bells==1);
 beep_setting="No";u_MessageBeep(0x1000,0);assert(bells==1);beep_setting="Yes";u_MessageBeep(0x1000,0);assert(bells==2);
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
   /* Showing/hiding non-client bars resizes the backing client and sends
    * WM_SIZE, without inventing a scrollbar-control implementation. */
   wr16(0x1000,0,1);wr16(0x1000,2,3);wr16(0x1000,4,id);
   assert(u_ShowScrollBar(0x1000,0)&&z->cw==184&&z->ch==84&&frame_clears);
   assert(sizes[client][0]==184&&sizes[client][1]==84&&lastmsg==WM_SIZE&&lastlp==0x005400b8UL);
   n=msgcount;assert(u_ShowScrollBar(0x1000,0)&&msgcount==n);
   wr16(0x1000,0,0);wr16(0x1000,2,1);assert(u_ShowScrollBar(0x1000,0)&&z->cw==200&&z->ch==84);
   wr16(0x1000,2,0);assert(u_ShowScrollBar(0x1000,0)&&z->cw==200&&z->ch==100);
   wr16(0x1000,2,2);assert(!u_ShowScrollBar(0x1000,0)&&z->cw==200);
   assert(!u_ShowScrollBar(0x1000,0xffff));
   wr16(0x1000,0,0);wr16(0x1000,2,102);wr16(0x1000,4,202);

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
 /* Automatic child bounds, mutual bar effects, half-page scrolling,
  * negative reach, thumb release, and physical direct-child movement. */
 {u16 parent,kid;struct wnd *c,*g;
  setup(WS_CHILD|WS_VISIBLE|WS_BORDER,HWND_BASE,0,0,202,102);parent=u_CreateWindow(0x1000,0);assert(parent);c=win_get(parent);
  setup(WS_CHILD|WS_VISIBLE,parent,-10,-5,310,110);kid=u_CreateWindow(0x1000,0);assert(kid);g=win_get(kid);
  wr16(0x1000,0,3);wr16(0x1000,2,parent);u_CalcChildScroll(0x1000,0);
  assert(c->cw==184&&c->ch==84&&(c->style&(WS_HSCROLL|WS_VSCROLL))==(WS_HSCROLL|WS_VSCROLL));
  assert(c->scroll_min[0]==-10&&c->scroll_max[0]==116&&c->scroll_min[1]==-5&&c->scroll_max[1]==21);
  wr16(0x1000,8,parent);wr16(0x1000,6,WM_HSCROLL);wr16(0x1000,4,3);wr32(0x1000,0,0);
  u_ScrollChildren(0x1000,0);assert(c->scroll_pos[0]==92&&g->x==-102&&c->invalid);
  wr16(0x1000,4,5);wr16(0x1000,0,116);u_ScrollChildren(0x1000,0);assert(c->scroll_pos[0]==92&&g->x==-102);
  wr16(0x1000,4,4);u_ScrollChildren(0x1000,0);assert(c->scroll_pos[0]==116&&g->x==-126);
  wr16(0x1000,4,6);u_ScrollChildren(0x1000,0);assert(c->scroll_pos[0]==-10&&g->x==0);
  wr16(0x1000,4,8);u_ScrollChildren(0x1000,0);assert(c->scroll_pos[0]==0&&c->scroll_min[0]==0&&c->scroll_max[0]==126);
  /* Hiding the oversized child removes both bars and restores the client. */
  g->visible=0;wr16(0x1000,0,3);wr16(0x1000,2,parent);u_CalcChildScroll(0x1000,0);
  assert(c->cw==200&&c->ch==100&&!(c->style&(WS_HSCROLL|WS_VSCROLL)));
  assert(!c->scroll_min[0]&&!c->scroll_max[0]&&!c->scroll_min[1]&&!c->scroll_max[1]);
  assert(!u_CalcChildScroll(0x1000,0xffff)&&!u_ScrollChildren(0x1000,0xffff));destroy_window(c);
 }
 /* Sparse placement: normal rectangle survives icon placement, veto,
  * rearrangement, maximization and restore; ordinary windows allocate none. */
 {u16 group,second;struct wnd *g,*g2;struct placement *saved;int pcw=wins[0].cw,pch=wins[0].ch;
  wins[0].cw=320;wins[0].ch=200;assert(!placements);
  setup(WS_CHILD|WS_VISIBLE|WS_BORDER,HWND_BASE,7,9,180,100);group=u_CreateWindow(0x1000,0);assert(group);g=win_get(group);assert(!placements);
  fail_extra=1;wr16(0x1000,0,2);wr16(0x1000,2,group);u_ShowWindow(0x1000,0);assert(!(g->style&WS_MINIMIZE)&&!placements);fail_extra=0;
  wr16(0x2000,180,11);wr16(0x2000,182,13);wr16(0x2000,184,191);wr16(0x2000,186,113);
  wr16(0x2000,188,45);wr16(0x2000,190,60);
  wr16(0x1000,10,group);wr16(0x1000,8,2);wr32(0x1000,4,0x200000b4UL);wr32(0x1000,0,0x200000bcUL);
  assert(u_SetInternalWindowPos(0x1000,0)==1&&g->x==45&&g->y==60&&g->w==ICON_WIDTH&&g->h==ICON_HEIGHT);
  assert((g->style&WS_MINIMIZE)&&!g->cw&&!g->ch&&unmaps[g->client]&&!g->invalid);saved=placements;assert(saved&&!saved->next);
  wr16(0x1000,0,group);assert(u_IsIconic(0x1000,0));
  wr16(0x1000,8,group);wr32(0x1000,4,0x200000c8UL);wr32(0x1000,0,0x200000d0UL);
  assert(u_GetInternalWindowPos(0x1000,0)==2&&rd16(0x2000,200)==11&&rd16(0x2000,204)==191&&rd16(0x2000,208)==45);
  query_open=0;wr16(0x1000,0,9);wr16(0x1000,2,group);u_ShowWindow(0x1000,0);assert(g->style&WS_MINIMIZE);query_open=1;
  u_ShowWindow(0x1000,0);assert(!(g->style&(WS_MINIMIZE|WS_MAXIMIZE))&&g->x==11&&g->y==13&&g->w==180&&g->h==100&&g->cw&&g->ch);
  assert(placements==saved&&!saved->next);
  wr16(0x1000,0,3);u_ShowWindow(0x1000,0);assert((g->style&WS_MAXIMIZE)&&g->x==0&&g->y==0&&g->w==wins[0].cw&&g->h==wins[0].ch);
  wr16(0x1000,0,2);u_ShowWindow(0x1000,0);wr16(0x1000,0,9);u_ShowWindow(0x1000,0);assert(g->style&WS_MAXIMIZE);
  wr16(0x1000,0,1);u_ShowWindow(0x1000,0);assert(g->x==11&&g->y==13&&g->w==180&&g->h==100&&!(g->style&(WS_MINIMIZE|WS_MAXIMIZE)));
  wr16(0x1000,10,group);wr16(0x1000,8,2);wr32(0x1000,4,0);wr32(0x1000,0,0);assert(u_SetInternalWindowPos(0x1000,0));
  setup(WS_CHILD|WS_VISIBLE|WS_BORDER,HWND_BASE,1,2,100,80);second=u_CreateWindow(0x1000,0);assert(second);g2=win_get(second);
  wr16(0x1000,0,2);wr16(0x1000,2,second);u_ShowWindow(0x1000,0);assert(g2->style&WS_MINIMIZE);
  wr16(0x1000,0,HWND_BASE);assert(u_ArrangeIconicWindows(0x1000,0)==ICON_STEP_Y&&g->x!=g2->x);
  wr16(0x1000,8,group);wr32(0x1000,4,0x200000c8UL);wr32(0x1000,0,0x200000d0UL);assert(u_GetInternalWindowPos(0x1000,0)==2&&rd16(0x2000,200)==11&&rd16(0x2000,204)==191);
  assert(!u_SetInternalWindowPos(0x1000,0xffff)&&!u_GetInternalWindowPos(0x1000,0xffff));
  destroy_window(g);destroy_window(g2);assert(!placements);
  setup(WS_CHILD|WS_VISIBLE|WS_BORDER,HWND_BASE,1,2,100,80);group=u_CreateWindow(0x1000,0);assert(group);
  destroy_on_size=group;wr16(0x1000,0,2);wr16(0x1000,2,group);u_ShowWindow(0x1000,0);
  assert(!win_get(group)&&!placements); /* WM_SIZE may destroy the sparse owner */
  wins[0].cw=pcw;wins[0].ch=pch;
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
  wr16(0x1000,0,2);wr16(0x1000,2,second);u_ShowWindow(0x1000,0);
  assert((g2->style&WS_MINIMIZE)&&c->mdi_active==group);
  wr16(0x1000,0,9);u_ShowWindow(0x1000,0);assert(!(g2->style&WS_MINIMIZE)&&c->mdi_active==second);
  third=win_send(c,WM_MDICREATE,0,0x20000040UL);assert(third);g3=win_get(third);
  native_info[g3->frame].wid=g3->frame;native_info[g3->frame].parent=c->client;
  native_info[g3->frame].sibling=native_info[c->client].child;native_info[c->client].child=g3->frame;
  /* The frame resizes its actual client, routes Window-menu ids, and
   * transfers focus to the client's active child without mutating arguments. */
  wr16(0x1000,10,HWND_BASE);wr16(0x1000,8,mdi);wr16(0x1000,6,WM_SIZE);wr16(0x1000,4,0);wr32(0x1000,0,0x008c0104UL);
  u_DefFrameProc(0x1000,0);assert(c->w==260&&c->h==140&&c->x==0&&c->y==0);
  assert(rd16(0x1000,10)==HWND_BASE&&rd16(0x1000,8)==mdi&&w16_app_sp==200);
  wr16(0x1000,6,WM_COMMAND);wr16(0x1000,4,g->id);u_DefFrameProc(0x1000,0);assert(c->mdi_active==group);
  wr16(0x1000,6,WM_SETFOCUS);u_DefFrameProc(0x1000,0);assert(nx_focus==g->client);
  wr16(0x1000,6,0x86);wr16(0x1000,4,1);assert(u_DefFrameProc(0x1000,0)==1&&senthw==group&&sentm==0x86);
  strcpy((char*)host_ptr(0x2000,160,16),"Program frame");wr16(0x1000,6,0x0c);wr32(0x1000,0,0x200000a0UL);
  assert(!u_DefFrameProc(0x1000,0)&&!strcmp(wins[0].title,"Program frame"));
  assert(!u_DefFrameProc(0x1000,0xffff));
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
  key_mods=MWKMOD_CTRL;assert(u_TranslateMDISysAccel(0x1000,0)&&c->mdi_active==second&&mdi_syslp==0x00010009UL);
  key_mods|=MWKMOD_SHIFT;wr16(0x2000,0x304,0x75);assert(u_TranslateMDISysAccel(0x1000,0)&&c->mdi_active==group);
  wr16(0x2000,0x304,0x76);assert(!u_TranslateMDISysAccel(0x1000,0));
  wr16(0x2000,0x304,0x73);wr16(0x2000,0x302,WM_KEYUP);assert(!u_TranslateMDISysAccel(0x1000,0));
  wr16(0x2000,0x302,WM_SYSKEYDOWN);key_mods=MWKMOD_CTRL;mdi_default_commands=0;
  assert(u_TranslateMDISysAccel(0x1000,0)&&win_get(group)&&c->mdi_active==group&&mdi_syslp==0x00010073UL); /* application veto */
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
 drag_protocol();
 puts("Window tree, MDI, drag protocol, system modality and bell policy checks passed");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(code)
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),str(root/'src/global.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
