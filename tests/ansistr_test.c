#include "w16.h"
#include <assert.h>
#include <string.h>
static u8 mem[65536];
u8 *host_ptr(u16 s,u16 o,u16 n){assert(s==0x2000 && (u32)o+n<=65536);return mem+o;}
u32 k_AnsiToOem(u16,u16),k_OemToAnsi(u16,u16);
u32 u_lstrcmp(u16,u16),u_lstrcmpi(u16,u16),u_AnsiNext(u16,u16),u_AnsiPrev(u16,u16);
int main(void){
 wr16(0x2000,0,80);wr16(0x2000,2,0x2000);wr16(0x2000,4,96);wr16(0x2000,6,0x2000);
 strcpy((char*)mem+80,"color");strcpy((char*)mem+96,"COLOR");
 assert(!u_lstrcmpi(0x2000,0)&&(i16)u_lstrcmp(0x2000,0)<0);
 mem[80]=0xe4;mem[96]=0xc4;assert(!u_lstrcmpi(0x2000,0));mem[96]=0xe5;assert(u_lstrcmpi(0x2000,0)==1);
 assert(u_AnsiNext(0x2000,0)==0x20000051UL);mem[80]=0;assert(u_AnsiNext(0x2000,0)==0x20000050UL);
 wr16(0x2000,4,80);assert(u_AnsiPrev(0x2000,0)==0x20000050UL);wr16(0x2000,0,81);assert(u_AnsiPrev(0x2000,0)==0x20000050UL);
 wr16(0x2000,0,0xffff);mem[0xffff]=1;assert(u_AnsiNext(0x2000,0)==0x2000ffffUL);
 /* KEYBOARD conversion: non-ASCII mappings, in-place use and atomic refusals. */
 wr16(0x2000,0,112);wr16(0x2000,2,0x2000);wr16(0x2000,4,80);wr16(0x2000,6,0x2000);
 memcpy(mem+80,"\xe9\xc4\xa3\xe7",5);
 assert(k_AnsiToOem(0x2000,0)==1&&!memcmp(mem+112,"\x82\x8e\x9c\x87",5));
 wr16(0x2000,0,128);wr16(0x2000,4,112);
 assert(k_OemToAnsi(0x2000,0)==1&&!memcmp(mem+128,mem+80,5));
 wr16(0x2000,0,160);wr16(0x2000,4,160);memcpy(mem+160,"\xe9",2);
 assert(k_AnsiToOem(0x2000,0)==1&&mem[160]==0x82);
 assert(k_OemToAnsi(0x2000,0)==1&&mem[160]==0xe9);
 memcpy(mem+160,"abc",4);wr16(0x2000,0,161);
 assert(!k_AnsiToOem(0x2000,0)&&!memcmp(mem+160,"abc",4));
 wr16(0x2000,0,0xffff);mem[0xffff]=77;
 assert(!k_AnsiToOem(0x2000,0)&&mem[0xffff]==77);
 wr16(0x2000,0,112);wr16(0x2000,4,0xfffe);mem[0xfffe]=0xe9;mem[0xffff]='A';mem[112]=86;
 assert(!k_AnsiToOem(0x2000,0)&&mem[112]==86);
 wr16(0x2000,4,16);wr16(0x2000,6,0xffff);
 assert(!k_AnsiToOem(0x2000,0)&&mem[112]==86);
 wr16(0x2000,4,0xffff);wr16(0x2000,6,0x2000);mem[0xffff]=0;
 assert(k_AnsiToOem(0x2000,0)==1&&!mem[112]);
 assert(!k_AnsiToOem(0x2000,0xfff9));
 return 0;
}
