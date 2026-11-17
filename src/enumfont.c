/* EnumFonts exposes the one raster face actually drawn by this runtime.
 * Callback descriptors live on this invocation's guest stack, not scratch. */
#include "w16.h"
u32 enum_system_font(u16 faceoff,u16 faceseg,u16 procoff,u16 procseg,u32 data,
                     u16 height,u16 ascent,u16 average,u16 maximum)
{
    static const char face[]="System";
    u16 save=w16_app_sp,ss=w16_app_ss,base,metric,words[7],i,result;
    if(!w16_mod || !procseg || save<128 || ascent>height ||
       ((u32)procseg<<4)+procoff>0xfffffUL) return 0;
    if(faceoff||faceseg) {
        for(i=0;i<sizeof(face);i++) {
            u8 c;
            if((u32)faceoff+i>0xffffUL || ((u32)faceseg<<4)+faceoff+i>0xfffffUL) return 0;
            c=rd8(faceseg,faceoff+i);
            if(c>='A'&&c<='Z')c+=32;
            if(c!=(u8)(i==0?'s':face[i])) return 0;
        }
    }
    base=save-82;metric=base+50;
    fzero(ss,base,82);
    wr16(ss,base,height);wr16(ss,base+2,average);wr16(ss,base+8,400);
    wr8(ss,base+16,1); /* DRAFT_QUALITY */
    wr8(ss,base+17,0x22); /* VARIABLE_PITCH | FF_SWISS */
    for(i=0;i<sizeof(face);i++)wr8(ss,base+18+i,face[i]);
    wr16(ss,metric,height);wr16(ss,metric+2,ascent);wr16(ss,metric+4,height-ascent);
    wr16(ss,metric+10,average);wr16(ss,metric+12,maximum);wr16(ss,metric+14,400);
    wr8(ss,metric+19,32);wr8(ss,metric+20,255);wr8(ss,metric+21,'?');
    wr8(ss,metric+22,' ');wr8(ss,metric+23,0x21);
    wr16(ss,metric+27,100);wr16(ss,metric+29,100);
    words[0]=ss;words[1]=base;words[2]=ss;words[3]=metric;words[4]=1;
    words[5]=(u16)(data>>16);words[6]=(u16)data;
    w16_app_sp=base;
    result=(u16)w16_call16(procoff,procseg,w16_mod->dgroup,7,words);
    w16_app_sp=save;
    return result;
}
