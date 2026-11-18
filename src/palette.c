/* Logical palettes own copied PALETTEENTRY data. The VGA device palette is
 * fixed; realization selects a mapping, never rewrites hardware colors. */
#include <stdlib.h>
#include <string.h>
#include "w16.h"
#define PALETTE_BASE 0x0e00
#define PALETTE_COUNT 8
#define DEFAULT_PALETTE 0x031e
static struct {u16 count;u8 *entries;} palettes[PALETTE_COUNT];
static const u8 default_entries[80]={
 0,0,0,0, 128,0,0,0, 0,128,0,0, 128,128,0,0,
 0,0,128,0, 128,0,128,0, 0,128,128,0, 192,192,192,0,
 192,220,192,0, 166,202,240,0, 255,251,240,0, 160,160,164,0,
 128,128,128,0, 255,0,0,0, 0,255,0,0, 255,255,0,0,
 0,0,255,0, 255,0,255,0, 0,255,255,0, 255,255,255,0};
static int palette_index(u16 h)
{
 u16 i=(h-PALETTE_BASE)/2;
 return h>=PALETTE_BASE&&!(h&1)&&i<PALETTE_COUNT&&palettes[i].entries?(int)i:-1;
}
int palette_valid(u16 h){return h==DEFAULT_PALETTE||palette_index(h)>=0;}
static const u8 *palette_data(u16 h,u16 *count)
{
 int i;if(h==DEFAULT_PALETTE){*count=20;return default_entries;}
 i=palette_index(h);if(i<0){*count=0;return 0;}*count=palettes[i].count;return palettes[i].entries;
}
/* GetObject on an HPALETTE returns its entry count as one WORD. */
int palette_object(u16 h,i16 n,u16 seg,u16 off)
{
 u16 count;
 if(!palette_data(h,&count)||n<2||!seg||(u32)off+2>65536UL||
    ((u32)seg<<4)+off+2>0x100000UL)return 0;
 wr16(seg,off,count);return 2;
}

int palette_delete(u16 h)
{
 int i=palette_index(h);if(h==DEFAULT_PALETTE)return 1;if(i<0)return -1;
 free(palettes[i].entries);palettes[i].entries=0;palettes[i].count=0;return 1;
}
void palette_shutdown(void)
{
 unsigned i;for(i=0;i<PALETTE_COUNT;i++){free(palettes[i].entries);palettes[i].entries=0;palettes[i].count=0;}
}
u32 g_CreatePalette(u16 ss,u16 ap)
{
 u16 off=AW(0),seg=AW(2),count,n;unsigned i;u8 *data;
 if((!off&&!seg)||(u32)off+4>65536UL||((u32)seg<<4)+off+4>0x100000UL)return 0;
 count=rd16(seg,off+2);if(rd16(seg,off)!=0x300||!count||count>256)return 0;
 n=count*4;
 if((u32)off+n+4>65536UL||((u32)seg<<4)+off+n+4>0x100000UL)return 0;
 for(i=0;i<PALETTE_COUNT&&palettes[i].entries;i++);
 if(i==PALETTE_COUNT)return 0;
 data=malloc(n);if(!data)return 0;
 fcopy_in(data,seg,off+4,n);palettes[i].entries=data;palettes[i].count=count;return PALETTE_BASE+2*i;
}
u32 g_GetPaletteEntries(u16 ss,u16 ap)
{
 u16 off=AW(0),seg=AW(2),n=AW(4),start=AW(6),count;const u8 *data=palette_data(AW(8),&count);
 if(!data)return 0;
 if(!off&&!seg)return count;
 if(start>=count)return 0;
 if(n>count-start)n=count-start;
 if((u32)off+(u32)n*4>65536UL||((u32)seg<<4)+off+(u32)n*4>0x100000UL)return 0;
 fcopy_out(seg,off,data+start*4,n*4);return n;
}
/* Explicit palette entries refer to the fixed device palette, not RGB bytes. */
u16 palette_nearest(u16 handle,u32 rgb,const u32 *device,u16 device_count)
{
 u16 count,i,best=0;const u8 *data=palette_data(handle,&count);u32 distance=0xffffffffUL;
 if(!data)return 0xffff;
 for(i=0;i<count;i++){
  long dr,dg,db;u32 c,d;const u8 *e=data+i*4;
  if(e[3]&2){u16 index=e[0]|((u16)e[1]<<8);if(index>=device_count)continue;c=device[index];}
  else c=(u32)e[0]|((u32)e[1]<<8)|((u32)e[2]<<16);
  dr=(long)(rgb&255)-(c&255);dg=(long)((rgb>>8)&255)-((c>>8)&255);db=(long)((rgb>>16)&255)-((c>>16)&255);
  d=dr*dr+dg*dg+db*db;if(d<distance){distance=d;best=i;}
 }
 return distance==0xffffffffUL?0xffff:best;
}

/* Resolve a DIB_PAL_COLORS WORD through the selected logical palette. */
int palette_entry(u16 h,u16 index,u8 *out)
{
 u16 count;const u8 *data=palette_data(h,&count);
 if(!data||index>=count)return 0;
 memcpy(out,data+index*4,4);return 1;
}
