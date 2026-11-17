#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[1]
s=(root/'src/kernel.c').read_text();case=s[s.index('    case 0x47:'):s.index('    case 0x41:')]
code=r'''
#include "w16.h"
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#define R_AX 0
#define R_DX 6
#define R_SI 8
#define R_DS 14
#define R_FL 22
static void dispatch(u16 ss,u16 f){u16 fl=rd16(ss,f+R_FL)&~1;
 switch(rd16(ss,f+R_AX)>>8){
'''+case+r'''
 }wr16(ss,f+R_FL,fl);
}
int main(void){u16 ss,ds;char path[124],buf[64];
 assert(!seg_alloc(64,&ss)&&!seg_alloc(64,&ds));
 assert(!mkdir("Pictures",0700));assert(!mkdir("Pictures/Sub",0700));
 assert(!files_directory("C:\\Pictures\\Sub",path,1));
 wr16(ss,R_AX,0x4700);wr16(ss,R_DX,3);wr16(ss,R_DS,ds);wr16(ss,R_SI,50);wr16(ss,R_FL,0x201);
 dispatch(ss,0);fcopy_in(buf,ds,50,13);
 assert(!strcmp(buf,"Pictures\\Sub"));assert(rd16(ss,R_AX)==0x100&&rd16(ss,R_FL)==0x200);
 wr16(ss,R_AX,0x4700);wr16(ss,R_DX,4);wr16(ss,R_FL,0x200);dispatch(ss,0);
 assert(rd16(ss,R_AX)==15&&rd16(ss,R_FL)==0x201);
 assert(!files_directory("C:\\",path,1));wr16(ss,R_AX,0x4700);wr16(ss,R_DX,0);dispatch(ss,0);
 assert(!rd8(ds,50)&&rd16(ss,R_AX)==0x100&&rd16(ss,R_FL)==0x200);
 seg_free(ss);seg_free(ds);puts("DOS AH47 drive, DS:SI buffer, directory formatting and carry/result passed");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(code)
 subprocess.run(['cc','-DW16_HOST','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'t.c'),str(root/'src/files.c'),str(root/'src/mem.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],cwd=p,check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
