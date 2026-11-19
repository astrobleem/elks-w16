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
#ifndef MEM_GETPCJR
#define MEM_GETPCJR 15          /* expanded-PCjr kernels (pcjr-video-v0.9.2); the Tandy headers lack it */
#endif
#include "wapi.h"
#include "polygon.h"
#include "dib.h"

#define OBJ_PEN   1
#define OBJ_BRUSH 2
#define OBJ_FONT  3
#define MAXOBJ    24
#define NSTOCK    17

struct gobj { u8 type, used, null; u16 style, width; u32 color; };   /* fonts: style = height, width = weight;
                                                                        pattern brushes: style BS_PATTERN, width = bitmap */
#define BS_PATTERN 3
#define MAXBMP 4
static struct { u8 used, w, h, mono; u16 rows[16]; u32 colors[2]; } bmps[MAXBMP];      /* monochrome bitmaps up to 16x16 */
static struct gobj stock[NSTOCK];
static struct gobj objs[MAXOBJ];
static struct dc dcs[MAXDC];
#define DC_SAVE_DEPTH 4
#ifdef W16_LAZY_UI
static struct dc *dc_saved[MAXDC];
#else
static struct dc dc_saved[MAXDC][DC_SAVE_DEPTH];
#endif
static u8 dc_saved_count[MAXDC];
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
static u8 nocache, servertext;   /* W16_NOCACHE, W16_SERVERTEXT: before/after measurements */
#define GS(d) (&gcs[((d)->hdc - HDC_BASE) >> 1])

static u8 srv_exclend;                  /* the server implements GR_MODE_EXCLUDEENDPOINT */

static void gs_mode(struct gcstate *g, int mode)
{
    if (srv_exclend)
        mode |= GR_MODE_EXCLUDEENDPOINT;
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
    stock[15].type=5;stock[15].used=1; /* DEFAULT_PALETTE */
    gdi_gc = GrNewGC();
    memset(gcs, 0, sizeof(gcs));
    gcs[MAXDC].gc = gdi_gc;
    {
        GR_GC_INFO gi;                  /* a patched server acknowledges the end-point flag */
        GrSetGCMode(gdi_gc, GR_MODE_COPY | GR_MODE_EXCLUDEENDPOINT);
        GrGetGCInfo(gdi_gc, &gi);
        srv_exclend = (gi.mode & GR_MODE_HAVE_EXCLUDEENDPOINT) != 0;
        GrSetGCMode(gdi_gc, GR_MODE_COPY);
        vlog("w16: Nano-X server %s line end points itself\n", srv_exclend ? "leaves out" : "draws");
    }
    {
        const char *e = getenv("W16_NOCACHE");
        nocache = e && *e == '1';
        if (nocache)
            wlog("w16: W16_NOCACHE: every GC state change is sent and every DC gets a new GC\n");
        e = getenv("W16_SERVERTEXT");
        servertext = e && *e == '1';
        if (servertext)
            wlog("w16: W16_SERVERTEXT: text is measured by the server (GrGetGCTextSize), not locally\n");
    }
    {
        GR_SIZE w, h, b;
        gdi_text_size("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, &w, &h, &b);
        if (h > 4 && h < 40) {
            fm_h = h;
            fm_asc = b;
            fm_avg = (w + 26) / 52;
        }
        gdi_text_size("W", 1, &w, &h, &b);
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
    for (i = 0; i < MAXBMP; i++)
        if (bmps[i].used)
            (*nobj)++;
}


void gdi_shutdown(void)
{
    int i;
    bitmap_shutdown();
    palette_shutdown();
    for (i = 0; i < MAXDC; i++) {
        dcs[i].used = 0;
        dc_saved_count[i] = 0;
#ifdef W16_LAZY_UI
        free(dc_saved[i]); dc_saved[i] = 0;
#endif
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
    dc_saved_count[i] = 0;
    memset(d, 0, sizeof(*d));
    d->used = 1; d->paint = paint; d->hdc = HDC_BASE + 2 * i; d->w = w;
    d->wid = w ? w->client : GR_ROOT_WINDOW_ID;    /* no window: the whole screen */
    if (!gcs[i].gc || nocache) {
        gcs[i].gc = GrNewGC();                     /* a round trip: done once per slot */
        gcs[i].known = 0;
    }
    d->gc = gcs[i].gc;
    d->rop2 = R2_COPYPEN;
    d->stretchmode = 1;
    gdi_map_init(d);
    d->pen = HSTOCK + 2 * 7;        /* BLACK_PEN */
    d->brush = HSTOCK + 2 * 0;      /* WHITE_BRUSH */
    d->palette=HSTOCK+2*15;
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
#ifdef W16_LAZY_UI
    free(dc_saved[(hdc-HDC_BASE)>>1]); dc_saved[(hdc-HDC_BASE)>>1] = 0;
#endif
    dc_saved_count[(hdc-HDC_BASE)>>1] = 0;
    d->used = 0;
}

/* Saved selections remain live references even after the live DC changes.
 * ReleaseDC/DeleteDC drops all saves before a numeric handle can be reused. */
static int saved_object(u16 h)
{
    int i,j;
    for(i=0;i<MAXDC;i++)for(j=0;j<dc_saved_count[i];j++)
        if(dc_saved[i][j].pen==h||dc_saved[i][j].brush==h||dc_saved[i][j].font==h||dc_saved[i][j].palette==h)return 1;
    return 0;
}
u32 g_SaveDC(u16 ss,u16 ap)
{
    struct dc*d=dc_get(AW(0));int i,n;
    if(!d)return bitmap_savedc(AW(0));
    i=(AW(0)-HDC_BASE)>>1;n=dc_saved_count[i];
    if(n==DC_SAVE_DEPTH)return 0;
#ifdef W16_LAZY_UI
    if(!dc_saved[i] && !(dc_saved[i]=calloc(DC_SAVE_DEPTH,sizeof(*dc_saved[i]))))return 0;
#endif
    dc_saved[i][n]=*d;dc_saved_count[i]++;return n+1;
}
u32 g_RestoreDC(u16 ss,u16 ap)
{
    struct dc*d=dc_get(AW(2));int i,n,target; i16 level=(i16)AW(0);
    if(!d)return bitmap_restoredc(AW(2),level);
    i=(AW(2)-HDC_BASE)>>1;n=dc_saved_count[i];
    target=level<0?(long)n+level:(long)level-1;
    if(!level||target<0||target>=n)return 0;
    *d=dc_saved[i][target];dc_saved_count[i]=target;
#ifdef W16_LAZY_UI
    if(!target){free(dc_saved[i]);dc_saved[i]=0;}
#endif
    GS(d)->known=0; /* force the next draw to reapply restored server state */
    return 1;
}
u32 g_GetROP2(u16 ss,u16 ap)
{
    struct dc*d=dc_get(AW(0));return d?d->rop2:bitmap_rop2(AW(0),0);
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

/* Validate ownership transfer, or release an unselected clipboard object. */
int gdi_clipboard(u16 h,u16 format,int release)
{
    int i;
    if(format==2)return bitmap_clipboard(h,release);
    if(format!=9||h==HSTOCK+2*15||!palette_valid(h)||
       saved_object(h)||bitmap_object_used(h))return 0;
    for(i=0;i<MAXDC;i++)if(dcs[i].used&&dcs[i].palette==h)return 0;
    if(release)palette_delete(h);
    return 1;
}

/* DeleteObject(hObject) */
u32 g_DeleteObject(u16 ss, u16 ap)
{
    u16 h = AW(0), i;
    struct gobj *o = obj_get(h);
    if(clipboard_owns_gdi(h))return 0;
    {int r=bitmap_delete(h);if(r>=0)return r;}
    if(palette_valid(h)){
        if(h==HSTOCK+2*15)return 1;
        if(saved_object(h)||bitmap_object_used(h))return 0;
        for(i=0;i<MAXDC;i++)if(dcs[i].used&&dcs[i].palette==h)return 0;
        return palette_delete(h);
    }
    if (!o)
        return 0;
    if (h < HOBJ_BASE)
        return 1;                       /* stock objects are never deleted */
    if(saved_object(h)||bitmap_object_used(h))return 0;
    for (i = 0; i < MAXDC; i++)
        if (dcs[i].used && (dcs[i].pen == h || dcs[i].brush == h || dcs[i].font == h)) {
            vlog("w16: DeleteObject %04x refused: still selected\n", h);
            return 0;
        }
    if(o->type==OBJ_BRUSH&&o->style==BS_PATTERN)bmps[o->width].used=0;
    o->used = 0;
    return 1;
}

/* SelectObject(hdc, hObject) */
u32 g_SelectObject(u16 ss, u16 ap)
{
    u16 h = AW(0), prev;
    struct dc *d = dc_get(AW(2));
    struct gobj *o = obj_get(h);
    if (!d) return o?bitmap_object(AW(2),o->type,h):bitmap_select(AW(2),h);
    if (!o) return 0;
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
    if (!d) return bitmap_setcolor(AW(4),AL(0),0);
    prev = d->textcolor; d->textcolor = AL(0) & 0xFFFFFFul;
    return prev;
}

u32 g_GetBkColor(u16 ss,u16 ap)
{
    struct dc *d=dc_get(AW(0));
    return d?d->bkcolor:bitmap_getcolor(AW(0),1);
}

u32 g_SetBkColor(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(4));
    u32 prev;
    if (!d) return bitmap_setcolor(AW(4),AL(0),1);
    prev = d->bkcolor; d->bkcolor = AL(0) & 0xFFFFFFul;
    return prev;
}

u32 g_SetBkMode(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(2));
    u16 prev;
    if (!d) return bitmap_textmode(AW(2),AW(0),0);
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

static int brush_pat(struct dc *d)
{
    struct gobj *o = obj_get(d->brush);
    return o && o->type == OBJ_BRUSH && o->style == BS_PATTERN && bmps[o->width].used ? o->width : -1;
}

/* Sample a validated solid or snapshotted pattern brush in device coordinates.
 * Monochrome pattern bits use the destination DC text/background colours. */
int gdi_pattern_color(struct dc*d,int x,int y,u32*color)
{
 struct gobj*o=obj_get(d->brush);int i,bit;
 if(!o||o->type!=OBJ_BRUSH||o->null)return 0;
 if(!o->style){*color=o->color;return 1;}
 if(o->style!=BS_PATTERN||(i=o->width)>=MAXBMP||!bmps[i].used)return 0;
 x=((long)x-d->brushx)%bmps[i].w;if(x<0)x+=bmps[i].w;
 y=((long)y-d->brushy)%bmps[i].h;if(y<0)y+=bmps[i].h;
 bit=(bmps[i].rows[y]>>(15-x))&1;
 *color=bmps[i].mono?(bit?d->bkcolor:d->textcolor):bmps[i].colors[bit];
 return 1;
}

/* Monochrome pattern phase is relative to the DC brush origin. Scratch holds
 * bounded rows; each packet leaves room for the Nano-X request header. */
static int pat_fill(struct dc *d, int x, int y, int w, int h, int bi)
{
    GR_BITMAP *buf = (GR_BITMAP *)w16_scratch;      /* at most1280 bytes */
    int wpr, batch, r, k, y0, bw = bmps[bi].w, bh = bmps[bi].h, same = 16 % bw == 0;
    if (w <= 0 || h <= 0)return 1;
    if (w>640||h>480||(long)x+w-1>32767L||(long)y+h-1>32767L)return 0;
    wpr = (w + 15) / 16;
    /* ELKS Nano-X accepts 512-byte requests, including the 16-byte header.
     * Its legacy GrBitmap splitter only checks payload size. Keep 32 bytes
     * spare here so neither exact-512 payloads nor full-width rows kill it. */
    batch = 480 / (2 * wpr);
    if (batch > 16) batch = 16;
    gs_mode(GS(d), GR_MODE_COPY);
    gs_fg(GS(d), rgb_to_gr(bmps[bi].mono?d->bkcolor:bmps[bi].colors[1]));
    gs_bg(GS(d), rgb_to_gr(bmps[bi].mono?d->textcolor:bmps[bi].colors[0]));
    gs_usebg(GS(d), 1);
    for (y0 = 0; y0 < h; y0 += batch) {
        int rows = h - y0 > batch ? batch : h - y0;
        for (r = 0; r < rows; r++) {
            u16 pr = bmps[bi].rows[(((long)y + y0 + r - d->brushy) % bh + bh) % bh], word = 0;
            for (k = 0; k < wpr; k++) {
                int px;
                if (!k || !same) {
                    word = 0;
                    for (px = 0; px < 16; px++)
                        if (pr & (0x8000u >> ((((long)x + 16 * k + px - d->brushx) % bw + bw) % bw)))
                            word |= (u16)(0x8000u >> px);
                }
                buf[r * wpr + k] = word;
            }
        }
        GrBitmap(d->wid, d->gc, x, y + y0, w, rows, buf);
    }
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
        return bitmap_rop2(AW(2),r);
    prev = d->rop2;
    if (r >= 1 && r <= 16)
        d->rop2 = (u8)r;
    return prev;
}

/* ALTERNATE is the only supported polygon fill mode. Querying the memory
 * DC's ROP with mode zero validates its handle without changing any state. */
u32 g_SetPolyFillMode(u16 ss,u16 ap)
{
 if(ap>0xfffcu||AW(0)!=1)return 0;
 return dc_get(AW(2))||bitmap_rop2(AW(2),0)?1:0;
}

struct polygon_screen { struct dc *d; GR_COLOR pen,brush; };
static void polygon_screen_span(void *ctx,int x,int y,int width,int kind)
{
 struct polygon_screen *s=ctx;
 if(set_rop(s->d,kind==2?s->pen:s->brush))
  GrFillRect(s->d->wid,s->d->gc,x,y,width,1);
}
/* ExtFloodFill(hdc,x,y,color,type): solid brush, surface or boundary fill. */
u32 g_ExtFloodFill(u16 ss,u16 ap)
{
 struct dc*d;struct gobj*brush;struct polygon_screen out;u16 h;int x,y;
 if(ap>0xfff4u)return 0;
 h=AW(10);x=(i16)AW(8);y=(i16)AW(6);d=dc_get(h);
 brush=obj_get(d?d->brush:bitmap_object(h,OBJ_BRUSH,0));
 if(!brush||(!brush->null&&brush->style))return 0;
 if(brush->null)return 1;
 if(d&&!gdi_map_point(d,&x,&y))return 0;
 out.d=d;out.brush=rgb_to_gr(brush->color);
 return bitmap_flood(h,d,x,y,AL(2),AW(0),brush->color,polygon_screen_span,&out);
}
/* A zero count selects the three-point rounded-box descriptor. */
static int draw_shape(u16 h,struct poly_point*p,int n)
{
 struct dc*d=dc_get(h);struct gobj *pen,*brush;
 struct polygon_screen out;GR_SCREEN_INFO si;int i,width,height,stroke,count=n<0?-n:n;
 pen=obj_get(d?d->pen:bitmap_object(h,OBJ_PEN,0));
 brush=obj_get(d?d->brush:bitmap_object(h,OBJ_BRUSH,0));
 if(!pen||!brush||(n>=0&&!brush->null&&brush->style))return 0;
 stroke=pen->null?0:pen->width?pen->width:1;
 /* PS_INSIDEFRAME is identical to PS_SOLID at cosmetic widths.
  * Polygons/polylines have no bounding-rectangle inset rule. */
 if(stroke&&(n?((pen->style!=0&&pen->style!=6)||stroke>32):
     ((pen->style!=0&&pen->style!=6)||(stroke>1&&pen->style!=6)||stroke>640)))return 0;
 /* Thick logical widths are supported in MM_TEXT only, never silently scaled. */
 if(stroke>1&&d&&d->mapmode!=1)return 0;
 if(!d)return bitmap_polygon(h,p,n,pen->color,stroke,brush->color,brush->null);
 if(n){for(i=0;i<count;i++)if(!gdi_map_point(d,&p[i].x,&p[i].y))return 0;}
 else{
  /* Corner diameters currently use MM_TEXT units. Ellipse uses the full box. */
  if(p[2].x!=32767&&d->mapmode!=1)return 0;
  if(!gdi_map_rect(d,&p[0].x,&p[0].y,&p[1].x,&p[1].y))return 0;
 }
 if(d->w){width=d->w->cw;height=d->w->ch;}
 else{memset(&si,0,sizeof(si));GrGetScreenInfo(&si);width=si.cols;height=si.rows;}
 out.d=d;out.pen=rgb_to_gr(pen->color);out.brush=rgb_to_gr(brush->color);
 return n?polygon_raster(p,n,width,height,stroke,!brush->null,polygon_screen_span,&out):
          oval_raster(p,width,height,stroke,!brush->null,polygon_screen_span,&out);
}
/* Polygon(hdc, far POINT[], count). Does not change the current position. */
static u32 polygon_api(u16 ss,u16 ap,int closed)
{
 u16 n,off,seg;int i,result;struct poly_point *p;
 if(ap>0xfff8u)return 0;
 n=AW(0);off=AW(2);seg=AW(4);
 if(n<2||n>128||(!seg&&!off)||(u32)off+4UL*n>65536UL||
    ((u32)seg<<4)+off+4UL*n>0x100000UL)return 0;
 p=malloc(n*sizeof(*p));if(!p)return 0;
 for(i=0;i<n;i++){
  p[i].x=(i16)rd16(seg,off+4*i);p[i].y=(i16)rd16(seg,off+4*i+2);
 }
 result=draw_shape(AW(6),p,closed?(int)n:-(int)n);free(p);return result;
}

u32 g_Polygon(u16 ss,u16 ap){return polygon_api(ss,ap,1);}
u32 g_Polyline(u16 ss,u16 ap){return polygon_api(ss,ap,0);}

/* Rectangle(hdc, left, top, right, bottom): right/bottom exclusive */
u32 g_Rectangle(u16 ss, u16 ap)
{
    int b = (i16)AW(0), r = (i16)AW(2), t = (i16)AW(4), l = (i16)AW(6);
    struct dc *d = dc_get(AW(8));
    GR_COLOR pc, bc;
    int pen, bi;
    if(!d){
        struct gobj*p=obj_get(bitmap_object(AW(8),OBJ_PEN,0)),*br=obj_get(bitmap_object(AW(8),OBJ_BRUSH,0));
        if(!p||!br)return 0;
        return bitmap_rectangle(AW(8),l,t,r,b,p->color,p->style,p->width,p->null,br->color,br->style,br->null);
    }
    if (!d || !gdi_map_rect(d,&l,&t,&r,&b) || r <= l || b <= t)
        return 0;
    pen = pen_of(d, &pc);
    if ((bi = brush_pat(d)) >= 0) {             /* pattern brush inside the outline */
        if(!pat_fill(d, l + pen, t + pen, r - l - 2 * pen, b - t - 2 * pen, bi))return 0;
        if (pen && set_rop(d, pc))
            GrRect(d->wid, d->gc, l, t, r - l, b - t);
        return 1;
    }
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

/* Solid hairline Ellipse and RoundRect share the same memory/display raster. */
u32 g_Ellipse(u16 ss,u16 ap)
{
 struct poly_point p[3];
 if(ap>0xfff6u)return 0;
 p[0].x=(i16)AW(6);p[0].y=(i16)AW(4);p[1].x=(i16)AW(2);p[1].y=(i16)AW(0);
 p[2].x=p[2].y=32767;
 return draw_shape(AW(8),p,0);
}
u32 g_RoundRect(u16 ss,u16 ap)
{
 struct poly_point p[3];
 if(ap>0xfff2u)return 0;
 p[0].x=(i16)AW(10);p[0].y=(i16)AW(8);p[1].x=(i16)AW(6);p[1].y=(i16)AW(4);
 p[2].x=(i16)AW(2);p[2].y=(i16)AW(0);
 return draw_shape(AW(12),p,0);
}

/* MoveTo(hdc, x, y): returns the previous position, y in the high word */
u32 g_MoveTo(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(4));
    u32 prev;
    if (!d)
        return bitmap_moveto(AW(4),(i16)AW(2),(i16)AW(0));
    prev = ((u32)(u16)d->cury << 16) | (u16)d->curx;
    d->curx = (i16)AW(2); d->cury = (i16)AW(0);
    return prev;
}

/* LineTo(hdc, x, y). Windows draws from the current position up to but not including (x, y), so a
 * zero-length line draws nothing and joined segments never touch their shared corner twice. The
 * patched Nano-X server leaves the end point out itself (GR_MODE_EXCLUDEENDPOINT, probed at
 * start); with a stock server the end point is drawn, and in the modes that combine with the
 * screen it is drawn again to undo it. In plain copy modes on a stock server it stays drawn. */
u32 g_LineTo(u16 ss, u16 ap)
{
    int y=(i16)AW(0),x=(i16)AW(2),tx=x,ty=y,fx,fy,k;
    struct dc*d=dc_get(AW(4));GR_COLOR c;
    if(!d){
        struct gobj*p=obj_get(bitmap_object(AW(4),OBJ_PEN,0));if(!p)return 0;
        return bitmap_lineto(AW(4),x,y,p->color,p->style,p->width,p->null);
    }
    fx=d->curx;fy=d->cury;
    if(!gdi_map_point(d,&fx,&fy)||!gdi_map_point(d,&tx,&ty))return 0;
    if((fx!=tx||fy!=ty)&&pen_of(d,&c)&&(k=set_rop(d,c))!=0){
        GrLine(d->wid,d->gc,fx,fy,tx,ty);
        if(k==2&&!srv_exclend)GrPoint(d->wid,d->gc,tx,ty);
    }
    d->curx=x;d->cury=y;return 1;
}

/* Display InvertRect: clip signed, exclusive Win16 bounds before computing
 * dimensions so a far offscreen RECT cannot wrap in 16-bit arithmetic. */
u32 u_InvertRect(u16 ss,u16 ap)
{
    struct dc *d; u16 off,seg; int l,t,r,b,width,height;
    GR_SCREEN_INFO si;
    if(ap>0xfffau) return 0;
    off=AW(0);seg=AW(2);d=dc_get(AW(4));
    if(!d || (!seg&&!off) || off>0xfff8u || ((u32)seg<<4)+off>0xffff8UL) return 0;
    if(d->w){width=d->w->cw;height=d->w->ch;}
    else{memset(&si,0,sizeof(si));GrGetScreenInfo(&si);width=si.cols;height=si.rows;}
    l=(i16)rd16(seg,off);t=(i16)rd16(seg,off+2);
    r=(i16)rd16(seg,off+4);b=(i16)rd16(seg,off+6);
    if(l<0)l=0;
    if(t<0)t=0;
    if(r>width)r=width;
    if(b>height)b=height;
    if(r>l&&b>t){
        gs_mode(GS(d),GR_MODE_INVERT); gs_fg(GS(d),GR_RGB(255,255,255));
        GrFillRect(d->wid,d->gc,l,t,r-l,b-t);
    }
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
    if(!gdi_map_rect(d,&l,&t,&r,&b))return 0;
    c = brush_color(hbr, &isnull);
    if (isnull || r <= l || b <= t)
        return 1;
    gs_mode(GS(d), GR_MODE_COPY);
    gs_fg(GS(d), rgb_to_gr(c));
    GrFillRect(d->wid, d->gc, l, t, r - l, b - t);
    return 1;
}

/* ---------------------------------------------------------------- bitmaps, pattern brushes, PatBlt */
/* All DDBs, including small monochrome masks, are selectable memory objects. */
u32 g_CreateBitmap(u16 ss,u16 ap)
{return bitmap_create(AW(10),AW(8),AW(6),AW(4),AW(2),AW(0));}

/* Get/SetBitmapBits(hBitmap,LONG count,far bits). */
u32 g_GetBitmapBits(u16 ss,u16 ap){return bitmap_bits(AW(8),AL(4),AW(2),AW(0),0);}
u32 g_SetBitmapBits(u16 ss,u16 ap){return bitmap_bits(AW(8),AL(4),AW(2),AW(0),1);}

/* CreatePatternBrush copies source rows, independent of the bitmap lifetime. */
u32 g_CreatePatternBrush(u16 ss,u16 ap)
{
 int i,type;u16 h;
 for(i=0;i<MAXBMP&&bmps[i].used;i++);
 if(i==MAXBMP)return 0;
 type=bitmap_pattern(AW(0),&bmps[i].w,&bmps[i].h,bmps[i].rows,bmps[i].colors);
 if(!type)return 0;
 bmps[i].mono=type==1;
 h=obj_new(OBJ_BRUSH,0x808080ul,BS_PATTERN,(u16)i);
 if(h)bmps[i].used=1;
 return h;
}

/* PatBlt(hDC, X, Y, nWidth, nHeight, dwRop): fill with the brush, invert, blacken or whiten */
u32 g_PatBlt(u16 ss, u16 ap)
{
    u32 rop = AL(0);
    int h = (i16)AW(4), w = (i16)AW(6), y = (i16)AW(8), x = (i16)AW(10), bi;
    struct dc *d = dc_get(AW(12));
    GR_COLOR c;
    if (!d) {
        struct gobj*o=obj_get(bitmap_object(AW(12),OBJ_BRUSH,0));
        if(!o)return 0;
        return bitmap_patblt(AW(12),x,y,w,h,rop,o->color,o->style,o->null);
    }
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    if (!w || !h)
        return 1;
    switch (rop) {
    case 0x00F00021ul:                                      /* PATCOPY */
        if ((bi = brush_pat(d)) >= 0) {
            return pat_fill(d, x, y, w, h, bi);
        }
        if (!brush_of(d, &c))
            return 1;
        gs_mode(GS(d), GR_MODE_COPY); gs_fg(GS(d), c);
        break;
    case 0x005A0049ul:                                      /* PATINVERT: brush XOR screen */
        if (!brush_of(d, &c))
            return 1;
        gs_mode(GS(d), GR_MODE_XOR); gs_fg(GS(d), c);
        break;
    case 0x00550009ul:                                      /* DSTINVERT */
        gs_mode(GS(d), GR_MODE_INVERT); gs_fg(GS(d), GR_RGB(255, 255, 255));
        break;
    case 0x00000042ul:                                      /* BLACKNESS */
        gs_mode(GS(d), GR_MODE_COPY); gs_fg(GS(d), GR_RGB(0, 0, 0));
        break;
    case 0x00FF0062ul:                                      /* WHITENESS */
        gs_mode(GS(d), GR_MODE_COPY); gs_fg(GS(d), GR_RGB(255, 255, 255));
        break;
    default:
        wlog("w16: PatBlt raster operation %08lx drawn as PATCOPY\n", (unsigned long)rop);
        if (!brush_of(d, &c))
            return 1;
        gs_mode(GS(d), GR_MODE_COPY); gs_fg(GS(d), c);
        break;
    }
    GrFillRect(d->wid, d->gc, x, y, w, h);
    return 1;
}

/* GetTextExtent(hDC, lpString, nCount): width in the low word, height in the high word */
u32 g_GetTextExtent(u16 ss, u16 ap)
{
    char buf[128];
    int n = (i16)AW(0);
    GR_SIZE w, h, b;
    struct dc *d = dc_get(AW(6));
    if ((!d && !bitmap_rop2(AW(6),0)) || n <= 0)
        return 0;
    if (n > (int)sizeof(buf))
        n = sizeof(buf);
    fcopy_in(buf, AW(4), AW(2), (u16)n);
    gdi_text_size(buf, n, &w, &h, &b);
    return ((u32)(u16)h << 16) | (u16)w;
}

u32 g_UnrealizeObject(u16 ss, u16 ap) { (void)ss; (void)ap; return 1; }   /* pattern realization is computed per draw */

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
        gdi_text_size(buf, n, &tw, &th, &tb);
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
    if (!d) return bitmap_text(AW(10),(i16)AW(8),(i16)AW(6),AW(4),AW(2),(i16)AW(0));
    if ((i16)AW(0) < 0) return 0;
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
        return bitmap_textmode(AW(2),AW(0),1);
    prev = d->textalign;
    d->textalign = AW(0);
    return prev;
}

/* EnumFonts(hdc,face,callback,lParam): only the actual system raster font. */
u32 g_EnumFonts(u16 ss,u16 ap)
{
    if(!dc_get(AW(12))) return 0;
    return enum_system_font(AW(8),AW(10),AW(4),AW(6),AL(0),fm_h,fm_asc,fm_avg,fm_max);
}

/* GetTextMetrics(hdc, lpMetrics): the Nano-X system font, whatever font is selected */
u32 g_GetTextMetrics(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2);
    struct dc *d = dc_get(AW(4));
    if (!d && !bitmap_rop2(AW(4),0))
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

/* GetNearestColor(hdc, crColor) */
u32 g_GetNearestColor(u16 ss, u16 ap)
{
    u32 c = AL(0) & 0xFFFFFFul, best = 0, bd = 0xFFFFFFFFul;
    int i;
    GR_PALETTE *p=(GR_PALETTE *)w16_scratch;
    memset(p,0,sizeof(*p));GrGetSystemPalette(p);
    if(p->count<1||p->count>16)return 0;
    for (i = 0; i < p->count; i++) {
        u32 actual=(u32)p->palette[i].r|((u32)p->palette[i].g<<8)|((u32)p->palette[i].b<<16);
        long dr = (long)(c & 0xFF) - (long)(actual & 0xFF);
        long dg = (long)((c >> 8) & 0xFF) - (long)((actual >> 8) & 0xFF);
        long db = (long)((c >> 16) & 0xFF) - (long)((actual >> 16) & 0xFF);
        u32 dd = (u32)(dr * dr + dg * dg + db * db);
        if (dd < bd) {
            bd = dd;
            best = actual;
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
    int r=ic_handle(AW(0),1); if(r>=0)return r;
    r=bitmap_deletedc(AW(0));if(r>=0)return r;
    if (!dc_get(AW(0)))
        return 0;
    dc_release(AW(0));
    return 1;
}

/* Pixel dimensions come from the display. Other legacy Tandy caps are unchanged. */
u32 g_GetDeviceCaps(u16 ss, u16 ap)
{
    if(ic_handle(AW(2),0)==0) return 0;
    switch (AW(0)) {
    case 0:  return 0x0300;     /* DRIVERVERSION */
    case 2:  return 1;          /* TECHNOLOGY: DT_RASDISPLAY */
    case 4:  return 240;        /* HORZSIZE, mm */
    case 6:  return 180;        /* VERTSIZE, mm */
    case 8: case 10: {
        GR_SCREEN_INFO si;
        memset(&si,0,sizeof(si));GrGetScreenInfo(&si);
        return AW(0)==8?si.cols:si.rows;
    }
    case 12: case 24: {
        GR_SCREEN_INFO si;memset(&si,0,sizeof(si));GrGetScreenInfo(&si);
        return AW(0)==12?si.bpp:si.ncolors;
    }
    case 14: return 1;          /* PLANES */
    case 16: return 16;         /* NUMBRUSHES */
    case 18: return 16;         /* NUMPENS */
    case 22: return 1;          /* NUMFONTS */
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
 * of up to 32x32 at 1 or 4 bits per pixel, using (destination AND mask) XOR image.
 * One Nano-X GrBitmap per colour present keeps the request count small. */
static int draw_icon_gc(GR_WINDOW_ID wid,struct gcstate *gc,int x0,int y0,const u8 *d,u16 len)
{
    GR_BITMAP bits[32*2];struct dib_info info;struct dib_memory memory={d,len};
    u32 and_off;u16 c,x,y,words,index;u8 used[16];
    if(!dib_icon_open(&info,dib_read_memory,&memory,len,&and_off) ||
       (long)x0+info.width>32767L || (long)y0+info.height>32767L)return -1;
    words=(info.width+15)/16;memset(used,0,sizeof(used));memset(bits,0,sizeof(bits));
    /* Validate all XOR indices before issuing either compositing pass. */
    for(y=0;y<info.height;y++)for(x=0;x<info.width;x++) {
        const u8 *xr=d+info.pixels+info.stride*(info.height-1-y),*ar=d+and_off+((info.width+31)/32)*4*(info.height-1-y);
        index=info.bpp==4?(xr[x>>1]>>((x&1)?0:4))&15:(xr[x>>3]>>(7-(x&7)))&1;
        if(index>=info.colors)return -1;
        used[index]=1;
        if(!(ar[x>>3]&(0x80>>(x&7))))bits[y*words+(x>>4)]|=(GR_BITMAP)(0x8000u>>(x&15));
    }
    /* Clearing exactly the AND-zero pixels implements destination AND mask
     * using the ordinary COPY primitive, without screen-readback support. */
    gs_usebg(gc,0);gs_mode(gc,GR_MODE_COPY);gs_fg(gc,GR_RGB(0,0,0));
    GrBitmap(wid,gc->gc,x0,y0,info.width,info.height,bits);
    gs_mode(gc,GR_MODE_XOR);
    for(c=0;c<info.colors;c++)if(used[c]) {
        memset(bits,0,sizeof(bits));
        for(y=0;y<info.height;y++)for(x=0;x<info.width;x++) {
            const u8 *xr=d+info.pixels+info.stride*(info.height-1-y);
            index=info.bpp==4?(xr[x>>1]>>((x&1)?0:4))&15:(xr[x>>3]>>(7-(x&7)))&1;
            if(index==c)bits[y*words+(x>>4)]|=(GR_BITMAP)(0x8000u>>(x&15));
        }
        gs_fg(gc,GR_RGB(info.palette[c].r,info.palette[c].g,info.palette[c].b));
        GrBitmap(wid,gc->gc,x0,y0,info.width,info.height,bits);
    }
    gs_mode(gc,GR_MODE_COPY);return 0;
}
int gdi_draw_icon(GR_WINDOW_ID wid,int x,int y,const u8 *d,u16 len)
{return draw_icon_gc(wid,&gcs[MAXDC],x,y,d,len);}
/* Win16 DrawIcon(HDC,short x,short y,HICON), eight Pascal argument bytes.
 * Clone the display DC's GC to retain its clipping without altering its state. */
u32 u_DrawIcon(u16 ss,u16 ap)
{
 struct dc *d;struct gcstate gc;struct dib_info info;struct dib_memory memory;
 const u8 *image;u8 *snapshot=0;u16 len;u32 mask;int x,y,result=0;
 if(ap>0xfff8u || !(d=dc_get(AW(6))))return 0;
 image=snapshot=image_snapshot(AW(0),&len);
 if(!image)return 0;
 memory.data=image;memory.length=len;
 if(!dib_icon_open(&info,dib_read_memory,&memory,len,&mask) || info.width!=32 || info.height!=32)goto done;
 x=(i16)AW(4);y=(i16)AW(2);if(!gdi_map_point(d,&x,&y))goto done;
 memset(&gc,0,sizeof(gc));gc.gc=GrCopyGC(d->gc);if(!gc.gc)goto done;
 result=draw_icon_gc(d->wid,&gc,x,y,image,len)==0;GrDestroyGC(gc.gc);
done:
 free(snapshot);return result;
}

/* ---------------------------------------------------------------- text extents
 * Every GC here draws with the server's standard font (w16 never sets another), whose text size is the
 * sum of its character widths, with characters outside the font measured as its first one (Nano-X
 * gen_gettextsize). The width table comes once from GrGetFontInfo, so measuring text needs no round
 * trip: the profile showed GrGetGCTextSize at 20-25 ms of server time each, plus the client's wait. */
static struct { u8 ok, fixed, first, last; i16 height, base, maxw; u8 w[256]; } fm;

void gdi_text_size(const char *s, int n, GR_SIZE *pw, GR_SIZE *ph, GR_SIZE *pb)
{
    int x = 0;
    if (servertext) {
        GrGetGCTextSize(gdi_gc, (void *)s, n, GR_TFASCII, pw, ph, pb);
        return;
    }
    if (!fm.ok) {
        /* the font info (about 290 bytes) goes in the shared scratch: the first call is from gdi_init */
        GR_FONT_INFO *f = (GR_FONT_INFO *)w16_scratch;
        int i;
        GrGetFontInfo(0, f);
        fm.ok = 1;
        fm.fixed = f->fixed != 0;
        fm.first = (u8)f->firstchar;
        fm.last = (u8)(f->lastchar > 255 ? 255 : f->lastchar);
        fm.height = (i16)f->height;
        fm.base = (i16)f->baseline;
        fm.maxw = (i16)f->maxwidth;
        for (i = 0; i < 256; i++)
            fm.w[i] = f->widths[i];
        if (opt_verbose) {              /* the log shows the table agrees with the server's own measure */
            static const char sample[] = "About Clock... Reversi 0123 &Wjy~\x7f\xe9";
            GR_SIZE sw, sh, sb, lw, lh, lb;
            GrGetGCTextSize(gdi_gc, (void *)sample, sizeof(sample) - 1, GR_TFASCII, &sw, &sh, &sb);
            gdi_text_size(sample, sizeof(sample) - 1, &lw, &lh, &lb);
            wlog("w16: text extents from the font's width table: %dx%d base %d, server %dx%d base %d%s\n",
                 lw, lh, lb, sw, sh, sb, lw == sw && lh == sh && lb == sb ? "" : " (MISMATCH)");
        }
    }
    if (fm.fixed)
        x = n * fm.maxw;
    else
        while (n-- > 0) {
            u8 c = (u8)*s++;
            x += fm.w[c < fm.first || c > fm.last ? fm.first : c];
        }
    *pw = x;
    *ph = fm.height;
    *pb = fm.base;
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
        if (k >= 0) {           /* the screen driver's test: Tandy 9800h, else expanded PCjr 1800h */
            if (ioctl(k, MEM_GETTANDY, &r) < 0 || r != 0x9800) {
                r = 0;
                if (ioctl(k, MEM_GETPCJR, &r) < 0 || r != 0x1800)
                    r = 0;
            }
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

/* SetBrushOrg(HDC,x,y): signed device coordinates, old POINT packed in DWORD. */
u32 g_SetBrushOrg(u16 ss,u16 ap)
{
 struct dc*d=dc_get(AW(4));u32 old; i16 x=(i16)AW(2),y=(i16)AW(0);
 if(!d)return bitmap_brushorg(AW(4),x,y,&old)?old:0;
 old=(u16)d->brushx|((u32)(u16)d->brushy<<16);
 d->brushx=x;d->brushy=y;return old;
}

/* Display SetPixel uses the actual native palette; memory pixels
 * use their owned storage and palette, never the screen. */
typedef char pixel_palette_fits_scratch[(sizeof(GR_PALETTE)<=W16_SCRATCH)?1:-1];
u32 g_SetPixel(u16 ss,u16 ap)
{
 struct dc*d=dc_get(AW(8));i16 x=(i16)AW(6),y=(i16)AW(4);u32 c=AL(0),best=0;
 GR_PALETTE*p=(GR_PALETTE*)w16_scratch;GR_SCREEN_INFO si;
 long dr,dg,db,dist,closest=0x7fffffffL;int i,w,h;
 if(!d)return bitmap_setpixel(AW(8),x,y,c);
 if(x<0||y<0||(c&0xff000000UL))return 0xffffffffUL;
 if(d->w){w=d->w->cw;h=d->w->ch;}else{memset(&si,0,sizeof(si));GrGetScreenInfo(&si);w=si.cols;h=si.rows;}
 if(x>=w||y>=h)return 0xffffffffUL;
 memset(p,0,sizeof(*p));GrGetSystemPalette(p);if(p->count<1||p->count>16)return 0xffffffffUL;
 for(i=0;i<p->count;i++){
  dr=(long)(c&255)-p->palette[i].r;dg=(long)((c>>8)&255)-p->palette[i].g;db=(long)((c>>16)&255)-p->palette[i].b;
  dist=dr*dr+dg*dg+db*db;if(dist<closest){closest=dist;best=(u32)p->palette[i].r|((u32)p->palette[i].g<<8)|((u32)p->palette[i].b<<16);}
 }
 gs_mode(GS(d),GR_MODE_COPY);gs_fg(GS(d),rgb_to_gr(best));GrPoint(d->wid,d->gc,x,y);return best;
}

/* DrawText supplies explicit top-left coordinates independent of TextAlign. */
void gdi_draw_text(struct dc *d, int x, int y, const char *text, int n)
{
    gs_mode(GS(d), GR_MODE_COPY);
    gs_fg(GS(d), rgb_to_gr(d->textcolor));
    gs_bg(GS(d), rgb_to_gr(d->bkcolor));
    gs_usebg(GS(d), d->bkmode == OPAQUE);
    GrText(d->wid, d->gc, x, y, text, n, GR_TFASCII | GR_TFTOP);
}

/* Owned memory pixel readback or negotiated visible framebuffer readback. */
u32 g_GetPixel(u16 ss, u16 ap)
{
    struct dc *d = 0;
    static u16 failures;
    u16 why = 1;
    i16 x=0,y=0;
    GR_GC_INFO gi;
    GR_PIXELVAL pixel = 255;
    GR_PALETTE *p = (GR_PALETTE *)w16_scratch;
    memset(&gi,0,sizeof(gi));
    if (ap > 0xfffa) goto failed;
    x=(i16)AW(2);y=(i16)AW(0);d=dc_get(AW(4));
    if(!d)return bitmap_getpixel(AW(4),x,y);
    why=2;
    if (x<0 || y<0 || (d->w && (x>=d->w->cw || y>=d->w->ch)))
        goto failed;
    memset(&gi,0,sizeof(gi));GrGetGCInfo(d->gc,&gi);
    why=3;
    if (gi.gcid!=d->gc || gi.region || !(gi.mode & 0x1000)) goto failed;
    GrReadArea(d->wid,x,y,1,1,&pixel);
    why=4;
    if (pixel>=16) goto failed;
    memset(p,0,sizeof(*p));GrGetSystemPalette(p);
    why=5;
    if (pixel>=p->count || p->count<1 || p->count>16) goto failed;
    return (u32)p->palette[pixel].r | ((u32)p->palette[pixel].g<<8) |
           ((u32)p->palette[pixel].b<<16);
failed:
    if (opt_trace && failures < 32) {
        failures++;
        wlog("w16: GetPixel refused reason=%u xy=%d,%d wid=%u client=%d,%d gc=%u mode=%04x region=%u pixel=%u\n",
             why,x,y,d?(unsigned)d->wid:0,d&&d->w?d->w->cw:-1,d&&d->w?d->w->ch:-1,
             (unsigned)gi.gcid,(unsigned)gi.mode,(unsigned)gi.region,(unsigned)pixel);
    }
    return 0xffffffffUL;
}

/* DC device origin in screen coordinates, including all ancestor clients.
 * Root/display and the separate source-memory DCs have origin(0,0). */
u32 g_GetDCOrg(u16 ss,u16 ap)
{
 struct dc*d=dc_get(AW(0));int x=0,y=0;
 if(d&&d->w)win_client_origin(d->w,&x,&y);
 return (u16)x|((u32)(u16)y<<16);
}

/* Same bounded visible native DC domain as GetPixel, including clip refusal. */
u32 g_PtVisible(u16 ss, u16 ap)
{
    return g_GetPixel(ss, ap) != 0xffffffffUL;
}

/* EnumObjects(hDC, OBJ_PEN/OBJ_BRUSH, callback, callerData). Enumerate the
 * actual physical colors advertised by our indexed display driver. */
u32 g_EnumObjects(u16 ss,u16 ap)
{
    GR_PALETTE *p=(GR_PALETTE *)w16_scratch;
    u32 colors[16]; u16 i,kind,hdc;
    if(ap>0xfff4u) return 0;
    kind=AW(8); hdc=AW(10);
    if((kind!=OBJ_PEN && kind!=OBJ_BRUSH) || !dc_get(hdc)) return 0;
    memset(p,0,sizeof(*p)); GrGetSystemPalette(p);
    if(p->count<1||p->count>16) return 0;
    for(i=0;i<p->count;i++) colors[i]=(u32)p->palette[i].r |
        ((u32)p->palette[i].g<<8) | ((u32)p->palette[i].b<<16);
    return enum_palette_objects(kind,AW(4),AW(6),AL(0),colors,p->count);
}

/* SelectPalette is USER in Win16, although the object belongs to GDI. */
u32 u_SelectPalette(u16 ss,u16 ap)
{
    u16 h=AW(2),dc=AW(4),old;struct dc*d=dc_get(dc);
    if(!palette_valid(h))return 0;
    if(!d)return bitmap_object(dc,5,h);
    old=d->palette;d->palette=h;return old;
}
u32 u_RealizePalette(u16 ss,u16 ap)
{
    /* RC_PALETTE is not advertised for this fixed-color VGA device. No
     * physical palette entries can change. Return zero changed entries. */
    struct dc*d=dc_get(AW(0));
    if(!d&&!bitmap_object(AW(0),5,0))return 0;
    return 0;
}
u32 g_GetNearestPaletteIndex(u16 ss,u16 ap)
{
    GR_PALETTE*p=(GR_PALETTE*)w16_scratch;u32 colors[16];int i,n;
    memset(p,0,sizeof(*p));GrGetSystemPalette(p);n=p->count;if(n>16)n=16;
    for(i=0;i<n;i++)colors[i]=(u32)p->palette[i].r|((u32)p->palette[i].g<<8)|((u32)p->palette[i].b<<16);
    return palette_nearest(AW(4),AL(0),colors,n);
}
