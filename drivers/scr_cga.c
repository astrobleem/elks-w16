/* CGA BIOS mode 6 (640x200 mono) or mode 4 (320x200 four colors).
 * Both use two interleaved 8K banks; CGA_COLOR selects mode 4 at build time.
 * Recreated from the earlier elks-w16 CGA implementation for native-size use.
 * CGA_PAN optionally uses a bounded far-memory shadow; defaults remain native-size.
 * Copyright (c) 2026 elks-w16 contributors. MPL 1.1 / GPLv2 alternative,
 * as for the Microwindows drivers this integrates with.
 */
#if CGA_PAN
#include <malloc.h>
#include <string.h>
#else
#include <stddef.h>
#endif
#include "device.h"
#include "vgaplan4.h"
#include "genmem.h"
#include "genfont.h"
#include "fb.h"
#if CGA_COLOR
#define CGA_BPP 2
#define CGA_WIDTH 320
#define CGA_FORMAT MWIF_PAL2
/* BIOS palette 1, high intensity: black, cyan, magenta, white. */
const MWPALENTRY mwstdpal2[4]={
    RGBDEF(0,0,0),RGBDEF(85,255,255),RGBDEF(255,85,255),RGBDEF(255,255,255)
};
#else
#define CGA_BPP 1
#define CGA_WIDTH 640
#define CGA_FORMAT MWIF_MONOBYTEMSB
#endif
#define CGA_PPB (8/CGA_BPP)
#define CGA_MASK ((1u<<CGA_BPP)-1)
DEFINE_applyOpR
#ifndef CGA_BASE
#define CGA_BASE MK_FP(0xb800,0)
#endif
#if CGA_PAN
#ifndef CGA_VWIDTH
#define CGA_VWIDTH CGA_WIDTH
#endif
#ifndef CGA_VHEIGHT
#define CGA_VHEIGHT 300
#endif
#if CGA_VWIDTH < CGA_WIDTH || CGA_VHEIGHT < 200 || CGA_VWIDTH % (2*CGA_PPB) || (CGA_VWIDTH/CGA_PPB)*CGA_VHEIGHT > 65520L
#error Invalid CGA virtual dimensions
#endif
static FARADDR shadow;
static unsigned short shadow_seg;
static int dirty;
int cga_viewx,cga_viewy;
static int present(PSD p)
{
    int y,x;
    if(!dirty)return 0;
    for(y=0;y<200;y++){
        volatile unsigned short __far *d=(volatile unsigned short __far *)(CGA_BASE+((y&1)<<13)+(y>>1)*80);
        volatile unsigned short __far *s=(volatile unsigned short __far *)(shadow+(unsigned)(y+cga_viewy)*p->pitch+cga_viewx/CGA_PPB);
        for(x=0;x<40;x++)d[x]=s[x];
    }
    dirty=0;return 0;
}
/* Alt+I/J/K/L pans up/left/down/right; Alt+H returns to the origin. */
int cga_pan_key(MWKEY key,int *dx,int *dy)
{
    int x=cga_viewx,y=cga_viewy;
    switch(key){
    case 'i': y-=100;break;case 'k': y+=100;break;
    case 'j': x-=CGA_WIDTH/2;break;case 'l': x+=CGA_WIDTH/2;break;
    case 'h': x=y=0;break;default:return 0;
    }
    if(x<0)x=0;
    if(x>CGA_VWIDTH-CGA_WIDTH)x=CGA_VWIDTH-CGA_WIDTH;
    if(y<0)y=0;
    if(y>CGA_VHEIGHT-200)y=CGA_VHEIGHT-200;
    *dx=x-cga_viewx;*dy=y-cga_viewy;cga_viewx=x;cga_viewy=y;dirty=1;
    EPRINTF("CGA-PAN viewport=%d,%d\n",x,y);
    return 1;
}
#endif
static FARADDR address(PSD p,int x,int y)
{
#if CGA_PAN
    if(p->flags&PSF_SCREEN)return shadow+(unsigned)y*p->pitch+x/CGA_PPB;
#else
    if(p->flags&PSF_SCREEN)
        return CGA_BASE+((y&1)<<13)+(y>>1)*80+(x/CGA_PPB);
#endif
    return (FARADDR)(p->addr+(unsigned)y*p->pitch+(x/CGA_PPB));
}
static MWPIXELVAL readpixel(PSD p,MWCOORD x,MWCOORD y)
{return (*address(p,x,y)>>((CGA_PPB-1-(x&(CGA_PPB-1)))*CGA_BPP))&CGA_MASK;}
static void putpixel(PSD p,int x,int y,MWPIXELVAL c,int op)
{
    FARADDR a=address(p,x,y);unsigned shift=(CGA_PPB-1-(x&(CGA_PPB-1)))*CGA_BPP,old=*a;
    unsigned value=op==MWROP_COPY?c:(unsigned)applyOpR(op,c,(old>>shift)&CGA_MASK);
    *a=(old&~(CGA_MASK<<shift))|((value&CGA_MASK)<<shift);
#if CGA_PAN
    if(p->flags&PSF_SCREEN)dirty=1;
#endif
}
static void pixel(PSD p,MWCOORD x,MWCOORD y,MWPIXELVAL c)
{putpixel(p,x,y,c,gr_mode);}
static void hline(PSD p,MWCOORD x,MWCOORD end,MWCOORD y,MWPIXELVAL c)
{
    unsigned char value=(c&CGA_MASK)*(255/CGA_MASK);
#if CGA_PAN
    if(p->flags&PSF_SCREEN)dirty=1;
#endif
    if(gr_mode!=MWROP_COPY){while(x<=end)putpixel(p,x++,y,c,gr_mode);return;}
    while(x<=end&&(x&(CGA_PPB-1)))putpixel(p,x++,y,c,MWROP_COPY);
    while(x+CGA_PPB-1<=end){*address(p,x,y)=value;x+=CGA_PPB;}
    while(x<=end)putpixel(p,x++,y,c,MWROP_COPY);
}
static void vline(PSD p,MWCOORD x,MWCOORD y,MWCOORD end,MWPIXELVAL c)
{while(y<=end)putpixel(p,x,y++,c,gr_mode);}
static void blit(PSD d,MWCOORD dx,MWCOORD dy,MWCOORD w,MWCOORD h,
                 PSD s,MWCOORD sx,MWCOORD sy,int op)
{
    int i,j,x,y,reverse=d==s&&(dy>sy||(dy==sy&&dx>sx));
    for(j=0;j<h;j++)for(i=0;i<w;i++){
        MWPIXELVAL c;x=reverse?w-1-i:i;y=reverse?h-1-j:j;
        c=s->ReadPixel(s,sx+x,sy+y);
        if(op==MWROP_SRCTRANSCOPY){if(c==(MWPIXELVAL)s->transcolor)continue;putpixel(d,dx+x,dy+y,c,MWROP_COPY);}
        else putpixel(d,dx+x,dy+y,c,op);
    }
}
static SUBDRIVER packed={pixel,readpixel,hline,vline,gen_fillrect,blit};
/* The ELKS memory selector expects this symbol; the build rejects other bpp. */
PSUBDRIVER memplan4[4]={&packed};
static PSD open_screen(PSD p)
{
    p->xres=p->xvirtres=CGA_WIDTH;p->yres=p->yvirtres=200;
    p->planes=1;p->bpp=CGA_BPP;p->ncolors=1u<<CGA_BPP;p->pixtype=MWPF_PALETTE;
    p->data_format=CGA_FORMAT;p->flags=PSF_SCREEN;
    p->pitch=80;p->addr=0;p->size=0;
#if CGA_PAN
    if(_fmemalloc(((unsigned long)(CGA_VWIDTH/CGA_PPB)*CGA_VHEIGHT+15)/16,&shadow_seg)<0){
        EPRINTF("CGA-PAN cannot allocate %lu shadow bytes\n",(unsigned long)(CGA_VWIDTH/CGA_PPB)*CGA_VHEIGHT);return NULL;
    }
    shadow=MK_FP(shadow_seg,0);
    fmemset((void __far *)shadow,0,(CGA_VWIDTH/CGA_PPB)*CGA_VHEIGHT);
    p->xres=p->xvirtres=CGA_VWIDTH;p->yres=p->yvirtres=CGA_VHEIGHT;
    p->pitch=CGA_VWIDTH/CGA_PPB;p->PreSelect=present;
    cga_viewx=cga_viewy=0;dirty=1;
    EPRINTF("CGA-PAN allocated %lu bytes; virtual=%dx%d physical=%dx200\n",(unsigned long)p->pitch*CGA_VHEIGHT,CGA_VWIDTH,CGA_VHEIGHT,CGA_WIDTH);
#endif
    set_subdriver(p,&packed);
#if CGA_COLOR
    int10(4,0);
    int10(0x0b00,0x0010); /* black background, high-intensity palette bit */
    int10(0x0b00,0x0101); /* palette 1 (cyan/magenta/white) */
#else
    int10(6,0);
    int10(0x0b00,0x000f); /* explicit white foreground in the high-resolution mode */
#endif
    return p;
}
static void close_screen(PSD p){
    (void)p;int10(3,0);
#if CGA_PAN
    if(shadow_seg){_fmemfree(shadow_seg);shadow_seg=0;shadow=0;}
#endif
}
static void palette(PSD p,int first,int count,MWPALENTRY *pal)
{(void)p;(void)first;(void)count;(void)pal;}
static void info(PSD p,PMWSCREENINFO s)
{
    s->rows=p->yvirtres;s->cols=p->xvirtres;s->planes=1;s->bpp=p->bpp;
    s->ncolors=p->ncolors;s->pixtype=p->pixtype;s->fonts=NUMBER_FONTS;
    s->xdpcm=27;s->ydpcm=11;
}
SCREENDEVICE scrdev={
    0,0,0,0,0,0,0,NULL,0,NULL,0,0,0,0,0,0,gen_fonts,
    open_screen,close_screen,palette,info,
    gen_allocatememgc,gen_mapmemgc,gen_freememgc,0,NULL
};
