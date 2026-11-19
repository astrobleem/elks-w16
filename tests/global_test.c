#include "w16.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
u32 k_GlobalAlloc(u16,u16),k_GlobalFree(u16,u16),k_GlobalReAlloc(u16,u16);
u32 k_GlobalLRUNewest(u16,u16);
extern int host_live,host_fail_after;
struct ne_module *w16_mod;
u32 u_LoadIconHandler(u16,u16),k_DirectResAlloc(u16,u16);
u32 u_CreateCursorIconIndirect(u16,u16),u_DumpIcon(u16,u16),u_DestroyCursor(u16,u16);
void test_resource(u16);
static u16 stack;
static u32 alloc(u16 flags,u32 bytes)
{ wr32(stack,0,bytes);wr16(stack,4,flags);return k_GlobalAlloc(stack,0); }
static u32 resize(u16 h,u32 bytes,u16 flags)
{wr16(stack,0,flags);wr32(stack,2,bytes);wr16(stack,6,h);return k_GlobalReAlloc(stack,0);}
static u32 release(u16 h)
{wr16(stack,0,h);return k_GlobalFree(stack,0);}
static void native_images(void)
{
 struct ne_module app;u16 src,h,copy,len;u32 ptr,p2;u8 *dib,*again;
 memset(&app,0,sizeof(app));app.dgroup=0x1234;app.loaded=1;w16_mod=&app;
 assert(!seg_alloc(64,&src));fzero(src,0,1024);
 /* 32x32 monochrome native layout, top-down, AND then XOR. */
 wr16(src,4,32);wr16(src,6,32);wr16(src,8,4);wr8(src,10,1);wr8(src,11,1);
 wr8(src,12,0x30);wr8(src,140,0x50);
 wr16(stack,0,140);wr16(stack,2,src);wr16(stack,4,12);wr16(stack,6,src);
 wr16(stack,8,0);wr16(stack,10,src);wr16(stack,12,0);
 host_fail_after=0;assert(!u_CreateCursorIconIndirect(stack,0));host_fail_after=-1;
 h=u_CreateCursorIconIndirect(stack,0);assert(h&&global_image(h,0)&&global_size(h)==272);
 assert(global_pointer(h,&ptr)&&ptr);assert(rd16(ptr>>16,4)==32&&rd8(ptr>>16,12)==0x30);
 /* DumpIcon returns live alias pointers and mask byte counts. */
 wr16(stack,0,32);wr16(stack,2,src);wr16(stack,4,36);wr16(stack,6,src);
 wr16(stack,8,40);wr16(stack,10,src);wr16(stack,12,0);wr16(stack,14,ptr>>16);
 assert(u_DumpIcon(stack,0)==0x00800080UL&&rd16(src,40)==12);
 assert(rd32(src,36)==(ptr+12)&&rd32(src,32)==(ptr+140));
 dib=image_snapshot(h,&len);assert(dib&&len==304&&dib[176+31*4]==0x30&&dib[48+31*4]==0x50);
 copy=image_copy(h);assert(copy&&copy!=h);assert(global_pointer(copy,&p2)&&p2!=ptr);
 wr8(ptr>>16,140,0xa0);assert(rd8(p2>>16,140)==0x50);global_unlock(copy);
 again=image_snapshot(h,&len);assert(again&&again[48+31*4]==0xa0);free(again);
 global_unlock(h);assert(image_destroy(h)&&!global_pointer(h,&ptr)&&!image_destroy(h));
 assert(image_destroy(copy));
 /* Handler conversion replaces the supplied allocation, keeps its identity,
  * and produces fixed black/white DDB indices with the masks intact. */
 wr16(stack,0,304);wr16(stack,2,0x10);wr16(stack,4,0x4321);assert(!k_DirectResAlloc(stack,0));
 wr16(stack,4,app.dgroup);app.loaded=0;assert(!k_DirectResAlloc(stack,0));app.loaded=1;
 h=k_DirectResAlloc(stack,0);assert(h&&global_pointer(h,&ptr));fcopy_out(ptr>>16,0,dib,304);global_unlock(h);
 wr16(stack,0,0);wr16(stack,2,h);assert(!u_LoadIconHandler(stack,0)&&!global_image(h,0));
 wr16(stack,0,1);assert(u_LoadIconHandler(stack,0)==h&&global_image(h,0));
 again=image_snapshot(h,&len);assert(again&&!memcmp(dib,again,304));free(again);free(dib);
 assert(global_pointer(h,&ptr));assert(!global_clipboard_take(h));global_unlock(h);
 wr16(stack,0,h);assert(u_DestroyCursor(stack,0)&&!global_pointer(h,&ptr));
 wr16(stack,0,0x500);assert(!u_DestroyCursor(stack,0)); /* shared cursor != owned block */
 /* Packed 4bpp pixels and bottom-up DIB rows round-trip through native data. */
 dib=calloc(1,744);assert(dib);dib[0]=40;dib[4]=32;dib[8]=64;dib[12]=1;dib[14]=4;
 dib[44]=128;dib[100]=dib[101]=dib[102]=255;dib[104+31*16]=0x1f;dib[616+31*4]=0x80;
 h=image_from_dib(0,dib,744);assert(h);again=image_snapshot(h,&len);
 assert(again&&len==744&&again[104+31*16]==0x1f&&again[616+31*4]==0x80);
 free(again);assert(image_destroy(h));
 /* Invalid palette/index/layout paths do not convert or leak the input. */
 h=global_alloc(744,0x42);assert(h);dib[14]=8;assert(!image_from_dib(h,dib,744)&&!global_image(h,0));
 assert(global_size(h)==752&&!global_free(h));free(dib);
 host_fail_after=0;assert(!image_from_dib(0,(const u8 *)"bad",3));host_fail_after=-1;
 assert(!u_DumpIcon(stack,0xffff)&&!u_CreateCursorIconIndirect(stack,0xffff));
 seg_free(src);w16_mod=0;
}
int main(void)
{
    u16 h,fix,i;u32 ptr,p2;u16 hs[48];
    assert(!seg_alloc(16,&stack));
    test_resource(stack);
    native_images();
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
