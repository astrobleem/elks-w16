#include "w16.h"
#include <assert.h>
#include <stdio.h>
u32 k_GlobalAlloc(u16,u16),k_GlobalFree(u16,u16),k_GlobalReAlloc(u16,u16);
u32 k_GlobalLRUNewest(u16,u16);
extern int host_live,host_fail_after;
void test_resource(u16);
static u16 stack;
static u32 alloc(u16 flags,u32 bytes)
{ wr32(stack,0,bytes);wr16(stack,4,flags);return k_GlobalAlloc(stack,0); }
static u32 resize(u16 h,u32 bytes,u16 flags)
{wr16(stack,0,flags);wr32(stack,2,bytes);wr16(stack,6,h);return k_GlobalReAlloc(stack,0);}
static u32 release(u16 h)
{wr16(stack,0,h);return k_GlobalFree(stack,0);}
int main(void)
{
    u16 h,fix,i;u32 ptr,p2;u16 hs[48];
    assert(!seg_alloc(16,&stack));
    test_resource(stack);
    wr16(stack,0,0xffff);assert(!k_GlobalLRUNewest(stack,0));
    h=alloc(0x42,64);assert(h && global_size(h)==64);
    wr16(stack,0,h);assert(k_GlobalLRUNewest(stack,0)==h&&global_size(h)==64);
    assert(global_pointer(h,&ptr)&&ptr);for(i=0;i<64;i++)assert(!rd8(ptr>>16,i));
    wr8(ptr>>16,0,77);assert(global_pointer(h,&p2)&&p2==ptr);
    assert(global_unlock(h));assert(!global_unlock(h));assert(!global_unlock(h));
    host_fail_after=0;assert(!resize(h,128,0x40));assert(global_size(h)==64);
    host_fail_after=-1;assert(resize(h,128,0x40)==h);
    assert(global_pointer(h,&p2)&&p2!=ptr&&rd8(p2>>16,0)==77);
    for(i=64;i<128;i++)assert(!rd8(p2>>16,i));
    assert(!resize(h,256,0));assert(!resize(h,0,0));
    assert(!global_unlock(h));assert(resize(h,0,0)==h);assert(!global_size(h));
    assert(global_pointer(h,&p2)&&!p2);assert(resize(h,33,0x40)==h);
    assert(global_size(h)==48);assert(!release(h));assert(release(h)==h);
    fix=alloc(0,33);assert(fix && global_pointer(fix,&ptr)&&ptr==((u32)fix<<16));
    assert(!resize(fix,256,0));assert(!global_unlock(fix));assert(!release(fix));
    h=alloc(0x42,65520);assert(h&&global_size(h)==65520);assert(global_pointer(h,&ptr));assert(!rd8(ptr>>16,65519));assert(!release(h));
    assert(!alloc(0,65521));assert(!alloc(0,65536));
    host_fail_after=0;assert(!alloc(2,64));host_fail_after=-1;
    h=alloc(2,0);assert(h&&global_pointer(h,&ptr)&&!ptr);assert(!release(h));
    for(i=0;i<48;i++){hs[i]=alloc(2,0);assert(hs[i]);}
    assert(!alloc(2,0));for(i=0;i<48;i++)assert(!release(hs[i]));
    assert(!global_pointer(0x1234,&ptr));assert(!release(0));
    assert(alloc(2,32));assert(alloc(0,32));global_shutdown();
    assert(host_live==1);seg_free(stack);assert(!host_live);
    puts("Global memory stable handles, locks, zero fill, realloc failure and teardown passed");
}
