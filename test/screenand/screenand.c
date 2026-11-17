#include <stdio.h>
#include "nano-X.h"
#ifndef GR_MODE_HAVE_SCREENAND
#define GR_MODE_HAVE_SCREENAND 0x2000
#endif
static int checks,errors;
static void check(int yes){++checks;if(!yes){++errors;printf("SCREENAND fail %d\n",checks);}}
static GR_PIXELVAL readp(int x,int y){
 GR_PIXELVAL pixel=255;GrReadArea(GR_ROOT_WINDOW_ID,x,y,1,1,&pixel);return pixel;
}
int main(int argc,char **argv){
 GR_GC_ID gc;GR_GC_INFO info;int s,d,x,y;
 if(GrOpen()<0)return 90;
 gc=GrNewGC();if(!gc){GrClose();return 91;}
 GrGetGCInfo(gc,&info);
 if(!(info.mode&GR_MODE_HAVE_SCREENAND)){
  puts("SCREENAND unsupported server refused");
  GrDestroyGC(gc);GrClose();return argc>1?0:2;
 }
 if(argc>1){puts("SCREENAND expected old server");GrDestroyGC(gc);GrClose();return 3;}
 if(!(info.mode&GR_MODE_HAVE_READPIXEL)){GrDestroyGC(gc);GrClose();return 4;}
 /* Query-only capabilities must not be passed into the drawing mode. */
 GrSetGCMode(gc,info.mode);GrGetGCInfo(gc,&info);
 check((info.mode&255)==GR_MODE_COPY);
 GrSetGCMode(gc,GR_MODE_AND|GR_MODE_HAVE_SCREENAND);
 GrGetGCInfo(gc,&info);check((info.mode&255)==GR_MODE_AND);
 for(s=0;s<16;s++)for(d=0;d<16;d++){
  x=10+s*3;y=10+d*3;
  GrSetGCMode(gc,GR_MODE_COPY);GrSetGCForegroundPixelVal(gc,d);
  GrFillRect(GR_ROOT_WINDOW_ID,gc,x,y,3,2);
  GrSetGCMode(gc,GR_MODE_AND);GrSetGCForegroundPixelVal(gc,s);
  GrFillRect(GR_ROOT_WINDOW_ID,gc,x,y,3,2);
  check(readp(x,y)==(s&d));
  check(readp(x+2,y+1)==(s&d)); /* odd-width tail and second row */
 }
 GrMoveCursor(100,100);
 GrSetGCMode(gc,GR_MODE_COPY);GrSetGCForegroundPixelVal(gc,13);
 GrFillRect(GR_ROOT_WINDOW_ID,gc,98,98,13,7);
 GrSetGCMode(gc,GR_MODE_AND);GrSetGCForegroundPixelVal(gc,11);
 GrFillRect(GR_ROOT_WINDOW_ID,gc,98,98,13,7);
 for(y=98;y<105;y++)for(x=98;x<111;x++)check(readp(x,y)==(13&11));
 GrDestroyGC(gc);GrClose();
 printf("SCREENAND %d checks, %d errors\n",checks,errors);
 return errors?1:0;
}
