#ifndef W16_DIB_H
#define W16_DIB_H
#include "w16.h"
struct dib_rgb { u8 r, g, b; };
struct dib_info {
    u16 width, height, colors, bpp;
    u32 stride, pixels, length;
    struct dib_rgb palette[16];
};
/* Return nonzero only after reading all n bytes. */
typedef int (*dib_reader)(void *ctx, u32 off, void *dst, u16 n);
int dib_open(struct dib_info *d, dib_reader read, void *ctx, u32 length);
/* Icon/cursor DIBs store twice the visible height; XOR data precedes a
 * monochrome AND plane. No hotspot prefix is included in this view. */
int dib_icon_open(struct dib_info *d,dib_reader read,void *ctx,u32 length,u32 *and_pixels);
struct dib_memory {const u8 *data;u16 length;};
int dib_read_memory(void *ctx,u32 off,void *dst,u16 count);
/* Coordinates are top-down. Failure may leave a prefix of out modified. */
int dib_read_indices(const struct dib_info *d, dib_reader read, void *ctx,
                     u16 x, u16 y, u8 *out, u16 count);
#endif
