#!/usr/bin/env python3
"""Actual resource bitmap/parser glue with bounded native and memory doubles."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
code=r"""
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef int16_t i16;
typedef int GR_WINDOW_ID;typedef int GR_GC_ID;
typedef struct{int cols,rows;}GR_SCREEN_INFO;
typedef struct{int count;struct{u8 r,g,b,pad;}palette[256];}GR_PALETTE;
static GR_PALETTE native_palette,scratch;
#define W16_SCRATCH 2048
#define w16_scratch (&scratch)
static void GrGetSystemPalette(GR_PALETTE*p){*p=native_palette;}
typedef struct{int foreground,mode;}GR_GC_INFO;
struct win{int cw,ch;};struct dc{struct win*w;int wid,gc;u32 bkcolor,textcolor;};
static struct win win={100,100};static struct dc dest={&win,1,2,0xffffff,0x123456};
struct module{u16 dgroup;}mod={5};static struct module*w16_mod=&mod;
static u16 args[10];
#define AW(n) args[(n)/2]
#define AL(n) ((u32)AW(n)|((u32)AW((n)+2)<<16))
#define GR_MODE_COPY 0
#define MWROP_COPY 0
#define GR_RGB(r,g,b) ((r)|((g)<<8)|((b)<<16))
static u8 resource[20000],rams[16][16384],output[65536];static u16 length;
#define ram rams[0]
static int segused[16],pixused[16];
static int outputwrites;
static void fcopy_out(u16 seg,u16 off,const void*p,u16 n){
 assert(seg==2&&(u32)off+n<=65536);memcpy(output+off,p,n);outputwrites++;
}
static int seglive,pixlive,gclive,segfail,pixfail,gcfail,readfail,readcount;
static u32 surface[10000],pixdata[640*480],foreground=77;static int mode=3;
static int checks;
static struct dc*dc_get(u16 h){return h==9?&dest:0;}
static int rgb_to_gr(u32 c){return c;}
static void res_name(u16 a,u16 b,char*n,u16*i){n[0]=0;*i=1;}
static int ne_res_find(struct module*m,int t,char*n,u16 id,u32*o,u16*l){*o=0;*l=length;return 0;}
static int ne_read(struct module*m,u32 o,void*p,u16 n){
 if(++readcount==readfail)return -1;
 if(o+n>length)return -1;memcpy(p,resource+o,n);return 0;
}
static int seg_alloc(u16 n,u16*s){int i;if(segfail)return -1;assert(n*16<=sizeof ram);for(i=0;i<16&&segused[i];i++);assert(i<16);segused[i]=1;seglive++;*s=i+1;memset(rams[i],0xa5,sizeof ram);return 0;}
static void seg_free(u16 s){assert(s>=1&&s<=16&&segused[s-1]);segused[s-1]=0;seglive--;}
static void fzero(u16 s,u16 o,u16 n){assert(s>=1&&s<=16&&segused[s-1]&&o+n<=sizeof ram);memset(rams[s-1]+o,0,n);}
static u8 rd8(u16 s,u16 o){assert(s>=1&&s<=16&&segused[s-1]&&o<sizeof ram);return rams[s-1][o];}
static void wr8(u16 s,u16 o,u8 v){assert(s>=1&&s<=16&&segused[s-1]&&o<sizeof ram);rams[s-1][o]=v;}
static int GrNewPixmap(int w,int h,void*p){assert(!"unexpected native allocation");return 0;}
static void GrDestroyWindow(int p){assert(!"unexpected native destruction");}
static int GrNewGC(void){assert(!"unexpected GC allocation");return 0;}
static void GrDestroyGC(int g){assert(!"unexpected GC destruction");}
static void GrSetGCMode(int g,int m){mode=m;}
static void GrSetGCForeground(int g,int c){foreground=c;}
static void GrGetGCInfo(int g,GR_GC_INFO*i){i->foreground=foreground;i->mode=mode;}
static void GrGetScreenInfo(GR_SCREEN_INFO*i){i->cols=i->rows=100;}
static void GrPoint(int p,int g,int x,int y){assert(x>=0&&y>=0);if(p==3){assert(pixlive&&x<640&&y<480);pixdata[y*640+x]=foreground;}else{assert(p==1&&x<100&&y<100);surface[y*100+x]=foreground;}}
static void GrFillRect(int p,int g,int x,int y,int w,int h){int a,b;for(b=0;b<h;b++)for(a=0;a<w;a++)GrPoint(p,g,x+a,y+b);}
static void GrCopyArea(int p,int g,int dx,int dy,int w,int h,int s,int sx,int sy,int op){
 int x,y;assert(p==1&&s==3&&pixlive&&op==0&&sx>=0&&sy>=0);
 for(y=0;y<h;y++)for(x=0;x<w;x++){assert(dx+x>=0&&dx+x<100&&dy+y<100);surface[(dy+y)*100+dx+x]=pixdata[(sy+y)*640+sx+x];}
}
"""
code+=(root/'src/dib.h').read_text().replace('#include "w16.h"','')
code+=(root/'src/dib.c').read_text().replace('#include "dib.h"','')
code+=(root/'src/bitmap.c').read_text().replace('#include "wapi.h"','').replace('#include "dib.h"','')
code+=r"""
static void wordput(int o,int v){resource[o]=v;resource[o+1]=v>>8;}
static int pattern(int x,int y){return ((x*3+y*7)%11)<5;}
static void setup(int w,int h,int bpp){
 int stride=(w*bpp+31)/32*4,colors=1<<bpp,x,y,at;
 bitmap_shutdown();assert(!seglive&&!pixlive&&!gclive);
 memset(resource,0,sizeof resource);wordput(0,40);wordput(4,w);wordput(8,h);wordput(12,1);wordput(14,bpp);
 resource[44]=resource[45]=resource[46]=255;
 for(y=0;y<h;y++)for(x=0;x<w;x++){
 at=40+colors*4+(h-1-y)*stride+x*bpp/8;
 resource[at]|=pattern(x,y)<<(8-bpp-(x%(8/bpp))*bpp);
 }
 length=40+colors*4+h*stride;segfail=pixfail=gcfail=readfail=readcount=0;
 memset(args,0,sizeof args);args[2]=5;
}
static u16 load(void){args[2]=5;return u_LoadBitmap(0,0);}
static void clean(void){int i;assert(!seglive&&!pixlive&&!gclive);for(i=0;i<NB;i++)assert(!bitmaps[i].used&&!bitmaps[i].seg);}
static u16 dcnew(void){args[0]=0;return g_CreateCompatibleDC(0,0);}
static int blit(u16 dc,int dx,int dy,int sx,int sy,int w,int h,u32 rop){
 args[0]=rop;args[1]=rop>>16;args[2]=sy;args[3]=sx;args[4]=dc;args[5]=h;args[6]=w;args[7]=dy;args[8]=dx;args[9]=9;return g_BitBlt(0,0);
}
static u16 compatible(u16 d,int w,int h){args[0]=h;args[1]=w;args[2]=d;return g_CreateCompatibleBitmap(0,0);}
static void object_check(u16 b,int w,int h,int depth){
 int stride=((w*depth+15)/16)*2;
 args[0]=100;args[1]=2;args[2]=14;args[3]=b;
 memset(output,0xa5,sizeof output);assert(g_GetObject(0,0)==14);
 assert(output[100]==0&&output[101]==0);
 assert((output[102]|output[103]<<8)==w&&(output[104]|output[105]<<8)==h);
 assert((output[106]|output[107]<<8)==stride&&output[108]==1&&output[109]==depth);
 assert(!output[110]&&!output[111]&&!output[112]&&!output[113]&&output[114]==0xa5);
}
int main(void){
 int widths[]={1,7,8,9,15,16,17,31,32,33,71},k,x,y,n;
 u16 b,d,e;
 native_palette.count=16;for(k=0;k<16;k++){native_palette.palette[k].r=k*13;native_palette.palette[k].g=k*7;native_palette.palette[k].b=k*17;}
 for(k=0;k<11;k++){
 setup(widths[k],96,1);b=load();assert(b==BBASE&&seglive&&!pixlive);
 for(y=0;y<96;y++)for(x=0;x<bitmaps[0].stride*8;x++){
 assert(((ram[y*bitmaps[0].stride+x/8]>>(7-x%8))&1)==(x<widths[k]?pattern(x,y):0));checks++;}
 for(n=bitmaps[0].stride*96;n<sizeof ram;n++)assert(ram[n]==0xa5);
 }
 setup(71,96,1);segfail=1;assert(!load());clean();
 for(k=1;k<10;k++){setup(71,96,1);readfail=k;assert(!load());clean();}
 setup(71,96,1);resource[40]=1;assert(!load());clean();
 setup(71,96,4);segfail=1;assert(!load());clean();
 for(k=19;k<=306;k+=287){setup(71,96,4);readfail=k;assert(!load());clean();}
 setup(71,96,4);b=load();assert(b&&seglive&&!pixlive&&!gclive);
 for(y=0;y<96;y++)for(x=0;x<71;x++)assert(((ram[y*36+x/2]>>((x&1)?0:4))&15)==pattern(x,y));
 d=dcnew();e=dcnew();assert(d&&e&&d!=e);
 assert(bitmap_select(d,b)==DEFAULT_BITMAP);assert(!bitmap_select(e,b));assert(!bitmap_delete(b));
 assert(bitmap_select(d,DEFAULT_BITMAP)==b);assert(bitmap_select(e,b)==DEFAULT_BITMAP);
 assert(bitmap_deletedc(e)==1);assert(bitmap_delete(b)==1);clean();
 setup(71,96,1);b=load();d=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
 memset(surface,0xaa,sizeof surface);foreground=77;mode=3;
 assert(blit(d,-3,4,2,1,15,7,0x00cc0020));
 for(y=0;y<7;y++)for(x=0;x<12;x++)assert(surface[(y+4)*100+x]==(pattern(x+5,y+1)?dest.bkcolor:dest.textcolor));
 assert(foreground==77&&mode==3);assert(surface[4*100+12]==0xaaaaaaaa);
 assert(!blit(d,0,0,0,0,3,3,0));assert(!blit(d,0,0,0,0,0,3,0x00cc0020));
 assert(blit(d,150,0,0,0,3,3,0x00cc0020));
 args[9]=d;assert(!g_BitBlt(0,0)); /* no memory destinations */

 /* Both source and destination clipping preserve translation for both formats. */
 for(k=1;k<=4;k+=3){
  int dx,dy,sx,sy,a,c;
  setup(71,96,k);b=load();d=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
  for(dx=-5;dx<=95;dx+=50)for(dy=-3;dy<=97;dy+=50)
  for(sx=-4;sx<=68;sx+=36)for(sy=-2;sy<=94;sy+=48){
   memset(surface,0xaa,sizeof surface);assert(blit(d,dx,dy,sx,sy,17,9,0x00cc0020));
   for(y=0;y<100;y++)for(x=0;x<100;x++){
    u32 expected=0xaaaaaaaa;a=x-dx;c=y-dy;
    if(a>=0&&a<17&&c>=0&&c<9&&sx+a>=0&&sx+a<71&&sy+c>=0&&sy+c<96)
     expected=k==1?(pattern(sx+a,sy+c)?dest.bkcolor:dest.textcolor):(pattern(sx+a,sy+c)?0xffffff:0);
    assert(surface[y*100+x]==expected);
   }
  }
 }

 /* Win16 BITMAP packed layout and full far-pointer write boundary. */
 for(k=1;k<=4;k+=3){
  int count,off,bad;u8 expected[14]={0,0,71,0,96,0,0,0,1,0,0,0,0,0};
  setup(71,96,k);b=load();expected[6]=k==1?10:36;expected[9]=k;
  for(off=0;off<=65522;off+=65522){
   memset(output,0xa5,sizeof output);outputwrites=0;
   args[0]=off;args[1]=2;args[2]=14;args[3]=b;
   assert(g_GetObject(0,0)==14&&outputwrites==1);
   for(n=0;n<65536;n++)assert(output[n]==(n>=off&&n<off+14?expected[n-off]:0xa5));
  }
  /* Larger cbBuffer does not authorize writing beyond the 14-byte structure. */
  memset(output,0xa5,sizeof output);outputwrites=0;
  args[0]=100;args[1]=2;args[2]=32767;args[3]=b;
  assert(g_GetObject(0,0)==14&&outputwrites==1);
  assert(!memcmp(output+100,expected,14)&&output[114]==0xa5);
  for(bad=0;bad<7;bad++){
   memset(output,0xa5,sizeof output);outputwrites=0;
   args[0]=100;args[1]=2;args[2]=14;args[3]=b;
   if(bad==0)args[3]=b+1;
   if(bad==1)args[3]=0;
   if(bad==2)args[2]=13;
   if(bad==3)args[2]=0;
   if(bad==4)args[2]=0xffff;
   if(bad==5)args[0]=65523;
   if(bad==6)args[1]=0;
   assert(!g_GetObject(0,0)&&!outputwrites);
   for(n=0;n<65536;n++)assert(output[n]==0xa5);
  }
 }


 /* Packed color rows are MS-nibble-first, WORD-aligned and zero padded. */
 for(k=0;k<11;k++){
  int stride=((widths[k]*4+15)/16)*2;
  setup(widths[k],96,4);b=load();assert(b);
  for(y=0;y<96;y++)for(x=0;x<stride*2;x++){
   assert(((ram[y*stride+x/2]>>((x&1)?0:4))&15)==(x<widths[k]?pattern(x,y):0));
  }
  for(n=stride*96;n<sizeof ram;n++)assert(ram[n]==0xa5);
 }
 /* Non-EGA resource colors and all 16 indices survive storage and output. */
 setup(71,96,4);
 for(k=0;k<16;k++){resource[40+4*k]=k*11;resource[41+4*k]=k*7;resource[42+4*k]=k*13;}
 for(y=0;y<96;y++)for(x=0;x<71;x++){
  int at=104+(95-y)*36+x/2,shift=(x&1)?0:4,index=(x+3*y)&15;
  resource[at]=(resource[at]&~(15<<shift))|(index<<shift);
 }
 b=load();d=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
 memset(resource,0,sizeof resource); /* resource palette no longer borrowed */
 assert(blit(d,0,0,0,0,71,96,0x00cc0020));
 for(y=0;y<96;y++)for(x=0;x<71;x++){
  int index=(x+3*y)&15;
  assert(((ram[y*36+x/2]>>((x&1)?0:4))&15)==index);
  assert(surface[y*100+x]==(u32)(index*13|((index*7)<<8)|((index*11)<<16)));
 }
 /* Compatible colors own a snapshot of the actual native palette. */
 setup(1,1,1);b=compatible(9,71,96);assert(b);
 {struct dib_rgb saved[16];memcpy(saved,bitmaps[bi(b)].palette,sizeof saved);
  memset(&native_palette.palette,0,sizeof native_palette.palette);
  assert(!memcmp(saved,bitmaps[bi(b)].palette,sizeof saved));
  d=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
  ram[0]=0x12;assert(blit(d,0,0,0,0,2,1,0x00cc0020));
  for(k=1;k<=2;k++)assert(surface[k-1]==(u32)(saved[k].r|((u32)saved[k].g<<8)|((u32)saved[k].b<<16)));
 }
 setup(1,1,1);
 for(k=0;k<3;k++){b=compatible(9,71,276);assert(b&&seglive==k+1&&!pixlive&&!gclive);object_check(b,71,276,4);
  for(n=0;n<36*276;n++)assert(rams[bitmaps[bi(b)].seg-1][n]==0);
  for(n=36*276;n<16384;n++)assert(rams[bitmaps[bi(b)].seg-1][n]==0xa5);
 }
 setup(1,1,1);native_palette.count=15;assert(!compatible(9,71,96));clean();
 native_palette.count=0;assert(!compatible(9,71,96));clean();native_palette.count=16;
 setup(1,1,1);b=compatible(9,71,96);assert(b&&seglive==1&&!pixlive);object_check(b,71,96,4);
 d=dcnew();e=compatible(d,71,96);assert(e&&seglive==2);object_check(e,71,96,1);
 assert(bitmap_select(d,e)==DEFAULT_BITMAP);
 {u16 f=compatible(d,17,9);assert(f&&seglive==3);object_check(f,17,9,1);assert(bitmap_delete(f)==1);}
 assert(bitmap_select(d,b)==e);
 {u16 f=compatible(d,17,9);assert(f&&seglive==3&&!pixlive);object_check(f,17,9,4);assert(bitmap_delete(f)==1);}
 assert(bitmap_select(d,DEFAULT_BITMAP)==b);bitmap_shutdown();clean();
 for(k=0;k<3;k++){
  setup(1,1,1);b=compatible(9,k==1?71:0,k==0?96:0);
  assert(b&&seglive==1&&!pixlive);object_check(b,1,1,1);
 }
 setup(1,1,1);d=dcnew();
 assert(!compatible(0,1,1)&&!compatible(123,1,1));
 assert(!compatible(9,-1,1)&&!compatible(9,1,-1));
 assert(!compatible(9,641,1)&&!compatible(9,1,481));
 assert(!compatible(9,640,52)); /* color 320*52 >16KiB */
 assert(!compatible(d,640,205)); /* mono 80*205 >16KiB */
 clean();
 segfail=1;assert(!compatible(d,71,96));clean();segfail=0;
 segfail=1;assert(!compatible(9,71,96));clean();segfail=0;
 b=compatible(9,640,51);assert(b);object_check(b,640,51,4);assert(bitmap_delete(b)==1);
 b=compatible(d,640,204);assert(b);object_check(b,640,204,1);assert(bitmap_delete(b)==1);
 for(k=0;k<NB;k++){b=compatible(d,1,1);assert(b);}assert(!compatible(d,1,1));

 bitmap_shutdown();clean();
 {u32 old=0xdeadbeef;
 d=dcnew();e=dcnew();
 assert(bitmap_brushorg(d,-32768,32767,&old)&&old==0);
 assert(bitmap_brushorg(e,-1,1,&old)&&old==0);
 assert(bitmap_brushorg(d,23,-29,&old)&&old==0x7fff8000);
 assert(bitmap_brushorg(e,0,0,&old)&&old==0x0001ffff);
 assert(bitmap_deletedc(d)==1);old=0xdeadbeef;
 assert(!bitmap_brushorg(d,1,2,&old)&&old==0xdeadbeef);
 d=dcnew();assert(bitmap_brushorg(d,1,2,&old)&&old==0);
 bitmap_shutdown();d=dcnew();assert(bitmap_brushorg(d,1,2,&old)&&old==0);
 }
 bitmap_shutdown();clean();printf("bitmap glue: %d mono bits/padding plus color, cleanup, selection, clipping and GetObject far-write checks passed\n",checks);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
