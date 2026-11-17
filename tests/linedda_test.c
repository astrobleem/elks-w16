#include "w16.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u8 memory[65536];
static struct ne_module module;
struct ne_module *w16_mod=&module;
static long ax,ay,bx,by,count,major,minor;
static int depth,reenter;
static unsigned long checks;
u32 g_LineDDA(u16,u16);
u8 *host_ptr(u16 seg,u16 off,u16 n) {assert(seg==0x1000);assert((u32)off+n<=65536UL);return memory+off;}
static void args(u16 ap,int x,int y,int ex,int ey,u16 off,u16 seg,u32 data)
{wr32(0x1000,ap,data);wr16(0x1000,ap+4,off);wr16(0x1000,ap+6,seg);wr16(0x1000,ap+8,ey);wr16(0x1000,ap+10,ex);wr16(0x1000,ap+12,y);wr16(0x1000,ap+14,x);}
u32 w16_call16(u16 off,u16 seg,u16 ds,u16 n,const u16 *w)
{
 long dx=bx-ax,dy=by-ay,step;int x,y;
 assert(off==0x3456 && seg==0x2000 && ds==0x1234 && n==4);
 assert(w[2]==0xabcd && w[3]==0xef01);
 if(depth){assert((i16)w[0]==7 && (i16)w[1]==-9);return 0;}
 assert(count<major);
 step=(2*count*minor+major-1)/(2*major);
 if((dx<0?-dx:dx)>=(dy<0?-dy:dy)) {x=ax+(dx<0?-count:count);y=ay+(dy<0?-step:step);}
 else{x=ax+(dx<0?-step:step);y=ay+(dy<0?-count:count);}
 assert((i16)w[0]==x && (i16)w[1]==y);count++;checks++;
 if(reenter && count==1){
  u16 saved[4];memcpy(saved,w,sizeof saved);depth++;
  args(64,7,-9,8,-9,off,seg,0xabcdef01UL);assert(g_LineDDA(0x1000,64)==0);
  depth--;assert(!memcmp(saved,w,sizeof saved));
  /* The outer invocation must already have copied its entire argument frame. */
  memset(memory,0,16);
 }
 return 0xdeadbeefUL; /* VOID callback: ignored, never cancellation. */
}
static void run(int x,int y,int ex,int ey,u16 ap)
{
 long dx=(long)ex-x,dy=(long)ey-y;
 ax=x;ay=y;bx=ex;by=ey;count=0;
 if(dx<0)dx=-dx;if(dy<0)dy=-dy;major=dx>dy?dx:dy;minor=dx>dy?dy:dx;
 args(ap,x,y,ex,ey,0x3456,0x2000,0xabcdef01UL);
 assert(g_LineDDA(0x1000,ap)==0);assert(count==major);
}
int main(void)
{
 int x,y,ex,ey;module.dgroup=0x1234;
 for(x=-8;x<=8;x++)for(y=-8;y<=8;y++)for(ex=-8;ex<=8;ex++)for(ey=-8;ey<=8;ey++)run(x,y,ex,ey,0);
 run(-32768,-32768,32767,32767,0);run(32767,-32768,-32768,32767,0);
 run(-32768,32767,32767,32766,0);run(32767,32767,32766,-32768,0);
 run(-32768,0,32767,0,65520);run(0,32767,0,-32768,65520);
 reenter=1;run(316,164,260,196,0);reenter=0;
 count=0;assert(!g_LineDDA(0x1000,65521)&&!count);
 args(0,0,0,1,1,0,0,1);assert(!g_LineDDA(0x1000,0)&&!count);
 args(0,0,0,1,1,0xffff,0xffff,1);assert(!g_LineDDA(0x1000,0)&&!count);
 w16_mod=0;assert(!g_LineDDA(0x1000,0)&&!count);
 printf("LineDDA: %lu callback points, octants, endpoints, full-range ABI and reentry passed\n",checks);
 return 0;
}
