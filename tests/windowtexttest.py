#!/usr/bin/env python3
"""Optional window-text allocation: failure, stable-slot reuse and cleanup."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];u=(root/'src/user.c').read_text();h=(root/'src/wapi.h').read_text()
def func(name):
 m=re.search(r'^(?:static )?(?:int |void |struct wnd \*)'+name+r'\(',u,re.M);assert m,name
 a=u.index('{',m.start());b=a+1;depth=1
 while depth:depth+=(u[b]=='{')-(u[b]=='}');b+=1
 return u[m.start():b]+'\n'
code='''#define W16_LAZY_UI
#include "w16.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define MAXWIN 32
#define HWND_BASE 0x100
#define GR_WINDOW_ID u16
'''+re.search(r'struct wnd \{.*?\n\};',h,re.S)[0]+'''
static struct wnd wins[MAXWIN];
static int left=-1,live;
static void *checked_calloc(unsigned n,unsigned bytes) {void*p;if(!left)return 0;if(left>0)left--;p=calloc(n,bytes);if(p)live++;return p;}
static void checked_free(void*p) {if(p){assert(live>0);live--;free(p);}}
#define calloc checked_calloc
#define free checked_free
#define wlog(...) ((void)0)
'''+''.join(func(n) for n in ('window_text_prepare','window_text_shutdown','win_alloc','win_get'))+'''
int main(void){
 struct wnd *w,*first;struct wnd other={0};char *text;unsigned i;
 left=0;assert(!win_alloc());assert(!wins[0].used&&!wins[0].title&&!live);
 left=-1;first=win_alloc();assert(first==wins);text=first->title;strcpy(text,"parent");
 assert(window_text_prepare(&other));strcpy(other.title,"child");assert(strcmp(text,other.title));checked_free(other.title);
 left=0;assert(!win_alloc());assert(!wins[1].used&&!wins[1].title&&live==1);
 first->used=0;assert(win_alloc()==first&&first->title==text&&!first->title[0]&&live==1);
 left=-1;for(i=1;i<MAXWIN;i++){w=win_alloc();assert(w==&wins[i]);assert(w->hwnd==HWND_BASE+4*i);assert(win_get(w->hwnd)==w);memset(w->title,'a',99);w->title[99]=0;}
 assert(!win_alloc()&&live==MAXWIN);assert(!win_get(HWND_BASE-1)&&!win_get(HWND_BASE+1)&&!win_get(HWND_BASE+4*MAXWIN));
 for(i=0;i<MAXWIN;i++)wins[i].used=0;
 left=0;for(i=0;i<MAXWIN;i++){w=win_alloc();assert(w==&wins[i]&&!w->title[0]);}assert(live==MAXWIN);
 window_text_shutdown();assert(!live);window_text_shutdown();assert(!live);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-DW16_HOST','-I'+str(root/'src'),'-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('PASS: text allocation failure leaves slot uncommitted; all 32 handles/capacity; stable reuse; private text; complete/idempotent cleanup')
