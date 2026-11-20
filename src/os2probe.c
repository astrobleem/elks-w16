/* Optional OS/2 real-mode console ABI experiment; no Windows dispatch. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "w16.h"

const char *const w16_modnames[NMODS] = {
    "DOSCALLS", "VIOCALLS", "~2", "~3", "~4", "~5", "~6"
};
const struct api api_table[] = {
    {1,19,"VioWrtTTY",8,0,0,0}, {0,5,"DosExit",4,0,0,0},
    {0,92,"DosGetVersion",4,0,0,0}, {0,49,"DosGetMachineMode",4,0,0,0}
};
const int api_count = sizeof(api_table)/sizeof(api_table[0]);
int opt_missing;
static struct ne_module image;
static int console_fd = -1;
#ifdef W16_HOST
extern int host_live;
#endif
u16 o16_done, o16_code;
int api_find_ord(int mod,u16 ord)
{
    int i; for(i=0;i<api_count;i++)
        if(api_table[i].mod==mod && api_table[i].ord==ord)return i;
    return -1;
}
int api_find_name(int mod,const char *name)
{
    int i;for(i=0;i<api_count;i++)
        if(api_table[i].mod==mod && !strcmp(api_table[i].name,name))return i;
    return -1;
}
const char *api_name(int i){return api_table[i].name;}
const char *api_known_name(int mod,u16 ord){(void)mod;(void)ord;return 0;}
#ifndef W16_HOST
#include <malloc.h>
int opt_verbose,opt_trace,opt_debugport;
u8 w16_scratch[W16_SCRATCH];
int seg_alloc(u16 paras,u16 *seg){return _fmemalloc(paras,seg)?-1:0;}
void seg_free(u16 seg){_fmemfree(seg);}
void fcopy_in(void *d,u16 s,u16 o,u16 n){u16 i;for(i=0;i<n;i++)((u8*)d)[i]=rd8(s,o+i);}
void fcopy_out(u16 s,u16 o,const void *d,u16 n){u16 i;for(i=0;i<n;i++)wr8(s,o+i,((const u8*)d)[i]);}
void fzero(u16 s,u16 o,u16 n){u16 i;for(i=0;i<n;i++)wr8(s,o+i,0);}
void wlog(const char *fmt,...){va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);}
void vlog(const char *fmt,...){va_list a;if(!opt_verbose)return;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);}
u16 W16_CDECL o16_run(u16 ip,u16 cs,u16 ss,u16 sp,u16 ds);
u16 W16_CDECL o16_cs(void);
void W16_CDECL o16_vio(void),o16_exit(void),o16_version(void),o16_mode(void);
#endif
int api_target(int i,u16 *seg,u16 *off)
{
    if(i<0 || i>=api_count)return -1;
#ifdef W16_HOST
    *seg=0xf000;*off=(u16)(16*i);
#else
    *seg=o16_cs();
    switch(i) {
    case 0:*off=(u16)(unsigned)o16_vio;break;
    case 1:*off=(u16)(unsigned)o16_exit;break;
    case 2:*off=(u16)(unsigned)o16_version;break;
    default:*off=(u16)(unsigned)o16_mode;break;
    }
#endif
    return 0;
}
/* Only the exact bases of image allocations are accepted. No arbitrary
 * selector, wrapped offset or paragraph alias can access the native task. */
static int span(u16 seg,u16 off,u16 len)
{
    int i;
    for(i=0;i<image.nseg;i++)
        if(image.seg[i].paras && image.seg[i].sel==seg)
            return (u32)off+len <= ((u32)image.seg[i].paras<<4);
    return 0;
}
/* Query outputs must be in a task-owned data allocation, including its stack.
 * An import operand in code is readable but is not a valid query output. */
static int output_span(u16 seg,u16 off,u16 len)
{
    int i;
    if(!span(seg,off,len))return 0;
    for(i=0;i<image.nseg;i++)
        if(image.seg[i].sel==seg)return !!(image.seg[i].flags&1);
    return 0;
}
/* Explicit compatibility policy, not the ELKS kernel's version. OS/2 1.x
 * encodes major 1 as 10 and minor .30 as 30: high byte major, low byte minor.
 * This does not promise any import beyond this file's strict table. */
#define O16_API_VERSION 0x0a1e
u16 W16_CDECL o16_dispatch(u16 idx,u16 ss,u16 ap)
{
    u16 n,off,seg,h; char buf[64];
    if(idx>=api_count || !span(ss,ap,api_table[idx].argbytes)) {
        o16_done=1;o16_code=W16_EUNSUPP;return 487;
    }
    if(idx==1){
        o16_done=1;
        o16_code=AW(2)==1 ? AW(0) : W16_EUNSUPP;
        return 0;
    }
    if(idx==2 || idx==3) {
        off=AW(0);seg=AW(2);n=idx==2?2:1;
        if(!output_span(seg,off,n))return 487;
        if(idx==2)wr16(seg,off,O16_API_VERSION);
        /* Our only executable mode is real mode, on both 8088 and later CPUs.
         * Never report OS/2 protected mode to unlock unsupported calls. */
        else wr8(seg,off,0);
        return 0;
    }
    h=AW(0);n=AW(2);off=AW(4);seg=AW(6);
    if(h)return 436; /* ERROR_VIO_INVALID_HANDLE */
    if(!n)return 0;
    if(!span(seg,off,n))return 487; /* ERROR_INVALID_ADDRESS */
    while(n){
        u16 k=n>sizeof(buf)?sizeof(buf):n;
        fcopy_in(buf,seg,off,k);
        if(write(console_fd,buf,k)!=(int)k)return 29;
        off+=k;n-=k;
    }
    return 0;
}
#ifndef O16_NO_MAIN
int main(int argc,char **argv)
{
    int rc,noexec=0;
    if(argc==3 && !strcmp(argv[1],"-n")){noexec=1;argv++;argc--;}
    if(argc!=2){fprintf(stderr,"usage: o16 [-n] OS2-TEXT.EXE\n");return 2;}
    rc=ne_open(&image,argv[1]);
    if(!rc)rc=ne_check_imports(&image);
    if(!rc)rc=ne_load(&image);
    if(!rc && !noexec){
#ifdef W16_HOST
        fprintf(stderr,"o16: host checks format/relocations only; cannot execute 16-bit code\n");
        rc=W16_EUNSUPP;
#else
        const char *tty=getenv("CONSOLE");
        console_fd=open(tty?tty:"/dev/tty1",O_WRONLY);
        if(console_fd<0 || !isatty(console_fd))rc=W16_EIO;
        else rc=o16_run(image.ip,image.seg[image.cs-1].sel,
                       image.seg[image.ss-1].sel,image.init_sp,image.dgroup);
#endif
    }
    if(console_fd>=0)close(console_fd);
    ne_free(&image);
    fprintf(stderr,"o16: result=%d (%s); owned image released\n",rc,ne_errstr(rc));
#ifdef W16_HOST
    fprintf(stderr,"o16: live allocations=%d\n",host_live);
#endif
    return rc;
}
#endif
