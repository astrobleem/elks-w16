#!/usr/bin/env python3
import pathlib,subprocess,tempfile,os
r=pathlib.Path(__file__).resolve().parents[1];s=(r/'src/user.c').read_text()
# Match the definition, not the forward declaration; isolate this function.
a=s.index('static u16 set_focus(struct wnd *w)\n{');b=s.index('\n}',a)+2
c=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint16_t u16;typedef uint32_t u32;
#define WM_SETFOCUS 7
#define WS_MINIMIZE 0x20000000UL
struct wnd {u16 hwnd;int ctl,frame,client;u32 style;u16 parent,mdi_active;} wins[3]={
 {.hwnd=1,.frame=10,.client=11},{.hwnd=2,.frame=20,.client=21},
 {.hwnd=3,.ctl=1,.frame=30,.client=31}};
static u16 focus_hwnd,native,redirect;static int count;static u16 msgs[8],owners[8],params[8];
static u16 set_focus(struct wnd*);
static struct wnd*win_get(u16 h){return h>=1&&h<=3?&wins[h-1]:0;}
/* These focus cases are nonmodal, non-MDI windows. MDI transitions have
 * their own fixture; keep dependencies explicit rather than extracting it. */
static int blocked(struct wnd*w){(void)w;return 0;}
static struct wnd*mdi_child(struct wnd*w){(void)w;return 0;}
static int mdi_activate(struct wnd*c,u16 h){(void)c;(void)h;assert(0);return 0;}
static void GrSetFocus(u16 n){native=n;}
static void win_send(struct wnd*w,u16 msg,u16 wp,u32 lp){(void)lp;owners[count]=w->hwnd;msgs[count]=msg;params[count++]=wp;if(redirect&&msg==7&&w->hwnd==1){redirect=0;set_focus(&wins[1]);}}
'''+s[a:b]+r'''
int main(void){
 assert(set_focus(&wins[0])==0);assert(focus_hwnd==1&&native==11&&count==1&&msgs[0]==7&&params[0]==0);
 assert(set_focus(&wins[0])==1&&count==1);
 assert(set_focus(&wins[2])==1);assert(count==3&&msgs[1]==8&&owners[1]==1&&params[1]==3&&msgs[2]==7&&params[2]==1&&native==30);
 set_focus(0);assert(focus_hwnd==0&&count==4&&msgs[3]==8&&params[3]==0);
 count=0;redirect=1;set_focus(&wins[0]);assert(focus_hwnd==2&&native==21&&count==3&&msgs[0]==7&&msgs[1]==8&&msgs[2]==7);
 puts("focus: owner transitions, child redirection, repeat and NULL passed");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);(p/'t.c').write_text(c)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
