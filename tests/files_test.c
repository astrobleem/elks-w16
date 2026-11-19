#include <stdlib.h>
#include <sys/wait.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "w16.h"
u32 k_lcreat(u16,u16),k_lopen(u16,u16),k_lclose(u16,u16),k_lread(u16,u16),k_lwrite(u16,u16),k_llseek(u16,u16),k_OpenFile(u16,u16);
u32 k_GetWindowsDirectory(u16,u16);
u32 u_GlobalAddAtom(u16,u16),u_GlobalFindAtom(u16,u16),u_GlobalDeleteAtom(u16,u16);
u32 k_GetTempDrive(u16,u16),k_GetTempFileName(u16,u16);
extern int host_live;
static u16 ss,ns,ds,ofs;
/* Match ELKS: rename cannot overwrite. Shared atom updates must work with
 * link/unlink alone, rather than relying on the host POSIX replacement. */
int rename(const char *old,const char *next){return link(old,next)?-1:unlink(old);}
static int sync_calls;
void sync(void){sync_calls++;}
static void arg(int n,u16 v){wr16(ss,n,v);}
static void name(const char *s){fcopy_out(ns,0,s,strlen(s)+1);}
static u16 create(const char *s){name(s);arg(0,0);arg(2,0);arg(4,ns);return k_lcreat(ss,0);}
static void closeh(u16 h){arg(0,h);assert(!k_lclose(ss,0));}
static u16 openh(const char *s){name(s);arg(0,0);arg(2,0);arg(4,ns);return k_lopen(ss,0);}
static u32 seekh(u16 h,i32 pos,u16 whence){arg(0,whence);wr32(ss,2,pos);arg(6,h);return k_llseek(ss,0);}
static u32 transfer(u16 h,u16 count,int writeit){arg(0,count);arg(2,0);arg(4,ds);arg(6,h);return writeit?k_lwrite(ss,0):k_lread(ss,0);}
static u16 openfile(const char *s,u16 flags){name(s);arg(0,flags);arg(2,0);arg(4,ofs);arg(6,0);arg(8,ns);return k_OpenFile(ss,0);}
/* Real BMP-shaped streams cross 64 KiB at VGA dimensions; API counts remain
 * 16 bit and seek offsets/file lengths must remain 32 bit. */
static void bitmap_roundtrip(u16 width,u16 height)
{
    u16 h,y,x,row=(width/2+3)&~3u;
    u32 size=118UL+(u32)row*height;
    const char *file=width==256?"SMALL.BMP":"VGA.BMP";
    h=create(file);assert(h!=0xffff);
    fzero(ds,0,118);wr8(ds,0,'B');wr8(ds,1,'M');wr32(ds,2,size);
    wr32(ds,10,118);wr32(ds,14,40);wr32(ds,18,width);wr32(ds,22,height);
    wr16(ds,26,1);wr16(ds,28,4);wr32(ds,34,(u32)row*height);wr32(ds,46,16);
    for(x=0;x<16;x++){wr8(ds,54+4*x,x*17);wr8(ds,55+4*x,x*17);wr8(ds,56+4*x,x*17);}
    assert(transfer(h,118,1)==118);
    for(y=0;y<height;y++) {
        for(x=0;x<row;x++)wr8(ds,x,(u8)(x+y));
        assert(transfer(h,row,1)==row);
    }
    assert(seekh(h,0,2)==size);closeh(h);
    h=openh(file);assert(h!=0xffff);
    assert(transfer(h,118,0)==118);assert(rd32(ds,2)==size&&rd32(ds,18)==width&&rd32(ds,22)==height);
    for(y=0;y<height;y++) {
        assert(transfer(h,row,0)==row);
        for(x=0;x<row;x++)assert(rd8(ds,x)==(u8)(x+y));
    }
    assert(!transfer(h,1,0));closeh(h);
}


static u16 atom(const char *s,int add)
{name(s);arg(0,0);arg(2,ns);return add?u_GlobalAddAtom(ss,0):u_GlobalFindAtom(ss,0);}
static void deleteatom(u16 a){arg(0,a);assert(!u_GlobalDeleteAtom(ss,0));}
static void atom_tests(const char *exe)
{
 u16 a,b,i,table[64];int status;pid_t child;char dir[80],number[12],longname[257];
 assert(!unsetenv("W16SESSION"));
 arg(0,42);arg(2,0);assert(u_GlobalAddAtom(ss,0)==42);assert(u_GlobalFindAtom(ss,0)==42);
 arg(0,0xc000);assert(!u_GlobalAddAtom(ss,0));
 assert(atom("#42",1)==42);assert(atom("#49152",1)==0);assert(atom("#000",0)==0);
 assert(atom("#9999999999999999999999",1)==0);
 assert(!atom("",1));
 a=atom("Shared-Name",1);assert(a==0xc000);strcpy(dir,session_directory());
 assert(atom("shared-NAME",0)==a);
 assert(!atom("#12x",0));b=atom("#12x",1);assert(b>a);deleteatom(b);
 b=atom("\xc0\xd7",1);assert(atom("\xe0\xd7",0)==b);assert(!atom("\xe0\xf7",0));deleteatom(b);
 sprintf(number,"%u",a);child=fork();assert(child>=0);
 if(!child){execl(exe,exe,"atoms-child",dir,number,(char *)0);_exit(99);}
 assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 deleteatom(a);assert(atom("shared-name",0)==a); /* child's reference survives exit */
 deleteatom(a);assert(!atom("Shared-Name",0));
 b=atom("only-child",0);assert(b);deleteatom(b);
 child=fork();assert(child>=0);
 if(!child){execl(exe,exe,"atoms-crash",dir,number,(char *)0);_exit(99);}
 assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 b=atom("crash-reference",0);assert(b);deleteatom(b); /* lifetime is the session, not task */
 assert(session_end_begin());session_end_cancel(); /* dead peer lease pruned */
 /* A separate root session does not see these names. */
 a=atom("isolation",1);child=fork();assert(child>=0);
 if(!child){execl(exe,exe,"atoms-isolated",(char *)0);_exit(99);}
 assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 assert(atom("isolation",0)==a);deleteatom(a);
 memset(longname,'x',255);longname[255]=0;a=atom(longname,1);assert(a);deleteatom(a);
 memset(longname,'x',256);longname[256]=0;
 /* Point at a segment with space for an unterminated 256-byte name. */
 fcopy_out(ns,0,longname,256);arg(0,0);arg(2,ns);assert(!u_GlobalAddAtom(ss,0));
 arg(0,65535);arg(2,ns); /* physical/offset wrap rejected before the second read */
 /* Check offset wrap with a mapped full-size segment, avoiding host unmapped reads. */
 {u16 large;assert(!seg_alloc(4096,&large));wr8(large,65535,'x');arg(2,large);
  assert(!u_GlobalAddAtom(ss,0));seg_free(large);}
 for(i=0;i<64;i++){sprintf(number,"name%u",i);table[i]=atom(number,1);assert(table[i]);}
 assert(!atom("full",1));for(i=0;i<64;i++)deleteatom(table[i]);
 assert(session_end_begin());assert(!session_end_begin());
 assert(atom("during-query",1));session_end_cancel();assert(session_end_begin());
 session_shutdown();assert(access(dir,F_OK)<0);
}
static void windir_tests(void)
{
 char path[124];u16 n;
 unsetenv("W16WINDIR");arg(0,124);arg(2,0);arg(4,ofs);assert(!k_GetWindowsDirectory(ss,0));
 setenv("W16WINDIR","C:\\pictures",1);n=k_GetWindowsDirectory(ss,0);assert(n==11);
 fcopy_in(path,ofs,0,n+1);assert(!strcmp(path,"C:\\Pictures"));
 assert(!files_directory("C:\\Pictures",path,1));assert(k_GetWindowsDirectory(ss,0)==n);
 arg(0,n);wr8(ofs,0,0x7f);assert(k_GetWindowsDirectory(ss,0)==(u32)n+1&&rd8(ofs,0)==0x7f);
 arg(0,0);assert(k_GetWindowsDirectory(ss,0)==(u32)n+1);
 arg(0,124);arg(2,65535);assert(!k_GetWindowsDirectory(ss,0));arg(2,0);
 setenv("W16WINDIR","C:\\linked",1);assert(!k_GetWindowsDirectory(ss,0));
 setenv("W16WINDIR","D:\\Pictures",1);assert(!k_GetWindowsDirectory(ss,0));
 setenv("W16WINDIR","Pictures",1);assert(!k_GetWindowsDirectory(ss,0));
 setenv("W16WINDIR","C:\\",1);assert(k_GetWindowsDirectory(ss,0)==3);
 fcopy_in(path,ofs,0,4);assert(!strcmp(path,"C:\\"));
 assert(!files_directory("C:\\",path,1));unsetenv("W16WINDIR");
}
int main(int argc,char **argv)
{
    u16 h,i,slots[16];char path[128];
    assert(!seg_alloc(8,&ss));assert(!seg_alloc(16,&ns));assert(!seg_alloc(256,&ds));assert(!seg_alloc(16,&ofs));

    if(argc>1) {
        if(!strcmp(argv[1],"atoms-child")||!strcmp(argv[1],"atoms-crash")) {
            u16 a;assert(argc==4);assert(!setenv("W16SESSION",argv[2],1));
            assert(!session_end_begin()); /* parent is a second live participant */
            if(!strcmp(argv[1],"atoms-crash")){assert(atom("crash-reference",1));_exit(0);}
            a=atoi(argv[3]);assert(atom("SHARED-name",0)==a);assert(atom("shared-name",1)==a);
            assert(atom("only-child",1));
        } else {assert(!strcmp(argv[1],"atoms-isolated"));unsetenv("W16SESSION");
            assert(!atom("isolation",0));assert(atom("local",1));}
        session_shutdown();seg_free(ss);seg_free(ns);seg_free(ds);seg_free(ofs);return 0;
    }
    atom_tests(argv[0]);
    h=create("Mixed.BMP");assert(h!=0xffff&&h>255);
    for(i=0;i<1400;i++)wr8(ds,i,(u8)(i*17));
    assert(transfer(h,1400,1)==1400);
    assert(seekh(h,-4,2)==1396);assert(transfer(h,4,0)==4);
    for(i=0;i<4;i++)assert(rd8(ds,i)==(u8)((i+1396)*17));
    assert(seekh(h,0,0)==0);
    assert(transfer(h,0,1)==0xffff); /* zero write must not pretend truncation */
    assert(seekh(h,0,2)==1400);assert(transfer(h,0,1)==0);
    assert(seekh(h,-1401,2)==0xffffffffUL);
    closeh(h);arg(0,h);assert(k_lclose(ss,0)==0xffff);
    h=openh("C:\\mixed.bmp");assert(h!=0xffff);
    fzero(ds,0,2000);assert(transfer(h,2000,0)==1400);
    for(i=0;i<1400;i++)assert(rd8(ds,i)==(u8)(i*17));
    assert(!transfer(h,1,0));closeh(h);
    assert(transfer(1,10,0)==0xffff);assert(transfer(2,10,1)==0xffff);
    assert(seekh(1,0,0)==0xffffffffUL);
    assert(openh("/etc/passwd")==0xffff);
    assert(openh("../Mixed.BMP")==0xffff);
    assert(openh("D:\\Mixed.BMP")==0xffff);
    assert(create("bad*.bmp")==0xffff);
    assert(!mkdir("Pictures",0700));
    h=create("pictures/One.Bmp");assert(h!=0xffff);assert(transfer(h,12,1)==12);closeh(h);
    h=openh("PICTURES\\one.bmp");assert(h!=0xffff);closeh(h);
#ifdef W16_HOST
    assert(!symlink("Mixed.BMP","link.bmp"));assert(openh("link.bmp")==0xffff);
    assert(create("link.bmp")==0xffff);
    assert(!symlink("Pictures","linked"));assert(openh("linked/one.bmp")==0xffff);
#endif
    h=create("Case.bmp");closeh(h);
    { int fd=open("case.BMP",O_CREAT|O_WRONLY,0600);assert(fd>=0);close(fd); }
    assert(openh("CASE.BMP")==0xffff);
    h=openh("Case.bmp");assert(h!=0xffff);closeh(h);
    h=openfile("c:\\mixed.bmp",0);assert(h!=0xffff);
    assert(rd8(ofs,0)==136&&rd8(ofs,1)==1&&!rd16(ofs,2));
    fcopy_in(path,ofs,8,128);assert(!strcmp(path,"C:\\Mixed.BMP"));closeh(h);
    h=openfile("ignored",0x8000);assert(h!=0xffff);closeh(h);
    assert(openfile("NoFile.bmp",0x100)==0&&!rd16(ofs,2));
    assert(openfile("NoFile.bmp",0)==0xffff&&rd16(ofs,2)==2);
    h=openfile("Exists.BMP",0x1000);assert(h!=0xffff);closeh(h);
    assert(openfile("exists.bmp",0x4000)!=0xffff);
    assert(openfile("exists.bmp",0x200)==0);assert(openh("exists.bmp")==0xffff);
    for(i=0;i<16;i++){slots[i]=openh("mixed.bmp");assert(slots[i]!=0xffff);}
    assert(create("Mixed.BMP")==0xffff); /* full table cannot truncate file */
    files_shutdown();for(i=0;i<16;i++){arg(0,slots[i]);assert(k_lclose(ss,0)==0xffff);}
    h=openh("mixed.bmp");assert(h!=0xffff&&seekh(h,0,2)==1400);
    arg(0,32);arg(2,65530);arg(4,ds);arg(6,h);assert(k_lread(ss,0)==0xffff);
    closeh(h);files_shutdown();
    {int before=sync_calls;h=openh("mixed.bmp");assert(h!=0xffff);closeh(h);assert(sync_calls==before);
     h=create("flush.bmp");assert(h!=0xffff);assert(transfer(h,12,1)==12);closeh(h);assert(sync_calls==before+1);}
    bitmap_roundtrip(256,256);bitmap_roundtrip(640,480);
    name("SMALL.BMP");assert(!files_unlink(ns,0));assert(files_unlink(ns,0)==2);
    name("../VGA.BMP");assert(files_unlink(ns,0)==5);assert(openh("VGA.BMP")!=0xffff);
    files_shutdown();
    assert(k_GetTempDrive(ss,0)==0x00013a43UL);
    name("PB");arg(0,0);arg(2,ofs);arg(4,0);arg(6,0);arg(8,ns);arg(10,0);
    i=k_GetTempFileName(ss,0);assert(i);
    fcopy_in(path,ofs,0,15);h=openh(path);assert(h!=0xffff);closeh(h);
    assert(openfile(path,0x200)==0);
    name("PB");arg(0,0);arg(2,ofs);arg(4,0x1234);arg(6,0);arg(8,ns);arg(10,'C');
    assert(k_GetTempFileName(ss,0)==0x1234);
    fcopy_in(path,ofs,0,15);assert(openh(path)==0xffff);
    /* Actual PBRUSH.DLL passes numeric drive 3, not ASCII C. */
    for(i=0;i<2;i++){
        name("PB");arg(0,0);arg(2,ofs);arg(4,0);arg(6,0);arg(8,ns);arg(10,i?0x83:3);
        assert(k_GetTempFileName(ss,0));
        fcopy_in(path,ofs,0,15);assert(!strncmp(path,"C:\\~PB",6));
        h=openfile(path,0x1002);assert(h!=0xffff);closeh(h);
        assert(openfile(path,0x200)==0);
    }
    name("PB");arg(0,0);arg(2,ofs);arg(4,0);arg(6,0);arg(8,ns);arg(10,4);
    assert(!k_GetTempFileName(ss,0)); /* unmapped D: */
    assert(!files_directory("c:\\pictures",path,1));assert(!strcmp(path,"Pictures"));
    files_directory_label(path);assert(!strcmp(path,"C:\\Pictures"));
    assert(!files_getcwd(0,ofs,200));fcopy_in(path,ofs,200,9);assert(!strcmp(path,"Pictures"));
    assert(!files_getcwd(3,ofs,200));
    wr8(ofs,200,0x77);assert(files_getcwd(4,ofs,200)==15&&rd8(ofs,200)==0x77);
    assert(files_getcwd(3,0,0)==87);
    assert(files_getcwd(3,ofs,65530)==87);

    h=create("current.bmp");assert(h!=0xffff);closeh(h);
    assert(!access("Pictures/current.bmp",F_OK));
    h=openh("C:\\Mixed.BMP");assert(h!=0xffff);closeh(h);
    assert(openh("Mixed.BMP")==0xffff);
    assert(files_directory("C:\\linked",path,1)<0);
    files_directory_label(path);assert(!strcmp(path,"C:\\Pictures"));
    assert(!files_directory("..",path,1));assert(!strcmp(path,"."));
    wr8(ofs,200,0x77);assert(!files_getcwd(3,ofs,200)&&!rd8(ofs,200));

    h=openh("pictures/CURRENT.BMP");assert(h!=0xffff);closeh(h);
    assert(files_directory("D:\\",path,1)<0);
    assert(!files_directory("..",path,1));assert(!strcmp(path,"."));
    windir_tests();
    seg_free(ss);seg_free(ns);seg_free(ds);seg_free(ofs);assert(!host_live);
    puts("Session atoms across exec, refcounts/isolation/exhaustion, configured Windows directory and Win16 file roundtrip, OFSTRUCT, path isolation, case lookup, exhaustion and cleanup passed");
    return 0;
}
