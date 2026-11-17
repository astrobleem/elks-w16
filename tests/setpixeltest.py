#!/usr/bin/env python3
"""SetPixel uses actual native palette values, coordinates and failure results."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
s=(root/'src/gdi.c').read_text();a=s.index('u32 g_SetPixel(');e=s.index('{',a)+1;n=1
while n:
 n+=(s[e]=='{')-(s[e]=='}');e+=1
code=r"""
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint16_t u16;typedef int16_t i16;typedef uint32_t u32;
typedef struct{int count;struct{u8 r,g,b,pad;}palette[256];}GR_PALETTE;
typedef struct{int cols,rows;}GR_SCREEN_INFO;
struct win{int cw,ch;};struct dc{struct win*w;int wid,gc;};
static struct win win={71,96};static struct dc dcs[2]={{&win,1,2},{0,3,4}};
static GR_PALETTE scratch,palette;
#define w16_scratch (&scratch)
static u16 args[5];
#define AW(n) args[(n)/2]
#define AL(n) ((u32)AW(n)|((u32)AW((n)+2)<<16))
#define GS(d) (d)
#define GR_MODE_COPY 0
static int draws,palettecalls,mode=-1,fg,xout,yout,widout,gcout;
static struct dc*dc_get(u16 h){return h==9?&dcs[0]:h==10?&dcs[1]:0;}
static void GrGetScreenInfo(GR_SCREEN_INFO*i){i->cols=640;i->rows=480;}
static void GrGetSystemPalette(GR_PALETTE*p){*p=palette;palettecalls++;}
static int rgb_to_gr(u32 c){return c;}
static void gs_mode(struct dc*d,int m){mode=m;}
static void gs_fg(struct dc*d,int c){fg=c;}
static void GrPoint(int w,int g,int x,int y){draws++;widout=w;gcout=g;xout=x;yout=y;}
""" + s[a:e]+r"""
static u32 call(u16 dc,int x,int y,u32 c){args[0]=c;args[1]=c>>16;args[2]=y;args[3]=x;args[4]=dc;return g_SetPixel(0,0);}
static u32 oracle(u32 c){
 uint64_t best=UINT64_MAX;u32 answer=0;int i;
 for(i=0;i<16;i++){int r=(c&255)-palette.palette[i].r;
 int g=((c>>8)&255)-palette.palette[i].g,b=((c>>16)&255)-palette.palette[i].b;
 uint64_t d=(int64_t)r*r+(int64_t)g*g+(int64_t)b*b;
 if(d<best){best=d;answer=palette.palette[i].r|((u32)palette.palette[i].g<<8)|((u32)palette.palette[i].b<<16);}}
 return answer;
}
static void refuse(u16 dc,int x,int y,u32 c){
 int olddraws=draws,oldmode=mode,oldfg=fg;
 assert(call(dc,x,y,c)==0xffffffffUL);
 assert(draws==olddraws&&mode==oldmode&&fg==oldfg);
}
int main(void){
 int i,r,g,b,tests=0;u32 c,got;
 palette.count=16;
 for(i=0;i<16;i++){palette.palette[i].r=(i*43)%256;palette.palette[i].g=(i*67)%256;palette.palette[i].b=(i*109)%256;}
 palette.palette[1].r=87;palette.palette[1].g=0;palette.palette[1].b=0;
 palette.palette[2].r=0;palette.palette[2].g=128;palette.palette[2].b=0;
 assert(call(9,3,4,0x00008000)==0x00008000&&fg==0x00008000);
 assert(call(9,3,4,87)==87&&fg==87); /* not hardcoded EGA128/170 */
 for(r=0;r<256;r+=17)for(g=0;g<256;g+=17)for(b=0;b<256;b+=17){
 c=r|((u32)g<<8)|((u32)b<<16);got=call(9,70,95,c);
 assert(got==oracle(c)&&fg==(int)got&&mode==GR_MODE_COPY);
 assert(xout==70&&yout==95&&widout==1&&gcout==2);tests++;
 }
 assert(call(10,639,479,87)==87&&widout==3&&gcout==4&&xout==639&&yout==479);
 refuse(9,-1,0,0);refuse(9,0,-32768,0);refuse(9,71,0,0);refuse(9,0,96,0);
 refuse(10,640,0,0);refuse(10,0,480,0);refuse(10,32767,32767,0);
 refuse(0,0,0,0);refuse(0xc00,0,0,0);refuse(999,0,0,0);
 refuse(9,0,0,0x01000002);refuse(9,0,0,0x02000000);refuse(9,0,0,0xff000000);
 for(i=0;i<256;i+=15){if(i==16)continue;palette.count=i;refuse(9,0,0,0);}
 palette.count=16;
 /* Equidistant but distinct entries must choose the first, not last. */
 for(i=0;i<16;i++)palette.palette[i].r=palette.palette[i].g=palette.palette[i].b=255;
 palette.palette[0].r=0;palette.palette[0].g=palette.palette[0].b=0;
 palette.palette[1].r=2;palette.palette[1].g=palette.palette[1].b=0;
 assert(call(9,0,0,1)==0&&fg==0);
 printf("SetPixel: %d nearest-native-palette colors, tie, bounds and refusals passed\n",tests);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-O1','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
