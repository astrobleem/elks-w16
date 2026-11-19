/* Win16 file APIs. Guest handles are never ELKS descriptors. DOS C: is rooted
 * at the task's working directory; paths cannot escape through .. or links. */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "w16.h"

#define FILES 16
#define FILE_BASE 0x4000
#define PATHLEN 124
#define OFSTRUCT_SIZE 136
static struct { int fd; u8 used,dirty; } files[FILES];
static char dos_cwd[PATHLEN];

static int far_range(u16 seg,u16 off,u16 n)
{
    return (seg||off) && (u32)off+n<=0x10000UL &&
           ((u32)seg<<4)+off+n<=0x100000UL;
}
static int read_name(u16 seg,u16 off,char *out)
{
    u16 i;
    for(i=0;i<PATHLEN;i++) {
        if(!far_range(seg,off,i+1)) break;
        out[i]=rd8(seg,off+i);
        if(!out[i]) return 0;
    }
    errno=EINVAL; return -1;
}
static int fold_equal(const char *a,const char *b)
{
    for(;*a&&*b;a++,b++) {
        unsigned char x=*a,y=*b;
        if(x>='a'&&x<='z') x-=32;
        if(y>='a'&&y<='z') y-=32;
        if(x!=y) return 0;
    }
    return *a==*b;
}
/* Resolve every existing component without following symbolic links. Exact
 * spelling wins; ambiguous case-fold matches fail rather than choose a file. */
static int local_path(const char *input,char *out,int allow_new)
{
    char component[PATHLEN],match[PATHLEN],parent[PATHLEN],joined[PATHLEN];
    const char *p=input;
    unsigned used=0;
    int drive=0;
    out[0]=0;
    if(p[0] && p[1]==':') {
        if(p[0]!='c'&&p[0]!='C') { errno=ENODEV; return -1; }
        p+=2;drive=1;
    }
    if(*p=='/'&&!drive) { errno=EACCES; return -1; }
    if(*p!='/'&&*p!='\\' && dos_cwd[0]) {
        if(strlen(dos_cwd)+strlen(p)+2>PATHLEN) { errno=ENAMETOOLONG;return -1; }
        strcpy(joined,dos_cwd);strcat(joined,"/");strcat(joined,p);p=joined;
    }
    while(*p=='/'||*p=='\\') p++;
    while(*p) {
        unsigned n=0;
        DIR *dir; struct dirent *entry; struct stat st;
        int matches=0,exact=0,last;
        while(*p && *p!='/' && *p!='\\') {
            unsigned char c=*p++;
            if(c<32 || c==':' || c=='*' || c=='?' || n+1>=sizeof(component)) {
                errno=EINVAL; return -1;
            }
            component[n++]=c;
        }
        component[n]=0;
        while(*p=='/'||*p=='\\') p++;
        last=!*p;
        if(!strcmp(component,"..")) { errno=EACCES; return -1; }
        if(!strcmp(component,".")) continue;
        if(!n || component[n-1]==' ' || component[n-1]=='.') { errno=EINVAL; return -1; }
        strcpy(parent,used?out:".");
        dir=opendir(parent);
        if(!dir) return -1;
        while((entry=readdir(dir))!=0) {
            if(!fold_equal(entry->d_name,component)) continue;
            if(!strcmp(entry->d_name,component)) {
                strcpy(match,component);exact=1;break;
            }
            if(strlen(entry->d_name)>=sizeof(match)) { matches=2; continue; }
            strcpy(match,entry->d_name);matches++;
        }
        closedir(dir);
        if(!exact && matches>1) { errno=EEXIST; return -1; }
        if(!exact && !matches) {
            if(!last || allow_new!=1) { errno=ENOENT; return -1; }
            strcpy(match,component);
        }
        n=strlen(match);
        if(used+n+(used?1:0)>=PATHLEN) { errno=ENAMETOOLONG; return -1; }
        if(used) out[used++]='/';
        memcpy(out+used,match,n+1);used+=n;
        if(lstat(out,&st)==0) {
            if(S_ISLNK(st.st_mode) || (last&&allow_new!=2?!S_ISREG(st.st_mode):!S_ISDIR(st.st_mode))) {
                errno=EACCES; return -1;
            }
        } else if(errno!=ENOENT || !last || allow_new!=1) return -1;
    }
    if(!used) { if(allow_new==2){strcpy(out,".");return 0;} errno=EINVAL; return -1; }
    return 0;
}
/* Shared dialog directory state. Never change the host process directory.
 * A leading C:\ path is rooted at launch; all subsequent relative file APIs
 * use this verified logical directory. The caller owns PATHLEN-byte buffers. */
int files_directory(const char *input,char *out,int change)
{
    char absolute[PATHLEN],next[PATHLEN],component[PATHLEN];
    const char *p=input;unsigned used=0,n;
    if(p[0]&&p[1]==':'){if(p[0]!='c'&&p[0]!='C')return -1;p+=2;}
    if(*p!='/'&&*p!='\\'){strcpy(next,dos_cwd);used=strlen(next);}else next[0]=0;
    while(*p){
        while(*p=='/'||*p=='\\')p++;
        n=0;while(*p&&*p!='/'&&*p!='\\'){
            if(n+1>=PATHLEN)return -1;
            component[n++]=*p++;
        }
        component[n]=0;if(!n||!strcmp(component,"."))continue;
        if(!strcmp(component,"..")){
            while(used&&next[used-1]!='/')used--;
            if(used)used--;
            next[used]=0;continue;
        }
        if(used+n+2>=PATHLEN)return -1;
        if(used)next[used++]='/';
        memcpy(next+used,component,n+1);used+=n;
    }
    if(used+4>PATHLEN)return -1;
    strcpy(absolute,"C:\\");strcat(absolute,next);
    if(local_path(absolute,out,2))return -1;
    if(change)strcpy(dos_cwd,!strcmp(out,".")?"":out);
    return 0;
}
void files_directory_label(char *out)
{
    unsigned i;strcpy(out,"C:\\");strcat(out,dos_cwd);
    for(i=3;out[i];i++)if(out[i]=='/')out[i]='\\';
}

/* DOS AH=47: a 64-byte ASCIIZ relative directory, without drive/root prefix. */
u16 files_getcwd(u16 drive,u16 seg,u16 off)
{
    char path[64];unsigned i,n=strlen(dos_cwd);
    if(drive!=0 && drive!=3)return 15; /* invalid drive */
    if(n>=sizeof(path))return 3; /* cannot represent this path in DOS buffer */
    if(!far_range(seg,off,n+1))return 87;
    for(i=0;i<n;i++)path[i]=dos_cwd[i]=='/'?'\\':dos_cwd[i];
    path[n]=0;fcopy_out(seg,off,path,n+1);return 0;
}

static int file_index(u16 handle)
{
    unsigned i=(unsigned)(handle-FILE_BASE);
    return i<FILES && files[i].used?(int)i:-1;
}
static int new_handle(const char *path,int flags)
{
    int i,fd; struct stat st;
    for(i=0;i<FILES && files[i].used;i++);
    if(i==FILES) { errno=EMFILE; return -1; }
    fd=open(path,flags,0600);
    if(fd<0) return -1;
    if(fstat(fd,&st)<0 || !S_ISREG(st.st_mode)) {
        close(fd);errno=EACCES;return -1;
    }
    files[i].fd=fd;files[i].used=1;files[i].dirty=(flags&(O_CREAT|O_TRUNC))!=0;
    return FILE_BASE+i;
}
static int access_mode(u16 mode)
{
    if((mode&3)==3 || (mode&~0x0073u) || (mode&0x70)>0x40) {
        errno=EINVAL;return -1;
    }
    return (mode&3)==0?O_RDONLY:(mode&3)==1?O_WRONLY:O_RDWR;
}
void files_shutdown(void)
{
    int i,dirty=0;
    for(i=0;i<FILES;i++) if(files[i].used) {
        dirty|=files[i].dirty;close(files[i].fd);files[i].used=0;
    }
    if(dirty)sync();
}
u32 k_lclose(u16 ss,u16 ap)
{
    int i=file_index(AW(0)),e;
    if(i<0) return 0xffff;
    e=close(files[i].fd);files[i].used=0;
    if(!e && files[i].dirty)sync();
    files[i].dirty=0;
    return e<0?0xffff:0;
}
u32 k_lopen(u16 ss,u16 ap)
{
    char name[PATHLEN],path[PATHLEN];
    int mode=access_mode(AW(0)),h;
    if(mode<0 || read_name(AW(4),AW(2),name) || local_path(name,path,0)) return 0xffff;
    h=new_handle(path,mode);return h<0?0xffff:(u16)h;
}
u32 k_lcreat(u16 ss,u16 ap)
{
    char name[PATHLEN],path[PATHLEN];int h;
    if(AW(0) || read_name(AW(4),AW(2),name) || local_path(name,path,1)) return 0xffff;
    h=new_handle(path,O_RDWR|O_CREAT|O_TRUNC);return h<0?0xffff:(u16)h;
}
static u32 transfer(u16 ss,u16 ap,int writing)
{
    unsigned char buf[256];
    u16 count=AW(0),off=AW(2),seg=AW(4),done=0;
    int i=file_index(AW(6));
    if(i<0 || (count && !far_range(seg,off,count))) return 0xffff;
    if(!count && writing) {
        struct stat st;long pos=lseek(files[i].fd,0L,SEEK_CUR);
        /* No libc ftruncate on ELKS. A zero-byte DOS write is a truncate
         * operation, not a no-op. Refuse unless no size change is needed. */
        return pos>=0 && !fstat(files[i].fd,&st) && pos==st.st_size?0:0xffff;
    }
    while(done<count) {
        u16 n=(u16)(count-done)>256u?256u:(u16)(count-done);
        int got;
        if(writing) fcopy_in(buf,seg,off+done,n);
        got=writing?write(files[i].fd,buf,n):read(files[i].fd,buf,n);
        if(got<0) { if(errno==EINTR) continue; return done?done:0xffff; }
        if(!got) break;
        if(writing)files[i].dirty=1;
        if(!writing) fcopy_out(seg,off+done,buf,got);
        done+=got;
        if(got<n) break;
    }
    return done;
}
u32 k_lread(u16 ss,u16 ap) { return transfer(ss,ap,0); }
u32 k_lwrite(u16 ss,u16 ap) { return transfer(ss,ap,1); }
u32 k_llseek(u16 ss,u16 ap)
{
    int i=file_index(AW(6));long result;
    if(i<0 || AW(0)>2) return 0xffffffffUL;
    result=lseek(files[i].fd,(long)(i32)AL(2),AW(0));
    return result<0 || (unsigned long)result>0x7fffffffUL?0xffffffffUL:(u32)result;
}
static u16 dos_error(void)
{
    switch(errno) {
    case ENOENT:return 2;
    case EMFILE:return 4;
    case EACCES:case EPERM:return 5;
    case EBADF:return 6;
    case ENOMEM:return 8;
    case ENODEV:return 15;
    case EEXIST:return 80;
    default:return 87;
    }
}
/* Shared by OpenFile(OF_DELETE) and the DLL's DOS3Call AH=41 helper. */
u16 files_unlink(u16 seg,u16 off)
{
    char name[PATHLEN],path[PATHLEN];
    if(read_name(seg,off,name) || local_path(name,path,0) || unlink(path)<0)
        return dos_error();
    return 0;
}

/* OpenFile(name,OFSTRUCT*,style); OFSTRUCT is 136 Win16 bytes. */
u32 k_OpenFile(u16 ss,u16 ap)
{
    u16 style=AW(0),off=AW(2),seg=AW(4),err=0;
    char name[PATHLEN],path[PATHLEN];
    int h=-1,mode;
    unsigned j;
    if(!far_range(seg,off,OFSTRUCT_SIZE)) return 0xffff;
    if(style&0x8000) {
        if(read_name(seg,off+8,name)) goto failed;
    } else if(read_name(AW(8),AW(6),name)) goto failed;
    if((style&0x048c) || ((style&0x70)>0x40) ||
       ((style&0x1000)&&(style&(0x200|0x4000)))) { errno=EINVAL;goto failed; }
    if(local_path(name,path,(style&(0x1000|0x100))!=0)) goto failed;
    if(style&0x100) { h=0;goto result; } /* OF_PARSE */
    if(style&0x200) { h=unlink(path)<0?-1:0; if(h<0)goto failed;goto result; }
    mode=access_mode(style&0x73);
    if(mode<0)goto failed;
    h=new_handle(path,style&0x1000?O_RDWR|O_CREAT|O_TRUNC:mode);
    if(h<0)goto failed;
    if(style&0x4000) { int k=file_index(h);close(files[k].fd);files[k].used=0; }
    goto result;
failed:
    err=dos_error();path[0]=0;
result:
    fzero(seg,off,OFSTRUCT_SIZE);wr8(seg,off,OFSTRUCT_SIZE);wr8(seg,off+1,1);
    wr16(seg,off+2,err);
    if(path[0]) {
        wr8(seg,off+8,'C');wr8(seg,off+9,':');wr8(seg,off+10,'\\');
        for(j=0;path[j];j++) wr8(seg,off+11+j,path[j]=='/'?'\\':path[j]);
    }
    return h<0?0xffff:(u16)h;
}

/* Win16 numbers drives from A:=0. The confined virtual C: volume is
 * local/fixed; no other DOS drive is mapped (Win16 unknown-drive result 0). */
u32 k_GetDriveType(u16 ss,u16 ap)
{ return ap<=0xfffeu && AW(0)==2 ? 3 : 0; }

/* Win16 returns AL=drive letter, AH=':', DX=1. The only mapped drive is C. */
u32 k_GetTempDrive(u16 ss,u16 ap)
{
    (void)ss;(void)ap;
    return 0x00013a43UL;
}
/* GetTempFileName(drive,prefix,unique,out): unique zero reserves the file,
 * nonzero only formats its name. Numeric DOS drive 3 names C:, including
 * the high-bit forced-drive form used by Win16. All files stay in mapped C:. */
u32 k_GetTempFileName(u16 ss,u16 ap)
{
    static u16 sequence=1;
    const char hex[]="0123456789ABCDEF";
    char prefix[PATHLEN],name[16],path[PATHLEN];
    u16 off=AW(0),seg=AW(2),unique=AW(4),drive=AW(10)&0x7f;
    unsigned tries,n;
    if((drive && drive!=3 && drive!='C' && drive!='c') || !far_range(seg,off,15)) return 0;
    if(AW(6)||AW(8)) { if(read_name(AW(8),AW(6),prefix)) return 0; }
    else strcpy(prefix,"TM");
    strcpy(name,"C:\\~TM0000.TMP");
    for(n=0;n<2 && prefix[n];n++) {
        unsigned char c=prefix[n];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')) return 0;
        name[4+n]=c>='a'&&c<='z'?c-32:c;
    }
    for(tries=0;tries<256;tries++) {
        u16 number=unique?unique:sequence++;
        int fd;
        if(!sequence) sequence=1;
        if(!number) number=1;
        for(n=0;n<4;n++) name[6+n]=hex[(number>>(12-4*n))&15];
        if(unique) { fcopy_out(seg,off,name,15); return number; }
        if(local_path(name,path,1)) { if(errno==EEXIST) continue; return 0; }
        fd=open(path,O_RDWR|O_CREAT|O_EXCL,0600);
        if(fd>=0) {
            close(fd);fcopy_out(seg,off,name,15);return number;
        }
        if(errno!=EEXIST) return 0;
    }
    return 0;
}
