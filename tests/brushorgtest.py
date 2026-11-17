#!/usr/bin/env python3
"""Actual pattern-fill phase and SetBrushOrg signed-point tests."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/gdi.c').read_text()
def extract(name):
 a=source.index(name);end=source.index('{',a)+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[a:end]
code=r"""
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint16_t u16;typedef int16_t i16;typedef uint32_t u32;
typedef u16 GR_BITMAP;
#define GR_MODE_COPY 0
struct dc {int wid,gc;i16 brushx,brushy;u32 bkcolor,textcolor;} dcs[2];
static struct {int w,h;u16 rows[16];}bmps[1];
static u16 scratch[640],args[3];
#define w16_scratch scratch
#define AW(n) args[(n)/2]
#define GS(d) (d)
static struct dc*dc_get(u16 h){return h==1||h==2?&dcs[h-1]:0;}
static int bitmap_brushorg(u16 h,i16 x,i16 y,u32*old){if(h!=3)return 0;*old=0x80007fff;return 1;}
static int rgb_to_gr(u32 c){return c;}
static int fg,bg,mode,usebg,calls,base_x,base_y,width,height;
static unsigned long checks;
static unsigned char seen[640*480];
static void gs_mode(struct dc*d,int v){mode=v;}
static void gs_fg(struct dc*d,int v){fg=v;}
static void gs_bg(struct dc*d,int v){bg=v;}
static void gs_usebg(struct dc*d,int v){usebg=v;}
static int modulo(long v,int n){long r=v%n;return r<0?r+n:r;}
static void GrBitmap(int wid,int gc,int x,int y,int w,int h,GR_BITMAP*data){
 int row,col;struct dc*d=&dcs[wid-1];
 assert(gc==wid&&x==base_x&&w==width&&h>0&&h<=16);
 assert(16+2*((w+15)/16)*h<=512);
 assert(y>=base_y&&y+h<=base_y+height);
 assert(mode==GR_MODE_COPY&&fg==d->bkcolor&&bg==d->textcolor&&usebg==1);
 calls++;
 for(row=0;row<h;row++)for(col=0;col<w;col++){
  int actual=(data[row*((w+15)/16)+col/16]>>(15-col%16))&1;
  int px=modulo((long)x+col-d->brushx,bmps[0].w);
  int py=modulo((long)y+row-d->brushy,bmps[0].h);
  int expected=(bmps[0].rows[py]>>(15-px))&1;
  int at=(y-base_y+row)*w+col;
  assert(at>=0&&at<640*480&&!seen[at]);seen[at]=1;
  assert(actual==expected);checks++;
 }
}
""" + extract('static int pat_fill(')+'\n'+extract('u32 g_SetBrushOrg(')+r"""
static void run(int x,int y,int w,int h){
 int i;base_x=x;base_y=y;width=w;height=h;calls=0;memset(seen,0,sizeof seen);
 assert(pat_fill(&dcs[0],x,y,w,h,0));
 {int batch=480/(2*((w+15)/16));if(batch>16)batch=16;
 assert(calls==(h+batch-1)/batch);}
 for(i=0;i<w*h;i++)assert(seen[i]);
}
int main(void){
 int bw,bh,i,j,k;
 int origins[]={-32768,-32767,-17,-1,0,1,17,32766,32767};
 int widths[]={1,7,15,16,17,31,32,33,255,256,257,639,640};
 dcs[0].wid=dcs[0].gc=1;dcs[1].wid=dcs[1].gc=2;
 dcs[0].bkcolor=123;dcs[0].textcolor=456;
 args[2]=1;args[1]=32767;args[0]=0x8000;
 assert(g_SetBrushOrg(0,0)==0);
 args[1]=0xffff;args[0]=1;assert(g_SetBrushOrg(0,0)==0x80007fff);
 assert(dcs[0].brushx==-1&&dcs[0].brushy==1&&!dcs[1].brushx&&!dcs[1].brushy);
 args[2]=2;args[1]=2;args[0]=3;assert(!g_SetBrushOrg(0,0));
 assert(dcs[0].brushx==-1&&dcs[1].brushx==2&&dcs[1].brushy==3);
 args[2]=3;assert(g_SetBrushOrg(0,0)==0x80007fff);
 args[2]=999;assert(!g_SetBrushOrg(0,0));
 for(bw=1;bw<=16;bw++)for(bh=1;bh<=16;bh++){
  bmps[0].w=bw;bmps[0].h=bh;
  for(i=0;i<16;i++)bmps[0].rows[i]=(u16)(0xa591u^(0x3719u*i));
  for(j=0;j<9;j++){
   dcs[0].brushx=origins[j];dcs[0].brushy=origins[8-j];
   for(k=0;k<13;k++)run(j%2?-29:23,j%2?31:-19,widths[k],17);
  }
 }
 bmps[0].w=7;bmps[0].h=13;run(-32768,32288,640,480);
 calls=0;assert(!pat_fill(&dcs[0],0,0,641,1,0)&&!calls);
 assert(!pat_fill(&dcs[0],0,0,1,481,0)&&!calls);
 assert(!pat_fill(&dcs[0],32767,0,2,1,0)&&!calls);
 assert(!pat_fill(&dcs[0],0,32767,1,2,0)&&!calls);
 assert(pat_fill(&dcs[0],0,0,0,1,0)&&!calls);
 printf("brush origin: %lu actual decoded pattern pixels passed\n",checks);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-O1','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
