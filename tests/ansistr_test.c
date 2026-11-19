#include "w16.h"
#include <assert.h>
#include <string.h>
static u8 mem[65536];
u8 *host_ptr(u16 s,u16 o,u16 n){assert(s==0x2000 && (u32)o+n<=65536);return mem+o;}
u32 k_AnsiToOem(u16,u16),k_OemToAnsi(u16,u16);
u32 u_lstrcmp(u16,u16),u_lstrcmpi(u16,u16),u_AnsiNext(u16,u16),u_AnsiPrev(u16,u16);
u32 u_AnsiLower(u16,u16),u_AnsiUpper(u16,u16),u_AnsiUpperBuff(u16,u16),u_IsCharAlpha(u16,u16),u_IsCharAlphaNumeric(u16,u16);
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
 /* Boundary vectors from the English Win3.0 rules, including bytes that
  * newer CP1252 routines classify or case-map differently. */
 {static const struct {u8 c,low,up,alpha,num;} v[]={
  {0,0,0,0,0},{'A','a','A',1,1},{'Z','z','Z',1,1},{'[','[','[',0,0},
  {'a','a','A',1,1},{'z','z','Z',1,1},{'{','{','{',0,0},
  {'0','0','0',0,1},{'9','9','9',0,1},{':',':',':',0,0},
  {0x8a,0x8a,0x8a,0,0},{0xaa,0xaa,0xaa,0,0},
  {0xc0,0xe0,0xc0,1,1},{0xd6,0xf6,0xd6,1,1},{0xd7,0xd7,0xd7,0,0},
  {0xd8,0xf8,0xd8,1,1},{0xde,0xfe,0xde,1,1},{0xdf,0xdf,0xdf,1,1},
  {0xe0,0xe0,0xc0,1,1},{0xf6,0xf6,0xd6,1,1},{0xf7,0xf7,0xf7,0,0},
  {0xf8,0xf8,0xd8,1,1},{0xfe,0xfe,0xde,1,1},{0xff,0xff,0xff,1,1}};
  unsigned i;for(i=0;i<sizeof(v)/sizeof(v[0]);i++){
   wr32(0x2000,0,v[i].c);
   assert(u_AnsiLower(0x2000,0)==v[i].low&&u_AnsiUpper(0x2000,0)==v[i].up);
   assert(!!u_IsCharAlpha(0x2000,0)==v[i].alpha&&!!u_IsCharAlphaNumeric(0x2000,0)==v[i].num);
  }}
 wr32(0x2000,0,'A');assert(u_AnsiLower(0x2000,0)=='a');
 wr32(0x2000,0,0x12c4);assert(u_AnsiLower(0x2000,0)==0x12e4);
 wr32(0x2000,0,0x20000100UL);memcpy(mem+256,"A\xc4\xd7\x8a",5);
 assert(u_AnsiLower(0x2000,0)==0x20000100UL&&!memcmp(mem+256,"a\xe4\xd7\x8a",5));
 assert(u_AnsiUpper(0x2000,0)==0x20000100UL&&!memcmp(mem+256,"A\xc4\xd7\x8a",5));
 wr32(0x2000,0,0x2000fffeUL);mem[0xfffe]='A';mem[0xffff]='B';
 assert(!u_AnsiLower(0x2000,0)&&mem[0xfffe]=='A'&&mem[0xffff]=='B');
 wr16(0x2000,0,5);wr16(0x2000,2,256);wr16(0x2000,4,0x2000);
 memcpy(mem+256,"a\xe4\xf7\xff\0",5);assert(u_AnsiUpperBuff(0x2000,0)==5);
 assert(!memcmp(mem+256,"A\xc4\xf7\xff\0",5));
 wr16(0x2000,2,0xfffe);mem[0xfffe]='a';assert(!u_AnsiUpperBuff(0x2000,0)&&mem[0xfffe]=='a');
 assert(!u_AnsiLower(0x2000,0xfffd)&&!u_AnsiUpperBuff(0x2000,0xfffb));
 /* Count zero is a complete 64K traversal, including embedded NUL bytes. */
 memset(mem,'b',sizeof(mem));wr16(0x2000,0,0);wr16(0x2000,2,0);wr16(0x2000,4,0x2000);
 assert(!u_AnsiUpperBuff(0x2000,0));assert(mem[6]=='B'&&mem[65535]=='B'&&mem[0]==0);
 return 0;
}
