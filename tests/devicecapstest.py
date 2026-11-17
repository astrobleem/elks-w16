#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
s=(root/'src/gdi.c').read_text();start=s.index('u32 g_GetDeviceCaps(');s=s[start:s.index('\n}',start)+2]
code=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
typedef uint16_t u16;typedef uint32_t u32;
typedef struct{int cols,rows;}GR_SCREEN_INFO;
static u16 args[2];static int width,height,calls;
#define AW(n) args[(n)/2]
static void GrGetScreenInfo(GR_SCREEN_INFO*p){p->cols=width;p->rows=height;calls++;}
'''+s+r'''
int main(void){int i;int sizes[][2]={{320,200},{640,480},{720,350},{0,0}};
for(i=0;i<4;i++){width=sizes[i][0];height=sizes[i][1];args[0]=8;assert(g_GetDeviceCaps(0,0)==width);args[0]=10;assert(g_GetDeviceCaps(0,0)==height);}
assert(calls==8);args[0]=12;assert(g_GetDeviceCaps(0,0)==4);args[0]=14;assert(g_GetDeviceCaps(0,0)==1);args[0]=88;assert(g_GetDeviceCaps(0,0)==34);assert(calls==8);return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(code)
 subprocess.run(['cc','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('GetDeviceCaps actual display dimensions and unchanged legacy caps passed')
