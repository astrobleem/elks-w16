#!/usr/bin/env python3
"""Actual ancestor-client coordinate accumulation and GetDCOrg packing."""
from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[1]
def extract(file,name):
 s=(root/'src'/file).read_text();a=s.index(name);e=s.index('{',a)+1;depth=1
 while depth:
  depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
code=r"""
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef uint16_t u16;typedef int16_t i16;typedef uint32_t u32;
#define MAXWIN 16
struct wnd{int x,y,cx,cy;u16 parent;}wins[MAXWIN];
struct dc{struct wnd*w;}dcs[2];
static int lookups;
static u16 arg;
#define AW(n) arg
static struct wnd*win_get(u16 h){assert(++lookups<=MAXWIN);return h>=1&&h<=MAXWIN?&wins[h-1]:0;}
static struct dc*dc_get(u16 h){return h==1||h==2?&dcs[h-1]:0;}
""" + extract('user.c','static void client_origin(')+'\n'+extract('user.c','void win_client_origin(')+'\n'+extract('gdi.c','u32 g_GetDCOrg(')+r"""
static u32 get(u16 h){arg=h;lookups=0;return g_GetDCOrg(0,0);}
static u32 packed(int x,int y){return (u16)x|((u32)(u16)y<<16);}
int main(void){
 int x,y,i;u32 expected;
 assert(get(1)==0&&get(0)==0&&get(0xc00)==0&&get(65535)==0);
 dcs[0].w=&wins[0];wins[0]=(struct wnd){30,40,3,21,0};
 assert(get(1)==packed(33,61)&&get(2)==0);
 wins[1]=(struct wnd){7,9,1,2,1};wins[2]=(struct wnd){-11,13,4,5,2};
 dcs[0].w=&wins[2];assert(get(1)==packed(34,90)&&lookups==2);
 /* Every ancestor contributes frame location plus client offset exactly once. */
 wins[0]=(struct wnd){-32768,32767,-7,8,0};wins[1]=(struct wnd){32767,-32768,9,-10,1};
 dcs[0].w=&wins[1];assert(get(1)==packed(1,-3));
 lookups=0;win_client_origin(0,&x,&y);assert(x==0&&y==0);
 wins[0]=(struct wnd){-22,-33,2,3,99};dcs[0].w=&wins[0];
 assert(get(1)==packed(-20,-30)&&lookups==1);
 /* Defensive MAXWIN traversal bounds malformed parent cycles. */
 wins[0]=(struct wnd){1,-2,3,-4,1};
 assert(get(1)==packed(MAXWIN*4,MAXWIN*-6)&&lookups==MAXWIN);
 for(i=0;i<MAXWIN;i++)wins[i]=(struct wnd){i,-i,1,2,(u16)(i+2)};
 wins[MAXWIN-1].parent=0;dcs[0].w=&wins[0];
 expected=packed(MAXWIN*(MAXWIN-1)/2+MAXWIN,-MAXWIN*(MAXWIN-1)/2+2*MAXWIN);
 assert(get(1)==expected&&lookups==MAXWIN-1);
 assert(get(2)==0);puts("GetDCOrg: signed/nested origins, packing, invalid handles and bounded chains passed");
 return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
