#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[1]
code=r"""
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef int16_t i16;
struct dc{u16 mapmode;i16 wx,wy,vx,vy,wex,wey,vex,vey;}d;
static u16 a[4];static u16 mem[32768];
#define AW(n) a[(n)/2]
static struct dc*dc_get(u16 h){return h==9?&d:0;}
static u16 rd16(u16 s,u16 o){assert(s==0x1000);return mem[o/2];}
static void wr16(u16 s,u16 o,u16 v){assert(s==0x1000);mem[o/2]=v;}
"""
code+='static u32 bitmap_origin(u16 h,int set,int v,i16 x,i16 y){return 0;}\n'
code+=(root/'src/mapping.c').read_text().replace('#include "wapi.h"','')
code+=r"""
int main(void){int x,y,i,j,l,t,r,b;u32 old;
 gdi_map_init(&d);x=-3;y=4;assert(gdi_map_point(&d,&x,&y)&&x==-3&&y==4);
 a[0]=8;a[1]=9;assert(g_SetMapMode(0,0)==1);
 a[0]=2;a[1]=9;assert(!g_SetMapMode(0,0)&&d.mapmode==8);
 a[0]=100;a[1]=100;a[2]=9;assert(g_SetWindowExt(0,0)==0x10001);
 a[0]=50;a[1]=200;assert(g_SetViewportExt(0,0)==0x10001);
 a[0]=4;a[1]=3;g_SetWindowOrg(0,0);a[0]=8;a[1]=7;g_SetViewportOrg(0,0);
 for(i=-50;i<50;i++)for(j=-50;j<50;j++){
  x=i*2+3;y=j*2+4;assert(gdi_map_point(&d,&x,&y));assert(x==i*4+7&&y==j+8);
  assert(unmap_point(&d,&x,&y)&&x==i*2+3&&y==j*2+4);
 }
 a[0]=0;a[1]=3;old=g_SetWindowExt(0,0);assert(!old&&d.wex==100&&d.wey==100);
 d.wx=d.wy=d.vx=d.vy=0;d.vex=-200;d.vey=50;
 l=0;t=0;r=10;b=10;assert(gdi_map_rect(&d,&l,&t,&r,&b)&&l==-20&&r==0&&t==0&&b==5);
 d.wex=1;d.vex=32767;x=32767;y=0;assert(!gdi_map_point(&d,&x,&y)&&x==32767);
 mem[0]=0;mem[1]=0;mem[2]=32767;mem[3]=0;
 a[0]=2;a[1]=0;a[2]=0x1000;a[3]=9;assert(!g_LPtoDP(0,0)&&mem[2]==32767);
 a[0]=2;a[1]=65532;assert(!g_LPtoDP(0,0));
 gdi_map_init(&d);a[0]=2;a[1]=0;assert(g_LPtoDP(0,0)&&g_DPtoLP(0,0));
 puts("Mapping signed anisotropic transforms, extents, inverse and overflow passed");return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-fsanitize=address,undefined','-no-pie',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
