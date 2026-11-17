#include "w16.h"
#include <assert.h>
#include <string.h>
static u8 mem[65536];
u8 *host_ptr(u16 s,u16 o,u16 n){assert(s==0x2000 && (u32)o+n<=65536);return mem+o;}
u32 u_lstrcmp(u16,u16),u_lstrcmpi(u16,u16),u_AnsiNext(u16,u16),u_AnsiPrev(u16,u16);
int main(void){
 wr16(0x2000,0,80);wr16(0x2000,2,0x2000);wr16(0x2000,4,96);wr16(0x2000,6,0x2000);
 strcpy((char*)mem+80,"color");strcpy((char*)mem+96,"COLOR");
 assert(!u_lstrcmpi(0x2000,0)&&(i16)u_lstrcmp(0x2000,0)<0);
 mem[80]=0xe4;mem[96]=0xc4;assert(!u_lstrcmpi(0x2000,0));mem[96]=0xe5;assert(u_lstrcmpi(0x2000,0)==1);
 assert(u_AnsiNext(0x2000,0)==0x20000051UL);mem[80]=0;assert(u_AnsiNext(0x2000,0)==0x20000050UL);
 wr16(0x2000,4,80);assert(u_AnsiPrev(0x2000,0)==0x20000050UL);wr16(0x2000,0,81);assert(u_AnsiPrev(0x2000,0)==0x20000050UL);
 wr16(0x2000,0,0xffff);mem[0xffff]=1;assert(u_AnsiNext(0x2000,0)==0x2000ffffUL);return 0;
}
