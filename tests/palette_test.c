#include "w16.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
u32 g_CreatePalette(u16,u16),g_GetPaletteEntries(u16,u16);
extern int host_live;
int main(void)
{
 u16 ss,ds,p,i,n,handles[8];u32 fixed[2]={0,0xff};u8 entry[4];
 assert(!seg_alloc(16,&ss));assert(!seg_alloc(80,&ds));
 wr16(ds,0,0x300);wr16(ds,2,3);
 wr32(ds,4,0);wr32(ds,8,0xff);wr32(ds,12,0x02000001UL);
 wr16(ss,0,0);wr16(ss,2,ds);p=g_CreatePalette(ss,0);assert(p&&palette_valid(p));
 assert(palette_entry(p,1,entry)&&entry[0]==255&&entry[1]==0&&entry[2]==0&&entry[3]==0);
 assert(palette_entry(p,2,entry)&&entry[0]==1&&entry[3]==2);
 assert(!palette_entry(p,3,entry)&&!palette_entry(0xffff,0,entry));
 wr16(ds,100,0xffff);assert(palette_object(p,2,ds,100)==2&&rd16(ds,100)==3);
 assert(palette_object(0x031e,2,ds,100)==2&&rd16(ds,100)==20);
 assert(!palette_object(p,1,ds,100)&&!palette_object(p,-1,ds,100));
 assert(!palette_object(p,2,0,100)&&!palette_object(p,2,ds,65535));
 assert(!palette_object(0xffff,2,ds,100)&&!palette_object(p,2,0xffff,16));
 wr32(ds,8,0xff00); /* caller buffer ownership must not leak into palette */
 assert(palette_nearest(p,0xfd,fixed,2)==1);assert(palette_nearest(p,0,fixed,2)==0);
 wr16(ss,0,32);wr16(ss,2,ds);wr16(ss,4,8);wr16(ss,6,1);wr16(ss,8,p);
 assert(g_GetPaletteEntries(ss,0)==2);assert(rd32(ds,32)==0xff&&rd32(ds,36)==0x02000001UL);
 wr16(ss,0,0);wr16(ss,2,0);assert(g_GetPaletteEntries(ss,0)==3);
 wr16(ss,0,65534);wr16(ss,2,ds);assert(!g_GetPaletteEntries(ss,0));
 wr16(ss,0,0);wr16(ss,2,ds);wr16(ds,0,0);assert(!g_CreatePalette(ss,0));wr16(ds,0,0x300);
 wr16(ds,2,257);assert(!g_CreatePalette(ss,0));wr16(ds,2,256);
 for(i=0;i<256;i++)wr32(ds,4+i*4,i);
 handles[0]=p;for(i=1;i<8;i++){handles[i]=g_CreatePalette(ss,0);assert(handles[i]);}assert(!g_CreatePalette(ss,0));
 assert(palette_nearest(handles[7],0xff,fixed,2)==255);
 for(i=0;i<8;i++)assert(palette_delete(handles[i])==1);
 assert(!palette_valid(p)&&palette_delete(p)==-1);
 assert(palette_valid(0x31e)&&palette_delete(0x31e)==1);
 assert(palette_nearest(0x31e,0xff,fixed,2)==13);
 wr16(ss,0,0);wr16(ss,2,0);wr16(ss,8,0x31e);assert(g_GetPaletteEntries(ss,0)==20);
 assert(palette_nearest(0xffff,0,fixed,2)==0xffff);
 wr16(ss,0,0);wr16(ss,2,ds);n=g_CreatePalette(ss,0);assert(n);palette_shutdown();assert(!palette_valid(n));
 seg_free(ss);seg_free(ds);assert(!host_live);
 puts("Logical palette ownership, 256 entries, ranges, nearest colors, default palette and cleanup passed");return 0;
}
