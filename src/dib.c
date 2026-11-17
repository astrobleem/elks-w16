#include "dib.h"

static u16 word(const u8 *p) { return (u16)(p[0] | ((u16)p[1] << 8)); }
static u32 dword(const u8 *p) { return word(p) | ((u32)word(p+2) << 16); }
/* Validated positive signed-WORD width and depth 1 or 4: result <=16384. */
static u16 row_stride(u16 width, u16 bpp)
{ return (u16)((((width-1) >> (bpp==1 ? 5 : 3))+1) << 2); }

int dib_open(struct dib_info *out, dib_reader read, void *ctx, u32 length)
{
    struct dib_info d;
    u8 h[40], rgb[4];
    u32 w, height, used=0;
    u16 header, entry, bpp, i;
    if (!out || !read || length<12 || !read(ctx,0,h,12)) return 0;
    if (dword(h)==12) {
        header=12; entry=3; w=word(h+4); height=word(h+6);
        if (word(h+8)!=1) return 0;
        bpp=word(h+10);
    } else {
        if (dword(h)!=40 || length<40 || !read(ctx,12,h+12,28)) return 0;
        header=40; entry=4; w=dword(h+4); height=dword(h+8);
        if (word(h+12)!=1 || dword(h+16)!=0) return 0;
        bpp=word(h+14); used=dword(h+32);
    }
    if (!w || w>32767 || !height || height>32767 ||
        (bpp!=1 && bpp!=4) || used>(1u<<bpp)) return 0;
    d.width=(u16)w; d.height=(u16)height; d.bpp=bpp;
    d.colors=used ? (u16)used : (1u<<bpp);
    if (header==40 && dword(h+36)>d.colors) return 0;
    d.stride=row_stride(d.width,bpp);
    d.pixels=header+d.colors*entry; d.length=length;
    if (d.pixels>length || d.height>(length-d.pixels)/d.stride) return 0;
    /* Some Windows resources have stale biSizeImage; the payload bounds govern. */
    for (i=0;i<d.colors;i++) {
        if (!read(ctx,(u16)(header+i*entry),rgb,entry)) return 0;
        d.palette[i].r=rgb[2]; d.palette[i].g=rgb[1]; d.palette[i].b=rgb[0];
    }
    for (;i<16;i++) d.palette[i].r=d.palette[i].g=d.palette[i].b=0;
    *out=d;
    return 1;
}

int dib_read_indices(const struct dib_info *d, dib_reader read, void *ctx,
                     u16 x,u16 y,u8 *out,u16 count)
{
    u8 packed[32];
    u16 shift, perbyte, phase, n, bytes, i, index, mask;
    u32 row;
    if (!d || !read || !out || !d->width || d->width>32767 ||
        !d->height || d->height>32767 || (d->bpp!=1 && d->bpp!=4) ||
        !d->colors || d->colors>(1u<<d->bpp) || y>=d->height ||
        x>d->width || count>d->width-x ||
        d->stride!=row_stride(d->width,d->bpp) || d->pixels>d->length ||
        d->height>(d->length-d->pixels)/d->stride) return 0;
    row=d->pixels+(u32)(d->height-1-y)*d->stride;
    shift=d->bpp==1 ? 3 : 1; perbyte=1u<<shift; mask=(1u<<d->bpp)-1;
    while (count) {
        phase=x&(perbyte-1); n=count;
        if (n>32*perbyte-phase) n=32*perbyte-phase;
        bytes=(n+phase+perbyte-1)>>shift;
        if (!read(ctx,row+(x>>shift),packed,bytes)) return 0;
        for (i=0;i<n;i++) {
            index=i+phase;
            index=(packed[index>>shift] >>
                (8-d->bpp-(index&(perbyte-1))*d->bpp)) & mask;
            if (index>=d->colors) return 0;
            out[i]=(u8)index;
        }
        out+=n; x+=n; count-=n;
    }
    return 1;
}
