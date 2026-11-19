#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[1]
s=(root/'src/kernel.c').read_text();case=s[s.index('    case 0x0E:'):s.index('    case 0x25:')]+s[s.index('    case 0x3B:'):s.index('    case 0x41:')]
code=r'''
#include "w16.h"
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#define R_CX 4
#define R_AX 0
#define R_DX 6
#define R_SI 8
#define R_DS 14
#define R_FL 22
static void dispatch(u16 ss,u16 f){u16 ax=rd16(ss,f+R_AX),al=ax&255,fl=rd16(ss,f+R_FL)&~1;
 switch(rd16(ss,f+R_AX)>>8){
'''+case+r'''
 }wr16(ss,f+R_FL,fl);
}
static void call(u16 ss,u16 ds,u16 ax,const char *name,u16 cx){
 fcopy_out(ds,0,name,strlen(name)+1);wr16(ss,R_AX,ax);wr16(ss,R_DS,ds);wr16(ss,R_DX,0);wr16(ss,R_CX,cx);wr16(ss,R_FL,0x201);dispatch(ss,0);
}
int main(void){u16 ss,ds;char path[124],buf[64],native[1024],after[1024];int fd,status;pid_t child;
 assert(getcwd(native,sizeof(native)));

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
 /* All three new DOS paths share existing confinement and private cwd. */
 wr16(ss,R_AX,0x0e00);wr16(ss,R_DX,2);wr16(ss,R_FL,0x201);dispatch(ss,0);
 assert(rd16(ss,R_AX)==0x0e03&&rd16(ss,R_FL)==0x201);
 wr16(ss,R_AX,0x0e00);wr16(ss,R_DX,25);dispatch(ss,0);
 wr16(ss,R_AX,0x1900);dispatch(ss,0);assert(rd16(ss,R_AX)==0x1902);
 call(ss,ds,0x3b00,"c:\\pictures\\sub",0);assert(!(rd16(ss,R_FL)&1));
 files_directory_label(path);assert(!strcmp(path,"C:\\Pictures\\Sub"));
 call(ss,ds,0x3b00,"C:\\missing",0);assert(rd16(ss,R_AX)==3&&(rd16(ss,R_FL)&1));
 call(ss,ds,0x3b00,"D:\\",0);assert(rd16(ss,R_AX)==3&&(rd16(ss,R_FL)&1));
 files_directory_label(path);assert(!strcmp(path,"C:\\Pictures\\Sub"));
 child=fork();assert(child>=0);
 if(!child){call(ss,ds,0x3b00,"..",0);files_directory_label(path);_exit(strcmp(path,"C:\\Pictures")!=0);}
 assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 files_directory_label(path);assert(!strcmp(path,"C:\\Pictures\\Sub"));
 assert(getcwd(after,sizeof(after))&&!strcmp(native,after));
 fd=open("Pictures/Sub/Card.GRP",O_CREAT|O_WRONLY,0600);assert(fd>=0&&write(fd,"PMCC",4)==4&&!close(fd));
 call(ss,ds,0x4300,"card.grp",0xbeef);assert(!(rd16(ss,R_FL)&1)&&!rd16(ss,R_CX));
 call(ss,ds,0x4301,"card.grp",1);assert(!(rd16(ss,R_FL)&1));
 call(ss,ds,0x4300,"card.grp",0);assert(!(rd16(ss,R_FL)&1)&&rd16(ss,R_CX)==1);
 {struct stat st;assert(!stat("Pictures/Sub/Card.GRP",&st)&&!(st.st_mode&0222));}
 fcopy_out(ds,0,"card.grp",9);wr16(ss,0,0);wr16(ss,2,0);wr16(ss,4,ds);
 {extern u32 k_lcreat(u16,u16);assert(k_lcreat(ss,0)==0xffff);}
 assert(files_unlink(ds,0)==5);{struct stat st;assert(!stat("Pictures/Sub/Card.GRP",&st)&&st.st_size==4);}
 call(ss,ds,0x4301,"card.grp",0x20);assert(rd16(ss,R_AX)==5&&(rd16(ss,R_FL)&1));
 call(ss,ds,0x4301,"card.grp",0);assert(!(rd16(ss,R_FL)&1));
 call(ss,ds,0x4300,"C:\\Pictures",0);assert(!(rd16(ss,R_FL)&1)&&rd16(ss,R_CX)==0x10);
 call(ss,ds,0x4300,"C:\\",0);assert(!(rd16(ss,R_FL)&1)&&rd16(ss,R_CX)==0x10);
 call(ss,ds,0x4302,"card.grp",0);assert(rd16(ss,R_AX)==1&&(rd16(ss,R_FL)&1));
 call(ss,ds,0x4300,"absent.grp",0);assert(rd16(ss,R_AX)==2&&(rd16(ss,R_FL)&1));
 call(ss,ds,0x4300,"C:\\missing\\x.grp",0);assert(rd16(ss,R_AX)==3&&(rd16(ss,R_FL)&1));
 assert(!symlink("/tmp","Pictures/Sub/escape"));
 call(ss,ds,0x4300,"escape",0);assert(rd16(ss,R_AX)==5&&(rd16(ss,R_FL)&1));
 call(ss,ds,0x3b00,"escape",0);assert(rd16(ss,R_AX)==3&&(rd16(ss,R_FL)&1));
 call(ss,ds,0x4300,"..\\Sub\\Card.GRP",0);assert(rd16(ss,R_FL)&1);
 seg_free(ss);seg_free(ds);puts("DOS AH0E/19/3B/43/47: mapped drive, private cwd, attributes, failures and read-only protection passed");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(code)
 subprocess.run(['cc','-DW16_HOST','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'t.c'),str(root/'src/files.c'),str(root/'src/mem.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],cwd=p,check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
