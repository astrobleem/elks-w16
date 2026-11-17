#include "w16.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
u8 host_mem[0x110000];
u8 *host_ptr(u16 s,u16 o,u16 n)
{ u32 p=LIN(s,o); assert((u32)o+n<=65536 && p+n<=0x100000); return host_mem+p; }
u32 u_CopyRect(u16,u16),u_PtInRect(u16,u16),u_IntersectRect(u16,u16);
static void args(u16 a,u16 b,u16 c,u16 d,u16 e,u16 f)
{ wr16(0x1000,0,a);wr16(0x1000,2,b);wr16(0x1000,4,c);wr16(0x1000,6,d);wr16(0x1000,8,e);wr16(0x1000,10,f); }
static void rect(u16 s,u16 o,i16 l,i16 t,i16 r,i16 b)
{ wr16(s,o,l);wr16(s,o+2,t);wr16(s,o+4,r);wr16(s,o+6,b); }
int main(void)
{
    int x,y,dx; u8 saved[8];
    rect(0x2000,16,-10,-20,30,40);
    for(y=-21;y<=41;y++) for(x=-11;x<=31;x++) {
        args(x,y,16,0x2000,0,0);
        assert(u_PtInRect(0x1000,0)==(u32)(x>=-10&&x<30&&y>=-20&&y<40));
    }
    for(dx=-7;dx<=7;dx++) {
        rect(0x2000,16,-32768,32767,0,-1); memcpy(saved,host_ptr(0x2000,16,8),8);
        args(16,0x2000,16+dx,0x2000,0,0); assert(u_CopyRect(0x1000,0));
        assert(!memcmp(saved,host_ptr(0x2000,16+dx,8),8));
    }
    rect(0x2000,16,-10,-20,30,40);
    args(0,0x2001,18,0x2000,0,0); assert(u_CopyRect(0x1000,0));
    assert((i16)rd16(0x2000,18)==-10); /* physical alias overlap */
    rect(0x2000,0xfff8,1,2,3,4); args(0xfff8,0x2000,0,0x3000,0,0);
    assert(u_CopyRect(0x1000,0));
    args(0xfff9,0x2000,0,0x3000,0,0); assert(!u_CopyRect(0x1000,0));
    args(0,0,0,0x3000,0,0); assert(!u_CopyRect(0x1000,0));
    args(9,0xffff,0,0x3000,0,0); assert(!u_CopyRect(0x1000,0));
    assert(!u_CopyRect(0x1000,0xfff9)); assert(!u_PtInRect(0x1000,0xfff9));
    for(dx=-4;dx<=4;dx++) {
        rect(0x2000,16,0,0,4,4); rect(0x3000,0,dx,1,dx+2,3);
        args(0,0x3000,16,0x2000,18,0x2000);
        assert(u_IntersectRect(0x1000,0)==(u32)(dx>-2&&dx<4));
        if(dx>-2&&dx<4) {
            assert((i16)rd16(0x2000,18)==(dx>0?dx:0));
            assert(rd16(0x2000,20)==1);
            assert((i16)rd16(0x2000,22)==(dx+2<4?dx+2:4));
            assert(rd16(0x2000,24)==3);
        } else assert(!rd32(0x2000,18)&&!rd32(0x2000,22));
    }
    rect(0x3000,0,9,8,7,6); memcpy(saved,host_ptr(0x3000,0,8),8);
    args(0xfff9,0x2000,0,0x3000,0,0); assert(!u_CopyRect(0x1000,0));
    assert(!memcmp(saved,host_ptr(0x3000,0,8),8));
    args(0xfff9,0x2000,16,0x2000,0,0x3000);
    assert(!u_IntersectRect(0x1000,0));
    assert(!memcmp(saved,host_ptr(0x3000,0,8),8));
    rect(0x2000,16,4,4,-4,-4); args(0,0,16,0x2000,0,0);
    assert(!u_PtInRect(0x1000,0));
    assert(!u_IntersectRect(0x1000,0xfff5));
    puts("RECT boundary, exclusive edges, overlap and alias checks passed"); return 0;
}
