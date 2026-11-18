/* Exact original Nano-X system glyphs, packed to preserve the near heap. */
#include "wapi.h"
#include "rasterfont-data.h"
u8 rasterfont_width(u8 c)
{
 return font_width[c<32?0:c-32];
}
u16 rasterfont_row(u8 c,int row)
{
 unsigned pos,at;u32 bits;
 if(c<32)c=32;
 pos=font_offset[c-32]+font_width[c-32]*row;at=pos>>3;
 bits=((u32)font_packed[at]<<16)|((u16)font_packed[at+1]<<8)|font_packed[at+2];
 return (u16)(bits>>(8-(pos&7)));
}
