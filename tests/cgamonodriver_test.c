/* Host model only: actual pinned Microwindows types, RAM in place of B800h. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
static volatile unsigned char vram[16384];
#define CGA_BASE vram
static unsigned char current_mode=3;
#define CGA_BIOS_MODE() current_mode
#include "../drivers/scr_cga.c"
int gr_mode;
MWPIXELVAL gr_background;
MWCOREFONT gen_fonts[NUMBER_FONTS];
static int bios_ax[32],bios_bx[32],bios_calls;
static int fail_open,fail_acquire,fail_bios,acquires,releases,closes;
static const char *console_override;static char opened_path[80];
char *cga_test_getenv(const char *name){assert(!strcmp(name,"CONSOLE"));return (char*)console_override;}
int cga_test_open(const char *path,int flags,...){assert(flags==(O_WRONLY|O_NONBLOCK));strcpy(opened_path,path);return fail_open?-1:7;}
int cga_test_close(int fd){assert(fd==7);closes++;return 0;}
int cga_test_ioctl(int fd,unsigned long request,...){assert(fd==7);if(request==DCGET_GRAPH){acquires++;return fail_acquire?-1:0;}assert(request==DCREL_GRAPH);assert(current_mode==3);releases++;return 0;}
FARADDR int10(int ax,int bx){assert(bios_calls<32);bios_ax[bios_calls]=ax;bios_bx[bios_calls++]=bx;if(ax==3||((ax==4||ax==6)&&!fail_bios))current_mode=ax;return 0;}
PSD gen_allocatememgc(PSD p){(void)p;return 0;}
MWBOOL gen_mapmemgc(PSD p,MWCOORD w,MWCOORD h,int planes,int bpp,
 MWIMGDATFMT format,unsigned int pitch,int size,void *addr)
{(void)p;(void)w;(void)h;(void)planes;(void)bpp;(void)format;(void)pitch;(void)size;(void)addr;return 0;}
void gen_freememgc(PSD p){(void)p;}
void gen_fillrect(PSD p,MWCOORD x,MWCOORD y,MWCOORD endx,MWCOORD endy,MWPIXELVAL c)
{for(;y<=endy;y++)p->DrawHorzLine(p,x,endx,y,c);}
void set_subdriver(PSD p,PSUBDRIVER s)
{
 p->DrawPixel=s->DrawPixel;p->ReadPixel=s->ReadPixel;
 p->DrawHorzLine=s->DrawHorzLine;p->DrawVertLine=s->DrawVertLine;
 p->FillRect=s->FillRect;p->BlitFallback=s->BlitFallback;
}
int main(void)
{
 unsigned char saved[32],memory[16]={0};int x,y;
 SCREENDEVICE m;MWSCREENINFO si;
 fail_open=1;assert(!scrdev.Open(&scrdev));assert(!bios_calls&&!acquires&&!closes);fail_open=0;
 fail_acquire=1;assert(!scrdev.Open(&scrdev));assert(!bios_calls&&acquires==1&&!releases&&closes==1);fail_acquire=0;
 fail_bios=1;assert(!scrdev.Open(&scrdev));assert(bios_calls==2&&acquires==2&&releases==1&&closes==2&&current_mode==3);fail_bios=0;
 bios_calls=acquires=releases=closes=0;
 assert(scrdev.Open(&scrdev)==&scrdev);assert(acquires==1&&!releases&&!strcmp(opened_path,"/dev/tty1"));
 {int old=bios_calls;assert(!scrdev.Open(&scrdev));assert(bios_calls==old&&acquires==1);}
 assert(scrdev.bpp==CGA_BPP&&scrdev.ncolors==(1u<<CGA_BPP)&&scrdev.pitch==80);
 assert(scrdev.xvirtres==CGA_WIDTH&&scrdev.yvirtres==200);
#if CGA_COLOR
 assert(bios_calls==3&&bios_ax[0]==4&&bios_ax[1]==0xb00&&bios_bx[1]==0x10&&bios_ax[2]==0xb00&&bios_bx[2]==0x101);
 assert(mwstdpal2[1].r==85&&mwstdpal2[1].g==255&&mwstdpal2[2].g==85&&mwstdpal2[3].b==255);
#else
 assert(bios_calls==2&&bios_ax[0]==6&&bios_ax[1]==0xb00&&bios_bx[1]==15);
#endif
 memset(&si,0,sizeof si);scrdev.GetScreenInfo(&scrdev,&si);
 assert(si.cols==CGA_WIDTH&&si.rows==200&&si.bpp==CGA_BPP&&si.ncolors==(1u<<CGA_BPP));
 gr_mode=MWROP_COPY;
 pixel(&scrdev,0,0,1);pixel(&scrdev,CGA_PPB-1,0,1);pixel(&scrdev,CGA_PPB,1,1);pixel(&scrdev,CGA_WIDTH-1,199,1);
 assert(vram[0]==((1u<<(8-CGA_BPP))|1)&&vram[8193]==(1u<<(8-CGA_BPP))&&vram[16191]==1);
 hline(&scrdev,3,20,2,CGA_MASK);
#if CGA_COLOR
 assert(vram[80]==3&&vram[81]==255&&vram[84]==255&&vram[85]==0xc0&&vram[86]==0);
 for(x=0;x<4;x++)pixel(&scrdev,x,12,x);
 assert(vram[480]==0x1b);
 /* Exercise all four source/destination indices and preserve neighboring pixels. */
 {unsigned a,b;int op;for(a=0;a<4;a++)for(b=0;b<4;b++)for(op=0;op<3;op++){
  int mode=op==0?MWROP_XOR:op==1?MWROP_AND:MWROP_OR;
  unsigned expected=op==0?(a^b):op==1?(a&b):(a|b);
  putpixel(&scrdev,10,14,3,MWROP_COPY);putpixel(&scrdev,9,14,b,MWROP_COPY);
  putpixel(&scrdev,9,14,a,mode);
  assert(readpixel(&scrdev,9,14)==expected&&readpixel(&scrdev,10,14)==3);
 }}
 for(x=0;x<32;x++){putpixel(&scrdev,x,16,x&3,MWROP_COPY);saved[x]=x&3;}
 blit(&scrdev,5,16,20,1,&scrdev,0,16,MWROP_COPY);
 for(x=0;x<20;x++)assert(readpixel(&scrdev,5+x,16)==saved[x]);
 blit(&scrdev,0,16,20,1,&scrdev,5,16,MWROP_COPY);
 for(x=0;x<20;x++)assert(readpixel(&scrdev,x,16)==saved[x]);
#else
 assert(vram[80]==0x1f&&vram[81]==255&&vram[82]==0xf8&&vram[83]==0);
#endif
 gr_mode=MWROP_XOR;hline(&scrdev,3,20,2,CGA_MASK);assert(!vram[80]&&!vram[81]&&!vram[82]);
 gr_mode=MWROP_COPY;pixel(&scrdev,15,4,1);gr_mode=MWROP_AND;pixel(&scrdev,15,4,0);assert(!readpixel(&scrdev,15,4));
 gr_mode=MWROP_OR;pixel(&scrdev,15,4,1);assert(readpixel(&scrdev,15,4));
 gr_mode=MWROP_INVERT;pixel(&scrdev,15,4,0);assert(readpixel(&scrdev,15,4)==(CGA_MASK^1));
 gr_mode=MWROP_COPY;
 for(x=0;x<32;x++){pixel(&scrdev,x,5,x%3==0);saved[x]=x%3==0;}
 blit(&scrdev,5,5,20,1,&scrdev,0,5,MWROP_COPY);
 for(x=0;x<20;x++)assert(readpixel(&scrdev,5+x,5)==saved[x]);
 for(y=6;y<9;y++)for(x=0;x<8;x++)pixel(&scrdev,x,y,(x+y)%2);
 blit(&scrdev,0,7,8,3,&scrdev,0,6,MWROP_COPY);
 for(y=7;y<10;y++)for(x=0;x<8;x++)assert(readpixel(&scrdev,x,y)==(unsigned)((x+y-1)%2));
 m=scrdev;m.flags=PSF_MEMORY;m.addr=memory;m.pitch=4;m.xres=m.xvirtres=4*CGA_PPB;m.yres=m.yvirtres=4;
 pixel(&m,9,2,1);assert(memory[8+9/CGA_PPB]==(1u<<((CGA_PPB-1-(9&(CGA_PPB-1)))*CGA_BPP)));blit(&scrdev,30,20,4*CGA_PPB,4,&m,0,0,MWROP_COPY);
 assert(readpixel(&scrdev,39,22)==1&&readpixel(&scrdev,38,22)==0);
 scrdev.Close(&scrdev);assert(bios_ax[bios_calls-1]==3&&releases==1&&closes==1);
 {int old=bios_calls;scrdev.Close(&scrdev);assert(bios_calls==old&&releases==1&&closes==1);}
 console_override="/dev/tty2";assert(scrdev.Open(&scrdev));assert(!strcmp(opened_path,"/dev/tty2"));scrdev.Close(&scrdev);assert(releases==2&&closes==2);
 puts("CGA packed host model: real driver types, banks, packed fills, ROPs, overlap, memory copy and BIOS sequence passed");
 return 0;
}
