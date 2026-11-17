#!/usr/bin/env python3
import pathlib,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
p=(root/'patches/nanox-readpixel.patch').read_text()
added='\n'.join(l[1:] for l in p.splitlines() if l.startswith('+') and not l.startswith('+++'))
start=added.index('void GrReadArea(',added.index('/* Negotiated one-pixel'))
end=added.index('\n#endif',start)
fn=added[start:end]
pre='''#include <assert.h>
#include <stdio.h>
typedef unsigned GR_DRAW_ID;typedef int GR_COORD,GR_SIZE;typedef unsigned char GR_PIXELVAL;
typedef struct {int ncolors,xvirtres,yvirtres;} SCREEN;
typedef struct {int realized,output,owner,width,height,x,y;SCREEN*psd;} GR_WINDOW;
static SCREEN screen={16,640,480};static GR_WINDOW root={1,1,0,640,480,0,0,&screen},win={1,1,7,30,20,10,20,&screen};
static GR_WINDOW *rootwp=&root,*visible=&win;static int curclient=7,reads,lastx,lasty;
GR_WINDOW*GsFindWindow(unsigned id){return id==1?&root:id==2?&win:0;}
GR_WINDOW*GsFindVisibleWindow(int x,int y){(void)x;(void)y;return visible;}
void GdReadArea(SCREEN*s,int x,int y,int w,int h,GR_PIXELVAL*p){assert(s==&screen&&w==1&&h==1);reads++;lastx=x;lasty=y;*p=12;}
'''
test='''
int main(void){GR_PIXELVAL p=0;int before;
GrReadArea(2,17,5,1,1,&p);assert(p==12&&lastx==27&&lasty==25);
GrReadArea(1,639,479,1,1,&p);assert(p==12&&lastx==639&&lasty==479);before=reads;
visible=&root;GrReadArea(2,17,5,1,1,&p);assert(p==255);visible=&win;
GrReadArea(2,30,5,1,1,&p);assert(p==255);GrReadArea(2,-1,0,1,1,&p);assert(p==255);
GrReadArea(1,640,0,1,1,&p);assert(p==255);GrReadArea(999,0,0,1,1,&p);assert(p==255);
win.owner=8;GrReadArea(2,0,0,1,1,&p);assert(p==255);win.owner=7;
win.realized=0;GrReadArea(2,0,0,1,1,&p);assert(p==255);win.realized=1;
win.output=0;GrReadArea(2,0,0,1,1,&p);assert(p==255);win.output=1;
win.x=-100;GrReadArea(2,0,0,1,1,&p);assert(p==255);win.x=10;
GrReadArea(2,0,0,2,1,&p);assert(p==255);assert(reads==before);
puts("ReadPixel backend ownership, visibility and bounds checks passed");return 0;}
'''
assert '!req->hilength && GetReqLen(req) == sizeof(*req)' in added
assert 'mode &= ~GR_MODE_HAVE_READPIXEL' in added
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(pre+fn+test)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
