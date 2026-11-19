#!/usr/bin/env python3
"""Window-text Pascal wrappers: truncation, bounds, callbacks and dialog default."""
import pathlib,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
base=(root/'tests/editcontroltest.py').read_text()
ns={'__file__':__file__}
exec(compile(base[:base.index('code=pre+')],str(root/'tests/editcontroltest.py'),'exec'),ns)
pre=ns['pre'].split('static u32 win_send(')[0].replace(',notifications','')
pre+='''static unsigned callbacks,custom;static u16 lastmsg,lastwp;static u32 lastlp;
static u32 win_send(struct wnd*w,u16 m,u16 p,u32 l){(void)w;callbacks++;lastmsg=m;lastwp=p;lastlp=l;return custom;}
'''
code=pre+''.join(ns['func'](n) for n in ('win_get','control_message','window_message','u_GetWindowText','u_GetWindowTextLength'))
code+='''int main(void){struct wnd*w=&wins[1];u16 ss=0x1000;
 w->used=1;w->hwnd=HWND_BASE+4;w->ctl=CTL_STATIC;strcpy(w->title,"Program Manager");
 wr16(ss,0,8);wr16(ss,2,32);wr16(ss,4,0x2000);wr16(ss,6,w->hwnd);
 assert(u_GetWindowText(ss,0)==7);assert(!strcmp((char*)host_ptr(0x2000,32,8),"Program"));
 wr16(ss,8,w->hwnd);assert(u_GetWindowTextLength(ss,8)==15);
 wr16(ss,0,1);assert(!u_GetWindowText(ss,0)&&!rd8(0x2000,32));
 wr8(0x2000,32,77);wr16(ss,0,0);assert(!u_GetWindowText(ss,0)&&rd8(0x2000,32)==77);
 wr16(ss,0,0xffff);assert(!u_GetWindowText(ss,0));
 wr16(ss,0,8);wr16(ss,2,0xffff);assert(!u_GetWindowText(ss,0));
 wr16(ss,2,32);assert(!u_GetWindowText(ss,0xfff9));
 w->ctl=0;custom=5;assert(u_GetWindowText(ss,0)==5);
 assert(lastmsg==0xd&&lastwp==8&&lastlp==0x20000020UL&&callbacks==1);
 custom=123;assert(u_GetWindowTextLength(ss,8)==123&&lastmsg==0xe);
 w->isdialog=1;custom=0;assert(u_GetWindowTextLength(ss,8)==15);
 wr16(ss,0,30);assert(u_GetWindowText(ss,0)==15);
 assert(!strcmp((char*)host_ptr(0x2000,32,16),"Program Manager"));
 wr16(ss,6,0xdead);assert(!u_GetWindowText(ss,0));
 puts("Window-text truncation, far bounds, custom callbacks and dialog defaults passed");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 p=pathlib.Path(td);(p/'test.c').write_text(code)
 subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
