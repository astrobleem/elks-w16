#!/usr/bin/env python3
"""Execute the compact Nano-X child-window clipping addition from its patch."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
patch=(root/'patches/nanox-childclip.patch').read_text()
block='\n'.join(line[1:] for line in patch.splitlines() if line.startswith('+') and not line.startswith('+++'))
pre=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define GR_MODE_EXCLUDECHILDREN 0x100
#define GR_TRUE 1
#define MAX_CLIPRECTS 6
struct window {int realized,output,x,y,width,height,bordersize;struct window*children,*siblings;};
struct rect {int x,y,width,height;};
static struct window root,*rootwp=&root;
static int pixels[12][16],calls;
/* The existing splitter is unchanged; observe the added loop's exact cuts. */
static int GsSplitClipRect(struct rect*a,struct rect*b,int l,int t,int r,int bot){
 int x,y;(void)a;(void)b;calls++;
 for(y=0;y<12;y++)for(x=0;x<16;x++)if(x>=l&&x<=r&&y>=t&&y<=bot)pixels[y][x]=0;
 return 0;
}
static int clip(struct window*wp,int flags,int initial_count){
 struct window*sibwp;struct rect cliprects[MAX_CLIPRECTS];
 int toomany=0,count=initial_count,newcount,i,bs,minx,miny,maxx,maxy;
'''
post=r'''
 return toomany;
}
static void reset(void){int x,y;calls=0;for(y=0;y<12;y++)for(x=0;x<16;x++)pixels[y][x]=1;}
int main(void){
 struct window visible={1,1,3,4,5,2,1,0,0},hidden={0,1,0,0,16,12,0,0,0};
 struct window inputonly={1,0,0,0,16,12,0,0,0},parent={1,1,0,0,16,12,0,&visible,0};
 int x,y;
 visible.siblings=&hidden;hidden.siblings=&inputonly;
 reset();assert(!clip(&parent,0,1)&&calls==1);
 for(y=0;y<12;y++)for(x=0;x<16;x++)assert(pixels[y][x]==!(x>=2&&x<=8&&y>=3&&y<=6));
 reset();assert(!clip(&parent,GR_MODE_EXCLUDECHILDREN,1)&&!calls);
 root.children=&visible;reset();assert(!clip(&root,0,1)&&!calls);
 parent.children=0;reset();assert(!clip(&parent,0,1)&&!calls);
 parent.children=&visible;reset();assert(clip(&parent,0,MAX_CLIPRECTS-1)&&!calls);
 visible.bordersize=0;reset();assert(!clip(&parent,0,1)&&calls==1);
 assert(pixels[3][3]&&!pixels[4][3]&&!pixels[5][7]&&pixels[6][7]&&pixels[4][8]);
 puts("Compact child clipping: child bounds/borders, hidden/input-only windows, root, override and capacity passed");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as t:
 c=Path(t)/'test.c';exe=Path(t)/'test';c.write_text(pre+block+post)
 subprocess.run(['gcc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={'ASAN_OPTIONS':'detect_leaks=0'})

# With a configured Microwindows tree, also execute the complete patched
# compact clipper and its real rectangle splitter, not just the new loop.
import os,shutil
mw=os.environ.get('MWDIR')
if mw:
    header=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef int GR_COUNT,GR_COORD,GR_SIZE,GR_BOOL;typedef void MWCLIPREGION;
typedef struct{int x,y,width,height;} MWCLIPRECT;
typedef struct window{int id,realized,output,x,y,width,height,bordersize;void*psd;struct window*parent,*children,*siblings;} GR_WINDOW;
#define GR_TRUE 1
#define GR_FALSE 0
#define GR_MODE_EXCLUDECHILDREN 0x100
#define GR_ERROR_TOO_MUCH_CLIPPING 1
#define MAX_CLIPRECTS 200
static GR_WINDOW *rootwp,*clipwp;
static int pixels[12][16],errors;
static void GsError(int e,int id){(void)e;(void)id;errors++;}
static void GdSetClipRects(void*p,int count,MWCLIPRECT*r){
 int i,x,y;(void)p;memset(pixels,0,sizeof(pixels));
 for(i=0;i<count;i++)for(y=0;y<12;y++)for(x=0;x<16;x++)
  if(x>=r[i].x&&y>=r[i].y&&x<r[i].x+r[i].width&&y<r[i].y+r[i].height)pixels[y][x]=1;
}
'''
    check=r'''
int main(void){
 GR_WINDOW root={1,1,1,0,0,16,12,0,0,0,0,0};
 GR_WINDOW parent={2,1,1,0,0,16,12,0,0,&root,0,0};
 GR_WINDOW child={3,1,1,3,4,5,2,1,0,&parent,0,0};
 int x,y;rootwp=&root;root.children=&parent;parent.children=&child;
 clipwp=0;GsSetClipWindow(&parent,0,0);
 for(y=0;y<12;y++)for(x=0;x<16;x++)assert(pixels[y][x]==!(x>=2&&x<=8&&y>=3&&y<=6));
 clipwp=0;GsSetClipWindow(&parent,0,GR_MODE_EXCLUDECHILDREN);
 for(y=0;y<12;y++)for(x=0;x<16;x++)assert(pixels[y][x]);
 child.realized=0;clipwp=0;GsSetClipWindow(&parent,0,0);
 for(y=0;y<12;y++)for(x=0;x<16;x++)assert(pixels[y][x]);
 child.realized=1;child.output=0;clipwp=0;GsSetClipWindow(&parent,0,0);
 for(y=0;y<12;y++)for(x=0;x<16;x++)assert(pixels[y][x]);
 child.output=1;parent.x=-2;parent.y=-2;parent.width=20;parent.height=16;
 clipwp=0;GsSetClipWindow(&parent,0,0);
 for(y=0;y<12;y++)for(x=0;x<16;x++)assert(pixels[y][x]==!(x>=2&&x<=8&&y>=3&&y<=6));
 assert(!errors);puts("Complete compact clipper and rectangle splitter preserve visible children and ancestor bounds");return 0;
}
'''
    with tempfile.TemporaryDirectory() as t:
        d=Path(t);target=d/'src/nanox';target.mkdir(parents=True)
        source=target/'srvclip1.c';shutil.copyfile(Path(mw)/'src/nanox/srvclip1.c',source)
        reverse=subprocess.run(['patch','--force','--reverse','--dry-run','-d',t,'-p1'],input=patch,text=True,capture_output=True)
        if reverse.returncode:
            subprocess.run(['patch','--batch','--forward','-d',t,'-p1'],input=patch,text=True,check=True,capture_output=True)
        (target/'serv.h').write_text(header)
        source.write_text(source.read_text()+check)
        exe=d/'full'
        subprocess.run(['gcc','-Wall','-Wextra','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-pie','-no-pie',str(source),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,env={'ASAN_OPTIONS':'detect_leaks=0'})
else:
    print('Full Nano-X clipper integration skipped: set MWDIR to exercise its rectangle splitter')
