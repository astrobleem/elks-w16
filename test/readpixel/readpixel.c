#include <stdio.h>
#include "nano-X.h"
static int checks, errors;
static GR_GC_ID gc;
static GR_PALETTE pal;
static void check(int yes) { ++checks; if(!yes){++errors;printf("READPIXEL fail %d\n",checks);} }
static GR_PIXELVAL readp(GR_WINDOW_ID w,int x,int y)
{ GR_PIXELVAL p=255;GrReadArea(w,x,y,1,1,&p);return p; }
int main(void)
{
    GR_GC_INFO gi; GR_SCREEN_INFO si;
    GR_WINDOW_ID w,cover;int i;GR_PIXELVAL p;
    if(GrOpen()<0)return 90;
    gc=GrNewGC();GrGetGCInfo(gc,&gi);
    if(!(gi.mode&GR_MODE_HAVE_READPIXEL)) { puts("READPIXEL unsupported server refused");GrDestroyGC(gc);GrClose();return 0; }
    GrSetGCMode(gc,gi.mode);GrGetGCInfo(gc,&gi);check((gi.mode & ~0x7000)==GR_MODE_COPY);
    GrGetScreenInfo(&si);GrGetSystemPalette(&pal);check(pal.count==16);
    for(i=0;i<16;i++) {
        GrSetGCForeground(gc,GR_RGB(pal.palette[i].r,pal.palette[i].g,pal.palette[i].b));
        GrPoint(GR_ROOT_WINDOW_ID,gc,i,0);
    }
    for(i=0;i<16;i++) check(readp(GR_ROOT_WINDOW_ID,i,0)==i);
    GrSetGCForeground(gc,GR_RGB(pal.palette[5].r,pal.palette[5].g,pal.palette[5].b));
    GrPoint(GR_ROOT_WINDOW_ID,gc,si.cols-1,si.rows-1);
    check(readp(GR_ROOT_WINDOW_ID,si.cols-1,si.rows-1)==5);
    GrPoint(GR_ROOT_WINDOW_ID,gc,si.cols/2,si.rows/2);
    check(readp(GR_ROOT_WINDOW_ID,si.cols/2,si.rows/2)==5);
    check(readp(GR_ROOT_WINDOW_ID,-1,0)==255);check(readp(GR_ROOT_WINDOW_ID,si.cols,0)==255);
    check(readp(9999,0,0)==255);
    w=GrNewWindow(GR_ROOT_WINDOW_ID,10,20,30,20,0,0,0);GrMapWindow(w);
    GrSetGCForeground(gc,GR_RGB(pal.palette[12].r,pal.palette[12].g,pal.palette[12].b));
    GrPoint(w,gc,17,5);check(readp(w,17,5)==12);check(readp(GR_ROOT_WINDOW_ID,27,25)==12);
    check(readp(w,30,5)==255);
    cover=GrNewWindow(GR_ROOT_WINDOW_ID,25,23,10,10,0,0,0);GrMapWindow(cover);
    check(readp(w,17,5)==255);GrDestroyWindow(cover);
    GrUnmapWindow(w);check(readp(w,17,5)==255);GrDestroyWindow(w);
    p=7;GrReadArea(GR_ROOT_WINDOW_ID,0,0,2,1,&p);check(p==255);
    GrDestroyGC(gc);GrClose();printf("READPIXEL %d checks, %d errors\n",checks,errors);return errors?1:0;
}
