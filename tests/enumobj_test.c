#include "w16.h"
#include <assert.h>
#include <string.h>
static struct ne_module module;
struct ne_module *w16_mod=&module;
u16 w16_app_ss=0x1000,w16_app_sp=300;
static u8 ram[512]; static int calls,kind,stop,depth;
static const u32 colors[]={0x112233,0x445566,0x778899};
u8 *host_ptr(u16 s,u16 o,u16 n)
{ assert(s==0x1000 && o+n<=sizeof(ram)); return ram+o; }
u32 w16_call16(u16 off,u16 seg,u16 ds,u16 n,const u16 *args)
{
    u16 p=args[1];
    assert(off==7&&seg==0x1234&&ds==module.dgroup&&n==4);
    assert(args[0]==w16_app_ss&&p==w16_app_sp&&args[2]==0xfeed&&args[3]==0x1987);
    assert(rd16(args[0],p)==0);
    if(depth) { assert(p==280); return 0; }
    assert(p==290&&rd32(args[0],p+(kind==1?6:2))==colors[calls]);
    if(kind==1) assert(rd16(args[0],p+2)==1&&!rd16(args[0],p+4));
    else assert(!rd16(args[0],p+6));
    depth=1;assert(!enum_palette_objects(2,7,0x1234,0xfeed1987,colors,1));depth=0;
    assert(w16_app_sp==p&&rd32(args[0],p+(kind==1?6:2))==colors[calls]);
    calls++;return calls==stop?0:27;
}
int main(void)
{
    module.dgroup=0x1700;
    for(kind=1;kind<=2;kind++) for(stop=1;stop<=4;stop++) {
        calls=0;assert(enum_palette_objects(kind,7,0x1234,0xfeed1987,colors,3)==(stop<=3?0u:27u));
        assert(calls==(stop<=3?stop:3)&&w16_app_sp==300);
    }
    assert(!enum_palette_objects(3,7,0x1234,0,colors,3));
    assert(!enum_palette_objects(1,7,0,0,colors,3));
    w16_app_sp=20;assert(!enum_palette_objects(1,7,0x1234,0,colors,3));
    return 0;
}
