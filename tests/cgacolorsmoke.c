/* Native 8086 CGA mode/palette probe; not a Win16 application or mouse test. */
#include <stdio.h>
#include <unistd.h>
#include "nano-X.h"
int main(void)
{
 GR_SCREEN_INFO si;GR_PALETTE pal;GR_GC_ID gc;int i;
 volatile unsigned char __far *bda=(volatile unsigned char __far *)0x00400000UL;
 volatile unsigned char __far *vram=(volatile unsigned char __far *)0xb8000000UL;
 if(GrOpen()<0)return 1;
 GrGetScreenInfo(&si);GrGetSystemPalette(&pal);
 fprintf(stderr,"CGA-COLOR screen=%dx%d bpp=%d colors=%ld BIOS-mode=%u color-select=%02x\n",si.cols,si.rows,si.bpp,(long)si.ncolors,bda[0x49],bda[0x66]);
 if(si.cols!=320||si.rows!=200||si.bpp!=2||si.ncolors!=4||bda[0x49]!=4){GrClose();return 2;}
 gc=GrNewGC();
 for(i=0;i<4;i++){
  fprintf(stderr,"CGA-COLOR palette[%d]=%u,%u,%u\n",i,pal.palette[i].r,pal.palette[i].g,pal.palette[i].b);
  GrSetGCForegroundPixelVal(gc,i);GrFillRect(GR_ROOT_WINDOW_ID,gc,i*80,0,80,200);
 }
 GrFlush();GrGetScreenInfo(&si); /* round trip after drawing before reading B800h */
 fprintf(stderr,"CGA-COLOR packed=%02x,%02x,%02x,%02x\n",vram[0],vram[20],vram[40],vram[60]);
 sleep(12);GrDestroyGC(gc);GrClose();return 0;
}
