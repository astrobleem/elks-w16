/* Opt-in shared-code Win16 tasks. A forked child abandons the copied live
 * runtime and restarts its private data from a pristine pre-task image.
 * Arguments are bounded fields, never shell commands. The parent returns a
 * real loaded hInstance only after strict child load and router attachment. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include "launch.h"
#include "taskimage.h"
#include "int21.h"
struct child {struct child *next;int pid;};
struct launch {char image[64],session[80];u16 bytes,sp,show,table;u32 vector;int ack;u8 owner;struct child *children;};
struct job {char image[64],session[80],path[124],cwd[128],tail[121];u16 show,table;u32 vector;};
struct packet {struct image_start start;struct job job;};
typedef char packet_fits[(sizeof(struct packet)<=IMAGE_PACKET_BYTES)?1:-1];
static struct launch *launch;
extern int files_executable(const char *,char *);
extern int session_join(const char *);
extern u16 session_tasks(void);
u16 W16_CDECL w16_get_ds(void);
void w16_launch_child(u16 packet);

static int transfer(int fd,void *buf,u16 n,int writing)
{
 u16 done=0;int got;
 while(done<n) {
  got=writing?write(fd,(char *)buf+done,n-done):read(fd,(char *)buf+done,n-done);
  if(got<0&&errno==EINTR)continue;
  if(got<=0)return -1;
  done+=got;
 }
 return 0;
}
static void reply_fd(int fd,u16 result)
{transfer(fd,&result,2,1);close(fd);}
int launch_prepare(u16 sp)
{
 char name[64];const char *ns;u16 bytes,i;int fd=-1,e=W16_EIO;
 if(w16_shared_safe!=1)return W16_EUNSUPP;
 for(i=0;i<16;i++) {
  sprintf(name,"/tmp/w16i.%u.%u",(unsigned)getpid(),i);
  fd=open(name,O_WRONLY|O_CREAT|O_EXCL,0600);
  if(fd>=0||errno!=EEXIST)break;
 }
 if(fd<0)return e;
 if(task_image_save(fd,sp,&bytes))goto failed;
 if(close(fd)){fd=-1;goto failed;}fd=-1;
 ns=session_directory();if(!ns){e=W16_ENOMEM;goto failed;}
 launch=calloc(1,sizeof(*launch));if(!launch){e=W16_ENOMEM;goto failed;}
 strcpy(launch->image,name);strcpy(launch->session,ns);
 launch->owner=1;launch->bytes=bytes;launch->sp=sp;launch->ack=-1;launch->show=1;
 return 0;
 failed:if(fd>=0)close(fd);unlink(name);session_shutdown();return e;
}
int launch_attach(u16 ss)
{
 if(launch&&!launch->owner)return int21_router_attach(ss,launch->table,launch->vector);
 return int21_router_open(ss);
}
void launch_reply(u16 result)
{if(launch&&launch->ack>=0){reply_fd(launch->ack,result);launch->ack=-1;}}
u16 launch_error(int e)
{
 if(e==W16_ENOMEM)return 8;
 if(e==W16_EIO)return errno==ENOENT?2:5;
 return 11;
}
void launch_poll(void)
{
 struct child **at,*c;int status,pid;
 if(!launch)return;
 at=&launch->children;
 while((c=*at)!=0) {
  pid=waitpid(c->pid,&status,WNOHANG);
  if(pid==c->pid||(pid<0&&errno==ECHILD)){*at=c->next;free(c);}
  else at=&c->next;
 }
}
u32 launch_wait(u32 wait)
{return launch&&launch->children&&wait>250?250:wait;}
u16 launch_show(void){return launch?launch->show:1;}
u16 launch_tasks(void){return launch?session_tasks():1;}
void launch_finish(void)
{
 struct child *c;int status;
 if(!launch)return;
 launch_reply(11);launch_poll();
 while((c=launch->children)!=0) {
  /* A non-owner may leave independent tasks alive; ELKS reparents them to
   * init and reaps orphan zombies. The router owner remains for all tasks. */
  if(launch->owner)while(waitpid(c->pid,&status,0)<0&&errno==EINTR);
  launch->children=c->next;free(c);
 }
 if(launch->owner)unlink(launch->image);
 free(launch);launch=0;
}
static void fork_child(struct job *job,int ack,u16 bytes,u16 sp)
{
 struct packet p;u16 seg;int fd,i;
 fd=open(job->image,O_RDONLY);
 if(fd<0){reply_fd(ack,5);_exit(1);}
 if(seg_alloc(IMAGE_PACKET_BYTES/16,&seg)){close(fd);reply_fd(ack,8);_exit(1);}
 p.start.fd=fd;p.start.ack=ack;p.start.bytes=bytes;p.start.sp=sp;
 p.start.ds=w16_get_ds();p.start.entry_off=FOFF(w16_launch_child);p.start.entry_seg=FSEG(w16_launch_child);p.start.error=11;
 memcpy(&p.job,job,sizeof(*job));fcopy_out(seg,0,&p,sizeof(p));
 /* Raw close only: never run parent Nano-X/file/global cleanup in a fork. */
 for(i=3;i<OPEN_MAX;i++)if(i!=fd&&i!=ack)close(i);
 task_image_enter(seg);_exit(1);
}
void w16_launch_child(u16 seg)
{
 struct packet *p;char native[124],*args[1];int ack=rd16(seg,2);
 p=malloc(sizeof(*p));
 if(!p){reply_fd(ack,8);seg_free(seg);_exit(1);}
 fcopy_in(p,seg,0,sizeof(*p));seg_free(seg);
 launch=calloc(1,sizeof(*launch));
 if(!launch){free(p);reply_fd(ack,8);_exit(1);}
 strcpy(launch->image,p->job.image);strcpy(launch->session,p->job.session);
 launch->bytes=p->start.bytes;launch->sp=p->start.sp;launch->show=p->job.show;
 launch->table=p->job.table;launch->vector=p->job.vector;launch->ack=ack;
 if(session_join(launch->session)||files_directory(p->job.cwd,native,1)) {
  launch_reply(3);session_shutdown();launch_finish();free(p);exit(1);
 }
 args[0]=p->job.tail;
 /* p owns the path/tail for the whole task, including borrowed NE path. */
 w16_run(p->job.path,p->job.tail[0]?1:0,args,0,0,0);
 launch_reply(11);exit(1);
}
/* WinExec(LPCSTR command, UINT show): Pascal show, offset, segment. */
u32 k_WinExec(u16 ss,u16 ap)
{
 struct job *job;struct child *child;char command[248],*p,*tail;u16 n,seg,off,result=2,used=0;
 int pipefd[2],pid,status,quoted;
 if(!launch||ap>0xfffau)return 0;
 launch_poll();seg=AW(4);off=AW(2);
 if(!seg)return 2;
 for(n=0;n<sizeof(command);n++) {
  if((u32)off+n>65535UL||((u32)seg<<4)+off+n>0xfffffUL)return 2;
  command[n]=rd8(seg,off+n);if(!command[n])break;
 }
 if(n==sizeof(command))return 2;
 job=calloc(1,sizeof(*job));child=malloc(sizeof(*child));
 if(!job||!child){free(job);free(child);return 8;}
 p=command;while(*p==' '||*p=='\t')p++;
 quoted=*p=='"';if(quoted)p++;
 while(*p && (quoted?*p!='"':(*p!=' '&&*p!='\t'))) {
  if(used+1>=sizeof(job->path))goto done;
  job->path[used++]=*p++;
 }
 job->path[used]=0;
 if(!used||(quoted&&*p!='"')){result=2;goto done;}
 if(quoted)p++;
 if(*p&&*p!=' '&&*p!='\t'){result=2;goto done;}
 while(*p==' '||*p=='\t')p++;
 tail=p;
 if(strlen(tail)>=sizeof(job->tail)){result=2;goto done;}
 strcpy(job->tail,tail);
 {char native[124];if(files_executable(job->path,native)) {
   result=errno==EACCES?5:(errno==ENOTDIR?3:2);goto done;}
  strcpy(job->path,native);
 }
 if(int21_router_info(&job->table,&job->vector)){result=0;goto done;}
 strcpy(job->image,launch->image);strcpy(job->session,launch->session);
 files_directory_label(job->cwd);job->show=AW(0);
 if(pipe(pipefd)){result=8;goto done;}
 pid=fork();
 if(pid<0){close(pipefd[0]);close(pipefd[1]);result=8;goto done;}
 if(!pid){close(pipefd[0]);fork_child(job,pipefd[1],launch->bytes,launch->sp);}
 close(pipefd[1]);
 if(transfer(pipefd[0],&result,2,0))result=11;
 close(pipefd[0]);
 if(result<=31){while(waitpid(pid,&status,0)<0&&errno==EINTR);goto done;}
 child->pid=pid;child->next=launch->children;launch->children=child;child=0;
 done:free(child);free(job);return result;
}
