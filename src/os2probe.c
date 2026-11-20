/* Optional OS/2 real-mode console ABI experiment; no Windows dispatch. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include "w16.h"

const char *const w16_modnames[NMODS] = {
    "DOSCALLS", "VIOCALLS", "KBDCALLS", "~3", "MSG", "NLS", "~6"
};
const struct api api_table[] = {
    {1,19,"VioWrtTTY",8,0,0,0}, {0,5,"DosExit",4,0,0,0},
    {0,92,"DosGetVersion",4,0,0,0}, {0,49,"DosGetMachineMode",4,0,0,0},
    {0,34,"DosAllocSeg",8,0,0,0}, {0,39,"DosFreeSeg",2,0,0,0},
    {0,59,"DosClose",2,0,0,0}, {0,77,"DosQHandType",10,0,0,0},
    {0,137,"DosRead",12,0,0,0}, {0,138,"DosWrite",12,0,0,0},
    {5,4,"DosGetDBCSEv",10,0,0,0}, {0,38,"DosReallocSeg",4,0,0,0},
    {0,127,"DosMemAvail",4,0,0,0}, {0,89,"DosSetVec",10,0,0,0}, {4,1,"DosPutMessage",8,0,0,0}, {5,1,"DosCaseMap",10,0,0,0}
};
const int api_count = sizeof(api_table)/sizeof(api_table[0]);
int opt_missing;
static int deny_setvec;
static struct ne_module image;
static int console_fd = -1;
/* Guest standard handles are private duplicates. Closing guest stderr must
 * not close native diagnostics; VIO remains the selected physical console. */
static int streams[3]={-1,-1,-1};
struct owned_segment { struct owned_segment *next; u16 sel,paras; };
static struct owned_segment *owned;
u16 o16_pop;
static void streams_shutdown(void)
{
    int i;for(i=0;i<3;i++)if(streams[i]>=0){close(streams[i]);streams[i]=-1;}
}
static void streams_init(void)
{
    int i;streams_shutdown();for(i=0;i<3;i++)streams[i]=dup(i);
}
/* Optional fixed, process-owned memory domain (-a KiB). The sorted live
 * blocks define all free extents; no moving, probing or host free-total estimate.
 * Every guest segment, including image DS, comes from this same arena.
 */
struct arena_block { struct arena_block *next;u16 start,paras; };
static struct arena_block *arena_blocks;
static u16 arena_base,arena_paras;
#ifdef W16_HOST
int o16_raw_alloc(u16,u16 *);void o16_raw_free(u16);
#else
#include <malloc.h>
static int o16_raw_alloc(u16 paras,u16 *seg){return _fmemalloc(paras,seg)?-1:0;}
static void o16_raw_free(u16 seg){_fmemfree(seg);}
#endif
static int arena_init(u16 paras)
{
    if(!paras || paras>0x4000 || arena_base || arena_blocks)return -1;
    if(o16_raw_alloc(paras,&arena_base))return -1;
    if((u32)arena_base+paras>0x10000ul){o16_raw_free(arena_base);arena_base=0;return -1;}
    arena_paras=paras;return 0;
}
static void arena_shutdown(void)
{
    while(arena_blocks){struct arena_block *n=arena_blocks->next;free(arena_blocks);arena_blocks=n;}
    if(arena_base)o16_raw_free(arena_base);
    arena_base=arena_paras=0;
}
int seg_alloc(u16 paras,u16 *seg)
{
    struct arena_block **link=&arena_blocks,*p;u16 start=0;
    if(!arena_base)return o16_raw_alloc(paras,seg);
    if(!paras || paras>0x1000)return -1;
    while(*link){
        if((*link)->start-start>=paras)break;
        start=(*link)->start+(*link)->paras;link=&(*link)->next;
    }
    if((u32)start+paras>arena_paras)return -1;
    p=malloc(sizeof(*p));if(!p)return -1;
    p->start=start;p->paras=paras;p->next=*link;*link=p;
    *seg=arena_base+start;return 0;
}
void seg_free(u16 seg)
{
    struct arena_block **link=&arena_blocks,*p;
    if(!arena_base){o16_raw_free(seg);return;}
    while(*link && (u16)(arena_base+(*link)->start)!=seg)link=&(*link)->next;
    if(!*link)abort(); /* internal ownership invariant, never a guest free */
    p=*link;*link=p->next;free(p);
}
/* Host bounds checks admit only exact live segment bases and their extents;
 * free arena addresses and paragraph aliases never gain access to the backing. */
int o16_arena_alias(u16 seg,u16 off,u16 n,u16 *base,u32 *relative)
{
    struct arena_block *p;
    if(!arena_base || seg<arena_base || (u32)seg>=((u32)arena_base+arena_paras))return 0;
    for(p=arena_blocks;p;p=p->next)if(seg==(u16)(arena_base+p->start)){
        if((u32)off+n>((u32)p->paras<<4))return -1;
        *base=arena_base;*relative=((u32)p->start<<4)+off;return 1;
    }
    return -1;
}
static u32 arena_available(void)
{
    struct arena_block *p;u16 end=0,largest=0;
    for(p=arena_blocks;p;p=p->next){
        if(p->start-end>largest)largest=p->start-end;
        end=p->start+p->paras;
    }
    if(arena_paras-end>largest)largest=arena_paras-end;
    return (u32)largest<<4;
}
static int arena_resize(u16 seg,u16 paras)
{
    struct arena_block *p;u16 limit;
    for(p=arena_blocks;p;p=p->next)if(seg==(u16)(arena_base+p->start)){
        limit=p->next?p->next->start:arena_paras;
        if(!paras || paras>0x1000 || (u32)p->start+paras>limit)return -1;
        {
            u16 old=p->paras;p->paras=paras;
            if(paras>old)fzero(seg,(u16)(old<<4),(u16)((paras-old)<<4));
        }
        return 0;
    }
    return -1;
}
static void owned_shutdown(void)
{
    while(owned){struct owned_segment *n=owned->next;seg_free(owned->sel);free(owned);owned=n;}
}
#ifdef W16_HOST
extern int host_live;
#endif
u16 o16_done, o16_code;
static int api_enabled(int i)
{
    if((i==11 || i==12) && !arena_base)return 0;
    if(i==13 && !deny_setvec)return 0;
    return 1;
}
int api_find_ord(int mod,u16 ord)
{
    int i; for(i=0;i<api_count;i++)
        if(api_table[i].mod==mod && api_table[i].ord==ord)
            return api_enabled(i)?i:-1;
    return -1;
}
int api_find_name(int mod,const char *name)
{
    int i;for(i=0;i<api_count;i++)
        if(api_table[i].mod==mod && !strcmp(api_table[i].name,name))
            return api_enabled(i)?i:-1;
    return -1;
}
const char *api_name(int i){return api_table[i].name;}
const char *api_known_name(int mod,u16 ord)
{
    /* Verified Open Watcom lib286 exports; names aid diagnostics, never bind. */
    if(mod==0)switch(ord){
    case 38:return "DosReallocSeg";case 89:return "DosSetVec";
    case 127:return "DosMemAvail";}
    if(mod==1)switch(ord){case 3:return "VioGetAnsi";case 5:return "VioSetAnsi";}
    if(mod==2)switch(ord){case 4:return "KbdCharIn";case 10:return "KbdGetStatus";
    case 11:return "KbdSetStatus";case 13:return "KbdFlushBuffer";case 22:return "KbdPeek";}
    if(mod==4)switch(ord){case 1:return "DosPutMessage";case 2:return "DosTrueGetMessage";}
    if(mod==5)switch(ord){case 1:return "DosCaseMap";case 4:return "DosGetDBCSEv";}
    return 0;
}
#ifndef W16_HOST
#include <malloc.h>
int opt_verbose,opt_trace,opt_debugport;
u8 w16_scratch[W16_SCRATCH];
void fcopy_in(void *d,u16 s,u16 o,u16 n){u16 i;for(i=0;i<n;i++)((u8*)d)[i]=rd8(s,o+i);}
void fcopy_out(u16 s,u16 o,const void *d,u16 n){u16 i;for(i=0;i<n;i++)wr8(s,o+i,((const u8*)d)[i]);}
void fzero(u16 s,u16 o,u16 n){u16 i;for(i=0;i<n;i++)wr8(s,o+i,0);}
void wlog(const char *fmt,...){va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);}
void vlog(const char *fmt,...){va_list a;if(!opt_verbose)return;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);}
u16 W16_CDECL o16_run(u16 ip,u16 cs,u16 ss,u16 sp,u16 ds);
u16 W16_CDECL o16_cs(void);
void W16_CDECL o16_vio(void),o16_exit(void),o16_version(void),o16_mode(void);
void W16_CDECL o16_alloc(void),o16_free(void),o16_close(void),o16_type(void),o16_read(void),o16_write(void),o16_dbcs(void),o16_realloc(void),o16_memavail(void),o16_setvec(void),o16_putmessage(void),o16_casemap(void);
#endif
int api_target(int i,u16 *seg,u16 *off)
{
    if(i<0 || i>=api_count || !api_enabled(i))return -1;
#ifdef W16_HOST
    *seg=0xf000;*off=(u16)(16*i);
#else
    *seg=o16_cs();
    switch(i) {
    case 0:*off=(u16)(unsigned)o16_vio;break;
    case 1:*off=(u16)(unsigned)o16_exit;break;
    case 2:*off=(u16)(unsigned)o16_version;break;
    case 3:*off=(u16)(unsigned)o16_mode;break;
    case 4:*off=(u16)(unsigned)o16_alloc;break;
    case 5:*off=(u16)(unsigned)o16_free;break;
    case 6:*off=(u16)(unsigned)o16_close;break;
    case 7:*off=(u16)(unsigned)o16_type;break;
    case 8:*off=(u16)(unsigned)o16_read;break;
    case 9:*off=(u16)(unsigned)o16_write;break;
    case 10:*off=(u16)(unsigned)o16_dbcs;break;
    case 11:*off=(u16)(unsigned)o16_realloc;break;
    case 12:*off=(u16)(unsigned)o16_memavail;break;
    case 13:*off=(u16)(unsigned)o16_setvec;break;
    case 14:*off=(u16)(unsigned)o16_putmessage;break;
    default:*off=(u16)(unsigned)o16_casemap;break;
    }
#endif
    return 0;
}
/* Only the exact bases of image allocations are accepted. No arbitrary
 * selector, wrapped offset or paragraph alias can access the native task. */
static int span(u16 seg,u16 off,u16 len)
{
    int i;struct owned_segment *p;
    for(p=owned;p;p=p->next)
        if(p->sel==seg)return (u32)off+len<=((u32)p->paras<<4);
    for(i=0;i<image.nseg;i++)
        if(image.seg[i].paras && image.seg[i].sel==seg)
            return (u32)off+len <= ((u32)image.seg[i].paras<<4);
    return 0;
}
/* Query outputs must be in a task-owned data allocation, including its stack.
 * An import operand in code is readable but is not a valid query output. */
static int output_span(u16 seg,u16 off,u16 len)
{
    int i;struct owned_segment *p;
    if(!span(seg,off,len))return 0;
    for(p=owned;p;p=p->next)if(p->sel==seg)return 1;
    for(i=0;i<image.nseg;i++)
        if(image.seg[i].sel==seg)return !!(image.seg[i].flags&1);
    return 0;
}
static u16 io_error(int writing)
{
    switch(errno){case EBADF:return 6;case EACCES:return 5;
    case ENOSPC:return 39;case EPIPE:return 109;case EINTR:return 95;}
    return writing?29:30;
}
/* DOS reports a broken pipe as an API error, not native task termination.
 * No guest signal handler is installed; ignore only during this synchronous
 * write, then restore the caller's exact disposition and write errno. */
static int stream_write(int fd,const void *buf,u16 n)
{
    void (*previous)(int);int rc,error;
    previous=signal(SIGPIPE,SIG_IGN);
    if(previous==SIG_ERR)return -1;
    rc=write(fd,buf,n);error=errno;
    signal(SIGPIPE,previous);errno=error;return rc;
}
/* Read/write validate the complete far spans before touching the descriptor.
 * Disk reads may span scratch chunks; pipe/terminal reads return after one
 * syscall, rather than blocking again merely to fill a larger guest request. */
static u16 stream_io(int writing,u16 h,u16 seg,u16 off,u16 count,
                     u16 outseg,u16 outoff)
{
    u16 done=0,rc=0;int fd,r,mode;struct stat st;
    if(!output_span(outseg,outoff,2) ||
       (count && !(writing?span(seg,off,count):output_span(seg,off,count))))return 487;
    if(h>=3 || (fd=streams[h])<0)return 6;
    if(fstat(fd,&st)<0)return io_error(writing);
    mode=fcntl(fd,F_GETFL,0);if(mode<0)return io_error(writing);
    if((writing && (mode&O_ACCMODE)==O_RDONLY) ||
       (!writing && (mode&O_ACCMODE)==O_WRONLY))return 5;
    while(done<count){
        u16 left=count-done,k=left>1024u?1024u:left;
        if(writing)fcopy_in(w16_scratch,seg,(u16)(off+done),k);
        r=writing?stream_write(fd,w16_scratch,k):read(fd,w16_scratch,k);
        if(r<0){rc=io_error(writing);break;}
        if(!writing && r)fcopy_out(seg,(u16)(off+done),w16_scratch,(u16)r);
        done+=(u16)r;
        if(r!=(int)k || (!writing && !S_ISREG(st.st_mode)))break;
    }
    wr16(outseg,outoff,done);
    return rc;
}
static u16 handle_type(u16 h,u16 typeseg,u16 typeoff,u16 attrseg,u16 attroff)
{
    struct stat st;u16 type,attr=0;int fd,mode;
    if(!output_span(typeseg,typeoff,2) || !output_span(attrseg,attroff,2))return 487;
    if(h>=3 || (fd=streams[h])<0)return 6;
    if(fstat(fd,&st)<0)return io_error(0);
    if(S_ISREG(st.st_mode))type=0;
    else if(S_ISFIFO(st.st_mode))type=2;
    else if(S_ISCHR(st.st_mode) && isatty(fd)){
        type=1;attr=0x8000; /* character device, no unimplemented IOCTL flags */
        mode=fcntl(fd,F_GETFL,0);
        if(mode<0)return io_error(0);
        /* Console input/output device bits, from the descriptor's real access. */
        if((mode&O_ACCMODE)!=O_WRONLY)attr|=1;
        if((mode&O_ACCMODE)!=O_RDONLY)attr|=2;
    }else return 50; /* unknown device attributes: ERROR_NOT_SUPPORTED */
    wr16(typeseg,typeoff,type);wr16(attrseg,attroff,attr);return 0;
}
static u16 alloc_segment(u16 size,u16 seg,u16 off,u16 flags)
{
    struct owned_segment *p;u16 paras;
    if(!output_span(seg,off,2))return 487;
    if(flags)return 50; /* sharing/discarding is unsupported; never silently downgrade */
    p=malloc(sizeof(*p));if(!p)return 8;
    paras=size?(u16)(((u32)size+15)>>4):0x1000;
    if(seg_alloc(paras,&p->sel)){free(p);return 8;}
    p->paras=paras;p->next=owned;owned=p;
    if(paras==0x1000){fzero(p->sel,0,0x8000);fzero(p->sel,0x8000,0x8000);}
    else fzero(p->sel,0,(u16)(paras<<4));
    wr16(seg,off,p->sel);return 0;
}
static u16 free_segment(u16 seg)
{
    struct owned_segment **p=&owned,*old;
    while(*p && (*p)->sel!=seg)p=&(*p)->next;
    if(!*p)return 5; /* never free an image, native, or another task's segment */
    old=*p;seg_free(seg);*p=old->next;free(old);return 0;
}
/* The narrow NLS profile is explicitly US SBCS: default 437, explicit
 * 437 or 850. Both have no DBCS lead bytes. Unsupported countries/codepages
 * fail without mutating either the input COUNTRYCODE or output buffer. */
static u16 dbcs_vector(u16 cb,u16 cseg,u16 coff,u16 seg,u16 off)
{
    u16 country,cp;
    if(!span(cseg,coff,4) || !output_span(seg,off,cb))return 487;
    country=rd16(cseg,coff);cp=rd16(cseg,coff+2);
    if(country && country!=1)return 398;
    if(cp && cp!=437 && cp!=850)return 398;
    fzero(seg,off,cb);
    return cb<2?399:0; /* ERROR_NLS_TABLE_TRUNCATED, even a one-byte truncation */
}
static u16 realloc_segment(u16 size,u16 seg,u16 ss,u16 ap)
{
    struct owned_segment *p;int i;u16 paras;
    if(!arena_base)return 50;
    paras=size?(u16)(((u32)size+15)>>4):0x1000;
    for(p=owned;p;p=p->next)if(p->sel==seg){
        if(ss==seg && paras<p->paras)return 5; /* cannot discard a live stack's upper frames */
        if(arena_resize(seg,paras))return 8;
        p->paras=paras;return 0;
    }
    /* CRT startup may resize its automatically loaded DS. Code segments and
     * other image data cannot be resized through this API. Protect the initial
     * stack and the complete live argument frame when SS shares that DS. */
    i=image.autodata-1;
    if(i<0 || i>=image.nseg || image.seg[i].sel!=seg)return 5;
    if(ss==seg && paras<image.seg[i].paras)return 5;
    if(((u32)paras<<4)<image.init_sp ||
       ((u32)paras<<4)<image.seg[i].minalloc ||
       (ss==seg && ((u32)paras<<4)<(u32)ap+4))return 8;
    if(arena_resize(seg,paras))return 8;
    image.seg[i].paras=paras;return 0;
}
/* Exact ASCII subdomain of the explicit US 437/850 NLS profile. This is
 * not a guessed extended-byte table: reject the complete operation before
 * mutation if a byte needs COUNTRY.SYS mapping which we do not yet provide.
 * Counted strings include embedded NULs; no locale-dependent libc toupper.
 */
static u16 case_map(u16 count,u16 cseg,u16 coff,u16 seg,u16 off)
{
    u16 country,cp,i;
    if(!span(cseg,coff,4) || !output_span(seg,off,count))return 487;
    country=rd16(cseg,coff);cp=rd16(cseg,coff+2);
    if((country && country!=1) || (cp && cp!=437 && cp!=850))return 398;
    for(i=0;i<count;i++)if(rd8(seg,(u16)(off+i))>=128)return 401;
    for(i=0;i<count;i++){
        u8 c=rd8(seg,(u16)(off+i));
        if(c>='a' && c<='z')wr8(seg,(u16)(off+i),c-'a'+'A');
    }
    return 0;
}
/* Explicit denial binding, NOT exception-handler support. OS/2 documents
 * ERROR_INVALID_FUNCTION (1) for DosSetVec failure. Validate the supplied
 * memory before returning that non-success; leave the previous-handler output
 * untouched because no registration/removal took place. Never access the IVT,
 * install a native signal handler, or retain a guest callback here.
 */
static u16 reject_setvec(u16 vector,u16 hseg,u16 hoff,u16 seg,u16 off)
{
    int i,valid=!(hseg|hoff);
    if(!output_span(seg,off,4))return 487;
    for(i=0;i<image.nseg && !valid;i++)
        if(image.seg[i].sel==hseg && !(image.seg[i].flags&1) &&
           image.seg[i].fileoff && hoff<image.seg[i].size)valid=1;
    if(!valid)return 487;
    wlog("o16: DosSetVec vector %u denied (ERROR_INVALID_FUNCTION); no handler registered\n",vector);
    return 1;
}
/* SBCS DosPutMessage: 80-column word wrapping, starting in column one.
 * A dry pass rejects unsupported control bytes and overlong words/indentation
 * before any output. Tabs and DBCS interpretation require a richer text backend;
 * report ERROR_MR_UN_PERFORM rather than silently rewriting those forms.
 */
static u16 message_line(int fd,const char *line,u16 n,int output)
{
    u16 done=0;
    if(!output)return 0;
    while(done<n){
        int r=stream_write(fd,line+done,n-done);
        if(r<0)return io_error(1);
        if(!r)return 321;
        done+=(u16)r;
    }
    return 0;
}
static u16 message_pass(int fd,u16 seg,u16 off,u16 count,int output)
{
    char line[80];u16 used=0,spaces=0;u32 pos=0;
    while(pos<count){
        u8 c=rd8(seg,(u16)(off+pos));u16 rc;
        if(c==' '){if(++spaces>80)return 321;pos++;continue;}
        if(c=='\r' || c=='\n'){
            char nl[2];u16 len=1;
            if(used+spaces>80)return 321;
            memset(line+used,' ',spaces);used+=spaces;spaces=0;
            if((rc=message_line(fd,line,used,output)))return rc;
            nl[0]=c;pos++;
            if(c=='\r' && pos<count && rd8(seg,(u16)(off+pos))=='\n'){nl[1]='\n';len=2;pos++;}
            if((rc=message_line(fd,nl,len,output)))return rc;
            used=0;continue;
        }
        {
            u32 start=pos;u16 len;
            while(pos<count){
                c=rd8(seg,(u16)(off+pos));
                if(c==' ' || c=='\r' || c=='\n')break;
                if(c<32 || c==127)return 321;
                if(pos-start==80)return 321;
                pos++;
            }
            len=(u16)(pos-start);
            if(used+spaces+len>80){
                if(!used)return 321;
                if((rc=message_line(fd,line,used,output)))return rc;
                if((rc=message_line(fd,"\r\n",2,output)))return rc;
                used=spaces=0;
            }
            memset(line+used,' ',spaces);used+=spaces;spaces=0;
            fcopy_in(line+used,seg,(u16)(off+start),len);used+=len;
        }
    }
    if(used+spaces>80)return 321;
    memset(line+used,' ',spaces);used+=spaces;
    return message_line(fd,line,used,output);
}
static u16 put_message(u16 h,u16 count,u16 seg,u16 off)
{
    int fd,mode;u16 rc;
    if(count && !span(seg,off,count))return 487;
    if(h>=3 || (fd=streams[h])<0)return 6;
    mode=fcntl(fd,F_GETFL,0);
    if(mode<0)return io_error(1);
    if((mode&O_ACCMODE)==O_RDONLY)return 5;
    rc=message_pass(fd,seg,off,count,0);
    return rc?rc:message_pass(fd,seg,off,count,1);
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
    o16_pop=api_table[idx].argbytes;
    if(idx==15)return case_map(AW(8),AW(6),AW(4),AW(2),AW(0));
    if(idx==14)return put_message(AW(6),AW(4),AW(2),AW(0));
    if(idx==13){
        if(!deny_setvec)return 1;
        return reject_setvec(AW(8),AW(6),AW(4),AW(2),AW(0));
    }
    if(idx==10)return dbcs_vector(AW(8),AW(6),AW(4),AW(2),AW(0));
    if(idx==11)return realloc_segment(AW(2),AW(0),ss,ap);
    if(idx==12){
        if(!arena_base)return 50;
        if(!output_span(AW(2),AW(0),4))return 487;
        wr32(AW(2),AW(0),arena_available());return 0;
    }
    if(idx==4)return alloc_segment(AW(6),AW(4),AW(2),AW(0));
    if(idx==5)return free_segment(AW(0));
    if(idx==6){
        int fd;h=AW(0);if(h>=3 || (fd=streams[h])<0)return 6;
        streams[h]=-1;return close(fd)<0?io_error(1):0;
    }
    if(idx==7)return handle_type(AW(8),AW(6),AW(4),AW(2),AW(0));
    if(idx==8 || idx==9)return stream_io(idx==9,AW(10),AW(8),AW(6),AW(4),AW(2),AW(0));
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
    int rc,noexec=0,ran=0;u16 reserve=0;
    while(argc>1 && argv[1][0]=='-'){
        if(!strcmp(argv[1],"-n")){noexec=1;argv++;argc--;}
        else if(!strcmp(argv[1],"--deny-setvec")){deny_setvec=1;argv++;argc--;}
        else if(!strcmp(argv[1],"-a") && argc>2){
            char *end;unsigned long kb=strtoul(argv[2],&end,10);
            if(!argv[2][0] || *end || kb<1 || kb>256)return 2;
            reserve=(u16)(kb*64);argv+=2;argc-=2;
        }else break;
    }
    if(argc!=2){fprintf(stderr,"usage: o16 [-n] [-a arena-KiB (1..256)] [--deny-setvec] OS2-TEXT.EXE\n");return 2;}
    if(reserve && arena_init(reserve))return W16_ENOMEM;
    if(arena_base)fprintf(stderr,"o16: private arena=%u KiB; memory queries cover this domain only\n",reserve/64);
    if(deny_setvec)fprintf(stderr,"o16: DosSetVec binding returns failure only; exception delivery is unsupported\n");
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
        else {
            streams_init();ran=1;
            rc=o16_run(image.ip,image.seg[image.cs-1].sel,
                       image.seg[image.ss-1].sel,image.init_sp,image.dgroup);
        }
#endif
    }
    streams_shutdown();owned_shutdown();
    if(console_fd>=0)close(console_fd);
    ne_free(&image);arena_shutdown();
    if(ran)fprintf(stderr,"o16: guest exit=%d; owned image released\n",rc);
    else fprintf(stderr,"o16: result=%d (%s); owned image released\n",rc,ne_errstr(rc));
#ifdef W16_HOST
    fprintf(stderr,"o16: live allocations=%d\n",host_live);
#endif
    return rc;
}
#endif
