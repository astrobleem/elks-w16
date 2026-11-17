/* Bounded single-line DrawText using the same real system-font metrics as TextOut. */
#include "wapi.h"
#define DT_CENTER 1
#define DT_RIGHT 2
#define DT_VCENTER 4
#define DT_BOTTOM 8
#define DT_SINGLELINE 0x20
#define DT_NOCLIP 0x100
#define DT_CALCRECT 0x400
#define DT_NOPREFIX 0x800
#define DT_SUPPORTED (DT_CENTER|DT_RIGHT|DT_VCENTER|DT_BOTTOM|DT_SINGLELINE|DT_NOCLIP|DT_CALCRECT|DT_NOPREFIX)
static int text_span(u16 seg,u16 off,u16 n)
{
    return (seg||off) && (u32)off+n<=0x10000UL &&
           ((u32)seg<<4)+off+n<=0x100000UL;
}
u32 u_DrawText(u16 ss,u16 ap)
{
    u16 flags,ro,rs,so,sg,n=0,i;
    i16 count;
    i32 l,t,r,b,x,y;
    GR_SIZE tw,th,base;
    struct dc *d;
    char text[128];
    if(ap>0xfff2u) return 0; /* actual Pascal signature is 14 bytes */
    flags=AW(0);ro=AW(2);rs=AW(4);count=(i16)AW(6);so=AW(8);sg=AW(10);
    d=dc_get(AW(12));
    if(!d || (flags & ~DT_SUPPORTED) || !(flags & DT_SINGLELINE) ||
       (!(flags & DT_NOCLIP) && !(flags & DT_CALCRECT)) ||
       (flags & 3)==3 || (flags & 12)==12 || count < -1 || count>128 ||
       !text_span(rs,ro,8)) return 0;
    if(count==-1) {
        while(n<128) {
            if(!text_span(sg,so,n+1)) return 0;
            text[n]=(char)rd8(sg,so+n);
            if(!text[n]) break;
            n++;
        }
        if(n==128) return 0;
    } else {
        n=(u16)count;
        if(!text_span(sg,so,n)) return 0;
        fcopy_in(text,sg,so,n);
    }
    /* Prefix underlines, tab expansion and line breaks are not implemented. */
    for(i=0;i<n;i++) if((unsigned char)text[i]<32 ||
        (text[i]=='&' && !(flags & DT_NOPREFIX))) return 0;
    if(!n) return 0;
    l=(i16)rd16(rs,ro);t=(i16)rd16(rs,ro+2);
    r=(i16)rd16(rs,ro+4);b=(i16)rd16(rs,ro+6);
    gdi_text_size(text,n,&tw,&th,&base);
    if(tw<0 || th<=0) return 0;
    if(flags & DT_CALCRECT) {
        if(l+tw>32767 || t+th>32767) return 0;
        wr16(rs,ro+4,(u16)(l+tw));wr16(rs,ro+6,(u16)(t+th));
        return th;
    }
    x=l;y=t;
    if(flags & DT_RIGHT) x=r-tw;
    else if(flags & DT_CENTER) x=l+(r-l-tw)/2;
    if(flags & DT_BOTTOM) y=b-th;
    else if(flags & DT_VCENTER) y=t+(b-t-th)/2;
    if(x < -32768L || x>32767 || y < -32768L || y>32767) return 0;
    gdi_draw_text(d,(int)x,(int)y,text,n);
    return th;
}
