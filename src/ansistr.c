/* Single-byte Western ANSI strings, as used by the English Win3.0 guest.
 * Never advance into the next 64 KB offset range or wrap the physical address. */
#include "w16.h"
static int readable(u16 s,u16 o)
{ return (s || o) && ((u32)s<<4)+o<=0xfffffUL; }
static u8 lower(u8 c)
{
    if((c>='A'&&c<='Z') || (c>=0xc0&&c<=0xde&&c!=0xd7)) return c+32;
    if(c==0x8a || c==0x8c || c==0x8e) return c+16;
    return c==0x9f?0xff:c;
}
static u32 compare(u16 ss,u16 ap,int fold)
{
    u16 a,as,b,bs;u8 x,y;
    if(ap>0xfff8u) return 0;
    a=AW(4);as=AW(6);b=AW(0);bs=AW(2);
    while(readable(as,a) && readable(bs,b)){
        x=rd8(as,a);y=rd8(bs,b);if(fold){x=lower(x);y=lower(y);}
        if(x!=y) return x<y?0xffffu:1;
        if(!x || a==0xffffu || b==0xffffu) break;
        a++;b++;
    }
    return 0;
}
u32 u_lstrcmpi(u16 ss,u16 ap){return compare(ss,ap,1);}
u32 u_lstrcmp(u16 ss,u16 ap){return compare(ss,ap,0);}
u32 u_AnsiNext(u16 ss,u16 ap)
{
    u16 s,o;
    if(ap>0xfffcu) return 0;
    s=AW(2);o=AW(0);
    if(readable(s,o) && o<0xffffu && readable(s,o+1) && rd8(s,o)) ++o;
    return ((u32)s<<16)|o;
}
u32 u_AnsiPrev(u16 ss,u16 ap)
{
    u16 s,o,base,bs;
    if(ap>0xfff8u) return 0;
    s=AW(2);o=AW(0);base=AW(4);bs=AW(6);
    if(readable(s,o) && readable(bs,base) && o && ((u32)s<<4)+o>((u32)bs<<4)+base) --o;
    return ((u32)s<<16)|o;
}
