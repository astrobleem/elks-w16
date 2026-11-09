/*
 * gdi.c - GDI: stock and created objects, device contexts and drawing,
 * mapped onto Nano-X graphics contexts. Coordinates are MM_TEXT client pixels.
 */
#include <string.h>
#include <stdlib.h>
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

struct gobj { u8 type, used, null; u16 style, width; u32 color; };   /* fonts: style = height, width = weight */
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

/* Windows 3.0 default system colours (COLORREF 0x00BBGGRR), also for hbrBackground = COLOR_xxx + 1 */
static const u32 syscolors[19] = {
    0xC0C0C0ul,   /* COLOR_SCROLLBAR */          0x808000ul,   /* COLOR_BACKGROUND */
    0x800000ul,   /* COLOR_ACTIVECAPTION */      0xFFFFFFul,   /* COLOR_INACTIVECAPTION */
    0xFFFFFFul,   /* COLOR_MENU */               0xFFFFFFul,   /* COLOR_WINDOW */
    0x000000ul,   /* COLOR_WINDOWFRAME */        0x000000ul,   /* COLOR_MENUTEXT */
    0x000000ul,   /* COLOR_WINDOWTEXT */         0xFFFFFFul,   /* COLOR_CAPTIONTEXT */
    0xC0C0C0ul,   /* COLOR_ACTIVEBORDER */       0xC0C0C0ul,   /* COLOR_INACTIVEBORDER */
    0xFFFFFFul,   /* COLOR_APPWORKSPACE */       0x800000ul,   /* COLOR_HIGHLIGHT */
    0xFFFFFFul,   /* COLOR_HIGHLIGHTTEXT */      0xC0C0C0ul,   /* COLOR_BTNFACE */
    0x808080ul,   /* COLOR_BTNSHADOW */          0x808080ul,   /* COLOR_GRAYTEXT */
    0x000000ul    /* COLOR_BTNTEXT */
};

u32 gdi_sys_color(u16 idx)
{
    return idx < 19 ? syscolors[idx] : 0xFFFFFFul;
}
#define sys_color gdi_sys_color

/* cached metrics of the Nano-X system font, which draws all text here */
static int fm_h = 13, fm_asc = 10, fm_avg = 6, fm_max = 11;

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

/* GC state as last sent to the server. On ELKS every Nano-X request costs the server a select()
 * and two read() calls, so only changes are sent. One GC per DC slot is kept for the life of the
 * program (GetDC/ReleaseDC then need no round trip); the last one is USER's scratch GC.
 * W16_NOCACHE=1 in the environment sends every state change and a new GC per DC, to measure. */
struct gcstate { GR_GC_ID gc; u8 known, usebg; int mode; GR_COLOR fg, bg; };
#define K_MODE  1
#define K_FG    2
#define K_BG    4
#define K_USEBG 8
static struct gcstate gcs[MAXDC + 1];
static u8 nocache;
#define GS(d) (&gcs[((d)->hdc - HDC_BASE) >> 1])

static void gs_mode(struct gcstate *g, int mode)
{
    if (nocache || !(g->known & K_MODE) || g->mode != mode) {
        GrSetGCMode(g->gc, mode);
        g->mode = mode;
        g->known |= K_MODE;
    }
}

static void gs_fg(struct gcstate *g, GR_COLOR c)
{
    if (nocache || !(g->known & K_FG) || g->fg != c) {
        GrSetGCForeground(g->gc, c);
        g->fg = c;
        g->known |= K_FG;
    }
}

static void gs_bg(struct gcstate *g, GR_COLOR c)
{
    if (nocache || !(g->known & K_BG) || g->bg != c) {
        GrSetGCBackground(g->gc, c);
        g->bg = c;
        g->known |= K_BG;
    }
}

static void gs_usebg(struct gcstate *g, int on)
{
    if (nocache || !(g->known & K_USEBG) || g->usebg != (u8)on) {
        GrSetGCUseBackground(g->gc, on ? GR_TRUE : GR_FALSE);
        g->usebg = (u8)on;
        g->known |= K_USEBG;
    }
}

/* USER's drawing (frames, menus, controls) goes through the scratch GC, always in copy mode */
void gdi_fg(GR_COLOR c)  { gs_fg(&gcs[MAXDC], c); }
void gdi_usebg(int on)   { gs_usebg(&gcs[MAXDC], on); }

void gdi_fill(GR_WINDOW_ID wid, GR_COLOR c, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    gdi_fg(c);
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
    memset(gcs, 0, sizeof(gcs));
    gcs[MAXDC].gc = gdi_gc;
    {
        const char *e = getenv("W16_NOCACHE");
        nocache = e && *e == '1';
        if (nocache)
            wlog("w16: W16_NOCACHE: every GC state change is sent and every DC gets a new GC\n");
    }
    {
        GR_SIZE w, h, b;
        GrGetGCTextSize(gdi_gc, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, GR_TFASCII, &w, &h, &b);
        if (h > 4 && h < 40) {
            fm_h = h;
            fm_asc = b;
            fm_avg = (w + 26) / 52;
        }
        GrGetGCTextSize(gdi_gc, "W", 1, GR_TFASCII, &w, &h, &b);
        if (w > 0)
            fm_max = w;
    }
    return 0;
}

void gdi_census(u16 *ndc, u16 *nobj)
{
    int i;
    *ndc = *nobj = 0;
    for (i = 0; i < MAXDC; i++)
        if (dcs[i].used)
            (*ndc)++;
    for (i = 0; i < MAXOBJ; i++)
        if (objs[i].used)
            (*nobj)++;
}

void gdi_shutdown(void)
{
    int i;
    for (i = 0; i < MAXDC; i++) {
        dcs[i].used = 0;
        if (gcs[i].gc)
            GrDestroyGC(gcs[i].gc);
        gcs[i].gc = 0;
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
    d->used = 1; d->paint = paint; d->hdc = HDC_BASE + 2 * i; d->w = w;
    d->wid = w ? w->client : GR_ROOT_WINDOW_ID;    /* no window: the whole screen */
    if (!gcs[i].gc || nocache) {
        gcs[i].gc = GrNewGC();                     /* a round trip: done once per slot */
        gcs[i].known = 0;
    }
    d->gc = gcs[i].gc;
    d->rop2 = R2_COPYPEN;
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
    if (nocache) {
        GrDestroyGC(d->gc);
        GS(d)->gc = 0;
    }
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
        if (dcs[i].used && (dcs[i].pen == h || dcs[i].brush == h || dcs[i].font == h)) {
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

/* Set the GC for pen or brush output in colour c under the DC's binary raster op (SetROP2).
 * Returns 0 when the op leaves the screen alone, 2 when it combines with the screen (XOR-like). */
static int set_rop(struct dc *d, GR_COLOR c)
{
    int mode = GR_MODE_COPY, r = 1;
    switch (d->rop2) {
    case R2_BLACK:       c = GR_RGB(0, 0, 0); break;
    case R2_WHITE:       c = GR_RGB(255, 255, 255); break;
    case R2_NOTCOPYPEN:  mode = GR_MODE_COPYINVERTED; break;
    case R2_NOT:         mode = GR_MODE_INVERT; r = 2; break;
    case R2_XORPEN:      mode = GR_MODE_XOR; r = 2; break;
    case R2_NOTXORPEN:   mode = GR_MODE_EQUIV; r = 2; break;
    case R2_MERGEPEN:    mode = GR_MODE_OR; break;
    case R2_MASKPEN:     mode = GR_MODE_AND; break;
    case R2_NOTMERGEPEN: mode = GR_MODE_NOR; break;
    case R2_NOTMASKPEN:  mode = GR_MODE_NAND; break;
    case R2_MERGEPENNOT: mode = GR_MODE_ORREVERSE; break;
    case R2_MASKPENNOT:  mode = GR_MODE_ANDREVERSE; break;
    case R2_MERGENOTPEN: mode = GR_MODE_ORINVERTED; break;
    case R2_MASKNOTPEN:  mode = GR_MODE_ANDINVERTED; break;
    case R2_NOP:         return 0;
    }
    gs_mode(GS(d), mode);
    gs_fg(GS(d), c);
    return r;
}

/* SetROP2(hdc, nDrawMode) */
u32 g_SetROP2(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(2));
    u16 prev, r = AW(0);
    if (!d)
        return 0;
    prev = d->rop2;
    if (r >= 1 && r <= 16)
        d->rop2 = (u8)r;
    return prev;
}

/* Rectangle(hdc, left, top, right, bottom): right/bottom exclusive */
u32 g_Rectangle(u16 ss, u16 ap)
{
    int b = (i16)AW(0), r = (i16)AW(2), t = (i16)AW(4), l = (i16)AW(6);
    struct dc *d = dc_get(AW(8));
    GR_COLOR pc, bc;
    int pen;
    if (!d || r <= l || b <= t)
        return 0;
    pen = pen_of(d, &pc);
    if (pen && d->rop2 == R2_COPYPEN && brush_of(d, &bc) && bc == pc && !nocache) {
        set_rop(d, bc);                         /* outline and inside alike: one request */
        GrFillRect(d->wid, d->gc, l, t, r - l, b - t);
        return 1;
    }
    if (brush_of(d, &bc) && set_rop(d, bc)) {
        if (!pen)
            GrFillRect(d->wid, d->gc, l, t, r - l, b - t);
        else if (r - l > 2 && b - t > 2)        /* inside the outline, so XOR modes do not hit it twice */
            GrFillRect(d->wid, d->gc, l + 1, t + 1, r - l - 2, b - t - 2);
    }
    if (pen && set_rop(d, pc))
        GrRect(d->wid, d->gc, l, t, r - l, b - t);
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
    if (brush_of(d, &c) && set_rop(d, c))
        GrFillEllipse(d->wid, d->gc, l + rx, t + ry, rx, ry);
    if (pen_of(d, &c) && set_rop(d, c))
        GrEllipse(d->wid, d->gc, l + rx, t + ry, rx, ry);
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

/* LineTo(hdc, x, y): like Windows, the end point itself is not drawn, which matters for XOR modes
 * where joined segments would otherwise invert their shared corner twice */
u32 g_LineTo(u16 ss, u16 ap)
{
    int y = (i16)AW(0), x = (i16)AW(2), k;
    struct dc *d = dc_get(AW(4));
    GR_COLOR c;
    if (!d)
        return 0;
    if (pen_of(d, &c) && (k = set_rop(d, c)) != 0) {
        GrLine(d->wid, d->gc, d->curx, d->cury, x, y);
        if (k == 2)
            GrPoint(d->wid, d->gc, x, y);       /* undo the end point */
    }
    d->curx = x; d->cury = y;
    return 1;
}

/* FillRect(hdc, lpRect, hbr): the brush's colour, regardless of the raster op */
u32 u_FillRect(u16 ss, u16 ap)
{
    u16 hbr = AW(0), off = AW(2), seg = AW(4);
    struct dc *d = dc_get(AW(6));
    int l, t, r, b, isnull;
    u32 c;
    if (!d)
        return 0;
    l = (i16)rd16(seg, off); t = (i16)rd16(seg, off + 2); r = (i16)rd16(seg, off + 4); b = (i16)rd16(seg, off + 6);
    c = brush_color(hbr, &isnull);
    if (isnull || r <= l || b <= t)
        return 1;
    gs_mode(GS(d), GR_MODE_COPY);
    gs_fg(GS(d), rgb_to_gr(c));
    GrFillRect(d->wid, d->gc, l, t, r - l, b - t);
    return 1;
}

/* ---------------------------------------------------------------- text */
#define TA_UPDATECP   1
#define TA_RIGHT      2
#define TA_CENTER     6
#define TA_BOTTOM     8
#define TA_BASELINE  24
#define ETO_OPAQUE    2
#define ETO_CLIPPED   4

static void text_out(struct dc *d, int x, int y, u16 seg, u16 off, int n, u16 opts, const int *rc)
{
    char buf[128];
    GR_SIZE tw = 0, th = fm_h, tb = fm_asc;
    if (n < 0)
        n = 0;
    if (n > (int)sizeof(buf))
        n = sizeof(buf);
    fcopy_in(buf, seg, off, (u16)n);
    gs_mode(GS(d), GR_MODE_COPY);
    if (rc && (opts & ETO_OPAQUE) && rc[2] > rc[0] && rc[3] > rc[1]) {
        gs_fg(GS(d), rgb_to_gr(d->bkcolor));
        GrFillRect(d->wid, d->gc, rc[0], rc[1], rc[2] - rc[0], rc[3] - rc[1]);
    }
    if (!n)
        return;
    if (d->textalign & (TA_CENTER | TA_UPDATECP))
        GrGetGCTextSize(d->gc, buf, n, GR_TFASCII, &tw, &th, &tb);
    if (d->textalign & TA_UPDATECP) {
        x = d->curx;
        y = d->cury;
    }
    if ((d->textalign & TA_CENTER) == TA_CENTER)
        x -= tw / 2;
    else if (d->textalign & TA_RIGHT)
        x -= tw;
    if ((d->textalign & TA_BASELINE) == TA_BASELINE)
        y -= fm_asc;
    else if (d->textalign & TA_BOTTOM)
        y -= fm_h;
    gs_fg(GS(d), rgb_to_gr(d->textcolor));
    gs_bg(GS(d), rgb_to_gr(d->bkcolor));
    gs_usebg(GS(d), d->bkmode == OPAQUE);
    GrText(d->wid, d->gc, x, y, buf, n, GR_TFASCII | GR_TFTOP);
    if (d->textalign & TA_UPDATECP)
        d->curx = (d->textalign & TA_RIGHT) ? d->curx - tw : d->curx + tw;
}

/* TextOut(hdc, x, y, lpString, nCount) */
u32 g_TextOut(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(10));
    if (!d || (i16)AW(0) < 0)
        return 0;
    text_out(d, (i16)AW(8), (i16)AW(6), AW(4), AW(2), (i16)AW(0), 0, 0);
    return 1;
}

/* ExtTextOut(hdc, x, y, wOptions, lpRect, lpString, nCount, lpDx): character spacing (lpDx) is ignored */
u32 g_ExtTextOut(u16 ss, u16 ap)
{
    u16 n = AW(4), so = AW(6), sg = AW(8), ro = AW(10), rs = AW(12), opts = AW(14);
    struct dc *d = dc_get(AW(20));
    int rc[4];
    static u8 warned;
    if (!d)
        return 0;
    if (rs || ro) {
        rc[0] = (i16)rd16(rs, ro); rc[1] = (i16)rd16(rs, ro + 2);
        rc[2] = (i16)rd16(rs, ro + 4); rc[3] = (i16)rd16(rs, ro + 6);
    }
    if ((opts & ETO_CLIPPED) && !warned) {
        warned = 1;
        vlog("w16: ExtTextOut: ETO_CLIPPED is drawn unclipped\n");
    }
    text_out(d, (i16)AW(18), (i16)AW(16), sg, so, (i16)n, opts, (rs || ro) ? rc : 0);
    return 1;
}

/* SetTextAlign(hdc, wFlags) */
u32 g_SetTextAlign(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(2));
    u16 prev;
    if (!d)
        return 0;
    prev = d->textalign;
    d->textalign = AW(0);
    return prev;
}

/* GetTextMetrics(hdc, lpMetrics): the Nano-X system font, whatever font is selected */
u32 g_GetTextMetrics(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2);
    struct dc *d = dc_get(AW(4));
    if (!d)
        return 0;
    fzero(seg, off, 31);
    wr16(seg, off + 0, fm_h);                   /* tmHeight */
    wr16(seg, off + 2, fm_asc);                 /* tmAscent */
    wr16(seg, off + 4, fm_h - fm_asc);          /* tmDescent */
    wr16(seg, off + 6, 0);                      /* tmInternalLeading */
    wr16(seg, off + 8, 0);                      /* tmExternalLeading */
    wr16(seg, off + 10, fm_avg);                /* tmAveCharWidth */
    wr16(seg, off + 12, fm_max);                /* tmMaxCharWidth */
    wr16(seg, off + 14, 400);                   /* tmWeight */
    wr8(seg, off + 19, 0x20);                   /* tmFirstChar */
    wr8(seg, off + 20, 0xFF);                   /* tmLastChar */
    wr8(seg, off + 21, '?');                    /* tmDefaultChar */
    wr8(seg, off + 22, ' ');                    /* tmBreakChar */
    wr8(seg, off + 23, 0x21);                   /* tmPitchAndFamily: variable pitch, FF_SWISS */
    wr16(seg, off + 27, 100);                   /* tmDigitizedAspectX */
    wr16(seg, off + 29, 100);                   /* tmDigitizedAspectY */
    return 1;
}

/* CreateFontIndirect(lpLogFont): recorded; text is drawn with the system font */
u32 g_CreateFontIndirect(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2);
    char face[33];
    i16 h = (i16)rd16(seg, off);
    u16 weight = rd16(seg, off + 8);
    fstrncpy_in(face, seg, off + 18, sizeof(face));
    vlog("w16: CreateFontIndirect \"%s\" height %d weight %u: drawn with the %d-pixel system font\n", face, h,
         weight, fm_h);
    return obj_new(OBJ_FONT, 0, (u16)h, weight);
}

/* AddFontResource(lpFilename) / RemoveFontResource(lpFilename): font files are not loaded */
u32 g_AddFontResource(u16 ss, u16 ap)
{
    char name[40];
    if (AW(2))
        fstrncpy_in(name, AW(2), AW(0), sizeof(name));
    else
        strcpy(name, "(module handle)");
    wlog("w16: AddFontResource(\"%s\"): font files are not supported; text uses the system font\n", name);
    return 0;
}

u32 g_RemoveFontResource(u16 ss, u16 ap) { (void)ss; (void)ap; return 0; }

/* the display's 16 colours (Tandy/EGA palette) */
static const u32 egacolors[16] = {
    0x000000ul, 0xAA0000ul, 0x00AA00ul, 0xAAAA00ul, 0x0000AAul, 0xAA00AAul, 0x0055AAul, 0xAAAAAAul,
    0x555555ul, 0xFF5555ul, 0x55FF55ul, 0xFFFF55ul, 0x5555FFul, 0xFF55FFul, 0x55FFFFul, 0xFFFFFFul
};

/* GetNearestColor(hdc, crColor) */
u32 g_GetNearestColor(u16 ss, u16 ap)
{
    u32 c = AL(0) & 0xFFFFFFul, best = 0, bd = 0xFFFFFFFFul;
    int i;
    for (i = 0; i < 16; i++) {
        long dr = (long)(c & 0xFF) - (long)(egacolors[i] & 0xFF);
        long dg = (long)((c >> 8) & 0xFF) - (long)((egacolors[i] >> 8) & 0xFF);
        long db = (long)((c >> 16) & 0xFF) - (long)((egacolors[i] >> 16) & 0xFF);
        u32 dd = (u32)(dr * dr + dg * dg + db * db);
        if (dd < bd) {
            bd = dd;
            best = egacolors[i];
        }
    }
    return best;
}

/* ---------------------------------------------------------------- device contexts for the display */
/* CreateDC(lpDriverName, lpDeviceName, lpOutput, lpInitData): the screen, for any driver name */
u32 g_CreateDC(u16 ss, u16 ap)
{
    char drv[16];
    if (AW(14))
        fstrncpy_in(drv, AW(14), AW(12), sizeof(drv));
    else
        strcpy(drv, "(null)");
    vlog("w16: CreateDC(\"%s\") -> the Nano-X screen\n", drv);
    return dc_create(0, 0);
}

u32 g_DeleteDC(u16 ss, u16 ap)
{
    if (!dc_get(AW(0)))
        return 0;
    dc_release(AW(0));
    return 1;
}

/* GetDeviceCaps(hdc, nIndex) for the 320x200 16-colour Tandy screen on a 4:3 monitor */
u32 g_GetDeviceCaps(u16 ss, u16 ap)
{
    switch (AW(0)) {
    case 0:  return 0x0300;     /* DRIVERVERSION */
    case 2:  return 1;          /* TECHNOLOGY: DT_RASDISPLAY */
    case 4:  return 240;        /* HORZSIZE, mm */
    case 6:  return 180;        /* VERTSIZE, mm */
    case 8:  return 320;        /* HORZRES */
    case 10: return 200;        /* VERTRES */
    case 12: return 4;          /* BITSPIXEL */
    case 14: return 1;          /* PLANES */
    case 16: return 16;         /* NUMBRUSHES */
    case 18: return 16;         /* NUMPENS */
    case 22: return 1;          /* NUMFONTS */
    case 24: return 16;         /* NUMCOLORS */
    case 36: return 1;          /* CLIPCAPS: rectangles */
    case 40: return 10;         /* ASPECTX: a pixel is 0.75 mm wide... */
    case 42: return 12;         /* ASPECTY: ...and 0.9 mm high */
    case 44: return 16;         /* ASPECTXY */
    case 88: return 34;         /* LOGPIXELSX */
    case 90: return 28;         /* LOGPIXELSY */
    }
    return 0;
}

/* ---------------------------------------------------------------- icons */
/* Draw a Windows 3.x DIB icon image (BITMAPINFOHEADER, palette, XOR bits, AND mask; rows bottom-up)
 * of up to 32x32 at 1 or 4 bits per pixel. Transparent pixels (AND bit set) are left alone.
 * One Nano-X GrBitmap per colour present keeps the request count small. */
int gdi_draw_icon(GR_WINDOW_ID wid, int x0, int y0, const u8 *d, u16 len)
{
    static GR_BITMAP bits[32 * 2];
    u32 hdr, w, h2, h, ncol, xs, ms, xor_off, and_off;
    u16 bpp, c, x, y;
    u8 used[16];

    if (len < 40)
        return -1;
    hdr = d[0] | ((u32)d[1] << 8);
    w = d[4] | ((u32)d[5] << 8);
    h2 = d[8] | ((u32)d[9] << 8);
    bpp = d[14] | (d[15] << 8);
    h = h2 / 2;
    ncol = d[32] | ((u32)d[33] << 8);
    if (!ncol)
        ncol = 1u << bpp;
    if (hdr < 40 || w == 0 || w > 32 || h == 0 || h > 32 || (bpp != 1 && bpp != 4) || ncol > 16)
        return -1;
    xs = ((w * bpp + 31) / 32) * 4;
    ms = ((w + 31) / 32) * 4;
    xor_off = hdr + 4 * ncol;
    and_off = xor_off + xs * h;
    if (and_off + ms * h > len)
        return -1;
    memset(used, 0, sizeof(used));
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            const u8 *xr = d + xor_off + xs * (h - 1 - y), *mr = d + and_off + ms * (h - 1 - y);
            if (mr[x >> 3] & (0x80 >> (x & 7)))
                continue;
            used[bpp == 4 ? (xr[x >> 1] >> ((x & 1) ? 0 : 4)) & 15 : (xr[x >> 3] >> (7 - (x & 7))) & 1] = 1;
        }
    gdi_usebg(0);
    for (c = 0; c < ncol; c++) {
        const u8 *pal = d + hdr + 4 * c;
        if (!used[c])
            continue;
        memset(bits, 0, sizeof(bits));
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                const u8 *xr = d + xor_off + xs * (h - 1 - y), *mr = d + and_off + ms * (h - 1 - y);
                u16 v;
                if (mr[x >> 3] & (0x80 >> (x & 7)))
                    continue;
                v = bpp == 4 ? (xr[x >> 1] >> ((x & 1) ? 0 : 4)) & 15 : (xr[x >> 3] >> (7 - (x & 7))) & 1;
                if (v == c)             /* GR_BITMAP rows: (w + 15) / 16 words, MSB first */
                    bits[y * ((w + 15) / 16) + (x >> 4)] |= (GR_BITMAP)(0x8000u >> (x & 15));
            }
        gdi_fg(GR_RGB(pal[2], pal[1], pal[0]));
        GrBitmap(wid, gdi_gc, x0, y0, (GR_SIZE)w, (GR_SIZE)h, bits);
    }
    return 0;
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
