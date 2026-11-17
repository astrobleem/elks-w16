/* Bounded Win16 RECT helpers. Snapshot inputs before writing overlapping output. */
#include "w16.h"
static int valid(u16 seg,u16 off)
{
    return (seg || off) && off<=0xfff8u &&
        (((u32)seg<<4)+off)<=0xffff8UL;
}
static void get(u16 seg,u16 off,i16 *r)
{
    u16 i;
    for(i=0;i<4;i++) r[i]=(i16)rd16(seg,off+2*i);
}
static void put(u16 seg,u16 off,const i16 *r)
{
    u16 i;
    for(i=0;i<4;i++) wr16(seg,off+2*i,(u16)r[i]);
}
u32 u_CopyRect(u16 ss,u16 ap)
{
    u16 so,ssrc,doff,ds; i16 r[4];
    if(ap>0xfff8u) return 0;
    so=AW(0); ssrc=AW(2); doff=AW(4); ds=AW(6);
    if(!valid(ssrc,so)||!valid(ds,doff)) return 0;
    get(ssrc,so,r); put(ds,doff,r); return 1;
}
u32 u_PtInRect(u16 ss,u16 ap)
{
    i16 r[4],x,y; u16 off,seg;
    if(ap>0xfff8u) return 0;
    x=(i16)AW(0); y=(i16)AW(2); off=AW(4); seg=AW(6);
    if(!valid(seg,off)) return 0;
    get(seg,off,r);
    return x>=r[0] && x<r[2] && y>=r[1] && y<r[3];
}
u32 u_IntersectRect(u16 ss,u16 ap)
{
    i16 a[4],b[4]; u16 ao,as,bo,bs,doff,ds,i; int nonempty;
    if(ap>0xfff4u) return 0;
    bo=AW(0); bs=AW(2); ao=AW(4); as=AW(6); doff=AW(8); ds=AW(10);
    if(!valid(as,ao)||!valid(bs,bo)||!valid(ds,doff)) return 0;
    get(as,ao,a); get(bs,bo,b);
    for(i=0;i<2;i++) if(a[i]<b[i]) a[i]=b[i];
    for(i=2;i<4;i++) if(a[i]>b[i]) a[i]=b[i];
    nonempty=a[0]<a[2] && a[1]<a[3];
    if(!nonempty) for(i=0;i<4;i++) a[i]=0;
    put(ds,doff,a); return nonempty;
}

static int empty(const i16 *r)
{
    return r[0] >= r[2] || r[1] >= r[3];
}

u32 u_IsRectEmpty(u16 ss,u16 ap)
{
    i16 r[4];
    if(ap>0xfffc || !valid(AW(2),AW(0))) return 1;
    get(AW(2),AW(0),r);
    return empty(r);
}

u32 u_SetRectEmpty(u16 ss,u16 ap)
{
    static const i16 zero[4]={0,0,0,0};
    if(ap>0xfffc || !valid(AW(2),AW(0))) return 0;
    put(AW(2),AW(0),zero);
    return 1;
}

u32 u_UnionRect(u16 ss,u16 ap)
{
    i16 a[4],b[4]; u16 ao,as,bo,bs,doff,ds,i;
    if(ap>0xfff4u) return 0;
    bo=AW(0); bs=AW(2); ao=AW(4); as=AW(6); doff=AW(8); ds=AW(10);
    if(!valid(as,ao)||!valid(bs,bo)||!valid(ds,doff)) return 0;
    get(as,ao,a); get(bs,bo,b);
    if(empty(a)) {
        int eb=empty(b);
        for(i=0;i<4;i++) a[i]=eb?0:b[i];
    } else if(!empty(b)) {
        for(i=0;i<2;i++) if(b[i]<a[i]) a[i]=b[i];
        for(i=2;i<4;i++) if(b[i]>a[i]) a[i]=b[i];
    }
    put(ds,doff,a);
    return !empty(a);
}
