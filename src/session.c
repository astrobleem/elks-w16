/* Session-shared atoms. Disk records avoid a resident atom broker or a large
 * DGROUP table. W16SESSION joins an existing session; absent it, the first
 * use creates a private session. Separate roots never share names.
 * A failed/stale lock fails closed; no live lock is stolen. */
#include "w16.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#define SESSION_TASKS 16
#define SESSION_ATOMS 64
#define SESSION_MAGIC 0x57313641UL
#define SESSION_CLOSING (SESSION_MAGIC|0x80000000UL)
struct session { char dir[80]; u32 pid; u16 slot; int livefd; };
struct atom { u16 refs,len; char name[256]; };
static struct session *session;

static void path(char *out,const char *name)
{ strcpy(out,session->dir);strcat(out,"/");strcat(out,name); }
static void atompath(char *out,u16 slot)
{ char name[4];name[0]='a';name[1]='0'+slot/10;name[2]='0'+slot%10;name[3]=0;path(out,name); }
static void leasepath(char *out,u16 slot)
{atompath(out,slot);out[strlen(session->dir)+1]='t';}
/* ELKS rejects kill(pid,0). A nonblocking FIFO reader is a kernel-owned
 * lifetime lease instead: writer-open fails with ENXIO after task death.
 * It costs one descriptor and the ELKS 80-byte pipe buffer per participant.
 * CLOEXEC prevents an independently executed task retaining its parent's
 * lease. Unknown I/O errors never count as proof a participant is dead. */
static int member_alive(u16 slot)
{
 char p[96];int fd;leasepath(p,slot);fd=open(p,O_WRONLY|O_NONBLOCK);
 if(fd>=0){close(fd);return 1;}
 if(errno==ENXIO||errno==ENOENT){unlink(p);return 0;}
 return -1;
}
static int putfile(const char *p,const void *buf,unsigned n)
{
 int fd=open(p,O_WRONLY|O_CREAT|O_EXCL,0600),ok;
 if(fd<0)return -1;
 ok=write(fd,buf,n)==n; if(close(fd))ok=0;
 if(!ok)unlink(p);
 return ok?0:-1;
}
/* link() is atomic on ELKS; its fcntl does not implement record locks. The
 * per-task link is created before acquisition, so two tasks cannot own it. */
static int lock(void)
{
 char own[96],common[96],name[12];int fd,i;
 sprintf(name,"p%lu",(unsigned long)session->pid);path(own,name);path(common,"lock");
 fd=open(own,O_WRONLY|O_CREAT|O_EXCL,0600);
 if(fd<0)return -1;
 close(fd);
 for(i=0;i<100;i++) {
  if(!link(own,common)) {
   char p[96];int pending=0;
   unlink(own);
   path(p,"next");if(!access(p,F_OK)||errno!=ENOENT)pending=1;
   path(p,"prior");if(!access(p,F_OK)||errno!=ENOENT)pending=1;
   if(!pending)return 0;
   unlink(common);return -1;
  }
  if(errno!=EEXIST)break;
  usleep(10000);
 }
 unlink(own);return -1;
}
static void unlock(void)
{char p[96];path(p,"lock");unlink(p);}
static int members(u32 *m)
{
 char p[96];int fd,n;path(p,"members");fd=open(p,O_RDONLY);
 if(fd<0)return -1;
 n=read(fd,m,(SESSION_TASKS+1)*sizeof(u32));close(fd);
 return n==(SESSION_TASKS+1)*sizeof(u32)&&(m[0]==SESSION_MAGIC||m[0]==SESSION_CLOSING)?0:-1;
}
/* ELKS rename is link+unlink and cannot replace a destination. Every
 * reader holds the same lock. Publish a complete new file, retaining a link
 * to the old one for rollback. Failed rollback leaves the journal links, so
 * the next acquisition refuses access instead of reading invented state.
 * A crash leaves the lock in place and likewise fails closed. */
static int replace_file(const char *dest,const void *buf,unsigned n)
{
 char next[96],prior[96];int old;
 path(next,"next");path(prior,"prior");
 if(putfile(next,buf,n))return -1;
 old=!link(dest,prior);
 if(!old&&errno!=ENOENT){unlink(next);return -1;}
 if(old&&unlink(dest)){unlink(prior);unlink(next);return -1;}
 if(link(next,dest)) {
  if(old&&link(prior,dest))return -1;
  if(old)unlink(prior);
  unlink(next);return -1;
 }
 unlink(next);if(old)unlink(prior);return 0;
}
static int save_members(const u32 *m)
{
 char p[96];path(p,"members");
 return replace_file(p,m,(SESSION_TASKS+1)*sizeof(u32));
}
/* Pointer is the only static near-data allocation. The small process record
 * lives in the existing heap. Callers can pass the returned path explicitly
 * to another native task via W16SESSION; no private atom table is aliased. */
const char *session_directory(void)
{
 const char *join;u32 m[SESSION_TASKS+1];unsigned i,j;int made=0,slot=-1,alive;char lease[96];
 if(session)return session->pid==(u32)getpid()?session->dir:0;
 session=malloc(sizeof(*session));if(!session)return 0;
 session->pid=(u32)getpid();session->slot=0;session->livefd=-1;join=getenv("W16SESSION");
 if(join&&*join) {
  if(*join!='/'||strlen(join)>=sizeof(session->dir))goto fail;
  strcpy(session->dir,join);
 } else {
  for(i=0;i<16;i++) {
   sprintf(session->dir,"/tmp/w16.%lu.%u",(unsigned long)session->pid,i);
   if(!mkdir(session->dir,0700)){made=1;break;}
   if(errno!=EEXIST)goto fail;
  }
  if(!made)goto fail;
 }
 if(lock())goto fail;
 if(made){memset(m,0,sizeof(m));m[0]=SESSION_MAGIC;}
 else if(members(m)||m[0]!=SESSION_MAGIC)goto bad;
 for(j=1;j<=SESSION_TASKS;j++) {
  /* Do not reopen an abandoned session as an on-disk atom cache. */
  if(m[j]){alive=member_alive(j);if(alive<0)goto bad;if(!alive)m[j]=0;}
  if(m[j]==session->pid)goto bad;
  if(!m[j]&&slot<0)slot=j;
 }
 if(!made) {
  for(j=1;j<=SESSION_TASKS&&!m[j];j++);
  if(j>SESSION_TASKS)goto bad;
 }
 if(slot<0)goto bad;
 leasepath(lease,slot);
 if(mknod(lease,S_IFIFO|0600,0))goto bad;
 session->slot=slot;
 session->livefd=open(lease,O_RDONLY|O_NONBLOCK);
 if(session->livefd<0||fcntl(session->livefd,F_SETFD,FD_CLOEXEC)<0)goto bad;
 m[slot]=session->pid;
 if(save_members(m))goto bad;
 unlock();return session->dir;
 bad:
 if(session->livefd>=0)close(session->livefd);
 if(session->slot){leasepath(lease,session->slot);unlink(lease);}
 unlock();
 fail:
 if(made){char p[96];path(p,"next");unlink(p);rmdir(session->dir);}
 free(session);session=0;return 0;
}
void session_shutdown(void)
{
 u32 m[SESSION_TASKS+1];unsigned i;int live=0;char p[96];
 if(!session)return;
 if(session->pid!=(u32)getpid())return;
 if(!lock()) {
  if(!members(m)&&m[session->slot]==session->pid) {
   m[session->slot]=0;
   for(i=1;i<=SESSION_TASKS;i++) {
    if(m[i]&&member_alive(i)==0)m[i]=0;
    if(m[i])live=1;
   }
   if(live)save_members(m);
   else {
    /* Invalidate membership before releasing the lock to any joiner. */
    path(p,"members");unlink(p);
    for(i=0;i<SESSION_ATOMS;i++){atompath(p,i);unlink(p);}
    path(p,"next");unlink(p);
   }
  } else live=1;
  leasepath(p,session->slot);unlink(p);
  unlock();if(!live)rmdir(session->dir);
 }
 if(session->livefd>=0)close(session->livefd);
 free(session);session=0;
}
/* Only the current single-task GUI session can yet negotiate shutdown.
 * Refuse other live participants rather than claiming to have queried them.
 * Closing membership prevents a new join during callbacks, while atoms remain
 * usable by the existing task. Cancel reopens the same live namespace. */
int session_end_begin(void)
{
 u32 m[SESSION_TASKS+1];unsigned i;int ok=0;
 if(!session_directory()||lock())return 0;
 if(members(m)||m[0]!=SESSION_MAGIC)goto done;
 for(i=1;i<=SESSION_TASKS;i++) {
  if(!m[i]||m[i]==session->pid)continue;
  if(member_alive(i)==0)m[i]=0;
  else goto done;
 }
 m[0]=SESSION_CLOSING;ok=!save_members(m);
 done:unlock();return ok;
}
void session_end_cancel(void)
{
 u32 m[SESSION_TASKS+1];
 if(!session||lock())return;
 if(!members(m)&&m[0]==SESSION_CLOSING&&m[session->slot]==session->pid) {
  m[0]=SESSION_MAGIC;save_members(m);
 }
 unlock();
}
static int atom_read(u16 i,struct atom *a)
{
 char p[96];int fd,n;atompath(p,i);fd=open(p,O_RDONLY);
 if(fd<0){if(errno!=ENOENT)return -1;memset(a,0,sizeof(*a));return 0;}
 n=read(fd,a,sizeof(*a));close(fd);
 if(n!=sizeof(*a)||!a->refs||!a->len||a->len>255||a->name[a->len]||strlen(a->name)!=a->len)return -1;
 return 0;
}
static int atom_write(u16 i,const struct atom *a)
{
 char p[96];atompath(p,i);
 if(!a->refs)return unlink(p);
 return replace_file(p,a,sizeof(*a));
}
static u8 fold(u8 c)
{ return (c>='A'&&c<='Z')||(c>=0xc0&&c<=0xde&&c!=0xd7)?c+32:c; }
static int same(const char *a,const char *b)
{for(;*a&&*b;a++,b++)if(fold((u8)*a)!=fold((u8)*b))return 0;return *a==*b;}
static u32 atom_string(u16 ss,u16 ap,int add)
{
 char name[256];struct atom a;u16 seg=AW(2),off=AW(0),n,i,empty=SESSION_ATOMS,result=0;
 u32 number=0;
 if(!seg)return off&&off<0xc000?off:0;
 for(n=0;n<256;n++) {
  if((u32)off+n>65535UL||((u32)seg<<4)+off+n>0xfffffUL)return 0;
  name[n]=rd8(seg,off+n);if(!name[n])break;
 }
 if(!n||n==256)return 0;
 if(name[0]=='#'&&n>1) {
  for(i=1;i<n&&name[i]>='0'&&name[i]<='9';i++) {
   if(number<0xc000UL)number=number*10+(name[i]-'0');
  }
  if(i==n)return number&&number<0xc000?number:0;
 }
 if(!session_directory()||lock())return 0;
 for(i=0;i<SESSION_ATOMS;i++) {
  if(atom_read(i,&a))goto done;
  if(!a.refs){if(empty==SESSION_ATOMS)empty=i;continue;}
  if(!same(name,a.name))continue;
  if(add){if(a.refs==0xffff)goto done;a.refs++;if(atom_write(i,&a))goto done;}
  result=0xc000u+i;goto done;
 }
 if(add&&empty<SESSION_ATOMS) {
  memset(&a,0,sizeof(a));a.refs=1;a.len=n;memcpy(a.name,name,n+1);
  if(!atom_write(empty,&a))result=0xc000u+empty;
 }
 done:unlock();return result;
}
u32 u_GlobalAddAtom(u16 ss,u16 ap){return atom_string(ss,ap,1);}
u32 u_GlobalFindAtom(u16 ss,u16 ap){return atom_string(ss,ap,0);}
u32 u_GlobalDeleteAtom(u16 ss,u16 ap)
{
 u16 atom=AW(0),result=atom;struct atom a;
 if(atom<0xc000)return 0;
 if(atom>=0xc000u+SESSION_ATOMS||!session_directory()||lock())return result;
 if(!atom_read(atom-0xc000,&a)&&a.refs){a.refs--;if(!atom_write(atom-0xc000,&a))result=0;}
 unlock();return result;
}
