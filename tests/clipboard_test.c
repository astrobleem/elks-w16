#include "w16.h"
#include <assert.h>
#include <stdio.h>
u32 k_GlobalAlloc(u16,u16),k_GlobalFree(u16,u16),k_GlobalReAlloc(u16,u16);
u32 u_OpenClipboard(u16,u16),u_CloseClipboard(u16,u16),u_EmptyClipboard(u16,u16);
u32 u_SetClipboardData(u16,u16),u_GetClipboardData(u16,u16),u_GetPriorityClipboardFormat(u16,u16);
extern int host_live;
struct wnd {int used;};static struct wnd window;
struct wnd *win_get(u16 h){return h==0x100?&window:0;}
static u16 ss,ds;
static u16 alloc(u16 flags){wr32(ss,0,32);wr16(ss,4,flags);return k_GlobalAlloc(ss,0);}
static u16 openclip(u16 h){wr16(ss,0,h);return u_OpenClipboard(ss,0);}
static u16 set(u16 f,u16 h){wr16(ss,0,h);wr16(ss,2,f);return u_SetClipboardData(ss,0);}
static u16 get(u16 f){wr16(ss,0,f);return u_GetClipboardData(ss,0);}
static u16 priority(u16 first,u16 second){wr16(ds,0,first);wr16(ds,2,second);wr16(ss,0,2);wr16(ss,2,0);wr16(ss,4,ds);return u_GetPriorityClipboardFormat(ss,0);}
int main(void)
{
 u16 h,n,fixed;u32 ptr;
 assert(!seg_alloc(16,&ss)&&!seg_alloc(16,&ds));
 assert(!u_EmptyClipboard(ss,0)&&!u_CloseClipboard(ss,0));
 assert(priority(8,2)==0);h=alloc(2);fixed=alloc(0);
 assert(!set(8,h)&&!get(8));assert(!openclip(0xdead));
 assert(openclip(0)&&!openclip(0));assert(u_EmptyClipboard(ss,0));assert(!set(8,h));assert(u_CloseClipboard(ss,0));
 assert(openclip(0x100)&&u_EmptyClipboard(ss,0));assert(!set(8,fixed)&&!set(2,h)&&!set(0xc000,h)&&!set(8,0));
 assert(global_pointer(h,&ptr));wr32(ptr>>16,0,0x12345678);assert(!set(8,h));global_unlock(h);
 assert(set(8,h)==h&&get(8)==h&&set(8,h)==h);assert(!set(1,h));
 wr16(ss,0,h);assert(k_GlobalFree(ss,0)==h);
 wr16(ss,0,2);wr32(ss,2,64);wr16(ss,6,h);assert(!k_GlobalReAlloc(ss,0));
 assert(u_CloseClipboard(ss,0));assert(!get(8));assert(priority(2,8)==8&&priority(7,1)==0xffff);
 assert(openclip(0x100));assert(global_pointer(get(8),&ptr)&&rd32(ptr>>16,0)==0x12345678);global_unlock(h);
 n=alloc(2);assert(set(8,n)==n&&!global_size(h));assert(priority(8,1)==8);
 h=alloc(2);assert(set(1,h)==h&&priority(1,8)==1&&priority(8,1)==8);
 assert(u_EmptyClipboard(ss,0)&&!global_size(h)&&!global_size(n));assert(!priority(8,1));
 h=alloc(2);assert(set(7,h)==h);assert(u_CloseClipboard(ss,0));clipboard_shutdown();assert(!global_size(h)&&!priority(7,1));
 wr16(ss,0,fixed);assert(!k_GlobalFree(ss,0));global_shutdown();seg_free(ss);seg_free(ds);assert(!host_live);
 puts("Actual clipboard availability, priority ordering, movable ownership, replacement and cleanup passed");return 0;
}
