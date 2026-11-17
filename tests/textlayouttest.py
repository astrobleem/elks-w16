#!/usr/bin/env python3
import pathlib,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
source=(root/'src/textlayout.c').read_text().replace('#include "wapi.h"','')
pre='''#include "w16.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef int GR_SIZE;
struct dc {int dummy;}; static struct dc dc;
static int draws,xx,yy,nn;
struct dc *dc_get(u16 h){return h==0x200?&dc:0;}
u8 host_mem[0x110000];
u8 *host_ptr(u16 s,u16 o,u16 n){assert((u32)o+n<=65536&&LIN(s,o)+n<=0x100000);return host_mem+LIN(s,o);}
void fcopy_in(void*d,u16 s,u16 o,u16 n){memcpy(d,host_ptr(s,o,n),n);}
void gdi_text_size(const char*s,int n,int*w,int*h,int*b){(void)s;*w=n*6;*h=13;*b=10;}
void gdi_draw_text(struct dc*d,int x,int y,const char*s,int n){assert(d==&dc);(void)s;draws++;xx=x;yy=y;nn=n;}
'''
test='''
static void setup(u16 flags){
 wr16(0x1000,0,flags);wr16(0x1000,2,0);wr16(0x1000,4,0x2000);wr16(0x1000,6,7);wr16(0x1000,8,0);wr16(0x1000,10,0x3000);wr16(0x1000,12,0x200);
 wr16(0x2000,0,10);wr16(0x2000,2,20);wr16(0x2000,4,110);wr16(0x2000,6,60);
 memcpy(host_ptr(0x3000,0,8),"Score:0",8);
}
int main(void){int before;u8 rect[8];
 setup(0x122);assert(u_DrawText(0x1000,0)==13);assert(xx==68&&yy==20&&nn==7);
 setup(0x125);assert(u_DrawText(0x1000,0)==13);assert(xx==39&&yy==33);
 setup(0x128);assert(u_DrawText(0x1000,0)==13);assert(xx==10&&yy==47);
 before=draws;setup(0x420);assert(u_DrawText(0x1000,0)==13);assert(draws==before&&rd16(0x2000,4)==52&&rd16(0x2000,6)==33);
 setup(0x122);wr16(0x1000,6,0xffff);assert(u_DrawText(0x1000,0)==13);
 before=draws;setup(0x22);assert(!u_DrawText(0x1000,0)); /* clipping unsupported */
 setup(0x132);assert(!u_DrawText(0x1000,0)); /* wordbreak unsupported */
 setup(0x122);wr16(0x1000,6,129);assert(!u_DrawText(0x1000,0));
 setup(0x122);wr16(0x1000,12,999);assert(!u_DrawText(0x1000,0));
 setup(0x122);wr16(0x1000,2,0xfff9);assert(!u_DrawText(0x1000,0));
 setup(0x122);wr16(0x1000,8,0xfffa);assert(!u_DrawText(0x1000,0));
 setup(0x122);wr8(0x3000,0,'&');assert(!u_DrawText(0x1000,0));
 setup(0x122);wr8(0x3000,0,9);assert(!u_DrawText(0x1000,0));
 setup(0x122);wr16(0x1000,6,0xffff);memset(host_ptr(0x3000,0,128),'x',128);assert(!u_DrawText(0x1000,0));
 setup(0x420);wr16(0x2000,0,32760);memcpy(rect,host_ptr(0x2000,0,8),8);assert(!u_DrawText(0x1000,0));assert(!memcmp(rect,host_ptr(0x2000,0,8),8));
 assert(!u_DrawText(0x1000,0xfff3));assert(draws==before);
 setup(0x922);wr8(0x3000,0,'&');assert(u_DrawText(0x1000,0)==13);
 puts("DrawText alignment, measurement, failure and far-bound checks passed");return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(pre+source+test)
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
