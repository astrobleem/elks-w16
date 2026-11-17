#!/usr/bin/env python3
"""Exercise the real single-line Win16 edit dispatcher and Pascal API wrappers."""
import pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
u=(root/'src/user.c').read_text();h=(root/'src/wapi.h').read_text()
def func(name):
    m=re.search(r'^(?:static )?(?:struct wnd \*|u32 |void |int )'+name+r'\([^;]*?\)\n\{',u,re.M)
    assert m,name
    start=u.index('{',m.start());level=1;p=start+1
    while level:
        level+=(u[p]=='{')-(u[p]=='}');p+=1
    return u[m.start():p]+'\n'
pre='''#include "w16.h"
#include "filelist.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef unsigned GR_WINDOW_ID;
#define MWKMOD_SHIFT 1
'''
pre+='\n'.join(l for l in h.splitlines() if l.startswith(('#define WM_','#define MAXWIN','#define HWND_BASE','#define CTL_')))+'\n'
pre+=re.search(r'struct wnd \{.*?\n\};',h,re.S)[0]+'\n'
pre+='''struct wnd wins[MAXWIN];static unsigned paints,notifications;
u8 host_mem[0x110000];
u8 *host_ptr(u16 s,u16 o,u16 n){assert((u32)o+n<=65536UL);assert(LIN(s,o)+n<=0x100000UL);return host_mem+LIN(s,o);}
void fcopy_out(u16 s,u16 o,const void *p,u16 n){memcpy(host_ptr(s,o,n),p,n);}
u16 fstrncpy_in(char*d,u16 s,u16 o,u16 n){strncpy(d,(char*)host_ptr(s,o,n),n);d[n-1]=0;return strlen(d);}
void fcopy_in(void*p,u16 s,u16 o,u16 n){memcpy(p,host_ptr(s,o,n),n);}
void fzero(u16 s,u16 o,u16 n){memset(host_ptr(s,o,n),0,n);}
static void paint_control(struct wnd*w){(void)w;paints++;}
static u32 win_send(struct wnd*w,u16 m,u16 p,u32 l){assert(w==wins);assert(m==WM_COMMAND&&p==105&&l==0x10104UL);notifications++;return 0;}
'''
code=pre+''.join(func(n) for n in ('win_get','find_ctl','list_directory','list_message','u_DlgDirList','u_DlgDirSelect'))
code+=r'''int main(void){struct wnd*w=&wins[1];struct filelist l;char out[128];unsigned i;int f;
 memset(&l,0,sizeof(l));l.selected=-1;
 wins[0].used=1;wins[0].hwnd=HWND_BASE;
 w->used=1;w->ctl=CTL_LIST;w->hwnd=HWND_BASE+4;w->parent=HWND_BASE;w->id=105;w->list=&l;w->h=64;
 assert(!mkdir("Pictures",0700));f=creat("color.BMP",0600);assert(f>=0);close(f);
 f=creat("notes.txt",0600);assert(f>=0);close(f);
 assert(!symlink("color.BMP","link.bmp"));
 strcpy((char*)host_ptr(0x2000,0,124),"*.bmp");
 wr16(0x1000,0,0);wr16(0x1000,2,0);wr16(0x1000,4,105);wr16(0x1000,6,0);wr16(0x1000,8,0x2000);wr16(0x1000,10,HWND_BASE);
 assert(u_DlgDirList(0x1000,0)==1);assert(l.count==1&&!strcmp(l.text[0],"color.BMP"));
 assert(list_message(w,0x40c,0,0)==1);assert(list_message(w,0x409,0,0)==0xffff);
 assert(list_message(w,0x407,0,0)==0);assert(list_message(w,0x40a,0,0x20000100)==9);
 assert(!strcmp((char*)host_ptr(0x2000,256,10),"color.BMP"));
 wr16(0x1000,0,105);wr16(0x1000,2,256);wr16(0x1000,4,0x2000);wr16(0x1000,6,HWND_BASE);
 assert(!u_DlgDirSelect(0x1000,0));assert(!strcmp((char*)host_ptr(0x2000,256,10),"color.BMP"));
 filelist_clear(&l);strcpy(out,"*.*");assert(!list_directory(w,out,0xc010,1));
 assert(l.count==2&&!strcmp(l.text[0],"[-c-]")&&!strcmp(l.text[1],"[Pictures]"));
 l.selected=1;assert(u_DlgDirSelect(0x1000,0)==1);assert(!strcmp((char*)host_ptr(0x2000,256,10),"Pictures\\"));
 l.selected=0;assert(u_DlgDirSelect(0x1000,0)==1);assert(!strcmp((char*)host_ptr(0x2000,256,4),"C:\\"));
 filelist_clear(&l);strcpy(out,"Pictures\\*.bmp");assert(!list_directory(w,out,0,1));assert(!strcmp(out,"*.bmp"));
 files_directory_label(out);assert(!strcmp(out,"C:\\Pictures"));
 strcpy(out,"*.*");assert(!list_directory(w,out,0x8010,1));assert(l.count==1&&!strcmp(l.text[0],"[..]"));
 assert(!files_directory("C:\\",out,1));
 filelist_clear(&l);assert(filelist_add(&l,"a",1)==0);assert(filelist_add(&l,"b",1)==1);
 list_message(w,WM_KEYDOWN,0x28,0);assert(l.selected==0&&notifications==1);
 list_message(w,WM_KEYDOWN,0x28,0);assert(l.selected==1&&notifications==2);
 assert(list_message(w,0x40a,5,0x20000100)==0xffff);
 filelist_clear(&l);for(i=0;i<FILELIST_MAX;i++)assert(filelist_add(&l,"name",0)>=0);
 assert(filelist_add(&l,"overflow",0)==-2);filelist_clear(&l);assert(l.count==0&&l.selected==-1);
 puts("Actual directory enumeration, list selection, DOS path and dialog ABI checks passed");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(code)
    subprocess.run(['cc','-DW16_HOST','-std=gnu99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),str(root/'src/filelist.c'),str(root/'src/files.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],cwd=p,check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
