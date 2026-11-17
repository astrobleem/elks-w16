#include <stdio.h>
#include "nano-X.h"
static unsigned calls,errors,checks;
static void check(int ok){checks++;if(!ok){errors++;printf("CAPTURE fail %u\n",checks);}}
static void row(void *ctx,unsigned y,const unsigned char*p,unsigned len)
{
 unsigned x;check(ctx==(void*)&calls);check(y==calls);calls++;
 check(len==36);check(!(p[35]&15));
 for(x=0;x<71;x++)check(((p[x/2]>>((x&1)?0:4))&15)==((x*3+y*5)&15));
}
int main(void)
{
 GR_GC_ID gc;GR_WINDOW_ID w,cover;GR_GC_INFO gi;GR_PALETTE pal;unsigned x,y,i;
 if(GrOpen()<0)return 90;gc=GrNewGC();GrGetGCInfo(gc,&gi);
 if(!(gi.mode&GR_MODE_HAVE_CAPTUREPIXELS)){
   check(!GrCapturePixels(1,gc,0,0,71,96,row,&calls));check(!calls);
   GrDestroyGC(gc);GrClose();puts("CAPTURE oldserver refused");return errors?1:0;
 }
 GrSetGCMode(gc,gi.mode);GrGetGCInfo(gc,&gi);check((gi.mode & ~0x7000)==GR_MODE_COPY);
 GrGetSystemPalette(&pal);check(pal.count==16);
 w=GrNewWindow(1,13,17,400,300,0,0,0);GrMapWindow(w);
 for(y=0;y<96;y++)for(x=0;x<71;x++){
   i=(x*3+y*5)&15;GrSetGCForeground(gc,GR_RGB(pal.palette[i].r,pal.palette[i].g,pal.palette[i].b));
   GrPoint(w,gc,281+x,116+y);
 }
 check(GrCapturePixels(w,gc,281,116,71,96,row,&calls));check(calls==96);
 calls=0;GrMoveCursor(330,160);check(GrCapturePixels(w,gc,281,116,71,96,row,&calls));check(calls==96);
 calls=0;check(!GrCapturePixels(w,gc,350,116,71,96,row,&calls));check(!calls);
 cover=GrNewWindow(1,350,210,1,1,0,0,0);GrMapWindow(cover);
 check(!GrCapturePixels(w,gc,281,116,71,96,row,&calls));check(!calls);GrDestroyWindow(cover);
 cover=GrNewWindow(w,340,200,1,1,0,0,0);GrMapWindow(cover);
 check(!GrCapturePixels(w,gc,281,116,71,96,row,&calls));check(!calls);GrDestroyWindow(cover);
 GrUnmapWindow(w);check(!GrCapturePixels(w,gc,281,116,71,96,row,&calls));check(!calls);
 check(!GrCapturePixels(1,gc,0,0,640,480,row,&calls));check(!calls);
 GrDestroyWindow(w);GrDestroyGC(gc);GrClose();printf("CAPTURE %u checks %u errors\n",checks,errors);return errors?1:0;
}
