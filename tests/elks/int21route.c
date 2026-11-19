/* Native two-process routing check, not a Windows application. Uses the real
 * W16 transition assembly with a tiny synthetic DOS service dispatcher. */
#include "int21.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
int opt_metrics;u32 w16_t0;
u32 now_ms(void){struct timeval t;gettimeofday(&t,0);return (u32)t.tv_sec*1000UL+t.tv_usec/1000;}
u16 W16_CDECL w16_start16(const u16 *);
void W16_CDECL w16_thunk_done(void);
u16 W16_CDECL w16_get_ds(void);
void probe_guest(void),probe_old(void);
u16 W16_CDECL probe_unknown(void);
u16 probe_third(void);
static u16 guest,marker,loader_ds,calls;
static void check(int yes,const char *what)
{
 if(yes)return;
 fprintf(stderr,"ROUTE-FAIL %s\n",what);
 /* Keep native code resident on failure: never leave a dangling test IVT. */
 for(;;)usleep(100000);
}
u32 W16_CDECL w16_api_dispatch(u16 index)
{(void)index;check(0,"unexpected API thunk");return 0;}
void W16_CDECL w16_int21_dispatch(void)
{
 u16 ss=w16_app_ss,f=w16_app_sp,ah=rd16(ss,f)>>8;
 check(ss==guest&&probe_third()==loader_ds,"private loader DS/guest SS");
 calls++;
 if(ah==0x30){wr16(ss,f,marker);wr16(ss,f+22,rd16(ss,f+22)&~1u);return;}
 if(ah==0x4c){w16_exit16(0);return;}
 check(0,"unexpected DOS call");
}
static void run_guest(void)
{
 u16 regs[13];memset(regs,0,sizeof(regs));
 regs[0]=FOFF(probe_guest);regs[1]=FSEG(probe_guest);
 regs[2]=regs[4]=guest;regs[3]=1024;
 check(!w16_start16(regs),"guest return");
 check(rd16(guest,0)==marker&&w16_get_ds()==loader_ds,"marker and native DS restored");
}
static void byte_write(int fd,char b){check(write(fd,&b,1)==1,"pipe write");}
static void byte_read(int fd,char expected)
{char b;check(read(fd,&b,1)==1&&b==expected,"pipe read");}
int main(int argc,char **argv)
{
 u16 table;u32 vector,old;int ack[2],go[2],status;pid_t child;
 char tab[12],vec[16],wr[12],rd[12],cs[12];char *args[8];
 loader_ds=w16_get_ds();check(!w16_thunk_init(),"private thunk CS");
 check(!seg_alloc(64,&guest),"guest stack allocation");fzero(guest,0,1024);
 if(argc>1) {
  check(argc==7,"child structured argv");marker=0xb222;
  table=(u16)atoi(argv[2]);vector=strtoul(argv[3],0,10);
  check(w16_get_cs()!=(u16)atoi(argv[6]),"native NE private CS");
  check(int21_router_open(guest)<0,"second root refused");
  check(!int21_router_attach(guest,table,vector),"attach child");
  byte_write(atoi(argv[4]),'Y');byte_read(atoi(argv[5]),'G');
  run_guest();byte_write(atoi(argv[4]),'D');byte_read(atoi(argv[5]),'X');
  run_guest();check(calls==4,"child DOS count");
  check(!int21_router_close(),"detach child");
  printf("ROUTE-CHILD b222 calls=%u DS=%04x\n",calls,loader_ds);fflush(stdout);
  seg_free(guest);w16_thunk_done();exit(0);
 }
 marker=0xa111;old=w16_getvect(0x21);
 w16_setvect(0x21,FOFF(probe_old),FSEG(probe_old));
 check(!int21_router_open(guest),"open router");
 check(!int21_router_info(&table,&vector),"router attachment info");
 check(probe_unknown()==0xc333,"unknown SS chains old vector");
 run_guest();check(!pipe(ack)&&!pipe(go),"handshake pipes");
 sprintf(tab,"%u",table);sprintf(vec,"%lu",(unsigned long)vector);
 sprintf(wr,"%d",ack[1]);sprintf(rd,"%d",go[0]);sprintf(cs,"%u",w16_get_cs());
 args[0]=argv[0];args[1]="child";args[2]=tab;args[3]=vec;
 args[4]=wr;args[5]=rd;args[6]=cs;args[7]=0;
 child=fork();check(child>=0,"fork");
 if(!child){close(ack[0]);close(go[1]);execv(argv[0],args);_exit(98);}
 close(ack[1]);close(go[0]);byte_read(ack[0],'Y');
 run_guest();byte_write(go[1],'G');byte_read(ack[0],'D');
 run_guest();check(calls==6,"parent DOS state survived child");
 check(int21_router_close()==1,"root stays resident for child");
 check(w16_getvect(0x21)==vector,"busy root retained IVT");
 byte_write(go[1],'X');
 check(waitpid(child,&status,0)==child&&!status,"child exit");
 check(!int21_router_close(),"restore after last task");
 check(w16_getvect(0x21)==(((u32)FSEG(probe_old)<<16)|FOFF(probe_old)),"exact old IVT");
 check(probe_unknown()==0xc333,"restored handler callable");
 w16_setvect(0x21,(u16)old,(u16)(old>>16));
 check(w16_getvect(0x21)==old,"original IVT restored");
 close(ack[0]);close(go[1]);seg_free(guest);w16_thunk_done();
 printf("ROUTE-PASS parent=a111 calls=%u DS=%04x child=b222; old=c333; IVT restored\n",calls,loader_ds);
 fflush(stdout);exit(0);
}
