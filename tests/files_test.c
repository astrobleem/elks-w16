#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "w16.h"
u32 k_lcreat(u16,u16),k_lopen(u16,u16),k_lclose(u16,u16),k_lread(u16,u16),k_lwrite(u16,u16),k_llseek(u16,u16),k_OpenFile(u16,u16);
u32 k_GetTempDrive(u16,u16),k_GetTempFileName(u16,u16);
extern int host_live;
static u16 ss,ns,ds,ofs;
static void arg(int n,u16 v){wr16(ss,n,v);}
static void name(const char *s){fcopy_out(ns,0,s,strlen(s)+1);}
static u16 create(const char *s){name(s);arg(0,0);arg(2,0);arg(4,ns);return k_lcreat(ss,0);}
static void closeh(u16 h){arg(0,h);assert(!k_lclose(ss,0));}
static u16 openh(const char *s){name(s);arg(0,0);arg(2,0);arg(4,ns);return k_lopen(ss,0);}
static u32 seekh(u16 h,i32 pos,u16 whence){arg(0,whence);wr32(ss,2,pos);arg(6,h);return k_llseek(ss,0);}
static u32 transfer(u16 h,u16 count,int writeit){arg(0,count);arg(2,0);arg(4,ds);arg(6,h);return writeit?k_lwrite(ss,0):k_lread(ss,0);}
static u16 openfile(const char *s,u16 flags){name(s);arg(0,flags);arg(2,0);arg(4,ofs);arg(6,0);arg(8,ns);return k_OpenFile(ss,0);}
int main(void)
{
    u16 h,i,slots[16];char path[128];
    assert(!seg_alloc(8,&ss));assert(!seg_alloc(16,&ns));assert(!seg_alloc(256,&ds));assert(!seg_alloc(16,&ofs));
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
    assert(!symlink("Mixed.BMP","link.bmp"));assert(openh("link.bmp")==0xffff);
    assert(create("link.bmp")==0xffff);
    assert(!symlink("Pictures","linked"));assert(openh("linked/one.bmp")==0xffff);
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
    assert(k_GetTempDrive(ss,0)==0x00013a43UL);
    name("PB");arg(0,0);arg(2,ofs);arg(4,0);arg(6,0);arg(8,ns);arg(10,0);
    i=k_GetTempFileName(ss,0);assert(i);
    fcopy_in(path,ofs,0,15);h=openh(path);assert(h!=0xffff);closeh(h);
    assert(openfile(path,0x200)==0);
    name("PB");arg(0,0);arg(2,ofs);arg(4,0x1234);arg(6,0);arg(8,ns);arg(10,'C');
    assert(k_GetTempFileName(ss,0)==0x1234);
    fcopy_in(path,ofs,0,15);assert(openh(path)==0xffff);
    seg_free(ss);seg_free(ns);seg_free(ds);seg_free(ofs);assert(!host_live);
    puts("Win16 file roundtrip, OFSTRUCT, path isolation, case lookup, exhaustion and cleanup passed");
    return 0;
}
