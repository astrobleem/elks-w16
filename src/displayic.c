/* Information contexts expose display capabilities without granting a drawable.
 * Handles deliberately don't fall into dc_get()/memory-DC ranges. */
#include "w16.h"
#define IC_BASE 0x0240
#define IC_COUNT 8
static u8 live[IC_COUNT];

u32 g_CreateIC(u16 ss,u16 ap)
{
    static const char display[]="DISPLAY";
    u16 seg,off,i; u8 c;
    if(ap>0xfff0u || AL(0) || AL(4) || AL(8)) return 0;
    off=AW(12);seg=AW(14);
    if(!seg || off>0xfff8u || ((u32)seg<<4)+off>0xffff8UL) return 0;
    for(i=0;i<sizeof(display);i++) {
        c=rd8(seg,off+i); if(c>='a' && c<='z') c-=32;
        if(c!=display[i]) return 0;
    }
    for(i=0;i<IC_COUNT;i++) if(!live[i]) {
        live[i]=1; return IC_BASE+2*i;
    }
    return 0;
}

/* Outside the namespace => -1; consumed inactive handle => zero. */
int ic_handle(u16 h,int release)
{
    u16 i;
    if(h<IC_BASE || h>=IC_BASE+2*IC_COUNT) return -1;
    i=(h-IC_BASE)/2;
    if((h&1) || !live[i]) return 0;
    if(release) live[i]=0;
    return 1;
}
