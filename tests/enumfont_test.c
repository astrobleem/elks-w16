#include "w16.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct ne_module module;
struct ne_module *w16_mod=&module;
u16 w16_app_ss=0x1000,w16_app_sp=400;
static u8 ram[512];static int calls,depth,stop;
u8 *host_ptr(u16 s,u16 o,u16 n){assert(s==0x1000&&(u32)o+n<=sizeof(ram));return ram+o;}
void fzero(u16 s,u16 o,u16 n){memset(host_ptr(s,o,n),0,n);}
u32 w16_call16(u16 o,u16 s,u16 ds,u16 n,const u16 *a)
{
    u16 lf=a[1],tm=a[3];
    assert(o==7&&s==0x1234&&ds==module.dgroup&&n==7);
    assert(a[0]==0x1000&&a[2]==0x1000&&a[4]==1&&a[5]==0x1234&&a[6]==0xabcd);
    assert(tm==lf+50&&lf==w16_app_sp);
    assert(rd16(0x1000,lf)==13&&rd16(0x1000,lf+2)==6&&rd16(0x1000,lf+8)==400);
    assert(!strcmp((char*)host_ptr(0x1000,lf+18,7),"System"));
    assert(rd16(0x1000,tm)==13&&rd16(0x1000,tm+2)==10&&rd16(0x1000,tm+4)==3);
    assert(rd16(0x1000,tm+12)==8);
    if(depth){assert(lf==236);return 1;}
    assert(lf==318);depth=1;
    assert(enum_system_font(0,0,7,0x1234,0x1234abcd,13,10,6,8)==1);
    depth=0;assert(w16_app_sp==318);assert(rd16(0x1000,tm)==13);
    calls++;return stop?0:27;
}
int main(void)
{
    module.dgroup=0x1700;
    assert(enum_system_font(0,0,7,0x1234,0x1234abcd,13,10,6,8)==27);
    assert(calls==1&&w16_app_sp==400);
    memcpy(ram+10,"sYsTeM",7);stop=1;
    assert(!enum_system_font(10,0x1000,7,0x1234,0x1234abcd,13,10,6,8));
    assert(calls==2&&w16_app_sp==400);
    memcpy(ram+10,"Arial",6);
    assert(!enum_system_font(10,0x1000,7,0x1234,0x1234abcd,13,10,6,8));assert(calls==2);
    assert(!enum_system_font(0,0,7,0,0,13,10,6,8));
    w16_app_sp=100;assert(!enum_system_font(0,0,7,0x1234,0,13,10,6,8));
    puts("System font enumeration, ABI, real metrics, filtering and reentry passed");
    return 0;
}
