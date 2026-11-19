#!/usr/bin/env python3
"""Actual resource bitmap/parser glue with bounded native and memory doubles."""
from pathlib import Path
import subprocess,tempfile,os,struct
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
static GR_PALETTE native_palette;
#define W16_SCRATCH 2049
static union {u16 align;u8 bytes[W16_SCRATCH];}scratch;
#define w16_scratch (scratch.bytes)
static void GrGetSystemPalette(GR_PALETTE*p){*p=native_palette;}
typedef u16 GR_BITMAP;
typedef struct{int foreground,background,mode,usebackground,fgispixelval,bgispixelval;}GR_GC_INFO;
struct win{int cw,ch;};struct dc{struct win*w;int wid,gc;u32 bkcolor,textcolor;u8 stretchmode;u16 mapmode,palette;i16 wx,wy,vx,vy,wex,wey,vex,vey,brushx,brushy;u16 brush;};
static struct win win={100,100};static struct dc dest={&win,1,2,0xffffff,0x123456};
struct module{u16 dgroup;}mod={5};static struct module*w16_mod=&mod;
static u16 args[14];
#define AW(n) args[(n)/2]
#define AL(n) ((u32)AW(n)|((u32)AW((n)+2)<<16))
#define GR_MODE_COPY 0
#define GR_MODE_AND 1
#define MWROP_COPY 0
#define GR_RGB(r,g,b) ((r)|((g)<<8)|((b)<<16))
static u8 resource[65536],rams[32][16384],output[65536];static u16 length;
#define ram rams[0]
static int segused[32],pixused[16];
static int outputwrites;
static void fcopy_out(u16 seg,u16 off,const void*p,u16 n){
 if(seg==99){assert((u32)off+n<=65536);memcpy(output+off,p,n);outputwrites++;}
 else{assert(seg>=1&&seg<=32&&segused[seg-1]&&(u32)off+n<=16384);memcpy(rams[seg-1]+off,p,n);}
}
static int failafter=-1,maxallocbytes=16384;
static int seglive,pixlive,gclive,segfail,pixfail,gcfail,readfail,readcount;
static u32 surface[640*480],pixdata[640*480],foreground=77;static int mode=3;
static int background=55,usebackground,pixel_flags,requests,bitmap_requests,max_payload;

#define GR_MODE_HAVE_CAPTUREPIXELS 0x4000
static int capture_fail_call;
static int capture_mode,capture_calls,capture_x,capture_y,capture_w,capture_h;
static int GrCapturePixels(int wid,int gc,int x,int y,int w,int h,
 void(*callback)(void*,unsigned,const unsigned char*,unsigned),void*ctx){
 unsigned row,col,len=(w+1)/2;u8 packed[320];
 assert(wid==1&&gc==2&&w>0&&w<=640&&h>0&&h<=480);
 capture_calls++;capture_x=x;capture_y=y;capture_w=w;capture_h=h;
 if(capture_mode==1||capture_calls==capture_fail_call||len*h>16384)return 0;
 for(row=0;row<(unsigned)h;row++){
  if(capture_mode==2&&row==(unsigned)h-1)break;
  memset(packed,0,sizeof packed);
  for(col=0;col<(unsigned)w;col++)packed[col/2]|=(capture_mode==8?surface[(y+row)*100+x+col]:((x+col+3*(y+row))&15))<<((col&1)?0:4);
  callback(ctx,capture_mode==3&&(row==1||h==1)?row+1:row,packed,capture_mode==4&&(row==1||h==1)?len-1:len);
 }
 if(capture_mode==6)callback(ctx,h,packed,len);
 if(capture_mode==7)memset(native_palette.palette,0,sizeof native_palette.palette);
 return capture_mode!=5;
}
static int checks,indexed,capability;
static int nearest(u32 c){int i,best=0;long closest=0x7fffffff;
 for(i=0;i<16;i++){long r=(long)(c&255)-native_palette.palette[i].r,g=(long)((c>>8)&255)-native_palette.palette[i].g,b=(long)((c>>16)&255)-native_palette.palette[i].b,d=r*r+g*g+b*b;if(d<closest){closest=d;best=i;}}return best;
}
static struct dc*dc_get(u16 h){return h==9?&dest:0;}
static int palette_object(u16 h,i16 n,u16 seg,u16 off){return h==0x031e&&n==2&&seg==9&&off==7?2:0;}
static int palette_entry(u16 h,u16 index,u8*out){
 if(h!=0xe00||index>=16)return 0;
 if(index==15){out[0]=3;out[1]=out[2]=0;out[3]=2;}
 else{out[0]=native_palette.palette[index].r;out[1]=native_palette.palette[index].g;out[2]=native_palette.palette[index].b;out[3]=0;}
 return 1;
}
static int rgb_to_gr(u32 c){return c;}
static void res_name(u16 a,u16 b,char*n,u16*i){n[0]=0;*i=1;}
static int ne_res_find(struct module*m,int t,char*n,u16 id,u32*o,u16*l){*o=0;*l=length;return 0;}
static int ne_read(struct module*m,u32 o,void*p,u16 n){
 if(++readcount==readfail)return -1;
 if(o+n>length)return -1;memcpy(p,resource+o,n);return 0;
}
static int seg_alloc(u16 n,u16*s){int i;if(segfail||failafter==0||n*16>maxallocbytes)return -1;if(failafter>0)failafter--;assert(n*16<=sizeof ram);for(i=0;i<32&&segused[i];i++);if(i==32)return -1;segused[i]=1;seglive++;*s=i+1;memset(rams[i],0xa5,sizeof ram);return 0;}
static void seg_free(u16 s){assert(s>=1&&s<=32&&segused[s-1]);segused[s-1]=0;seglive--;}
static void fzero(u16 s,u16 o,u16 n){assert(s>=1&&s<=32&&segused[s-1]&&o+n<=sizeof ram);memset(rams[s-1]+o,0,n);}
static u8 rd8(u16 s,u16 o){if(s==99)return output[o];assert(s>=1&&s<=32&&segused[s-1]&&o<sizeof ram);return rams[s-1][o];}
static void wr8(u16 s,u16 o,u8 v){if(s==99){output[o]=v;return;}assert(s>=1&&s<=32&&segused[s-1]&&o<sizeof ram);rams[s-1][o]=v;}
static int GrNewPixmap(int w,int h,void*p){assert(!"unexpected native allocation");return 0;}
static void GrDestroyWindow(int p){assert(!"unexpected native destruction");}
static int GrNewGC(void){assert(!"unexpected GC allocation");return 0;}
static void GrDestroyGC(int g){assert(!"unexpected GC destruction");}
static void GrSetGCMode(int g,int m){requests++;mode=m&~0x2000;}
static void GrSetGCForeground(int g,int c){requests++;foreground=c;}
static void GrSetGCBackground(int g,int c){requests++;background=c;}
static void GrSetGCUseBackground(int g,int v){requests++;usebackground=v;}
static void GrGetGCInfo(int g,GR_GC_INFO*i){requests++;i->foreground=foreground;i->mode=mode|capability;i->background=background;i->usebackground=usebackground;i->fgispixelval=pixel_flags&1;i->bgispixelval=(pixel_flags>>1)&1;}
static void GrGetScreenInfo(GR_SCREEN_INFO*i){i->cols=i->rows=100;}
static void GrPoint(int p,int g,int x,int y){assert(x>=0&&y>=0);if(p==3){assert(pixlive&&x<640&&y<480);pixdata[y*640+x]=foreground;}else{assert(p==1&&x<100&&y<100);surface[y*100+x]=indexed?(mode==GR_MODE_AND?(surface[y*100+x]&nearest(foreground)):nearest(foreground)):foreground;}}
static void GrFillRect(int p,int g,int x,int y,int w,int h){int a,b;requests++;for(b=0;b<h;b++)for(a=0;a<w;a++)GrPoint(p,g,x+a,y+b);}
/* Model the existing server's COPY bitmap words, including opaque zero bits. */
static void GrBitmap(int p,int g,int x,int y,int w,int h,GR_BITMAP*bits){
 int xx,yy,fg=foreground,bytes=2*((w+15)/16)*h;
 assert(mode==GR_MODE_COPY&&bytes<=480&&bytes+16<=512);
 requests++;bitmap_requests++;if(bytes>max_payload)max_payload=bytes;
 for(yy=0;yy<h;yy++)for(xx=0;xx<w;xx++){
  int bit=(bits[yy*((w+15)/16)+xx/16]>>(15-xx%16))&1;
  if(bit||usebackground){foreground=bit?fg:background;GrPoint(p,g,x+xx,y+yy);}
 }
 foreground=fg;
}
static void GrCopyArea(int p,int g,int dx,int dy,int w,int h,int s,int sx,int sy,int op){
 int x,y;assert(p==1&&s==3&&pixlive&&op==0&&sx>=0&&sy>=0);
 for(y=0;y<h;y++)for(x=0;x<w;x++){assert(dx+x>=0&&dx+x<100&&dy+y<100);surface[(dy+y)*100+dx+x]=pixdata[(sy+y)*640+sx+x];}
}
"""
code+=(root/'src/dib.h').read_text().replace('#include "w16.h"','')
code+=(root/'src/dib.c').read_text().replace('#include "dib.h"','')
code+="static u16 rd16(u16 s,u16 o){return rd8(s,o)|((u16)rd8(s,o+1)<<8); }\nstatic void wr16(u16 s,u16 o,u16 v){wr8(s,o,v);wr8(s,o+1,v>>8); }\n"
code+='u32 bitmap_origin(u16,int,int,i16,i16);\n'
code+=(root/'src/mapping.c').read_text().replace('#include "wapi.h"','')
code+=(root/'src/polygon.c').read_text()
code+=(root/'src/oval.c').read_text()
gdi=(root/'src/gdi.c').read_text()
code+=r"""
#define OBJ_BRUSH 2
#define BS_PATTERN 3
#define MAXBMP 4
struct gobj {int type,null,style,width;u32 color;};
static struct gobj testbrush={OBJ_BRUSH,0,0,0,0};
static struct {u8 used,w,h,mono;u16 rows[16];u32 colors[2];} bmps[MAXBMP];
static struct gobj*obj_get(u16 h){return h==1?&testbrush:0;}
"""
code+=gdi[gdi.index('int gdi_pattern_color('):gdi.index('/* Monochrome pattern phase')]
code+=r"""
typedef int GR_SIZE;
#define wlog(...) ((void)0)
static int opt_trace;
static u16 testfont_bits[224*13];static u8 testfont_width[224];
static u8 rasterfont_width(u8 c){return testfont_width[c<32?0:c-32];}
static u16 rasterfont_row(u8 c,int row){return testfont_bits[(c<32?0:c-32)*13+row];}
static void gdi_text_size(const char*s,int n,GR_SIZE*w,GR_SIZE*h,GR_SIZE*b){
 *w=0;while(n--){u8 c=*s++;*w+=testfont_width[c<32?0:c-32];}*h=13;*b=11;
}
"""
code+='#include <stdlib.h>\nstatic int flood_malloc_fail,flood_malloc_limit;\nstatic void*test_malloc(size_t n){return flood_malloc_fail||(flood_malloc_limit&&n>flood_malloc_limit)?0:malloc(n); }\n#define malloc test_malloc\n'
code+=(root/'src/bitmap.c').read_text().replace('#include "wapi.h"','').replace('#include "dib.h"','')
code+='\n#undef malloc\n'
gdi=(root/'src/gdi.c').read_text()
code+=gdi[gdi.index('u32 g_GetBkColor('):gdi.index('u32 g_SetBkColor(')]
code+=gdi[gdi.index('u32 g_GetBitmapBits('):gdi.index('/* CreatePatternBrush copies')]
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
static void clean(void){int i;assert(!seglive&&!pixlive&&!gclive);for(i=0;i<NB;i++)assert(!bitmaps[i].used&&!bitmaps[i].bank[0]);}
static u16 dcnew(void){args[0]=0;return g_CreateCompatibleDC(0,0);}
static int blit(u16 dc,int dx,int dy,int sx,int sy,int w,int h,u32 rop){
 args[0]=rop;args[1]=rop>>16;args[2]=sy;args[3]=sx;args[4]=dc;args[5]=h;args[6]=w;args[7]=dy;args[8]=dx;args[9]=9;return g_BitBlt(0,0);
}
static int memblit(u16 dst,u16 src,int dx,int dy,int sx,int sy,int w,int h,u32 rop){
 args[0]=rop;args[1]=rop>>16;args[2]=sy;args[3]=sx;args[4]=src;args[5]=h;args[6]=w;args[7]=dy;args[8]=dx;args[9]=dst;return g_BitBlt(0,0);
}
static u16 compatible(u16 d,int w,int h){args[0]=h;args[1]=w;args[2]=d;return g_CreateCompatibleBitmap(0,0);}
static void object_check(u16 b,int w,int h,int depth){
 int stride=((w*depth+15)/16)*2;
 args[0]=100;args[1]=99;args[2]=14;args[3]=b;
 memset(output,0xa5,sizeof output);assert(g_GetObject(0,0)==14);
 assert(output[100]==0&&output[101]==0);
 assert((output[102]|output[103]<<8)==w&&(output[104]|output[105]<<8)==h);
 if((output[106]|output[107]<<8)!=stride||output[109]!=depth)fprintf(stderr,"Object %d x %d expected depth%d actual%d stride%d/%d\n",w,h,depth,output[109],stride,output[106]|output[107]<<8);
 assert((output[106]|output[107]<<8)==stride&&output[108]==1&&output[109]==depth);
 assert(!output[110]&&!output[111]&&!output[112]&&!output[113]&&output[114]==0xa5);
}
static int stretch(u16 dc,int dx,int dy,int dw,int dh,int sx,int sy,int sw,int sh,u32 rop){
 args[0]=rop;args[1]=rop>>16;args[2]=sh;args[3]=sw;args[4]=sy;args[5]=sx;
 args[6]=dc;args[7]=dh;args[8]=dw;args[9]=dy;args[10]=dx;args[11]=9;
 return g_StretchBlt(0,0);
}
static void stretch_tests(void){
 int dw,dh,x,y,dx,dy;u16 b,d;u32 want;
 setup(13,11,1);b=load();d=dcnew();assert(bitmap_select(d,b));
 gdi_map_init(&dest);dest.stretchmode=1;args[0]=3;args[1]=9;assert(g_SetStretchBltMode(0,0)==1);
 args[0]=4;assert(!g_SetStretchBltMode(0,0)&&dest.stretchmode==3);
 args[0]=2;args[1]=d;assert(g_SetStretchBltMode(0,0)==1);
 for(dw=1;dw<=29;dw+=4)for(dh=1;dh<=25;dh+=3)
 for(dx=-7;dx<=98;dx+=21)for(dy=-3;dy<=99;dy+=17){
  for(x=0;x<10000;x++)surface[x]=0xabcdef;
  foreground=77;mode=3;
  assert(stretch(d,dx,dy,dw,dh,2,1,9,8,0x00cc0020UL));
  assert(foreground==77&&mode==3);
  for(y=0;y<100;y++)for(x=0;x<100;x++){
   want=0xabcdef;
   if(x>=dx&&x<dx+dw&&y>=dy&&y<dy+dh)
    want=pattern(2+(x-dx)*9/dw,1+(y-dy)*8/dh)?dest.bkcolor:dest.textcolor;
   assert(surface[y*100+x]==want);
  }
 }
 for(x=0;x<10000;x++)surface[x]=0xabcdef;
 assert(!stretch(d,0,0,10,10,0,0,13,11,0x008800c6UL));
 assert(!stretch(d,0,0,10,10,-1,0,13,11,0x00cc0020UL));
 assert(!stretch(d,0,0,-1,10,0,0,13,11,0x00cc0020UL));
 dest.stretchmode=1;assert(!stretch(d,0,0,10,10,0,0,13,11,0x00cc0020UL));
 for(x=0;x<10000;x++)assert(surface[x]==0xabcdef);
 bitmap_shutdown();clean();
 setup(13,11,4);b=load();d=dcnew();assert(bitmap_select(d,b));dest.stretchmode=3;
 for(x=0;x<16;x++){bitmaps[bi(b)].palette[x].r=x*13;bitmaps[bi(b)].palette[x].g=x*7;bitmaps[bi(b)].palette[x].b=x*17;}
 for(y=0;y<11;y++)for(x=0;x<13;x++)bitmap_put(&bitmaps[bi(b)],x,y,(x+y*3)&15);
 dest.mapmode=8;dest.wex=dest.wey=1;dest.vex=3;dest.vey=2;
 assert(stretch(d,0,0,13,11,0,0,13,11,0x00cc0020UL));
 for(y=0;y<22;y++)for(x=0;x<39;x++)assert(surface[y*100+x]==bitmap_rgb(&bitmaps[bi(b)],(x/3+(y/2)*3)&15));
 bitmap_shutdown();clean();
 gdi_map_init(&dest);
 /* The CGA path keeps both source rows in the exact 2:1 BLACKONWHITE reduction. */
 setup(71,96,1);b=load();d=dcnew();assert(bitmap_select(d,b));gdi_map_init(&dest);dest.stretchmode=1;
 for(y=0;y<96;y++)for(x=0;x<71;x++)bitmap_put(&bitmaps[bi(b)],x,y,(y&1)?((x+y/2)%5!=0):(x%3!=0));
 for(x=0;x<10000;x++)surface[x]=0xabcdef;
 assert(stretch(d,-2,-3,71,48,0,0,71,96,0x00cc0020UL));
 for(y=0;y<45;y++)for(x=0;x<69;x++)assert(surface[y*100+x]==(((x+2)%3!=0&&(x+2+y+3)%5!=0)?dest.bkcolor:dest.textcolor));
 dest.textcolor=0;dest.bkcolor=0xffffff;
 for(x=0;x<16;x++)native_palette.palette[x].r=native_palette.palette[x].g=native_palette.palette[x].b=x*17;
 for(x=0;x<10000;x++)surface[x]=7;
 capability=0;assert(!stretch(d,0,0,71,48,0,0,71,96,0x008800c6UL));
 for(x=0;x<10000;x++)assert(surface[x]==7);
 capability=0x2000;indexed=1;assert(stretch(d,0,0,71,48,0,0,71,96,0x008800c6UL));
 for(y=0;y<48;y++)for(x=0;x<71;x++)assert(surface[y*100+x]==((x%3!=0&&(x+y)%5!=0)?7:0));
 indexed=capability=0;bitmap_shutdown();clean();
 setup(71,96,4);b=load();d=dcnew();assert(bitmap_select(d,b));gdi_map_init(&dest);dest.stretchmode=1;
 for(x=0;x<16;x++)bitmaps[bi(b)].palette[x].r=bitmaps[bi(b)].palette[x].g=bitmaps[bi(b)].palette[x].b=x*17;
 for(y=0;y<96;y++)for(x=0;x<71;x++)bitmap_put(&bitmaps[bi(b)],x,y,(y&1)?((x+y/2)&15):(x&15));
 assert(stretch(d,0,0,71,48,0,0,71,96,0x00cc0020UL));
 for(y=0;y<48;y++)for(x=0;x<71;x++){u32 c=((x&15)&((x+y)&15))*17;assert(surface[y*100+x]==(c|(c<<8)|(c<<16)));}
 bitmap_shutdown();clean();
 puts("StretchBlt mono/color pixels, scaling, clipped phase, state and atomic refusals passed");
}
/* Force the unchanged run fallback through the pixel-value guard, then compare
 * every destination pixel and count API requests on identical synthetic input. */
static void stretch_batch_tests(void){
 static u32 reference[10000];u16 b,d;int x,y,i,k,oldcalls,clipped,depth,mono;
 const int widths[]={1,15,16,17,71};
 for(depth=1;depth<=4;depth+=3)for(k=0;k<5;k++)for(clipped=0;clipped<2;clipped++){
  int w=widths[k],h=48,dx=clipped?-(w>3?3:w-1):2,dy=clipped?-5:3;
  setup(w,96,depth);b=load();d=dcnew();assert(bitmap_select(d,b));gdi_map_init(&dest);dest.stretchmode=1;
  if(depth==4){bitmaps[bi(b)].palette[0]=(struct dib_rgb){255,0,0};bitmaps[bi(b)].palette[1]=(struct dib_rgb){255,255,255};}
  for(y=0;y<96;y++)for(x=0;x<w;x++)bitmap_put(&bitmaps[bi(b)],x,y,((x*7+y*11)%13)<7);
  dest.textcolor=0x193847;dest.bkcolor=0xaccdef;
  for(i=0;i<10000;i++)surface[i]=0xabcdef;
  foreground=77;background=55;usebackground=clipped;mode=3;pixel_flags=1;requests=bitmap_requests=0;
  assert(stretch(d,dx,dy,w,h,0,0,w,96,0x00cc0020UL));
  oldcalls=requests;assert(!bitmap_requests);memcpy(reference,surface,sizeof reference);
  for(i=0;i<10000;i++)surface[i]=0xabcdef;
  pixel_flags=0;requests=bitmap_requests=max_payload=0;
  assert(stretch(d,dx,dy,w,h,0,0,w,96,0x00cc0020UL));
  assert(!memcmp(reference,surface,sizeof reference));
  assert(foreground==77&&background==55&&usebackground==clipped&&mode==3);
  assert(bitmap_requests==1&&requests==10);
  if(w==71)printf("71x96->71x48 %dbpp %s: fallback %d requests; batch %d, payload %d bytes; identical pixels\n",depth,clipped?"clipped":"full",oldcalls,requests,max_payload);
  bitmap_shutdown();clean();
 }
 /* Seven indices as observed in the default card resource; compare with
  * the unchanged run fallback using the pixel-value guard. */
 for(mono=0;mono<2;mono++)for(clipped=0;clipped<2;clipped++){
  static const u8 colors[7]={0,1,2,3,6,7,15};int dx=clipped?-3:2,dy=clipped?-5:3;
  indexed=mono;for(i=0;i<16;i++)native_palette.palette[i].r=native_palette.palette[i].g=native_palette.palette[i].b=i?255:0;
  setup(71,96,4);b=load();d=dcnew();assert(bitmap_select(d,b));gdi_map_init(&dest);dest.stretchmode=1;
  for(y=0;y<96;y++)for(x=0;x<71;x++)bitmap_put(&bitmaps[bi(b)],x,y,colors[(x*((y/2)<38?16:15)/71)%7]);
  for(i=0;i<16;i++){bitmaps[bi(b)].palette[i].r=i*13;bitmaps[bi(b)].palette[i].g=i*7;bitmaps[bi(b)].palette[i].b=i*17;}
  for(i=0;i<10000;i++)surface[i]=0xabcdef;
  foreground=77;background=55;usebackground=clipped;mode=3;pixel_flags=1;requests=bitmap_requests=0;
  assert(stretch(d,dx,dy,71,48,0,0,71,96,0x00cc0020UL));oldcalls=requests;
  assert(!bitmap_requests);memcpy(reference,surface,sizeof reference);
  for(i=0;i<10000;i++)surface[i]=0xabcdef;
  pixel_flags=0;requests=bitmap_requests=max_payload=0;
  assert(stretch(d,dx,dy,71,48,0,0,71,96,0x00cc0020UL));
  assert(!memcmp(reference,surface,sizeof reference));
  assert(foreground==77&&background==55&&usebackground==clipped&&mode==3);
  assert(bitmap_requests<=14&&requests<=34&&max_payload<=480);
  printf("Seven-index card %s %s: %d -> %d requests (%d masks), payload <=%d; identical pixels\n",mono?"mono-native":"RGB",clipped?"clipped":"full",oldcalls,requests,bitmap_requests,max_payload);
  bitmap_shutdown();clean();
 }
 indexed=0;
 /* Two bounded packets rather than asking the client to split an oversized one. */
 setup(71,192,1);b=load();d=dcnew();assert(bitmap_select(d,b));gdi_map_init(&dest);dest.stretchmode=1;
 requests=bitmap_requests=max_payload=0;assert(stretch(d,0,0,71,96,0,0,71,192,0x00cc0020UL));
 assert(bitmap_requests==2&&max_payload==480);bitmap_shutdown();clean();
 /* A third color appearing in the final row takes tiled color masks,
  * preserving the exact palette across tile boundaries. */
 setup(71,192,4);b=load();d=dcnew();assert(bitmap_select(d,b));gdi_map_init(&dest);dest.stretchmode=1;
 for(y=0;y<192;y++)for(x=0;x<71;x++)bitmap_put(&bitmaps[bi(b)],x,y,(x+y)&1);
 bitmap_put(&bitmaps[bi(b)],70,190,7);bitmap_put(&bitmaps[bi(b)],70,191,7);
 requests=bitmap_requests=0;assert(stretch(d,0,0,71,96,0,0,71,192,0x00cc0020UL));assert(bitmap_requests>1);
 for(y=0;y<96;y++)for(x=0;x<71;x++)assert(surface[y*100+x]==bitmap_rgb(&bitmaps[bi(b)],bitmap_index(&bitmaps[bi(b)],x,2*y)&bitmap_index(&bitmaps[bi(b)],x,2*y+1)));
 bitmap_shutdown();clean();
 /* AND retains the exact indexed run path; no bitmap background prefill. */
 setup(71,96,1);b=load();d=dcnew();assert(bitmap_select(d,b));gdi_map_init(&dest);dest.stretchmode=1;
 capability=0x2000;indexed=1;dest.textcolor=0;dest.bkcolor=0xffffff;
 for(i=0;i<16;i++)native_palette.palette[i].r=native_palette.palette[i].g=native_palette.palette[i].b=i*17;
 for(i=0;i<10000;i++)surface[i]=7;
 requests=bitmap_requests=0;assert(stretch(d,0,0,71,48,0,0,71,96,0x008800c6UL));assert(!bitmap_requests);
 for(y=0;y<48;y++)for(x=0;x<71;x++)assert(surface[y*100+x]==((bitmap_index(&bitmaps[bi(b)],x,2*y)&bitmap_index(&bitmaps[bi(b)],x,2*y+1))?7:0));
 indexed=capability=0;bitmap_shutdown();clean();
 puts("StretchBlt batching: bit boundaries, clipped phase, opaque colors, GC restore, packet split and AND fallback passed");
}
static void origin_tests(void){
 u16 d,e,b,c;int x,y;native_palette.count=16;for(x=0;x<16;x++){native_palette.palette[x].r=x*17;native_palette.palette[x].g=x*7;native_palette.palette[x].b=x*3;}
 d=dcnew();e=dcnew();b=bitmap_create(16,8,1,4,0,0);c=bitmap_create(16,8,1,4,0,0);
 assert(bitmap_select(d,b)&&bitmap_select(e,c));
 for(y=0;y<8;y++)for(x=0;x<16;x++)bitmap_put(&bitmaps[bi(b)],x,y,(x+y)&15);
 args[0]=300;args[1]=20;args[2]=d;assert(g_SetWindowOrg(0,0)==0);
 args[0]=d;assert(g_GetWindowOrg(0,0)==((u32)300<<16|20));
 bitmap_origin(e,1,0,40,400);assert(bitmap_savedc(e)==1);bitmap_origin(e,1,1,2,1);
 assert(memblit(e,d,40,400,20,300,8,4,0xcc0020));
 for(y=0;y<4;y++)for(x=0;x<8;x++)assert(bitmap_index(&bitmaps[bi(c)],x+2,y+1)==((x+y)&15));
 assert(bitmap_restoredc(e,-1));assert(bitmap_origin(e,0,1,0,0)==0);
 assert(bitmap_origin(e,0,0,0,0)==((u32)400<<16|40));
 assert(bitmap_patblt(e,42,402,3,2,0xff0062,0,0,0));
 for(y=2;y<4;y++)for(x=2;x<5;x++)assert(bitmap_index(&bitmaps[bi(c)],x,y)==15);
 bitmap_shutdown();clean();puts("Memory origin BitBlt/PatBlt and SaveDC restoration passed");
}
static void dib_band_test(void){
 u16 b,s,t,start,n,x,y;int i,live;u8 saved;
 for(i=0;i<16;i++){native_palette.palette[i].r=i*17;native_palette.palette[i].g=255-i*17;native_palette.palette[i].b=(i^7)*17;}
 native_palette.count=16;b=bitmap_create(640,480,1,4,0,0);assert(b);
 assert(!seg_alloc(1024,&s)&&!seg_alloc(1024,&t));
 memset(args,0,sizeof args);args[8]=9;args[7]=b;args[2]=s;
 fzero(s,0,104);dib_wlong(s,0,40);assert(g_GetDIBits(0,0)==480);
 assert(rd16(s,14)==4&&dib_long(s,20)==153600);
 assert(g_GetDIBits(0,0)==480);
 for(i=0;i<16;i++){assert(rd8(s,40+4*i)==native_palette.palette[i].b);assert(rd8(s,42+4*i)==native_palette.palette[i].r);}
 args[4]=t;live=seglive;
 for(start=0;start<480;start+=n){
  n=480-start;if(n>17)n=17;args[6]=start;args[5]=n;
  for(y=0;y<n;y++)for(x=0;x<320;x++)wr8(t,y*320+x,(((2*x+start+y)&15)<<4)|((2*x+start+y+1)&15));
  assert(g_SetDIBits(0,0)==n);assert(seglive==live);
 }
 for(start=0;start<480;start+=n){
  n=480-start;if(n>23)n=23;args[6]=start;args[5]=n;
  memset(rams[t-1],0xa5,16384);assert(g_GetDIBits(0,0)==n);
  for(y=0;y<n;y++)for(x=0;x<320;x++)assert(rd8(t,y*320+x)==((((2*x+start+y)&15)<<4)|((2*x+start+y+1)&15)));
  assert(rd8(t,n*320)==0xa5);
 }
 args[6]=0;args[5]=480;assert(!g_GetDIBits(0,0)); /* far span >64KiB */
 args[5]=1;args[3]=65500;assert(!g_SetDIBits(0,0));args[3]=0;
 dib_wlong(s,16,2);assert(!g_SetDIBits(0,0));dib_wlong(s,16,0);
 saved=bitmap_index(&bitmaps[bi(b)],0,479);dib_wlong(s,32,1);wr8(t,0,0xf0);
 assert(!g_SetDIBits(0,0));assert(bitmap_index(&bitmaps[bi(b)],0,479)==saved);
 /* CORE BGR triples and odd-width, DWORD-padded native rows. */
 release_bitmap(bi(b));b=bitmap_create(7,3,1,4,0,0);args[7]=b;args[5]=3;
 fzero(s,0,100);dib_wlong(s,0,12);assert(g_GetDIBits(0,0)==0); /* non-null bits needs header */
 args[4]=0;assert(g_GetDIBits(0,0)==3);assert(g_GetDIBits(0,0)==3);args[4]=t;
 for(y=0;y<3;y++){wr8(t,y*4,0x12);wr8(t,y*4+1,0x34);wr8(t,y*4+2,0x56);wr8(t,y*4+3,0x7f);}
 assert(g_SetDIBits(0,0)==3);assert(bitmap_index(&bitmaps[bi(b)],0,2)==1);
 assert(g_GetDIBits(0,0)==3);for(y=0;y<3;y++)assert(rd8(t,y*4+3)==0x70);
 /* WORD palette references resolve via the DC logical palette, including
  * PC_EXPLICIT native device references and invalid entry atomic refusal. */
 dest.palette=0xe00;args[0]=1;
 for(i=0;i<16;i++)wr16(s,12+i*2,i);
 assert(g_SetDIBits(0,0)==3);assert(bitmap_index(&bitmaps[bi(b)],0,2)==1);
 wr8(t,0,0xf2);assert(g_SetDIBits(0,0)==3);assert(bitmap_index(&bitmaps[bi(b)],0,2)==3);
 wr16(s,12+15*2,99);assert(!g_SetDIBits(0,0));assert(bitmap_index(&bitmaps[bi(b)],0,2)==3);
 args[0]=0;wr8(t,0,0x12);
 /* Reversed source RGB table must map colors, not blindly copy indices. */
 for(i=0;i<16;i++){wr8(s,12+i*3,native_palette.palette[15-i].b);wr8(s,13+i*3,native_palette.palette[15-i].g);wr8(s,14+i*3,native_palette.palette[15-i].r);}
 assert(g_SetDIBits(0,0)==3);assert(bitmap_index(&bitmaps[bi(b)],0,2)==14);
 release_bitmap(bi(b));b=bitmap_create(9,2,1,1,0,0);args[7]=b;args[5]=2;args[4]=0;
 fzero(s,0,104);dib_wlong(s,0,40);assert(g_GetDIBits(0,0)==2);assert(g_GetDIBits(0,0)==2);args[4]=t;
 wr8(t,0,0xa5);wr8(t,1,0xff);wr8(t,4,0x5a);wr8(t,5,0);
 assert(g_SetDIBits(0,0)==2);assert(g_GetDIBits(0,0)==2);
 assert(rd8(t,0)==0xa5&&rd8(t,1)==0x80&&rd8(t,2)==0&&rd8(t,3)==0&&rd8(t,4)==0x5a);
 release_bitmap(bi(b));seg_free(s);seg_free(t);
 puts("DIB 640x480 exact native palette and bounded bands, CORE, padding, atomic failures passed");
}


static void dib_create_device_test(void){
 u16 info,bits,b,d,c;int i,x,y,live;u32 oldfg;int oldmode;
 bitmap_shutdown();clean();
 native_palette.count=16;
 for(i=0;i<16;i++){native_palette.palette[i].r=i*17;native_palette.palette[i].g=i*7;native_palette.palette[i].b=i*3;}
 assert(!seg_alloc(1024,&info)&&!seg_alloc(1024,&bits));
 fzero(info,0,104);dib_wlong(info,0,40);dib_wlong(info,4,7);dib_wlong(info,8,3);
 wr16(info,12,1);wr16(info,14,4);
 for(i=0;i<16;i++){wr8(info,40+i*4,native_palette.palette[i].b);wr8(info,41+i*4,native_palette.palette[i].g);wr8(info,42+i*4,native_palette.palette[i].r);}
 for(y=0;y<3;y++){wr8(bits,y*4,0x12+y*0x11);wr8(bits,y*4+1,0x34);wr8(bits,y*4+2,0x56);wr8(bits,y*4+3,0x70);}
 memset(args,0,sizeof args);args[9]=9;args[8]=info;args[5]=4;args[4]=bits;args[2]=info;
 live=seglive;b=g_CreateDIBitmap(0,0);assert(b&&seglive==live+1);
 assert(bitmaps[bi(b)].w==7&&bitmaps[bi(b)].h==3&&bitmaps[bi(b)].bpp==4);
 for(y=0;y<3;y++)assert(bitmap_index(&bitmaps[bi(b)],0,y)==3-y);
 bitmap_delete(b);assert(seglive==live);
 /* Bad source index, compression, init bits, and a far wrap are atomic. */
 dib_wlong(info,32,1);assert(!g_CreateDIBitmap(0,0)&&seglive==live);dib_wlong(info,32,0);
 dib_wlong(info,16,1);assert(!g_CreateDIBitmap(0,0)&&seglive==live);dib_wlong(info,16,0);
 args[5]=8;assert(!g_CreateDIBitmap(0,0));args[5]=4;
 args[3]=65532;assert(!g_CreateDIBitmap(0,0)&&seglive==live);args[3]=0;
 args[5]=0;args[2]=args[4]=0;b=g_CreateDIBitmap(0,0);assert(b);bitmap_delete(b);
 /* Device draw: bottom-up DIB at 10,20, clipped scan band keeps its row. */
 memset(args,0,sizeof args);args[13]=9;args[12]=10;args[11]=20;args[10]=7;args[9]=3;
 args[5]=3;args[4]=bits;args[2]=info;oldfg=foreground;oldmode=mode;
 assert(g_SetDIBitsToDevice(0,0)==3&&seglive==live&&foreground==oldfg&&mode==oldmode);
 for(y=0;y<3;y++)assert(surface[(20+y)*100+10]==(u32)(native_palette.palette[3-y].r|((u32)native_palette.palette[3-y].g<<8)|((u32)native_palette.palette[3-y].b<<16)));
 memset(surface,0,sizeof(surface));args[6]=1;args[5]=1;args[3]=4;
 assert(g_SetDIBitsToDevice(0,0)==1&&surface[20*100+10]==0&&surface[22*100+10]==0);
 assert(surface[21*100+10]!=0&&seglive==live);
 /* Source crop: first two columns of bottom row go to top destination row. */
 memset(surface,0,sizeof(surface));args[6]=0;args[5]=3;args[3]=0;args[9]=1;
 assert(g_SetDIBitsToDevice(0,0)==1&&surface[20*100+10]==(u32)(17|((u32)7<<8)|((u32)3<<16)));
 /* Memory destination and allocation failure have the same no-leak contract. */
 d=dcnew();c=bitmap_create(20,30,1,4,0,0);assert(c&&bitmap_select(d,c));
 args[13]=d;assert(g_SetDIBitsToDevice(0,0)==1&&bitmap_index(&bitmaps[bi(c)],10,20)==1);
 live=seglive;failafter=0;assert(!g_SetDIBitsToDevice(0,0)&&seglive==live);failafter=-1;
 bitmap_shutdown();seg_free(info);seg_free(bits);clean();
 puts("CreateDIBitmap and SetDIBitsToDevice palette, orientation, scan bands, device/memory and rollback passed");
}
static void ddb_bits_test(void){
 u16 h;unsigned i;u32 n;
 native_palette.count=16;
 h=bitmap_create(256,256,1,4,0,0);assert(h);n=32768;
 for(i=0;i<n;i++)output[i]=(i*31)^17;
 assert(bitmap_bits(h,n,99,0,1)==n);
 memset(output,0,n);assert(bitmap_bits(h,n+10,99,0,0)==n);
 for(i=0;i<n;i++)assert(output[i]==(u8)((i*31)^17));
 assert(bitmap_bits(h,0,0,0,0)==n);
 assert(!bitmap_bits(h,n,99,65500,1));
 assert(!bitmap_bits(h,0xffffffffUL,99,0,1));
 assert(!bitmap_bits(0xffff,4,99,0,0));
 output[0]=0xe3;assert(bitmap_bits(h,1,99,0,1)==1);
 assert(bitmap_index(&bitmaps[bi(h)],0,0)==14);
 assert(bitmap_index(&bitmaps[bi(h)],1,0)==3);
 bitmap_shutdown();clean();
 h=bitmap_create(1,1,1,1,0,0);assert(h);
 args[0]=0;{u16 dc=g_CreateCompatibleDC(0,0);assert(dc);
 assert(bitmap_select(dc,h)==DEFAULT_BITMAP);
 assert(bitmap_select(dc,DEFAULT_BITMAP)==h);assert(bitmap_delete(h));bitmap_deletedc(dc);}
 h=bitmap_create(17,2,1,1,0,0);assert(h);
 assert(bitmap_bits(h,0,0,0,0)==8);bitmap_shutdown();clean();
 h=bitmap_create(640,480,1,4,0,0);assert(h);
 assert(bitmap_bits(h,0,0,0,0)==153600UL);
 assert(!bitmap_bits(h,153600UL,99,0,0));bitmap_shutdown();clean();
}
static void small_ddb_bits_test(void){
 u32 n;u16 dc,h,rows[16];u8 pw,ph;u32 colors[2];
 args[0]=0;dc=g_CreateCompatibleDC(0,0);assert(dc);args[0]=dc;
 assert(g_GetBkColor(0,0)==0xffffffUL);bitmap_setcolor(dc,0x123456,1);
 assert(g_GetBkColor(0,0)==0x123456);assert(bitmap_savedc(dc));
 bitmap_setcolor(dc,0xabcdef,1);assert(bitmap_restoredc(dc,-1));
 assert(g_GetBkColor(0,0)==0x123456);bitmap_shutdown();
 args[0]=0xffff;assert(g_GetBkColor(0,0)==0xffffffffUL);
 args[0]=9;dest.bkcolor=0x654321;assert(g_GetBkColor(0,0)==0x654321);
 h=bitmap_create(16,2,1,1,0,0);assert(h);
 output[20]=0x81;output[21]=0x23;output[22]=0xab;output[23]=0xcd;
 args[4]=h;args[0]=20;args[1]=99;args[2]=4;args[3]=0;
 assert(g_SetBitmapBits(0,0)==4);memset(output+20,0,4);
 n=g_GetBitmapBits(0,0);assert(n==4&&output[20]==0x81&&output[21]==0x23&&output[23]==0xcd);
 assert(bitmap_pattern(h,&pw,&ph,rows,colors)&&pw==16&&ph==2&&rows[0]==0x8123&&rows[1]==0xabcd);
 output[20]=0x67;args[2]=1;assert(g_SetBitmapBits(0,0)==1&&rows[0]==0x8123);
 args[0]=65535;args[2]=2;assert(!g_SetBitmapBits(0,0));
 args[0]=0;dc=g_CreateCompatibleDC(0,0);assert(bitmap_select(dc,h)==DEFAULT_BITMAP);
 assert(bitmap_patblt(dc,0,0,16,2,0x00ff0062UL,0,0,0));
 assert(bitmap_pattern(h,&pw,&ph,rows,colors)&&rows[0]==0xffff&&rows[1]==0xffff);
 bitmap_shutdown();clean();
 h=bitmap_create(17,2,1,1,0,0);assert(h&&!bitmap_pattern(h,&pw,&ph,rows,colors));
 bitmap_shutdown();clean();
 native_palette.count=16;native_palette.palette[1].r=255;native_palette.palette[2].g=255;native_palette.palette[3].b=255;
 h=bitmap_create(8,8,1,4,0,0);assert(h);
 bitmap_put(&bitmaps[bi(h)],0,0,1);bitmap_put(&bitmaps[bi(h)],1,0,2);
 for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)bitmap_put(&bitmaps[bi(h)],xx,yy,xx&1?2:1);
 assert(bitmap_pattern(h,&pw,&ph,rows,colors)==2&&pw==8&&ph==8&&rows[0]==0x5500);
 assert(colors[0]==bitmap_rgb(&bitmaps[bi(h)],1)&&colors[1]==bitmap_rgb(&bitmaps[bi(h)],2));
 bitmap_put(&bitmaps[bi(h)],3,3,3);assert(!bitmap_pattern(h,&pw,&ph,rows,colors));
 bitmap_shutdown();clean();

}
static int flood_wide_pixels;
static void flood_wide_output(void*ctx,int x,int y,int w,int kind){assert(x>=0&&y>=0&&x+w<=256&&y<256&&kind==1);flood_wide_pixels+=w;}
static void flood_output(void*ctx,int x,int y,int w,int kind){
 int i;(void)ctx;assert(kind==1&&x>=0&&y>=0&&x+w<=100&&y<100);
 for(i=x;i<x+w;i++)surface[y*100+i]=7;
}


static void stock_gray_mask_test(void){
 u16 b,d;int gray,rop,x,y;u32 old;
 const u8 bits[3][2]={{1,3},{2,1},{1,0}};
 struct poly_point box[4]={{0,0},{8,0},{8,8},{0,8}};
 setup(8,8,1);b=load();d=dcnew();assert(bitmap_select(d,b));
 assert(bitmap_brushorg(d,-1,3,&old));
 for(gray=0;gray<3;gray++)for(rop=1;rop<=16;rop++){
  bitmap_object(d,2,0x302+2*gray);bitmap_rop2(d,rop);
  for(y=0;y<8;y++)for(x=0;x<8;x++)bitmap_put(&bitmaps[bi(b)],x,y,(x+y)&1);
  assert(bitmap_polygon(d,box,4,0,0,0x404040,0));
  for(y=0;y<8;y++)for(x=0;x<8;x++){
   int source=(bits[gray][(y+1)%2]>>((x+1)%2))&1,destbit=(x+y)&1;
   assert(bitmap_index(&bitmaps[bi(b)],x,y)==(((rop-1)>>(2*source+destbit))&1));
  }
 }
 /* An ordinary solid gray brush is not mistaken for a stock-gray handle. */
 bitmap_object(d,2,0x400);bitmap_rop2(d,13);
 assert(bitmap_polygon(d,box,4,0,0,0x404040,0));
 for(y=0;y<8;y++)for(x=0;x<8;x++)assert(!bitmap_index(&bitmaps[bi(b)],x,y));
 bitmap_shutdown();
 setup(8,8,4);b=load();d=dcnew();assert(bitmap_select(d,b));bitmap_object(d,2,0x306);
 assert(bitmap_polygon(d,box,4,0,0,0x404040,0));
 for(y=0;y<8;y++)for(x=0;x<8;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==bitmap_nearest(&bitmaps[bi(b)],0x404040));
 bitmap_shutdown();
 puts("Monochrome stock gray spray masks: three stipples, origin phase and all16 ROP2 tables passed");
}
static void masked_pattern_test(void){
 u16 b,d;int x,y,invert,mono;u32 before[10000];
 setup(8,8,1);b=load();d=dcnew();assert(bitmap_select(d,b));
 dest.brush=1;dest.brushx=-2;dest.brushy=3;
 testbrush.style=BS_PATTERN;testbrush.width=0;
 bmps[0].used=1;bmps[0].w=3;bmps[0].h=2;
 bmps[0].rows[0]=0xa000;bmps[0].rows[1]=0x4000;
 bmps[0].colors[0]=0x123456;bmps[0].colors[1]=0xabcdef;
 for(invert=0;invert<2;invert++)for(mono=0;mono<2;mono++){
  dest.textcolor=invert?0xffffff:0;dest.bkcolor=invert?0:0xffffff;
  bmps[0].mono=mono;foreground=77;mode=3;
  for(y=0;y<100;y++)for(x=0;x<100;x++)surface[y*100+x]=0x656565;
  args[0]=0x746;args[1]=0xe2;args[2]=1;args[3]=2;args[4]=d;
  args[5]=5;args[6]=7;args[7]=4;args[8]=-1;args[9]=9;
  assert(g_BitBlt(0,0));assert(foreground==77&&mode==3);
  for(y=0;y<100;y++)for(x=0;x<100;x++){
   u32 expect=0x656565;
   if(x<5&&y>=4&&y<9&&bitmap_index(&bitmaps[bi(b)],x+3,y-3)!=invert){
    int py=((y-3)%2+2)%2,px=(x+2)%3,v=(bmps[0].rows[py]>>(15-px))&1;
    expect=mono?(v?dest.bkcolor:dest.textcolor):bmps[0].colors[v];
   }
   assert(surface[y*100+x]==expect);
  }
 }
 memcpy(before,surface,sizeof before);dest.textcolor=0x123456;
 assert(!g_BitBlt(0,0));assert(!memcmp(before,surface,sizeof before));
 dest.textcolor=0;dest.bkcolor=0xffffff;testbrush.style=99;
 assert(!g_BitBlt(0,0));assert(!memcmp(before,surface,sizeof before));
 testbrush.style=BS_PATTERN;bmps[0].used=0;
 assert(!g_BitBlt(0,0));assert(!memcmp(before,surface,sizeof before));bmps[0].used=1;
 testbrush.style=99;args[9]=d;assert(!g_BitBlt(0,0));args[9]=9;
 testbrush.style=0;testbrush.color=0xfedcba;assert(g_BitBlt(0,0));
 assert(surface[4*100]==(bitmap_index(&bitmaps[bi(b)],3,1)?0xfedcba:before[4*100]));
 puts("Masked pattern E2: clipped/inverted mono masks, device brush phase, preserved pixels and atomic refusals passed");
 bitmap_shutdown();memset(bmps,0,sizeof bmps);dest.textcolor=0x123456;
 dest.brushx=dest.brushy=dest.brush=0;testbrush.style=0;
}
static void lowmemory_flood_test(void){
 u16 d,b;int x,y;bitmap_shutdown();
 native_palette.count=16;for(x=0;x<16;x++)native_palette.palette[x].r=native_palette.palette[x].g=native_palette.palette[x].b=x*17;
 d=dcnew();b=bitmap_create(256,256,1,4,0,0);assert(b&&bitmap_select(d,b));
 segfail=1;flood_malloc_limit=1024;
 assert(!bitmap_flood(d,0,12,13,0,1,0xffffff,0,0));
 for(y=0;y<256;y++)for(x=0;x<256;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==0);
 /* The smaller worklist can still fill a small isolated rectangle. */
 for(y=2;y<10;y++)for(x=2;x<10;x++)bitmap_put(&bitmaps[bi(b)],x,y,7);
 assert(bitmap_flood(d,0,3,3,0x777777,1,0,0,0));
 for(y=0;y<256;y++)for(x=0;x<256;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==0);
 flood_malloc_limit=0;
 assert(bitmap_flood(d,0,12,13,0,1,0xffffff,0,0));segfail=0;
 for(y=0;y<256;y++)for(x=0;x<256;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==15);
 /* Exactly512 connected runs fit; a wider comb exceeds the bound atomically. */
 for(y=0;y<256;y++)for(x=0;x<256;x++)bitmap_put(&bitmaps[bi(b)],x,y,x<5&&(!(y&1)||!(x&1))?0:15);
 assert(bitmap_flood(d,0,0,0,0,1,0x777777,0,0));
 for(y=0;y<256;y++)for(x=0;x<256;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==(x<5&&(!(y&1)||!(x&1))?7:15));
 for(y=0;y<256;y++)for(x=0;x<256;x++)bitmap_put(&bitmaps[bi(b)],x,y,x<7&&(!(y&1)||!(x&1))?0:15);
 assert(!bitmap_flood(d,0,0,0,0,1,0x777777,0,0));
 for(y=0;y<256;y++)for(x=0;x<256;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==(x<7&&(!(y&1)||!(x&1))?0:15));
 maxallocbytes=16384;bitmap_shutdown();clean();
 printf("flood: full256 canvas succeeds with all far allocations disabled\n");
}
static void memory_text_test(void){
 u16 d,b;int x,y,k;bitmap_shutdown();
 for(k=0;k<224;k++){testfont_width[k]=3;for(y=0;y<13;y++)testfont_bits[k*13+y]=0xa000;}
 native_palette.count=16;for(k=0;k<16;k++)native_palette.palette[k].r=native_palette.palette[k].g=native_palette.palette[k].b=k*17;
 d=dcnew();b=bitmap_create(32,32,1,4,0,0);assert(bitmap_select(d,b));
 output[0]='A';memory[di(d)].textcolor=0xffffff;memory[di(d)].bkcolor=0;
 assert(bitmap_text(d,2,3,99,0,1));
 for(y=0;y<32;y++)for(x=0;x<32;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==((y>=3&&y<16&&(x==2||x==4))?15:0));
 assert(bitmap_textmode(d,1,0)==2);assert(bitmap_textmode(d,0,0)==0);
 assert(bitmap_text(d,3,3,99,0,1));assert(bitmap_index(&bitmaps[bi(b)],4,3)==15);
 assert(bitmap_savedc(d));assert(bitmap_textmode(d,25,1)==0);bitmap_moveto(d,6,20);
 assert(bitmap_text(d,123,123,99,0,1));assert(memory[di(d)].curx==9);assert(bitmap_index(&bitmaps[bi(b)],6,9)==15);
 assert(bitmap_restoredc(d,-1));assert(memory[di(d)].textalign==0&&memory[di(d)].bkmode==1);
 bitmap_origin(d,1,1,4,5);assert(bitmap_text(d,-4,-5,99,0,1));assert(bitmap_index(&bitmaps[bi(b)],0,0)==15);
 assert(!bitmap_text(d,0,0,99,65535,2));assert(!bitmap_text(d,0,0,99,0,-1));
 bitmap_origin(d,1,1,0,0);assert(bitmap_text(d,32767,0,99,0,2));
 bitmap_moveto(d,17,18);bitmap_patblt(d,0,0,32,32,0x00000042,0,0,0);
 assert(bitmap_text(d,-1,-12,99,0,1));
 for(y=0;y<32;y++)for(x=0;x<32;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==((x==1&&y==0)?15:0));
 assert(memory[di(d)].curx==17&&memory[di(d)].cury==18);
 bitmap_patblt(d,0,0,32,32,0x00000042,0,0,0);assert(bitmap_text(d,31,31,99,0,1));
 for(y=0;y<32;y++)for(x=0;x<32;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==((x==31&&y==31)?15:0));
 bitmap_textmode(d,1,1);bitmap_moveto(d,10,20);bitmap_origin(d,1,1,4,5);
 assert(bitmap_text(d,99,99,99,0,1));assert(memory[di(d)].curx==13&&memory[di(d)].cury==20);
 bitmap_shutdown();clean();
 printf("memory text: opaque/transparent, origin, alignment, current position, save/restore and bounds passed\n");
}
static void memory_rop_truth_test(void){
 u16 b,c,d,e;int si,di_,pi,op,bit;u32 ops[]={0x220326,0x660046,0xe20746,0xcc0020};
 native_palette.count=16;for(pi=0;pi<16;pi++)native_palette.palette[pi].r=native_palette.palette[pi].g=native_palette.palette[pi].b=pi*17;
 setup(1,1,4);d=dcnew();e=dcnew();b=bitmap_create(1,1,1,4,0,0);c=bitmap_create(1,1,1,4,0,0);
 assert(bitmap_select(d,b)&&bitmap_select(e,c));memory[di(e)].brush=1;testbrush.style=0;
 for(op=0;op<4;op++)for(pi=0;pi<16;pi++)for(si=0;si<16;si++)for(di_=0;di_<16;di_++){
  int expected=0,table=ops[op]>>16;
  testbrush.color=bitmap_rgb(&bitmaps[bi(c)],pi);bitmap_put(&bitmaps[bi(b)],0,0,si);bitmap_put(&bitmaps[bi(c)],0,0,di_);
  for(bit=0;bit<4;bit++){int address=(((pi>>bit)&1)<<2)|(((si>>bit)&1)<<1)|((di_>>bit)&1);expected|=((table>>address)&1)<<bit;}
  assert(memblit(e,d,0,0,0,0,1,1,ops[op]));assert(bitmap_index(&bitmaps[bi(c)],0,0)==expected);
 }
 bitmap_shutdown();puts("Memory ROP22/66/E2/CC: all16384 palette-index triples match independent ternary truth-table oracle");
}
static void memory_mask_test(void){
 int x,y,invert,mono,depth;u16 b,d,e,c;u8 before[64];
 native_palette.count=16;for(x=0;x<16;x++)native_palette.palette[x].r=native_palette.palette[x].g=native_palette.palette[x].b=x*17;
 for(depth=1;depth<=4;depth+=3)for(invert=0;invert<2;invert++)for(mono=0;mono<2;mono++){
  setup(8,8,1);b=load();d=dcnew();e=dcnew();c=bitmap_create(8,8,1,depth,0,0);assert(bitmap_select(d,b)&&bitmap_select(e,c));
  memory[di(e)].textcolor=invert?0xffffff:0;memory[di(e)].bkcolor=invert?0:0xffffff;
  memory[di(e)].brush=1;memory[di(e)].brushx=-2;memory[di(e)].brushy=3;
  testbrush.style=BS_PATTERN;testbrush.width=0;bmps[0].used=1;bmps[0].w=3;bmps[0].h=2;
  bmps[0].rows[0]=0xa000;bmps[0].rows[1]=0x4000;bmps[0].mono=mono;
  bmps[0].colors[0]=0;bmps[0].colors[1]=0xffffff;
  for(y=0;y<8;y++)for(x=0;x<8;x++){before[y*8+x]=(x+y)&(depth==1?1:15);bitmap_put(&bitmaps[bi(c)],x,y,before[y*8+x]);}
  assert(memblit(e,d,-1,2,2,1,7,5,0xe20746));
  for(y=0;y<8;y++)for(x=0;x<8;x++){
   int expected=before[y*8+x];
   if(x<5&&y>=2&&y<7){
    int bit=bitmap_index(&bitmaps[bi(b)],x+3,y-1),py=((y-3)%2+2)%2,px=(x+2)%3,pbit=(bmps[0].rows[py]>>(15-px))&1;
    u8 src=depth==1?bit:bitmap_nearest(&bitmaps[bi(c)],bit?memory[di(e)].bkcolor:memory[di(e)].textcolor);
    u32 pc=mono?(pbit?memory[di(e)].bkcolor:memory[di(e)].textcolor):bmps[0].colors[pbit];
    expected=(src&bitmap_nearest(&bitmaps[bi(c)],pc))|((~src)&expected);
   }
   assert(bitmap_index(&bitmaps[bi(c)],x,y)==expected);
  }
  testbrush.style=99;assert(!memblit(e,d,0,0,0,0,8,8,0xe20746));
 }
 bitmap_shutdown();memset(bmps,0,sizeof bmps);testbrush.style=0;
 puts("Memory E2 mono expansion, pattern phase, clipping and unsupported-brush refusal passed");
}
int main(int argc,char**argv){
 if(getenv("BITMAP_STRETCH_ONLY")){stretch_tests();stretch_batch_tests();return 0;}
 stretch_batch_tests();
 memory_rop_truth_test();memory_mask_test();
 stock_gray_mask_test();
 lowmemory_flood_test();
 memory_text_test();
 masked_pattern_test();
 small_ddb_bits_test();
 ddb_bits_test();
 origin_tests();
 dib_band_test();dib_create_device_test();
 stretch_tests();
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
 setup(71,96,1);resource[40]=1;b=load();assert(b&&bitmaps[bi(b)].bpp==4);bitmap_shutdown();clean();
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
 args[9]=d;args[0]=0xc6;args[1]=0x88;assert(!g_BitBlt(0,0)); /* memory AND unsupported */

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
   args[0]=off;args[1]=99;args[2]=14;args[3]=b;
   assert(g_GetObject(0,0)==14&&outputwrites==1);
   for(n=0;n<65536;n++)assert(output[n]==(n>=off&&n<off+14?expected[n-off]:0xa5));
  }
  /* Larger cbBuffer does not authorize writing beyond the 14-byte structure. */
  memset(output,0xa5,sizeof output);outputwrites=0;
  args[0]=100;args[1]=99;args[2]=32767;args[3]=b;
  assert(g_GetObject(0,0)==14&&outputwrites==1);
  assert(!memcmp(output+100,expected,14)&&output[114]==0xa5);
  for(bad=0;bad<7;bad++){
   memset(output,0xa5,sizeof output);outputwrites=0;
   args[0]=100;args[1]=99;args[2]=14;args[3]=b;
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
  for(n=0;n<36*276;n++)assert(rams[bitmaps[bi(b)].bank[0]-1][n]==0);
  for(n=36*276;n<16384;n++)assert(rams[bitmaps[bi(b)].bank[0]-1][n]==0xa5);
 }
 setup(1,1,1);native_palette.count=17;assert(!compatible(9,71,96));clean();
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
 b=compatible(9,640,52);assert(b);assert(bitmap_delete(b)==1);
 b=compatible(d,640,205);assert(b);assert(bitmap_delete(b)==1);
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

 /* Native AND combines palette indices, not RGB components. */
 bitmap_shutdown();clean();indexed=1;capability=0x2000;mode=3;foreground=77;
 for(k=0;k<16;k++){native_palette.palette[k].r=(k*43)%256;native_palette.palette[k].g=(k*67)%256;native_palette.palette[k].b=(k*109)%256;}
 for(k=1;k<=4;k+=3){
  int src,dst,remap;
  for(remap=0;remap<2;remap++){
   setup(16,16,k);
   if(k==4){
    for(n=0;n<16;n++){int pi=remap?15-n:n;resource[40+n*4]=native_palette.palette[pi].b;resource[41+n*4]=native_palette.palette[pi].g;resource[42+n*4]=native_palette.palette[pi].r;}
    for(y=0;y<16;y++)for(x=0;x<16;x++){int at=104+(15-y)*8+x/2,shift=(x&1)?0:4;resource[at]=(resource[at]&~(15<<shift))|(x<<shift);}
   }
   b=load();d=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
   for(src=0;src<16;src++)for(dst=0;dst<16;dst++){
    u32 rgb=native_palette.palette[src].r|((u32)native_palette.palette[src].g<<8)|((u32)native_palette.palette[src].b<<16);
    int sx=k==4?(remap?15-src:src):0;
    dest.textcolor=dest.bkcolor=rgb;surface[0]=dst;
    assert(blit(d,0,0,sx,0,1,1,0x008800c6));
    assert(surface[0]==(u32)(src&dst)&&mode==3&&foreground==77);
    if(k==1){
     dest.bkcolor=0xffffff;dest.textcolor=rgb;surface[0]=dst;
     assert(blit(d,0,0,2,0,1,1,0x008800c6));
     assert(surface[0]==(u32)(src&dst)&&mode==3&&foreground==77);
    }
   }
   for(n=0;n<10000;n++)surface[n]=n%16;
   capability=0;assert(!blit(d,0,0,0,0,5,5,0x008800c6));
   for(n=0;n<10000;n++)assert(surface[n]==(u32)(n%16));
   assert(mode==3&&foreground==77);
   capability=0x2000;assert(!blit(d,0,0,0,0,5,5,0x00660046));
   for(n=0;n<10000;n++)assert(surface[n]==(u32)(n%16));
   if(k==4){
    assert(blit(d,-3,98,1,14,10,5,0x008800c6));
    for(y=0;y<100;y++)for(x=0;x<100;x++){
     u32 expected=(y*100+x)%16;
     if(y>=98&&x<7)expected&=remap?15-(x+4):x+4;
     assert(surface[y*100+x]==expected);
    }
   }
  }
 }
 indexed=0;capability=0;

 /* Memory copies use a pre-copy oracle for every overlap direction. */
 for(k=1;k<=4;k+=3){
  int dx,dy,sx,sy,op;u32 ops[]={0x00cc0020,0x00220326,0x00660046,0xe20746};u8 before[17*11],expected[17*11];
  setup(17,11,k);b=load();d=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
  memory[di(d)].brush=1;testbrush.style=0;testbrush.color=bitmap_rgb(&bitmaps[bi(b)],1);
  for(op=0;op<4;op++)for(dx=0;dx<5;dx++)for(dy=0;dy<5;dy++)for(sx=0;sx<5;sx++)for(sy=0;sy<5;sy++){
   for(y=0;y<11;y++)for(x=0;x<17;x++){u8 v=(x+3*y)&(k==1?1:15);bitmap_put(&bitmaps[bi(b)],x,y,v);before[y*17+x]=v;}
   memcpy(expected,before,sizeof before);
   for(y=0;y<6;y++)for(x=0;x<12;x++){u8 sv=before[(sy+y)*17+sx+x],dv=before[(dy+y)*17+dx+x];expected[(dy+y)*17+dx+x]=op==0?sv:op==1?(dv&~sv):op==2?(sv^dv):((sv&1)|((~sv)&dv));}
   assert(memblit(d,d,dx,dy,sx,sy,12,6,ops[op]));
   for(y=0;y<11;y++)for(x=0;x<17;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==expected[y*17+x]);
  }
 }
 /* Odd-width monochrome DIB -> Win16 memory DC. Compare all 6,867
  * visible pixels to the pattern oracle, not raw words (padding is not ink). */
 setup(109,63,1);b=load();d=dcnew();e=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
 {u16 target=compatible(e,109,63);assert(target&&bitmap_select(e,target)==DEFAULT_BITMAP);
  assert(memblit(e,d,0,0,0,0,109,63,0x00cc0020));
  for(y=0;y<63;y++)for(x=0;x<109;x++)
   assert(bitmap_index(&bitmaps[bi(target)],x,y)==pattern(x,y));
 }
 puts("Monochrome memory-DC copy: all 6867 odd-width image pixels match");
 /* Exact Solitaire 71x96 source to destination y=-3 crop. */
 setup(71,96,1);b=load();d=dcnew();e=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
 {u16 target=compatible(e,71,96);assert(bitmap_select(e,target)==DEFAULT_BITMAP);
 assert(memblit(e,d,0,-3,0,0,71,96,0x00cc0020));
 for(y=0;y<96;y++)for(x=0;x<71;x++)assert(bitmap_index(&bitmaps[bi(target)],x,y)==(y<93?pattern(x,y+3):0));
 }
 /* Mono->color uses destination text/background; reverse uses source background. */
 setup(16,4,1);b=load();d=dcnew();e=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
 for(k=0;k<16;k++){native_palette.palette[k].r=k*13;native_palette.palette[k].g=k*7;native_palette.palette[k].b=k*11;}
 {u16 target=compatible(9,16,4),mono;u32 text=13|(7UL<<8)|(11UL<<16),bk=39|(21UL<<8)|(33UL<<16);
 assert(bitmap_select(e,target)==DEFAULT_BITMAP);
 assert(bitmap_setcolor(e,text,0)==0&&bitmap_setcolor(e,bk,1)==0xffffff);
 assert(memblit(e,d,0,0,0,0,16,4,0x00cc0020));
 for(y=0;y<4;y++)for(x=0;x<16;x++)assert(bitmap_index(&bitmaps[bi(target)],x,y)==(pattern(x,y)?3:1));
 assert(bitmap_setcolor(e,bk,1)==bk);
 assert(memblit(d,e,0,0,0,0,16,4,0x00cc0020));
 for(y=0;y<4;y++)for(x=0;x<16;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==pattern(x,y));
 assert(bitmap_setpixel(e,0,0,text)==text&&bitmap_getpixel(e,0,0)==text);
 assert(bitmap_setpixel(d,0,0,0x010101)==0&&bitmap_getpixel(d,0,0)==0);
 assert(bitmap_setpixel(d,0,0,0xfefefe)==0xffffff&&bitmap_getpixel(d,0,0)==0xffffff);
 for(k=0;k<4;k++){int xx=k==0?-1:k==1?16:0,yy=k==2?-1:k==3?4:0;
 assert(bitmap_getpixel(e,xx,yy)==0xffffffffUL&&bitmap_setpixel(e,xx,yy,0)==0xffffffffUL);}
 assert(bitmap_setpixel(e,0,0,0x01000000)==0xffffffffUL&&bitmap_getpixel(e,0,0)==text);
 {u8 saved[32];memcpy(saved,rams[bitmaps[bi(target)].bank[0]-1],32);
 assert(!memblit(e,d,0,0,0,0,16,4,0x008800c6));assert(!memblit(e,d,0,0,0,0,16,4,0x001100a6));
 assert(!memcmp(saved,rams[bitmaps[bi(target)].bank[0]-1],32));}
 }
 /* Selection ROPs convert source pixels before operating on destination
  * bits. Exercise mono expansion, reverse background matching, and clipping. */
 for(k=0;k<2;k++){
  int op;u32 ops[]={0x00220326,0x00660046};
  for(op=0;op<2;op++){
   u16 target;struct bitmap *sb,*tb;
   setup(16,4,k?4:1);b=load();d=dcnew();e=dcnew();assert(bitmap_select(d,b));
   target=bitmap_create(16,4,1,k?1:4,0,0);assert(target&&bitmap_select(e,target));
   sb=&bitmaps[bi(b)];tb=&bitmaps[bi(target)];
   if(k){for(x=0;x<16;x++){sb->palette[x].r=x*13;sb->palette[x].g=x*7;sb->palette[x].b=x*11;}memory[di(d)].bkcolor=bitmap_rgb(sb,3);}
   else{memory[di(e)].textcolor=bitmap_rgb(tb,1);memory[di(e)].bkcolor=bitmap_rgb(tb,3);}
   for(y=0;y<4;y++)for(x=0;x<16;x++){
    bitmap_put(sb,x,y,k?(x&15):(x&1));bitmap_put(tb,x,y,k?((x+y)&1):((x+y)&15));
   }
   assert(memblit(e,d,-2,-1,0,0,16,4,ops[op]));
   for(y=0;y<4;y++)for(x=0;x<16;x++){
    int dv=(x+y)&(k?1:15),sv=k?((x+2)==3):((x&1)?3:1);
    int expected=x<14&&y<3?(op?(sv^dv):(dv&~sv)):dv;
    assert(bitmap_index(tb,x,y)==expected);
   }
  }
 }
 puts("Selection DSna/SRCINVERT overlap, mono conversion and clipped phase passed");
 /* Color->color remaps through owned RGB palettes, nearest tie is first. */
 setup(16,1,4);b=load();d=dcnew();e=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
 {u16 target=compatible(9,16,1);struct bitmap*s=&bitmaps[bi(b)],*t=&bitmaps[bi(target)];
 assert(bitmap_select(e,target)==DEFAULT_BITMAP);
 for(k=0;k<16;k++){s->palette[k].r=k*10;s->palette[k].g=s->palette[k].b=0;
 t->palette[k].r=(15-k)*10;t->palette[k].g=t->palette[k].b=0;bitmap_put(s,k,0,k);}
 assert(memblit(e,d,0,0,0,0,16,1,0x00cc0020));
 for(k=0;k<16;k++)assert(bitmap_index(t,k,0)==15-k);
 assert(bitmap_setpixel(e,0,0,145)==150&&bitmap_index(t,0,0)==0);
 }
 bitmap_shutdown();clean();d=dcnew();
 assert(bitmap_getpixel(d,0,0)==0xffffffffUL&&bitmap_setpixel(d,0,0,0)==0xffffffffUL);
 assert(bitmap_getpixel(999,0,0)==0xffffffffUL&&bitmap_setpixel(999,0,0,0)==0xffffffffUL);
 assert(bitmap_setcolor(d,12,0)==0&&bitmap_setcolor(d,34,1)==0xffffff);
 assert(bitmap_setcolor(d,0x01000000,0)==0xffffffffUL);
 assert(bitmap_setcolor(d,56,0)==12);
 bitmap_deletedc(d);d=dcnew();assert(bitmap_setcolor(d,1,0)==0&&bitmap_setcolor(d,2,1)==0xffffff);

 /* Streamed display capture commits only after every validated row succeeds. */
 setup(1,1,1);win.cw=640;win.ch=480;
 for(k=0;k<16;k++){native_palette.palette[k].r=k*13;native_palette.palette[k].g=k*7;native_palette.palette[k].b=k*11;}
 b=compatible(9,71,96);d=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
 {struct bitmap*target=&bitmaps[bi(b)];u8 saved[3456];
  /* Reverse destination palette so capture must perform real palette remap. */
  for(k=0;k<16;k++){target->palette[k].r=(15-k)*13;target->palette[k].g=(15-k)*7;target->palette[k].b=(15-k)*11;}
  memset(rams[target->bank[0]-1],0x65,3456);memcpy(saved,rams[target->bank[0]-1],3456);
  for(capture_mode=1;capture_mode<=6;capture_mode++){
   capture_calls=0;assert(!memblit(d,9,0,0,281,116,71,96,0x00cc0020));
   assert(capture_calls==1&&seglive==1&&!memcmp(saved,rams[target->bank[0]-1],3456));
  }
  capture_mode=0;segfail=1;capture_calls=0;
  assert(!memblit(d,9,0,0,281,116,71,96,0x00cc0020));
  assert(!capture_calls&&seglive==1&&!memcmp(saved,rams[target->bank[0]-1],3456));segfail=0;
  native_palette.count=17;assert(!memblit(d,9,0,0,281,116,71,96,0x00cc0020));
  assert(!capture_calls&&!memcmp(saved,rams[target->bank[0]-1],3456));native_palette.count=16;
  capture_mode=7;assert(memblit(d,9,0,0,281,116,71,96,0x00cc0020));
  assert(capture_x==281&&capture_y==116&&capture_w==71&&capture_h==96&&seglive==1);
  for(y=0;y<96;y++)for(x=0;x<71;x++)assert(bitmap_index(target,x,y)==15-((281+x+3*(116+y))&15));
  for(y=0;y<96;y++)assert((rams[target->bank[0]-1][y*36+35]&15)==5); /* pad nibble unchanged */
  for(n=3456;n<16384;n++)assert(rams[target->bank[0]-1][n]==0xa5);
  for(k=0;k<16;k++){native_palette.palette[k].r=k*13;native_palette.palette[k].g=k*7;native_palette.palette[k].b=k*11;}
  capture_mode=0;memset(rams[target->bank[0]-1],0x65,3456);
  assert(memblit(d,9,-2,-3,281,116,71,96,0x00cc0020));
  assert(capture_x==283&&capture_y==119&&capture_w==69&&capture_h==93);
  for(y=0;y<96;y++)for(x=0;x<71;x++)assert(bitmap_index(target,x,y)==(x<69&&y<93?15-((283+x+3*(119+y))&15):(x&1?5:6)));
  /* Small selections retain atomic capture when conventional far allocation
   * is exhausted, including palette conversion and malformed transports. */
  segfail=1;
  for(capture_mode=1;capture_mode<=6;capture_mode++){
   memcpy(saved,rams[target->bank[0]-1],3456);
   assert(!memblit(d,9,2,3,5,7,19,11,0x00cc0020));
   assert(seglive==1&&!memcmp(saved,rams[target->bank[0]-1],3456));
  }
  capture_mode=0;flood_malloc_fail=1;capture_calls=0;
  assert(!memblit(d,9,2,3,5,7,19,11,0x00cc0020)&&!capture_calls);
  flood_malloc_fail=0;
  assert(memblit(d,9,2,3,5,7,19,11,0x00cc0020));
  for(y=0;y<11;y++)for(x=0;x<19;x++)assert(bitmap_index(target,x+2,y+3)==15-((5+x+3*(7+y))&15));
  assert(seglive==1);segfail=0;
  capture_calls=0;assert(!memblit(d,9,0,0,0,0,3,3,0x008800c6)&&!capture_calls);
 }
 bitmap_shutdown();clean();win.cw=win.ch=100;


 /* Banked VGA surfaces and full capture keep destination atomic on failure. */
 setup(1,1,1);win.cw=640;win.ch=480;
 for(k=0;k<16;k++){native_palette.palette[k].r=k*13;native_palette.palette[k].g=k*7;native_palette.palette[k].b=k*11;}
 for(k=0;k<10;k++){failafter=k;assert(!compatible(9,640,480));assert(!seglive);}
 failafter=-1;b=compatible(9,640,480);assert(b&&seglive==10);d=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
 {struct bitmap*target=&bitmaps[bi(b)];
  for(y=0;y<480;y++)for(x=0;x<640;x++)bitmap_put(target,x,y,(x+y)&15);
  for(y=0;y<480;y++)for(x=0;x<640;x++)assert(bitmap_index(target,x,y)==((x+y)&15));
  capture_mode=0;
  for(k=0;k<10;k++){failafter=k;capture_calls=0;assert(!memblit(d,9,0,0,0,0,640,480,0x00cc0020));assert(!capture_calls&&seglive==10);}
  failafter=-1;
  for(capture_mode=1;capture_mode<=6;capture_mode++){
   assert(!memblit(d,9,0,0,0,0,640,480,0x00cc0020));assert(seglive==10);
   for(y=0;y<480;y++)for(x=0;x<640;x++)assert(bitmap_index(target,x,y)==((x+y)&15));
  }
  capture_mode=0;assert(memblit(d,9,0,0,0,0,640,480,0x00cc0020));assert(seglive==10);
  for(y=0;y<480;y++)for(x=0;x<640;x++)assert(bitmap_index(target,x,y)==((x+3*y)&15));
 }
 bitmap_shutdown();clean();
 b=bitmap_create(640,480,1,4,0,0);assert(b&&seglive==10);assert(bitmap_delete(b)==1);clean();
 b=bitmap_create(640,480,1,1,0,0);assert(b&&seglive==3);assert(bitmap_delete(b)==1);clean();
 assert(!bitmap_create(640,480,1,4,1,0));assert(!bitmap_create(640,480,4,1,0,0));
 {u16 src;assert(!seg_alloc(2,&src));rams[src-1][0]=0x12;rams[src-1][1]=0x30;
  b=bitmap_create(3,1,1,4,src,0);assert(b);assert(bitmap_index(&bitmaps[bi(b)],0,0)==1&&bitmap_index(&bitmaps[bi(b)],2,0)==3);
  assert(bitmap_delete(b)==1);seg_free(src);clean();}
 win.cw=win.ch=100;

 /* Saved bitmap selections own references until restore/delete/release. */
 setup(1,1,1);d=dcnew();e=dcnew();b=compatible(9,71,96);
 assert(bitmap_select(d,b)==DEFAULT_BITMAP);assert(bitmap_savedc(d)==1);
 bitmap_setcolor(d,0x123456,0);bitmap_rop2(d,7);assert(bitmap_savedc(d)==2);
 assert(bitmap_select(d,DEFAULT_BITMAP)==b);assert(!bitmap_delete(b));assert(!bitmap_select(e,b));
 assert(!bitmap_restoredc(d,0)&&!bitmap_restoredc(d,-3));
 assert(bitmap_restoredc(d,-1));assert(memory[di(d)].bitmap==b&&memory[di(d)].textcolor==0x123456&&bitmap_rop2(d,0)==7);
 assert(bitmap_restoredc(d,1));assert(memory[di(d)].textcolor==0&&bitmap_rop2(d,0)==13);
 assert(!bitmap_restoredc(d,-1));
 for(k=0;k<4;k++)assert(bitmap_savedc(d)==k+1);assert(!bitmap_savedc(d));
 assert(bitmap_deletedc(d)==1);assert(bitmap_select(e,b)==DEFAULT_BITMAP);
 assert(bitmap_deletedc(e)==1);assert(bitmap_delete(b)==1);d=dcnew();assert(!bitmap_restoredc(d,-1));
 assert(!bitmap_savedc(999)&&!bitmap_restoredc(999,-1));bitmap_shutdown();clean();

 /* Genuine memory-DC brush state and solid/indexed raster operations. */
 setup(1,1,1);win.cw=640;win.ch=480;
 for(k=0;k<16;k++){native_palette.palette[k].r=k*17;native_palette.palette[k].g=k*17;native_palette.palette[k].b=k*17;}
 b=compatible(9,640,480);d=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);
 assert(bitmap_object(d,2,0x444)==0x300);assert(bitmap_object(d,1,0x446)==0x30e);
 assert(bitmap_object(d,5,0xe00)==0x31e);
 assert(bitmap_savedc(d)==1);assert(bitmap_object(d,5,0xe02)==0xe00);
 assert(bitmap_object_used(0xe00)&&bitmap_object_used(0xe02));
 assert(bitmap_object(d,2,0x448)==0x444);
 assert(bitmap_object_used(0x444)&&bitmap_object_used(0x448)&&bitmap_object_used(0x446));
 assert(bitmap_patblt(d,0,0,640,480,0x00000042,0,0,0));
 assert(bitmap_patblt(d,0,0,640,3,0x00f00021,0xffffff,0,0));
 for(y=0;y<480;y++)for(x=0;x<640;x++)assert(bitmap_index(&bitmaps[bi(b)],x,y)==(y<3?15:0));
 assert(bitmap_patblt(d,637,52,20,3,0x00ff0062,0,0,0));
 assert(bitmap_patblt(d,640,55,-3,-3,0x00550009,0,0,0));
 for(y=52;y<55;y++)for(x=637;x<640;x++)assert(!bitmap_index(&bitmaps[bi(b)],x,y));
 assert(bitmap_patblt(d,-2,-2,4,4,0x00f00021,0x555555,0,0));
 assert(bitmap_index(&bitmaps[bi(b)],0,0)==5&&bitmap_index(&bitmaps[bi(b)],1,1)==5&&bitmap_index(&bitmaps[bi(b)],2,1)==15);
 assert(bitmap_patblt(d,0,0,2,2,0x005a0049,0x333333,0,0));assert(bitmap_index(&bitmaps[bi(b)],0,0)==6);
 assert(!bitmap_patblt(d,0,0,2,2,0x123456,0,0,0));
 assert(!bitmap_patblt(d,0,0,2,2,0x00f00021,0,3,0));assert(bitmap_index(&bitmaps[bi(b)],0,0)==6);
 assert(bitmap_patblt(d,0,0,2,2,0x00f00021,0,0,1));assert(bitmap_index(&bitmaps[bi(b)],0,0)==6);
 assert(bitmap_restoredc(d,-1));assert(bitmap_object(d,2,0)==0x444&&!bitmap_object_used(0x448));
 assert(bitmap_object(d,5,0)==0xe00&&!bitmap_object_used(0xe02));
 assert(bitmap_deletedc(d));assert(!bitmap_object_used(0xe00));assert(!bitmap_object_used(0x444)&&!bitmap_object_used(0x446));
 bitmap_shutdown();clean();win.cw=win.ch=100;

 /* Memory line endpoints, all binary ROP2 truth tables and rectangle corners. */
 setup(1,1,1);
 for(k=0;k<16;k++){native_palette.palette[k].r=k*17;native_palette.palette[k].g=k*17;native_palette.palette[k].b=k*17;}
 b=compatible(9,32,32);d=dcnew();bitmap_select(d,b);
 for(k=1;k<=16;k++)for(x=0;x<16;x++)for(y=0;y<16;y++){
  int z,expect=0;bitmap_put(&bitmaps[bi(b)],0,0,y);bitmap_rop2(d,k);bitmap_moveto(d,0,0);
  assert(bitmap_lineto(d,1,0,x*0x111111UL,0,1,0));
  for(z=0;z<4;z++)expect|=(((k-1)>>((((x>>z)&1)<<1)|((y>>z)&1)))&1)<<z;
  assert(bitmap_index(&bitmaps[bi(b)],0,0)==expect);
 }
 bitmap_rop2(d,13);assert(bitmap_patblt(d,0,0,32,32,0x00000042,0,0,0));
 /* Surface/boundary flood fill, disconnected regions, all ROP2 modes,
  * failure atomicity and DC origin without changing current position. */
 {
  int rop,xx,yy,bit;
  for(rop=1;rop<=16;rop++){
   for(yy=0;yy<32;yy++)for(xx=0;xx<32;xx++)bitmap_put(&bitmaps[bi(b)],xx,yy,3);
   for(xx=2;xx<=8;xx++){bitmap_put(&bitmaps[bi(b)],xx,2,15);bitmap_put(&bitmaps[bi(b)],xx,8,15);}
   for(yy=2;yy<=8;yy++){bitmap_put(&bitmaps[bi(b)],2,yy,15);bitmap_put(&bitmaps[bi(b)],8,yy,15);}
   bitmap_rop2(d,rop);bitmap_moveto(d,27,29);
   assert(bitmap_flood(d,0,4,4,0xffffff,0,0x555555,0,0));
   assert(memory[di(d)].curx==27&&memory[di(d)].cury==29);
   for(yy=0;yy<32;yy++)for(xx=0;xx<32;xx++){
    int expect=3;
    if(xx>=2&&xx<=8&&yy>=2&&yy<=8){
     if(xx==2||xx==8||yy==2||yy==8)expect=15;
     else{expect=0;for(bit=0;bit<4;bit++)expect|=(((rop-1)>>((((5>>bit)&1)<<1)|((3>>bit)&1)))&1)<<bit;}
    }
    assert(bitmap_index(&bitmaps[bi(b)],xx,yy)==expect);
   }
  }
  bitmap_rop2(d,13);
  assert(bitmap_flood(d,0,0,0,0x333333,1,0x777777,0,0));
  assert(bitmap_index(&bitmaps[bi(b)],0,0)==7&&bitmap_index(&bitmaps[bi(b)],4,4)==15);
  assert(!bitmap_flood(d,0,-1,0,0x777777,1,0,0,0));
  assert(!bitmap_flood(d,0,0,0,0x777777,2,0,0,0));
  flood_malloc_fail=1;assert(!bitmap_flood(d,0,0,0,0x777777,1,0,0,0));flood_malloc_fail=0;
  assert(bitmap_index(&bitmaps[bi(b)],0,0)==7);
  bitmap_origin(d,1,1,3,4);
  assert(bitmap_flood(d,0,-3,-4,0x777777,1,0,0,0));
  assert(!bitmap_index(&bitmaps[bi(b)],0,0));bitmap_origin(d,1,1,0,0);
  assert(bitmap_patblt(d,0,0,32,32,0x00000042,0,0,0));
 }
 /* Screen capture is fully validated before emitting a single fill span. */
 {
  int xx,yy,fail;win.cw=win.ch=16;
  for(yy=0;yy<16;yy++)for(xx=0;xx<16;xx++)surface[yy*100+xx]=3;
  for(xx=2;xx<=8;xx++){surface[2*100+xx]=15;surface[8*100+xx]=15;}
  for(yy=2;yy<=8;yy++){surface[yy*100+2]=15;surface[yy*100+8]=15;}
  capture_mode=8;assert(bitmap_flood(9,&dest,4,4,0xffffff,0,0x777777,flood_output,0));
  assert(surface[4*100+4]==7&&surface[0]==3&&surface[2*100+4]==15);
  for(fail=1;fail<=6;fail++){
   for(yy=0;yy<16;yy++)for(xx=0;xx<16;xx++)surface[yy*100+xx]=3;
   capture_mode=fail;assert(!bitmap_flood(9,&dest,0,0,0xffffff,0,0,flood_output,0));
   for(yy=0;yy<16;yy++)for(xx=0;xx<16;xx++)assert(surface[yy*100+xx]==3);
  }
  /* Full 256 canvas crosses the real capture packet ceiling. A refusal on
   * a later row must still emit no output. */
  win.cw=win.ch=256;
  for(xx=0;xx<640*480;xx++)surface[xx]=3;
  capture_mode=8;capture_calls=0;capture_fail_call=3;flood_wide_pixels=0;
  assert(!bitmap_flood(9,&dest,0,0,0x333333,1,0x777777,flood_wide_output,0));
  assert(!flood_wide_pixels);
  capture_calls=0;capture_fail_call=0;
  assert(bitmap_flood(9,&dest,0,0,0x333333,1,0x777777,flood_wide_output,0));
  assert(flood_wide_pixels==65536&&capture_calls==511);
  capture_mode=0;win.cw=win.ch=100;
 }
 /* Polygon shares the memory DC translation and full ROP2 truth table. */
 {struct poly_point square[4]={{2,2},{6,2},{6,6},{2,6}};
  int rop,src,dst,xx,yy;
  for(rop=1;rop<=16;rop++)for(src=0;src<16;src++)for(dst=0;dst<16;dst++){
   for(yy=0;yy<10;yy++)for(xx=0;xx<10;xx++)bitmap_put(&bitmaps[bi(b)],xx,yy,dst);
   bitmap_rop2(d,rop);bitmap_moveto(d,8,9);
   assert(bitmap_polygon(d,square,4,src*0x111111UL,1,src*0x111111UL,0));
   assert(memory[di(d)].curx==8&&memory[di(d)].cury==9);
   for(yy=0;yy<10;yy++)for(xx=0;xx<10;xx++){
    int expected=dst,bit;
    if(xx>=2&&xx<=6&&yy>=2&&yy<=6){
     expected=0;for(bit=0;bit<4;bit++)expected|=(((rop-1)>>((((src>>bit)&1)<<1)|((dst>>bit)&1)))&1)<<bit;
    }
    assert(bitmap_index(&bitmaps[bi(b)],xx,yy)==expected);
   }
  }
  {struct poly_point oval[3]={{2,2},{7,7},{5,5}};
   for(rop=1;rop<=16;rop++){
    for(yy=0;yy<10;yy++)for(xx=0;xx<10;xx++)bitmap_put(&bitmaps[bi(b)],xx,yy,10);
    bitmap_rop2(d,rop);assert(bitmap_polygon(d,oval,0,0xffffff,1,0x555555,0));
    for(yy=0;yy<10;yy++)for(xx=0;xx<10;xx++){
     int dx=xx-4,dy=yy-4,inside=dx*dx+dy*dy<=6,edge=inside&&
      ((dx-1)*(dx-1)+dy*dy>6||(dx+1)*(dx+1)+dy*dy>6||dx*dx+(dy-1)*(dy-1)>6||dx*dx+(dy+1)*(dy+1)>6);
     int expected=10,bit,source=edge?15:5;
     if(inside){expected=0;for(bit=0;bit<4;bit++)expected|=(((rop-1)>>((((source>>bit)&1)<<1)|((10>>bit)&1)))&1)<<bit;}
     assert(bitmap_index(&bitmaps[bi(b)],xx,yy)==expected);
    }
   }
  }
  /* Wide Polyline repeats the same segment in reverse: its brush footprint
   * is unioned before all sixteen ROP2s, so crossings never cancel in XOR. */
  {struct poly_point wide[3]={{2,4},{7,4},{2,4}};
   for(rop=1;rop<=16;rop++){
    for(yy=0;yy<10;yy++)for(xx=0;xx<10;xx++)bitmap_put(&bitmaps[bi(b)],xx,yy,10);
    bitmap_rop2(d,rop);assert(bitmap_polygon(d,wide,-3,0xffffff,2,0x555555,0));
    for(yy=0;yy<10;yy++)for(xx=0;xx<10;xx++){
     int inside=xx>=1&&xx<=7&&yy>=3&&yy<=4,expected=10,bit;
     if(inside){expected=0;for(bit=0;bit<4;bit++)expected|=(((rop-1)>>((1<<1)|((10>>bit)&1)))&1)<<bit;}
     assert(bitmap_index(&bitmaps[bi(b)],xx,yy)==expected);
    }
   }
  }
  /* Inside-frame width two through the memory path, including XOR.
   * Each ring/interior destination is updated exactly once. */
  {struct poly_point frame[3]={{1,1},{9,9},{0,0}};
   for(rop=1;rop<=16;rop++){
    for(yy=0;yy<10;yy++)for(xx=0;xx<10;xx++)bitmap_put(&bitmaps[bi(b)],xx,yy,10);
    bitmap_rop2(d,rop);assert(bitmap_polygon(d,frame,0,0xffffff,2,0x555555,0));
    for(yy=0;yy<10;yy++)for(xx=0;xx<10;xx++){
     int inside=xx>=1&&xx<9&&yy>=1&&yy<9;
     int edge=inside&&(xx<3||xx>=7||yy<3||yy>=7);
     int expected=10,bit,source=edge?15:5;
     if(inside){expected=0;for(bit=0;bit<4;bit++)expected|=(((rop-1)>>((((source>>bit)&1)<<1)|((10>>bit)&1)))&1)<<bit;}
     assert(bitmap_index(&bitmaps[bi(b)],xx,yy)==expected);
    }
   }
  }
  bitmap_rop2(d,13);
  for(yy=0;yy<10;yy++)for(xx=0;xx<10;xx++)bitmap_put(&bitmaps[bi(b)],xx,yy,0);
 }
 bitmap_moveto(d,0,0);assert(bitmap_lineto(d,5,5,0xffffff,0,1,0));
 for(k=0;k<5;k++)assert(bitmap_index(&bitmaps[bi(b)],k,k)==15);
 assert(!bitmap_index(&bitmaps[bi(b)],5,5));
 assert(bitmap_moveto(d,-5,10)==0x00050005UL);assert(bitmap_lineto(d,5,10,0xffffff,0,1,0));
 for(k=0;k<5;k++)assert(bitmap_index(&bitmaps[bi(b)],k,10)==15);assert(!bitmap_index(&bitmaps[bi(b)],5,10));
 assert(!bitmap_lineto(d,20,20,0xffffff,1,1,0)&&memory[di(d)].curx==5);
 assert(bitmap_lineto(d,20,20,0,5,1,1));assert(memory[di(d)].curx==20);
 assert(bitmap_patblt(d,0,0,32,32,0x00000042,0,0,0));bitmap_rop2(d,7);
 assert(bitmap_rectangle(d,1,1,5,5,0xffffff,0,1,0,0x555555,0,0));
 for(y=0;y<6;y++)for(x=0;x<6;x++){
  int expect=x<1||x>=5||y<1||y>=5?0:x==1||x==4||y==1||y==4?15:5;
  assert(bitmap_index(&bitmaps[bi(b)],x,y)==expect);
 }
 assert(bitmap_rectangle(d,-8,-8,-1,-1,0,0,1,0,0,0,0));
 assert(!bitmap_rectangle(d,1,1,5,5,0,1,1,0,0,0,0));
 assert(bitmap_index(&bitmaps[bi(b)],1,1)==15);
 assert(bitmap_savedc(d));bitmap_moveto(d,29,30);assert(bitmap_restoredc(d,-1));assert(memory[di(d)].curx==20);
 bitmap_shutdown();clean();
 /* Colored one-bit resources preserve RGB instead of text/background expansion. */
 for(k=0;k<4;k++){
  u32 first,second;
  setup(71,96,1);
  if(k==0){resource[40]=resource[41]=0;resource[42]=255;}
  if(k==1){resource[40]=17;resource[41]=29;resource[42]=41;resource[44]=53;resource[45]=67;resource[46]=79;}
  if(k==2){ /* A single legal palette entry and all-zero indices. */
   wordput(32,1);resource[40]=17;resource[41]=29;resource[42]=41;
   memset(resource+44,0,length-44);
  }
  if(k==3){resource[40]=resource[41]=resource[42]=255;resource[44]=resource[45]=resource[46]=0;}
  first=resource[42]|((u32)resource[41]<<8)|((u32)resource[40]<<16);
  second=resource[46]|((u32)resource[45]<<8)|((u32)resource[44]<<16);
  b=load();assert(b);object_check(b,71,96,4);
  d=dcnew();assert(bitmap_select(d,b)==DEFAULT_BITMAP);dest.textcolor=0x123456;dest.bkcolor=0xabcdef;
  assert(blit(d,0,0,0,0,71,96,0x00cc0020));
  for(y=0;y<96;y++)for(x=0;x<71;x++){
   int index=k==2?0:pattern(x,y),stored=index;
   assert(bitmap_index(&bitmaps[bi(b)],x,y)==stored);
   assert(surface[y*100+x]==(index?second:first));
  }
 }
 /* Optional real NE resources are temporary inputs, never committed assets. */
 for(k=1;k<argc;k++){
  FILE*f;long size;struct dib_info parsed;struct resource rr;u8 indices[32];int reverse,mono;
  bitmap_shutdown();clean();f=fopen(argv[k],"rb");assert(f);
  assert(!fseek(f,0,SEEK_END));size=ftell(f);assert(size>0&&size<=65535);rewind(f);
  assert(fread(resource,1,size,f)==(size_t)size);fclose(f);length=size;
  readfail=readcount=segfail=0;rr.off=0;rr.len=length;
  assert(dib_open(&parsed,read_bitmap,&rr,length));
  mono=parsed.bpp==1&&parsed.colors==2&&parsed.palette[0].r==0&&parsed.palette[0].g==0&&parsed.palette[0].b==0&&parsed.palette[1].r==255&&parsed.palette[1].g==255&&parsed.palette[1].b==255;
  reverse=0;
  b=load();assert(b);object_check(b,parsed.width,parsed.height,mono?1:4);
  for(y=0;y<parsed.height;y++)for(x=0;x<parsed.width;x+=32){
   int j,count=parsed.width-x;if(count>32)count=32;
   assert(dib_read_indices(&parsed,read_bitmap,&rr,x,y,indices,count));
   for(j=0;j<count;j++){
    int index=indices[j];assert(bitmap_index(&bitmaps[bi(b)],x+j,y)==(index^reverse));
    if(!mono)assert(!memcmp(&bitmaps[bi(b)].palette[index],&parsed.palette[index],sizeof(struct dib_rgb)));
   }
  }
 }
 if(argc>1)printf("LoadBitmap: %d real resource payloads passed\n",argc-1);
 bitmap_shutdown();b=bitmap_create(20,30,1,4,0,0);assert(b);
 args[2]=b;args[1]=209;args[0]=254;assert(g_SetBitmapDimension(0,0)==0);
 args[1]=17;args[0]=19;assert(g_SetBitmapDimension(0,0)==((u32)254<<16|209));
 assert(bitmaps[bi(b)].w==20&&bitmaps[bi(b)].h==30);
 args[2]=0xffff;assert(!g_SetBitmapDimension(0,0));
 args[3]=0x031e;args[2]=2;args[1]=9;args[0]=7;assert(g_GetObject(0,0)==2);
 bitmap_shutdown();clean();printf("bitmap glue: %d mono bits/padding plus color, cleanup, selection, clipping and GetObject far-write checks passed\n",checks);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-I'+str(root/'src'),'-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'t.c'),'-o',str(p/'t')],check=True)
 args=[str(p/'t')]
 if 'SOL_EXE' in os.environ:
  data=Path(os.environ['SOL_EXE']).read_bytes()
  ne=struct.unpack_from('<I',data,0x3c)[0]
  assert data[ne:ne+2]==b'NE'
  pos=ne+struct.unpack_from('<H',data,ne+0x24)[0]
  shift=struct.unpack_from('<H',data,pos)[0];pos+=2
  while True:
   typ=struct.unpack_from('<H',data,pos)[0]
   if not typ:break
   count=struct.unpack_from('<H',data,pos+2)[0];pos+=8
   for _ in range(count):
    off,size=struct.unpack_from('<HH',data,pos);pos+=12
    if typ!=0x8002:continue
    off<<=shift;size<<=shift
    assert off+size<=len(data)
    target=p/('resource'+str(len(args))+'.dib')
    target.write_bytes(data[off:off+size]);args.append(str(target))
  assert len(args)>1
 subprocess.run(args,check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
