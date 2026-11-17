#include "w16.h"
#include <assert.h>
#include <string.h>
static u8 buf[256];
u8 *host_ptr(u16 s,u16 o,u16 n) {assert(s==0x2000 && o+n<=256);return buf+o;}
u32 g_CreateIC(u16,u16);
int main(void){
 u16 h[8];int i;
 wr16(0x2000,12,80);wr16(0x2000,14,0x2000);strcpy((char*)buf+80,"dIsPlAy");
 for(i=0;i<8;i++){h[i]=g_CreateIC(0x2000,0);assert(h[i]&&ic_handle(h[i],0)==1);}
 assert(!g_CreateIC(0x2000,0));assert(ic_handle(h[2],1)==1&&!ic_handle(h[2],1));
 assert(g_CreateIC(0x2000,0)==h[2]);assert(ic_handle(h[0]+1,0)==0&&ic_handle(0x200,1)<0);
 for(i=0;i<8;i++)assert(ic_handle(h[i],1)==1);
 strcpy((char*)buf+80,"PRINTER");assert(!g_CreateIC(0x2000,0));strcpy((char*)buf+80,"display");
 wr32(0x2000,0,1);assert(!g_CreateIC(0x2000,0));wr32(0x2000,0,0);
 wr16(0x2000,12,0xfff9);assert(!g_CreateIC(0x2000,0));return 0;
}
