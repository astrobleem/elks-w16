/* Win16 display-device pen/brush enumeration. The palette snapshot belongs to
 * this invocation; callbacks can reenter GDI and overwrite its shared scratch. */
#include "w16.h"

u32 enum_palette_objects(u16 kind,u16 proc_off,u16 proc_seg,u32 data,
                         const u32 *colors,u16 count)
{
    u16 save=w16_app_sp,ss=w16_app_ss,base,words[4],i,result=0;
    if (!w16_mod || !proc_seg || (kind!=1 && kind!=2) || !count ||
        save<48 || ((u32)proc_seg<<4)+proc_off>0xfffffUL)
        return 0;
    /* Reserve a ten-byte LOGPEN (LOGBRUSH needs eight) above the callback
     * frame. Nested enumerations get their own guest stack storage. */
    base=save-10;
    words[0]=ss; words[1]=base; words[2]=(u16)(data>>16); words[3]=(u16)data;
    w16_app_sp=base;
    for (i=0;i<count;i++) {
        wr16(ss,base,0); /* PS_SOLID or BS_SOLID */
        if(kind==1) {
            wr16(ss,base+2,1); wr16(ss,base+4,0); /* one device pixel wide */
            wr32(ss,base+6,colors[i]);
        } else {
            wr32(ss,base+2,colors[i]); wr16(ss,base+6,0);
        }
        result=(u16)w16_call16(proc_off,proc_seg,w16_mod->dgroup,4,words);
        if(!result) break;
    }
    w16_app_sp=save;
    return result;
}
