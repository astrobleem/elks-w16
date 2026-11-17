#include "w16.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#undef malloc
#undef free
#include <stdlib.h>
static int allocations, fail_alloc;
void *test_malloc(size_t n) { void *p; if(fail_alloc)return 0; p=malloc(n); if(p)allocations++; return p; }
void test_free(void *p) { if(p)allocations--; free(p); }
u8 host_mem[0x110000];
u8 *host_ptr(u16 s,u16 o,u16 n)
{ u32 p=LIN(s,o); assert((u32)o+n<=65536 && p+n<=0x100000); return host_mem+p; }
u32 u_RegisterWindowMessage(u16,u16);
void registered_reset(void);
static u32 call(u16 s,u16 o)
{ wr16(0x1000,0,o);wr16(0x1000,2,s);return u_RegisterWindowMessage(0x1000,0); }
int main(void)
{
    unsigned i; char name[32];
    registered_reset(); assert(!call(0,0));
    assert(!call(0x2000,0));
    strcpy((char *)host_ptr(0x2000,0,6),"Alpha"); fail_alloc=1; assert(!call(0x2000,0)); fail_alloc=0; assert(call(0x2000,0)==0xc000);
    strcpy((char *)host_ptr(0x3000,0,6),"Alpha"); assert(call(0x3000,0)==0xc000);
    strcpy((char *)host_ptr(0x2000,0,6),"alpha"); assert(call(0x2000,0)==0xc001);
    wr8(0x2000,0xffff,'a'); assert(!call(0x2000,0xffff));
    wr8(0x2000,0xfffe,'b');wr8(0x2000,0xffff,0); assert(call(0x2000,0xfffe)==0xc002);
    memset(host_ptr(0x2000,0,256),'c',256); assert(!call(0x2000,0));
    wr8(0x2000,255,0); assert(call(0x2000,0)==0xc003);
    wr8(0xffff,15,'x'); assert(!call(0xffff,15));
    assert(!u_RegisterWindowMessage(0x1000,0xfffd));
    for(i=4;i<32;i++) {
        sprintf(name,"message%u",i); strcpy((char *)host_ptr(0x2000,0,32),name);
        assert(call(0x2000,0)==0xc000+i);
    }
    strcpy((char *)host_ptr(0x2000,0,32),"overflow"); assert(!call(0x2000,0));
    assert(call(0x3000,0)==0xc000); /* existing name still works when full */
    registered_reset(); registered_reset(); assert(call(0x3000,0)==0xc000);
    registered_reset(); assert(allocations==0); puts("Registered message identity, capacity and bounds checks passed");return 0;
}
