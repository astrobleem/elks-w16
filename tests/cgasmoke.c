/* Native Nano-X 8086 startup probe, not a Win16/control-interaction test.
 * Link with the matching small-model libnano-X.a and ELKS libc.
 * Shows a window for 25 seconds, then closes the connection.
 */
#include <stdio.h>
#include <unistd.h>
#include "nano-X.h"
int main(void) {
 GR_WINDOW_ID w; GR_GC_ID gc; GR_SCREEN_INFO si;
 fprintf(stderr,"CGA-CLIENT before GrOpen\n");
 if(GrOpen()<0) { fprintf(stderr,"CGA-CLIENT connect failed\n");return 1; }
 fprintf(stderr,"CGA-CLIENT connected\n");GrGetScreenInfo(&si);
 fprintf(stderr,"CGA-CLIENT screen %d %d bpp=%d colors=%ld\n",si.cols,si.rows,si.bpp,(long)si.ncolors);
 w=GrNewWindow(GR_ROOT_WINDOW_ID,60,35,510,125,2,GR_RGB(255,255,255),GR_RGB(255,255,255));
 gc=GrNewGC();GrSetGCForeground(gc,GR_RGB(0,0,0));GrSetGCBackground(gc,GR_RGB(255,255,255));
 GrMapWindow(w);GrText(w,gc,20,28,"IBM XT CGA 640 x 200",-1,GR_TFASCII);
 GrText(w,gc,20,53,"Nano-X connected: monochrome window",-1,GR_TFASCII);
 GrRect(w,gc,200,75,100,30);GrText(w,gc,225,96,"OK",-1,GR_TFASCII);
 GrFlush();fprintf(stderr,"CGA-CLIENT window drawn\n");sleep(25);
 GrDestroyWindow(w);GrClose();fprintf(stderr,"CGA-CLIENT closed\n");return 0;
}
