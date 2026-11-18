#!/usr/bin/env python3
import pathlib,subprocess,tempfile,os
r=pathlib.Path(__file__).resolve().parents[1];s=(r/'src/user.c').read_text()
a=s.index('static u16 set_focus(');b=s.index('/* ShowWindow',a)
c=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint16_t u16;typedef uint32_t u32;
#define WM_SETFOCUS 7
struct wnd {u16 hwnd;int ctl,frame,client;} wins[3]={{1,0,10,11},{2,0,20,21},{3,1,30,31}};
static u16 focus_hwnd,native,redirect;static int count;static u16 msgs[8],owners[8],params[8];
static u16 set_focus(struct wnd*);
static struct wnd*win_get(u16 h){return h>=1&&h<=3?&wins[h-1]:0;}
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
