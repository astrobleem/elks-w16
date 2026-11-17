/* Win16 LineDDA: Pascal callback(x,y,LPARAM), start included/end excluded.
 * Integer error accumulation avoids float state and signed-16 subtraction overflow.
 * All iteration/callback state is local, so a callback can reenter the runtime.
 * Win16 declares this API void; the dispatcher return value is not an API result. */
#include "w16.h"
u32 g_LineDDA(u16 ss,u16 ap)
{
    i32 x,y,xe,ye,dx,dy,major,minor,error,i;
    i16 sx,sy;
    u16 off,seg,ds,words[4];
    int xmajor;
    if(ap>0xfff0u || !w16_mod) return 0;
    off=AW(4);seg=AW(6);
    if(!seg || ((u32)seg<<4)+off>0xfffffUL) return 0;
    words[2]=AW(2);words[3]=AW(0);
    x=(i16)AW(14);y=(i16)AW(12);xe=(i16)AW(10);ye=(i16)AW(8);
    ds=w16_mod->dgroup;
    dx=xe-x;dy=ye-y;sx=dx<0?-1:1;sy=dy<0?-1:1;
    if(dx<0)dx=-dx;
    if(dy<0)dy=-dy;
    xmajor=dx>=dy;major=xmajor?dx:dy;minor=xmajor?dy:dx;
    error=major/2;
    for(i=0;i<major;i++) {
        words[0]=(u16)x;words[1]=(u16)y;
        (void)w16_call16(off,seg,ds,4,words);
        error-=minor;
        if(error<0) { if(xmajor)y+=sy;else x+=sx;error+=major; }
        if(xmajor)x+=sx;else y+=sy;
    }
    return 0;
}
