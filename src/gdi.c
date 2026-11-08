/*
 * gdi.c - GDI: stock and created objects, device contexts and drawing,
 * mapped onto Nano-X graphics contexts. Coordinates are MM_TEXT client pixels.
 */
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linuxmt/mem.h>
#include "wapi.h"

#define OBJ_PEN   1
#define OBJ_BRUSH 2
#define OBJ_FONT  3
#define MAXOBJ    24
#define NSTOCK    17

struct gobj { u8 type, used, null; u16 style, width; u32 color; };
static struct gobj stock[NSTOCK];
static struct gobj objs[MAXOBJ];
static struct dc dcs[MAXDC];
GR_GC_ID gdi_gc;

#define PS_NULL 5
#define OPAQUE 2
#define TRANSPARENT 1

GR_COLOR rgb_to_gr(u32 c)
{
    return GR_RGB((u8)c, (u8)(c >> 8), (u8)(c >> 16));
}

static struct gobj *obj_get(u16 h)
{
    if (h >= HSTOCK && h < HSTOCK + 2 * NSTOCK && !(h & 1))
        return &stock[(h - HSTOCK) >> 1];
    if (h >= HOBJ_BASE && h < HOBJ_BASE + 2 * MAXOBJ && !(h & 1) && objs[(h - HOBJ_BASE) >> 1].used)
        return &objs[(h - HOBJ_BASE) >> 1];
    return 0;
}

/* Windows 3.0 default system colours for hbrBackground = COLOR_xxx + 1 */
static u32 sys_color(u16 idx)
{
    switch (idx) {
    case 0:  return 0xC0C0C0ul;   /* COLOR_SCROLLBAR */
    case 1:  return 0x808000ul;   /* COLOR_BACKGROUND */
    case 2:  return 0x800000ul;   /* COLOR_ACTIVECAPTION */
    case 4:  return 0xFFFFFFul;   /* COLOR_MENU */
    case 5:  return 0xFFFFFFul;   /* COLOR_WINDOW */
    case 6:  return 0x000000ul;   /* COLOR_WINDOWFRAME */
    case 12: return 0xFFFFFFul;   /* COLOR_APPWORKSPACE */
    }
    return 0xFFFFFFul;
}

u32 brush_color(u16 hbrush, int *isnull)
{
    struct gobj *o;
    *isnull = 0;
    if (hbrush >= 1 && hbrush <= 21)
        return sys_color(hbrush - 1);
    o = obj_get(hbrush);
    if (!o || o->type != OBJ_BRUSH) {
        *isnull = 1;
        return 0;
    }
    *isnull = o->null;
    return o->color;
}

void gdi_fill(GR_WINDOW_ID wid, GR_COLOR c, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    GrSetGCForeground(gdi_gc, c);
    GrFillRect(wid, gdi_gc, x, y, w, h);
}

int gdi_init(void)
{
    static const u32 brushes[5] = { 0xFFFFFFul, 0xC0C0C0ul, 0x808080ul, 0x404040ul, 0x000000ul };
    int i;
    memset(stock, 0, sizeof(stock));
    memset(objs, 0, sizeof(objs));
    memset(dcs, 0, sizeof(dcs));
    for (i = 0; i < 5; i++) {               /* WHITE, LTGRAY, GRAY, DKGRAY, BLACK brushes */
        stock[i].type = OBJ_BRUSH; stock[i].color = brushes[i]; stock[i].used = 1;
    }
    stock[5].type = OBJ_BRUSH; stock[5].null = 1; stock[5].used = 1;     /* NULL_BRUSH */
    stock[6].type = OBJ_PEN; stock[6].color = 0xFFFFFFul; stock[6].used = 1;   /* WHITE_PEN */
    stock[7].type = OBJ_PEN; stock[7].color = 0; stock[7].used = 1;            /* BLACK_PEN */
    stock[8].type = OBJ_PEN; stock[8].null = 1; stock[8].used = 1;             /* NULL_PEN */
    for (i = 10; i <= 16; i++)              /* fonts: all map to the Nano-X system font */
        if (i != 15) { stock[i].type = OBJ_FONT; stock[i].used = 1; }
    gdi_gc = GrNewGC();
    return 0;
}

void gdi_shutdown(void)
{
    int i;
    for (i = 0; i < MAXDC; i++)
        if (dcs[i].used) {
            GrDestroyGC(dcs[i].gc);
            dcs[i].used = 0;
        }
    if (gdi_gc)
        GrDestroyGC(gdi_gc);
    gdi_gc = 0;
}

struct dc *dc_get(u16 hdc)
{
    u16 i = (u16)(hdc - HDC_BASE) >> 1;
    if (hdc < HDC_BASE || (hdc & 1) || i >= MAXDC || !dcs[i].used)
        return 0;
    return &dcs[i];
}

u16 dc_create(struct wnd *w, int paint)
{
    int i;
    struct dc *d;
    for (i = 0; i < MAXDC && dcs[i].used; i++)
        ;
    if (i == MAXDC) {
        wlog("w16: out of device contexts (%d)\n", MAXDC);
        return 0;
    }
    d = &dcs[i];
    memset(d, 0, sizeof(*d));
    d->used = 1; d->paint = paint; d->hdc = HDC_BASE + 2 * i; d->w = w; d->wid = w->client;
    d->gc = GrNewGC();
    d->pen = HSTOCK + 2 * 7;        /* BLACK_PEN */
    d->brush = HSTOCK + 2 * 0;      /* WHITE_BRUSH */
    d->font = HSTOCK + 2 * 13;      /* SYSTEM_FONT */
    d->textcolor = 0; d->bkcolor = 0xFFFFFFul; d->bkmode = OPAQUE;
    return d->hdc;
}

void dc_release(u16 hdc)
{
    struct dc *d = dc_get(hdc);
    if (!d)
        return;
    GrDestroyGC(d->gc);
    d->used = 0;
}

/* ---------------------------------------------------------------- objects */
static u16 obj_new(u8 type, u32 color, u16 style, u16 width)
{
    int i;
    for (i = 0; i < MAXOBJ && objs[i].used; i++)
        ;
    if (i == MAXOBJ) {
        wlog("w16: GDI object table full (%d)\n", MAXOBJ);
        return 0;
    }
    objs[i].used = 1; objs[i].type = type; objs[i].color = color & 0xFFFFFFul;
    objs[i].style = style; objs[i].width = width; objs[i].null = type == OBJ_PEN && style == PS_NULL;
    return HOBJ_BASE + 2 * i;
}

/* GetStockObject(fnObject) */
u32 g_GetStockObject(u16 ss, u16 ap)
{
    u16 i = AW(0);
    if (i >= NSTOCK || !stock[i].used)
        return 0;
    return HSTOCK + 2 * i;
}

/* CreateSolidBrush(crColor) */
u32 g_CreateSolidBrush(u16 ss, u16 ap) { return obj_new(OBJ_BRUSH, AL(0), 0, 0); }

/* CreatePen(fnPenStyle, nWidth, crColor) */
u32 g_CreatePen(u16 ss, u16 ap)
{
    u16 style = AW(6), width = AW(4);
    if (style != 0 && style != PS_NULL)
        vlog("w16: CreatePen style %u drawn as a solid line\n", style);
    return obj_new(OBJ_PEN, AL(0), style, width);
}

/* DeleteObject(hObject) */
u32 g_DeleteObject(u16 ss, u16 ap)
{
    u16 h = AW(0), i;
    struct gobj *o = obj_get(h);
    if (!o)
        return 0;
    if (h < HOBJ_BASE)
        return 1;                       /* stock objects are never deleted */
    for (i = 0; i < MAXDC; i++)
        if (dcs[i].used && (dcs[i].pen == h || dcs[i].brush == h)) {
            vlog("w16: DeleteObject %04x refused: still selected\n", h);
            return 0;
        }
    o->used = 0;
    return 1;
}

/* SelectObject(hdc, hObject) */
u32 g_SelectObject(u16 ss, u16 ap)
{
    u16 h = AW(0), prev;
    struct dc *d = dc_get(AW(2));
    struct gobj *o = obj_get(h);
    if (!d || !o)
        return 0;
    switch (o->type) {
    case OBJ_PEN:   prev = d->pen;   d->pen = h;   return prev;
    case OBJ_BRUSH: prev = d->brush; d->brush = h; return prev;
    case OBJ_FONT:  prev = d->font;  d->font = h;  return prev;
    }
    return 0;
}

/* SetTextColor(hdc, crColor) / SetBkColor(hdc, crColor) / SetBkMode(hdc, iBkMode) */
u32 g_SetTextColor(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(4));
    u32 prev;
    if (!d) return 0x80000000ul;
    prev = d->textcolor; d->textcolor = AL(0) & 0xFFFFFFul;
    return prev;
}

u32 g_SetBkColor(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(4));
    u32 prev;
    if (!d) return 0x80000000ul;
    prev = d->bkcolor; d->bkcolor = AL(0) & 0xFFFFFFul;
    return prev;
}

u32 g_SetBkMode(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(2));
    u16 prev;
    if (!d) return 0;
    prev = d->bkmode; d->bkmode = AW(0);
    return prev;
}

/* ---------------------------------------------------------------- drawing */
static int pen_of(struct dc *d, GR_COLOR *c)
{
    struct gobj *o = obj_get(d->pen);
    if (!o || o->null)
        return 0;
    *c = rgb_to_gr(o->color);
    return 1;
}

static int brush_of(struct dc *d, GR_COLOR *c)
{
    struct gobj *o = obj_get(d->brush);
    if (!o || o->null)
        return 0;
    *c = rgb_to_gr(o->color);
    return 1;
}

/* Rectangle(hdc, left, top, right, bottom): right/bottom exclusive */
u32 g_Rectangle(u16 ss, u16 ap)
{
    int b = (i16)AW(0), r = (i16)AW(2), t = (i16)AW(4), l = (i16)AW(6);
    struct dc *d = dc_get(AW(8));
    GR_COLOR c;
    if (!d || r <= l || b <= t)
        return 0;
    if (brush_of(d, &c)) {
        GrSetGCForeground(d->gc, c);
        GrFillRect(d->wid, d->gc, l, t, r - l, b - t);
    }
    if (pen_of(d, &c)) {
        GrSetGCForeground(d->gc, c);
        GrRect(d->wid, d->gc, l, t, r - l, b - t);
    }
    return 1;
}

/* Ellipse(hdc, left, top, right, bottom) */
u32 g_Ellipse(u16 ss, u16 ap)
{
    int b = (i16)AW(0), r = (i16)AW(2), t = (i16)AW(4), l = (i16)AW(6);
    struct dc *d = dc_get(AW(8));
    int rx = (r - l - 1) / 2, ry = (b - t - 1) / 2;
    GR_COLOR c;
    if (!d || rx < 0 || ry < 0)
        return 0;
    if (brush_of(d, &c)) {
        GrSetGCForeground(d->gc, c);
        GrFillEllipse(d->wid, d->gc, l + rx, t + ry, rx, ry);
    }
    if (pen_of(d, &c)) {
        GrSetGCForeground(d->gc, c);
        GrEllipse(d->wid, d->gc, l + rx, t + ry, rx, ry);
    }
    return 1;
}

/* MoveTo(hdc, x, y): returns the previous position, y in the high word */
u32 g_MoveTo(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(4));
    u32 prev;
    if (!d)
        return 0;
    prev = ((u32)(u16)d->cury << 16) | (u16)d->curx;
    d->curx = (i16)AW(2); d->cury = (i16)AW(0);
    return prev;
}

/* LineTo(hdc, x, y) */
u32 g_LineTo(u16 ss, u16 ap)
{
    int y = (i16)AW(0), x = (i16)AW(2);
    struct dc *d = dc_get(AW(4));
    GR_COLOR c;
    if (!d)
        return 0;
    if (pen_of(d, &c)) {
        GrSetGCForeground(d->gc, c);
        GrLine(d->wid, d->gc, d->curx, d->cury, x, y);
    }
    d->curx = x; d->cury = y;
    return 1;
}

/* TextOut(hdc, x, y, lpString, nCount) */
u32 g_TextOut(u16 ss, u16 ap)
{
    int n = (i16)AW(0), y = (i16)AW(6), x = (i16)AW(8);
    u16 off = AW(2), seg = AW(4);
    struct dc *d = dc_get(AW(10));
    char buf[128];
    if (!d || n < 0)
        return 0;
    if (n > (int)sizeof(buf))
        n = sizeof(buf);
    fcopy_in(buf, seg, off, n);
    GrSetGCForeground(d->gc, rgb_to_gr(d->textcolor));
    GrSetGCBackground(d->gc, rgb_to_gr(d->bkcolor));
    GrSetGCUseBackground(d->gc, d->bkmode == OPAQUE ? GR_TRUE : GR_FALSE);
    GrText(d->wid, d->gc, x, y, buf, n, GR_TFASCII | GR_TFTOP);
    return 1;
}

/* ---------------------------------------------------------------- frame buffer dump (-d) */
int w16_dump_frame(const char *tag)
{
    static unsigned seg;
    char name[64], buf[512];
    unsigned off;
    int fd;
    if (!seg) {
        int k = open("/dev/kmem", O_RDONLY);
        unsigned r = 0;
        if (k >= 0) {
            if (ioctl(k, MEM_GETTANDY, &r) < 0)
                r = 0;
            close(k);
        }
        seg = r ? r : 0xB800;
    }
    sprintf(name, "%s%s.bin", opt_dump, tag);
    fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return -1;
    for (off = 0; off < 0x8000; off += sizeof(buf)) {
        fcopy_in(buf, seg, off, sizeof(buf));
        write(fd, buf, sizeof(buf));
    }
    close(fd);
    vlog("w16: frame buffer %04x:0000 (32768 bytes) written to %s\n", seg, name);
    if (opt_debugport) {
        char line[80];
        sprintf(line, "@@DUMP %s\n", name);
        e9puts(line);
    }
    return 0;
}
