/*
 * user.c - USER: window classes, windows, the message queue and painting,
 * mapped onto Nano-X. Each Win16 top-level window is an undecorated Nano-X
 * window (frame, caption and border drawn here) with a child Nano-X window
 * for the client area, so GDI output is clipped to the client like on Windows.
 */
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include "wapi.h"

struct wnd wins[MAXWIN];
static struct wclass classes[MAXCLASS];

#define QSIZE 24
struct qmsg { u16 hwnd, msg, wp; u32 lp; i16 x, y; u32 time; };
static struct qmsg queue[QSIZE];
static int qhead, qcount;
static int quit_pending;
static u16 quit_code;
static u32 msgclock;
static struct { u32 time; u8 ch; } keychars[8];
static int kcnext;
static u16 keyup_hwnd, keyup_vk;
static u32 keyup_lp;
static int keyup_pending, saw_real_keyup;
static int npaints;
static int font_h = 13;
static int sysclick_armed;
static u32 sysclick_ms;
char opt_dump[40];
int  opt_ready;
static char readyfile[40] = "/tmp/w16.ready";

#define CAPH  (font_h + 1)

static u32 now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (u32)tv.tv_sec * 1000ul + (u32)(tv.tv_usec / 1000);
}

/* ---------------------------------------------------------------- windows */
struct wnd *win_get(u16 hwnd)
{
    u16 i = (u16)(hwnd - HWND_BASE) >> 2;
    if (hwnd < HWND_BASE || ((hwnd - HWND_BASE) & 3) || i >= MAXWIN || !wins[i].used)
        return 0;
    return &wins[i];
}

static struct wnd *win_by_nx(GR_WINDOW_ID wid, int *isclient)
{
    int i;
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used && (wins[i].frame == wid || wins[i].client == wid)) {
            *isclient = wins[i].client == wid;
            return &wins[i];
        }
    return 0;
}

u32 win_send(struct wnd *w, u16 msg, u16 wp, u32 lp)
{
    u16 words[5];
    words[0] = w->hwnd; words[1] = msg; words[2] = wp;
    words[3] = (u16)(lp >> 16); words[4] = (u16)lp;
    return w16_call16(w->procoff, w->procseg, w->hinst, 5, words);
}

void win_invalidate_all(struct wnd *w, int erase)
{
    w->ul = 0; w->ut = 0; w->ur = w->cw; w->ub = w->ch;
    w->invalid = 1;
    if (erase)
        w->erase = 1;
}

static void union_update(struct wnd *w, int l, int t, int r, int b)
{
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r > w->cw) r = w->cw;
    if (b > w->ch) b = w->ch;
    if (l >= r || t >= b)
        return;
    if (!w->invalid) {
        w->ul = l; w->ut = t; w->ur = r; w->ub = b;
    } else {
        if (l < w->ul) w->ul = l;
        if (t < w->ut) w->ut = t;
        if (r > w->ur) w->ur = r;
        if (b > w->ub) w->ub = b;
    }
    w->invalid = 1;
}

static int border_of(u32 style)
{
    return (style & WS_THICKFRAME) ? 3 : (style & (WS_BORDER | WS_CAPTION)) ? 1 : 0;
}

/* Windows 3.0 style non-client area: frame, caption, system menu and min/max boxes */
static void draw_nc(struct wnd *w)
{
    GR_WINDOW_ID f = w->frame;
    GR_GC_ID gc = gdi_gc;
    int b = border_of(w->style), W = w->w, H = w->h, n = strlen(w->title);
    GR_COLOR black = GR_RGB(0, 0, 0), gray = GR_RGB(170, 170, 170), white = GR_RGB(255, 255, 255);
    GR_SIZE tw, th, tb;

    gdi_fill(f, gray, 0, 0, W, H);
    GrSetGCForeground(gc, black);
    GrRect(f, gc, 0, 0, W, H);
    if (b > 1)
        GrRect(f, gc, b - 1, b - 1, W - 2 * (b - 1), H - 2 * (b - 1));
    if ((w->style & WS_CAPTION) == WS_CAPTION) {
        int x0 = b, x1 = W - b, y = b;
        gdi_fill(f, GR_RGB(0, 0, 170), x0, y, x1 - x0, CAPH);
        if (w->style & WS_SYSMENU) {
            gdi_fill(f, gray, x0, y, CAPH, CAPH);
            gdi_fill(f, white, x0 + 3, y + CAPH / 2 - 1, CAPH - 6, 2);
            GrSetGCForeground(gc, black);
            GrLine(f, gc, x0 + 4, y + CAPH / 2 + 1, x0 + CAPH - 3, y + CAPH / 2 + 1);
            GrLine(f, gc, x0 + CAPH, y, x0 + CAPH, y + CAPH - 1);
            x0 += CAPH + 1;
        }
        if (w->style & WS_MAXIMIZEBOX) {
            int bx = x1 - CAPH, k;
            gdi_fill(f, gray, bx, y, CAPH, CAPH);
            GrSetGCForeground(gc, black);
            GrLine(f, gc, bx - 1, y, bx - 1, y + CAPH - 1);
            for (k = 0; k < 4; k++)                    /* up arrow */
                GrLine(f, gc, bx + CAPH / 2 - k, y + 4 + k, bx + CAPH / 2 + k, y + 4 + k);
            x1 = bx - 1;
        }
        if (w->style & WS_MINIMIZEBOX) {
            int bx = x1 - CAPH, k;
            gdi_fill(f, gray, bx, y, CAPH, CAPH);
            GrSetGCForeground(gc, black);
            GrLine(f, gc, bx - 1, y, bx - 1, y + CAPH - 1);
            for (k = 0; k < 4; k++)                    /* down arrow */
                GrLine(f, gc, bx + CAPH / 2 - k, y + CAPH - 5 - k, bx + CAPH / 2 + k, y + CAPH - 5 - k);
            x1 = bx - 1;
        }
        GrGetGCTextSize(gc, w->title, n, GR_TFASCII, &tw, &th, &tb);
        GrSetGCForeground(gc, white);
        GrSetGCUseBackground(gc, GR_FALSE);
        GrText(f, gc, x0 + ((x1 - x0) - tw) / 2 > x0 ? x0 + ((x1 - x0) - tw) / 2 : x0, y + 1, w->title, n,
               GR_TFASCII | GR_TFTOP);
        GrSetGCForeground(gc, black);
        GrLine(f, gc, b, b + CAPH, W - b - 1, b + CAPH);
    }
}

/* ---------------------------------------------------------------- queue */
static void post(u16 hwnd, u16 msg, u16 wp, u32 lp, int x, int y)
{
    struct qmsg *m;
    if (qcount == QSIZE) {
        wlog("w16: message queue full, dropping message %04x\n", msg);
        return;
    }
    m = &queue[(qhead + qcount++) % QSIZE];
    m->hwnd = hwnd; m->msg = msg; m->wp = wp; m->lp = lp; m->x = x; m->y = y;
    m->time = ++msgclock;
}

static void purge(u16 hwnd)
{
    int i, n = qcount, k = 0;
    struct qmsg tmp[QSIZE];
    for (i = 0; i < n; i++) {
        struct qmsg *m = &queue[(qhead + i) % QSIZE];
        if (m->hwnd != hwnd)
            tmp[k++] = *m;
    }
    for (i = 0; i < k; i++)
        queue[i] = tmp[i];
    qhead = 0; qcount = k;
}

static void write_msg(u16 seg, u16 off, struct qmsg *m)
{
    wr16(seg, off + 0, m->hwnd);
    wr16(seg, off + 2, m->msg);
    wr16(seg, off + 4, m->wp);
    wr32(seg, off + 6, m->lp);
    wr32(seg, off + 10, m->time);
    wr16(seg, off + 14, (u16)m->x);
    wr16(seg, off + 16, (u16)m->y);
}

static u16 vk_from_key(GR_KEY ch, u8 *ascii)
{
    *ascii = 0;
    if (ch >= 'a' && ch <= 'z') { *ascii = (u8)ch; return ch - 32; }
    if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) { *ascii = (u8)ch; return ch; }
    switch (ch) {
    case ' ':  *ascii = ' '; return 0x20;
    case 13:   *ascii = 13;  return 0x0D;   /* VK_RETURN */
    case 27:   *ascii = 27;  return 0x1B;   /* VK_ESCAPE */
    case 8:    *ascii = 8;   return 0x08;   /* VK_BACK */
    case 9:    *ascii = 9;   return 0x09;   /* VK_TAB */
    case MWKEY_LEFT:  return 0x25;
    case MWKEY_UP:    return 0x26;
    case MWKEY_RIGHT: return 0x27;
    case MWKEY_DOWN:  return 0x28;
    case MWKEY_DELETE: return 0x2E;
    }
    if (ch > ' ' && ch < 127) { *ascii = (u8)ch; return 0xBA; }   /* punctuation: an OEM key */
    return 0;
}

static void translate_event(GR_EVENT *ev)
{
    struct wnd *w;
    int isclient = 0;

    switch (ev->type) {
    case GR_EVENT_TYPE_EXPOSURE:
        w = win_by_nx(ev->exposure.wid, &isclient);
        if (!w)
            break;
        if (isclient) {
            union_update(w, ev->exposure.x, ev->exposure.y, ev->exposure.x + ev->exposure.width,
                         ev->exposure.y + ev->exposure.height);
            w->erase = 1;
        } else
            draw_nc(w);
        break;
    case GR_EVENT_TYPE_KEY_DOWN: {
        u8 ascii;
        u16 vk = vk_from_key(ev->keystroke.ch, &ascii);
        u32 lp = 1ul | ((u32)(ev->keystroke.scancode & 0xFF) << 16);
        w = win_by_nx(ev->keystroke.wid, &isclient);
        if (!w || !vk)
            break;
        if (keyup_pending)                 /* previous key never got a real key-up */
            post(keyup_hwnd, WM_KEYUP, keyup_vk, keyup_lp | 0xC0000000ul, 0, 0);
        post(w->hwnd, WM_KEYDOWN, vk, lp, 0, 0);
        keychars[kcnext].time = msgclock;
        keychars[kcnext].ch = ascii;
        kcnext = (kcnext + 1) & 7;
        keyup_pending = !saw_real_keyup;
        keyup_hwnd = w->hwnd; keyup_vk = vk; keyup_lp = lp;
        if (opt_verbose)
            wlog("w16: key %04x -> WM_KEYDOWN vk %02x\n", ev->keystroke.ch, vk);
        break;
    }
    case GR_EVENT_TYPE_KEY_UP: {
        u8 ascii;
        u16 vk = vk_from_key(ev->keystroke.ch, &ascii);
        w = win_by_nx(ev->keystroke.wid, &isclient);
        saw_real_keyup = 1;
        keyup_pending = 0;
        if (w && vk)
            post(w->hwnd, WM_KEYUP, vk, 0xC0000001ul | ((u32)(ev->keystroke.scancode & 0xFF) << 16), 0, 0);
        break;
    }
    case GR_EVENT_TYPE_BUTTON_DOWN:
    case GR_EVENT_TYPE_BUTTON_UP: {
        int down = ev->type == GR_EVENT_TYPE_BUTTON_DOWN;
        w = win_by_nx(ev->button.wid, &isclient);
        if (!w)
            break;
        if (isclient) {
            u16 msg = (ev->button.changebuttons & MWBUTTON_R) ? (down ? WM_RBUTTONDOWN : WM_RBUTTONUP)
                                                             : (down ? WM_LBUTTONDOWN : WM_LBUTTONUP);
            u16 mk = (ev->button.buttons & MWBUTTON_L ? 1 : 0) | (ev->button.buttons & MWBUTTON_R ? 2 : 0);
            post(w->hwnd, msg, mk, ((u32)(u16)ev->button.y << 16) | (u16)ev->button.x, ev->button.rootx, ev->button.rooty);
            if (opt_verbose)
                wlog("w16: button %s at %d,%d -> %s\n", down ? "down" : "up", ev->button.x, ev->button.y,
                     msg == WM_LBUTTONDOWN ? "WM_LBUTTONDOWN" : msg == WM_LBUTTONUP ? "WM_LBUTTONUP" : "right button");
        } else if (down && (w->style & WS_SYSMENU)) {
            int b = border_of(w->style);
            if (ev->button.x >= b && ev->button.x < b + CAPH && ev->button.y >= b && ev->button.y < b + CAPH) {
                u32 t = now_ms();
                if (sysclick_armed && t - sysclick_ms < 800) {
                    sysclick_armed = 0;
                    vlog("w16: system menu double-click -> WM_SYSCOMMAND SC_CLOSE\n");
                    post(w->hwnd, WM_SYSCOMMAND, SC_CLOSE, 0, ev->button.rootx, ev->button.rooty);
                } else {
                    sysclick_armed = 1;
                    sysclick_ms = t;
                    vlog("w16: system menu click (the menu itself is not implemented; double-click closes)\n");
                }
            }
        }
        break;
    }
    case GR_EVENT_TYPE_CLOSE_REQ:
        w = win_by_nx(ev->general.wid, &isclient);
        if (w)
            post(w->hwnd, WM_CLOSE, 0, 0, 0, 0);
        break;
    }
}

/* ---------------------------------------------------------------- USER entry points */
u32 u_InitApp(u16 ss, u16 ap) { (void)ss; (void)ap; return 1; }

u32 u_PostQuitMessage(u16 ss, u16 ap)
{
    quit_pending = 1;
    quit_code = AW(0);
    vlog("w16: PostQuitMessage(%u)\n", quit_code);
    return 0;
}

/* GetClientRect(hwnd, lpRect) */
u32 u_GetClientRect(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2);
    struct wnd *w = win_get(AW(4));
    wr16(seg, off, 0); wr16(seg, off + 2, 0);
    wr16(seg, off + 4, w ? w->cw : 0); wr16(seg, off + 6, w ? w->ch : 0);
    return 0;
}

/* BeginPaint(hwnd, lpPaint) */
u32 u_BeginPaint(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2), hdc, ferase = 0;
    struct wnd *w = win_get(AW(4));
    int l, t, r, b;
    if (!w)
        return 0;
    hdc = dc_create(w, 1);
    if (!hdc)
        return 0;
    if (!w->invalid)
        win_invalidate_all(w, 0);
    l = w->ul; t = w->ut; r = w->ur; b = w->ub;
    if (w->erase) {
        w->erase = 0;
        ferase = win_send(w, WM_ERASEBKGND, hdc, 0) ? 0 : 1;
    }
    w->invalid = 0;
    fzero(seg, off, 32);
    wr16(seg, off + 0, hdc);
    wr16(seg, off + 2, ferase);
    wr16(seg, off + 4, l); wr16(seg, off + 6, t); wr16(seg, off + 8, r); wr16(seg, off + 10, b);
    return hdc;
}

/* EndPaint(hwnd, lpPaint) */
u32 u_EndPaint(u16 ss, u16 ap)
{
    u16 hdc = rd16(AW(2), AW(0));
    struct wnd *w = win_get(AW(4));
    dc_release(hdc);
    if (w)
        paint_done(w);
    return 1;
}

static int find_class(u16 off, u16 seg)
{
    char name[24];
    int i, k;
    if (!seg) {
        if (off >= 0xC000 && off < 0xC000 + MAXCLASS && classes[off - 0xC000].used)
            return off - 0xC000;
        return -1;
    }
    fstrncpy_in(name, seg, off, sizeof(name));
    for (i = 0; i < MAXCLASS; i++) {
        if (!classes[i].used)
            continue;
        for (k = 0; name[k] && classes[i].name[k]; k++) {
            char a = name[k], b = classes[i].name[k];
            if (a >= 'a' && a <= 'z') a -= 32;
            if (b >= 'a' && b <= 'z') b -= 32;
            if (a != b)
                break;
        }
        if (!name[k] && !classes[i].name[k])
            return i;
    }
    return -1;
}

/* RegisterClass(lpWndClass) */
u32 u_RegisterClass(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2), noff = rd16(seg, off + 22), nseg = rd16(seg, off + 24);
    struct wclass *c;
    int i;
    if (!nseg) {
        wlog("w16: RegisterClass: class atoms are not supported\n");
        return 0;
    }
    if (find_class(noff, nseg) >= 0)
        return 0;
    for (i = 0; i < MAXCLASS && classes[i].used; i++)
        ;
    if (i == MAXCLASS) {
        wlog("w16: RegisterClass: class table full (%d)\n", MAXCLASS);
        return 0;
    }
    c = &classes[i];
    c->used = 1;
    c->style = rd16(seg, off);
    c->procoff = rd16(seg, off + 2); c->procseg = rd16(seg, off + 4);
    c->cbwndextra = rd16(seg, off + 8);
    c->hinst = rd16(seg, off + 10);
    c->hbrush = rd16(seg, off + 16);
    fstrncpy_in(c->name, nseg, noff, sizeof(c->name));
    if (c->cbwndextra)
        wlog("w16: RegisterClass %s: %u window extra bytes are not provided\n", c->name, c->cbwndextra);
    vlog("w16: RegisterClass \"%s\" wndproc %04x:%04x background %04x\n", c->name, c->procseg, c->procoff, c->hbrush);
    return 0xC000 + i;
}

/* CreateWindow(lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam) */
u32 u_CreateWindow(u16 ss, u16 ap)
{
    u16 hinst = AW(4), hmenu = AW(6), parent = AW(8);
    int h = (i16)AW(10), wd = (i16)AW(12), y = (i16)AW(14), x = (i16)AW(16);
    u32 style = AL(18);
    u16 toff = AW(22), tseg = AW(24), coff = AW(26), cseg = AW(28);
    int ci = find_class(coff, cseg), i, b, cap;
    struct wclass *c;
    struct wnd *w;
    u32 r;
    int isnull;
    GR_COLOR bg;

    if (ci < 0) {
        char nm[24];
        if (cseg)
            fstrncpy_in(nm, cseg, coff, sizeof(nm));
        else
            strcpy(nm, "(atom)");
        wlog("w16: CreateWindow: class \"%s\" is not registered (system control classes are not provided)\n", nm);
        return 0;
    }
    if (style & WS_CHILD) {
        wlog("w16: CreateWindow: child windows are not supported yet\n");
        return 0;
    }
    if (hmenu)
        wlog("w16: CreateWindow: menu handle %04x ignored (menus are not supported)\n", hmenu);
    (void)parent;
    for (i = 0; i < MAXWIN && wins[i].used; i++)
        ;
    if (i == MAXWIN) {
        wlog("w16: CreateWindow: window table full (%d)\n", MAXWIN);
        return 0;
    }
    c = &classes[ci];
    w = &wins[i];
    memset(w, 0, sizeof(*w));
    w->used = 1;
    w->hwnd = HWND_BASE + 4 * i;
    w->cls = ci;
    w->style = style;
    w->hinst = c->hinst ? c->hinst : hinst;
    w->procoff = c->procoff; w->procseg = c->procseg;
    if (x == (int)(i16)0x8000) { x = 16; y = 12; }
    if (wd == (int)(i16)0x8000) { wd = 288; h = 168; }
    if (wd < 40) wd = 40;
    if (h < 30) h = 30;
    w->x = x; w->y = y; w->w = wd; w->h = h;
    b = border_of(style);
    cap = (style & WS_CAPTION) == WS_CAPTION ? CAPH + 1 : 0;
    w->cx = b; w->cy = b + cap; w->cw = wd - 2 * b; w->ch = h - 2 * b - cap;
    if (tseg)
        fstrncpy_in(w->title, tseg, toff, sizeof(w->title));
    bg = rgb_to_gr(brush_color(c->hbrush, &isnull));
    w->frame = GrNewWindowEx(GR_WM_PROPS_NODECORATE | GR_WM_PROPS_NOMOVE, w->title, GR_ROOT_WINDOW_ID,
                             x, y, wd, h, GR_RGB(170, 170, 170));
    w->client = GrNewWindow(w->frame, w->cx, w->cy, w->cw, w->ch, 0, bg, 0);
    GrSelectEvents(w->frame, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_CLOSE_REQ |
                   GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP);
    GrSelectEvents(w->client, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_BUTTON_UP |
                   GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP);
    vlog("w16: CreateWindow \"%s\" class \"%s\" style %08lx at %d,%d %dx%d -> hwnd %04x (client %dx%d)\n",
          w->title, c->name, (unsigned long)style, x, y, wd, h, w->hwnd, w->cw, w->ch);

    /* WM_CREATE with a CREATESTRUCT on the program stack, as USER does */
    {
        u16 cs_off = w16_app_sp - 40, save = w16_app_sp, aseg = w16_app_ss;
        fzero(aseg, cs_off, 32);
        wr32(aseg, cs_off + 0, AL(0));          /* lpCreateParams */
        wr16(aseg, cs_off + 4, hinst);
        wr16(aseg, cs_off + 6, hmenu);
        wr16(aseg, cs_off + 8, parent);
        wr16(aseg, cs_off + 10, h); wr16(aseg, cs_off + 12, wd);
        wr16(aseg, cs_off + 14, y); wr16(aseg, cs_off + 16, x);
        wr32(aseg, cs_off + 18, style);
        wr16(aseg, cs_off + 22, toff); wr16(aseg, cs_off + 24, tseg);
        wr16(aseg, cs_off + 26, coff); wr16(aseg, cs_off + 28, cseg);
        w16_app_sp = cs_off;
        r = win_send(w, WM_CREATE, 0, ((u32)aseg << 16) | cs_off);
        w16_app_sp = save;
    }
    if (r == 0xFFFFFFFFul) {
        vlog("w16: WM_CREATE returned -1; window destroyed\n");
        GrDestroyWindow(w->frame);
        w->used = 0;
        return 0;
    }
    if (style & WS_VISIBLE) {
        GrMapWindow(w->client);
        GrMapWindow(w->frame);
        w->visible = 1;
        win_invalidate_all(w, 1);
    }
    return w->hwnd;
}

/* ShowWindow(hwnd, nCmdShow) */
u32 u_ShowWindow(u16 ss, u16 ap)
{
    u16 cmd = AW(0);
    struct wnd *w = win_get(AW(2));
    u16 prev;
    if (!w)
        return 0;
    prev = w->visible;
    if (cmd == 0) {
        GrUnmapWindow(w->frame);
        w->visible = 0;
        return prev;
    }
    GrMapWindow(w->client);
    GrMapWindow(w->frame);
    GrSetFocus(w->client);
    w->visible = 1;
    if (!w->shown) {
        w->shown = 1;
        win_send(w, WM_SIZE, 0, ((u32)w->ch << 16) | (u16)w->cw);
    }
    win_invalidate_all(w, 1);
    return prev;
}

static void destroy_window(struct wnd *w)
{
    u16 hwnd = w->hwnd;
    if (w->active)                      /* already being destroyed (re-entered from WM_DESTROY) */
        return;
    w->active = 1;
    vlog("w16: DestroyWindow %04x\n", hwnd);
    win_send(w, WM_DESTROY, 0, 0);
    win_send(w, WM_NCDESTROY, 0, 0);
    GrDestroyWindow(w->frame);
    purge(hwnd);
    if (keyup_hwnd == hwnd)
        keyup_pending = 0;
    w->used = 0;
}

/* DestroyWindow(hwnd) */
u32 u_DestroyWindow(u16 ss, u16 ap)
{
    struct wnd *w = win_get(AW(0));
    if (!w)
        return 0;
    destroy_window(w);
    return 1;
}

/* DefWindowProc(hwnd, msg, wParam, lParam) */
u32 u_DefWindowProc(u16 ss, u16 ap)
{
    u16 wp = AW(4), msg = AW(6), hwnd = AW(8);
    struct wnd *w = win_get(hwnd);
    int isnull;
    u32 c;
    if (!w)
        return 0;
    switch (msg) {
    case WM_CLOSE:
        destroy_window(w);
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_CLOSE)
            return win_send(w, WM_CLOSE, 0, 0);
        return 0;
    case WM_ERASEBKGND:
        c = brush_color(classes[w->cls].hbrush, &isnull);
        if (isnull)
            return 0;
        gdi_fill(w->client, rgb_to_gr(c), 0, 0, w->cw, w->ch);
        return 1;
    case WM_PAINT:
        w->invalid = 0;
        w->erase = 0;
        return 0;
    }
    return 0;
}

/* GetMessage(lpMsg, hwnd, wMsgFilterMin, wMsgFilterMax) */
u32 u_GetMessage(u16 ss, u16 ap)
{
    u16 off = AW(6), seg = AW(8), i;
    GR_EVENT ev;
    struct qmsg m;

    for (;;) {
        for (;;) {                      /* take whatever Nano-X already has */
            GrGetNextEventTimeout(&ev, GR_TIMEOUT_POLL);
            if (ev.type == GR_EVENT_TYPE_NONE || ev.type == GR_EVENT_TYPE_TIMEOUT)
                break;
            translate_event(&ev);
        }
        if (qcount) {
            m = queue[qhead];
            qhead = (qhead + 1) % QSIZE;
            qcount--;
            write_msg(seg, off, &m);
            return 1;
        }
        if (quit_pending) {
            memset(&m, 0, sizeof(m));
            m.msg = WM_QUIT; m.wp = quit_code; m.time = ++msgclock;
            quit_pending = 0;
            write_msg(seg, off, &m);
            return 0;
        }
        if (keyup_pending) {
            keyup_pending = 0;
            post(keyup_hwnd, WM_KEYUP, keyup_vk, keyup_lp | 0xC0000000ul, 0, 0);
            continue;
        }
        for (i = 0; i < MAXWIN; i++)
            if (wins[i].used && wins[i].visible && wins[i].invalid) {
                memset(&m, 0, sizeof(m));
                m.hwnd = wins[i].hwnd; m.msg = WM_PAINT; m.time = ++msgclock;
                write_msg(seg, off, &m);
                return 1;
            }
        GrGetNextEvent(&ev);            /* nothing to do: block in the Nano-X client library */
        translate_event(&ev);
    }
}

/* TranslateMessage(lpMsg): WM_KEYDOWN -> WM_CHAR for keys with a character */
u32 u_TranslateMessage(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2), msg = rd16(seg, off + 2);
    u32 t = rd32(seg, off + 10);
    int i;
    if (msg != WM_KEYDOWN)
        return 0;
    for (i = 0; i < 8; i++)
        if (keychars[i].time == t && keychars[i].ch) {
            post(rd16(seg, off), WM_CHAR, keychars[i].ch, rd32(seg, off + 6), 0, 0);
            return 1;
        }
    return 0;
}

/* DispatchMessage(lpMsg) */
u32 u_DispatchMessage(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2);
    struct wnd *w = win_get(rd16(seg, off));
    if (!w)
        return 0;
    return win_send(w, rd16(seg, off + 2), rd16(seg, off + 4), rd32(seg, off + 6));
}

/* UpdateWindow(hwnd) */
u32 u_UpdateWindow(u16 ss, u16 ap)
{
    struct wnd *w = win_get(AW(0));
    if (w && w->visible && w->invalid)
        win_send(w, WM_PAINT, 0, 0);
    return 0;
}

/* InvalidateRect(hwnd, lpRect, bErase) */
u32 u_InvalidateRect(u16 ss, u16 ap)
{
    u16 erase = AW(0), off = AW(2), seg = AW(4), hwnd = AW(6), i;
    for (i = 0; i < MAXWIN; i++) {
        struct wnd *w = &wins[i];
        if (!w->used || (hwnd && w->hwnd != hwnd))
            continue;
        if (!seg && !off)
            win_invalidate_all(w, erase);
        else {
            union_update(w, (i16)rd16(seg, off), (i16)rd16(seg, off + 2), (i16)rd16(seg, off + 4), (i16)rd16(seg, off + 6));
            if (erase)
                w->erase = 1;
        }
    }
    return 0;
}

static u32 load_resource(u16 ss, u16 ap, u16 handle, const char *what)
{
    u16 off = AW(0), seg = AW(2), hinst = AW(4);
    if (!hinst && !seg)
        return handle;                  /* predefined IDC_ / IDI_ */
    wlog("w16: %s from the program's resources is not supported (resource %04x:%04x)\n", what, seg, off);
    return 0;
}

u32 u_LoadCursor(u16 ss, u16 ap) { return load_resource(ss, ap, HCURSOR, "LoadCursor"); }
u32 u_LoadIcon(u16 ss, u16 ap)   { return load_resource(ss, ap, HICON, "LoadIcon"); }

/* wsprintf(lpOutput, lpFormat, ...) - cdecl, all string arguments are far pointers */
u32 u_wsprintf(u16 ss, u16 ap)
{
    u16 ooff = AW(0), oseg = AW(2), foff = AW(4), fseg = AW(6), va = ap + 8, n = 0;
    char c;
    for (;;) {
        char tmp[12], *p;
        int left = 0, zero = 0, width = 0, islong = 0, len, neg;
        u32 v;
        c = (char)rd8(fseg, foff++);
        if (!c)
            break;
        if (c != '%') {
            if (n < 1023) wr8(oseg, ooff + n++, c);
            continue;
        }
        c = (char)rd8(fseg, foff++);
        if (c == '-') { left = 1; c = (char)rd8(fseg, foff++); }
        if (c == '0') { zero = 1; c = (char)rd8(fseg, foff++); }
        while (c >= '0' && c <= '9') { width = width * 10 + c - '0'; c = (char)rd8(fseg, foff++); }
        if (c == 'l' || c == 'L') { islong = 1; c = (char)rd8(fseg, foff++); }
        p = tmp + sizeof(tmp) - 1; *p = 0; neg = 0;
        switch (c) {
        case 'd': case 'i': case 'u': case 'x': case 'X':
            if (islong) { v = rd32(ss, va); va += 4; } else { v = rd16(ss, va); va += 2; if (c == 'd' || c == 'i') v = (u32)(i32)(i16)v; }
            if ((c == 'd' || c == 'i') && (i32)v < 0) { neg = 1; v = (u32)(-(i32)v); }
            if (!islong && (c == 'd' || c == 'i')) v &= 0xFFFFul;
            do {
                unsigned dgt = (unsigned)(v % ((c == 'x' || c == 'X') ? 16 : 10));
                *--p = dgt < 10 ? '0' + dgt : (c == 'x' ? 'a' : 'A') + dgt - 10;
                v /= (c == 'x' || c == 'X') ? 16 : 10;
            } while (v);
            if (neg) *--p = '-';
            break;
        case 'c':
            *--p = (char)rd16(ss, va); va += 2;
            break;
        case 's': {
            u16 so = rd16(ss, va), sg = rd16(ss, va + 2), k;
            va += 4;
            len = 0;
            while (rd8(sg, so + len) && len < 512) len++;
            for (k = 0; !left && (int)k + len < width; k++) if (n < 1023) wr8(oseg, ooff + n++, ' ');
            for (k = 0; k < len; k++) if (n < 1023) wr8(oseg, ooff + n++, rd8(sg, so + k));
            for (k = 0; left && (int)k + len < width; k++) if (n < 1023) wr8(oseg, ooff + n++, ' ');
            continue;
        }
        case '%':
            *--p = '%';
            break;
        default:
            wlog("w16: wsprintf: format %%%c not supported\n", c);
            *--p = '?';
            break;
        }
        len = strlen(p);
        while (!left && len < width) { if (n < 1023) wr8(oseg, ooff + n++, zero ? '0' : ' '); width--; }
        while (*p) if (n < 1023) wr8(oseg, ooff + n++, *p++); else p++;
        while (left && len < width) { if (n < 1023) wr8(oseg, ooff + n++, ' '); width--; }
    }
    wr8(oseg, ooff + n, 0);
    return n;
}

/* ---------------------------------------------------------------- paint completion hooks */
void paint_done(struct wnd *w)
{
    GR_SCREEN_INFO si;
    char line[48];
    npaints++;
    GrFlush();
    GrGetScreenInfo(&si);               /* round trip: the server has drawn everything before this */
    sprintf(line, "@@PAINT %d hwnd %04x\n", npaints, w->hwnd);
    if (opt_debugport)
        e9puts(line);
    vlog("w16: paint %d of hwnd %04x complete\n", npaints, w->hwnd);
    if (opt_dump[0]) {
        char tag[12];
        sprintf(tag, "%d", npaints);
        w16_dump_frame(tag);
    }
    if (opt_ready && npaints == 1) {
        int fd = open(readyfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            write(fd, "painted\n", 8);
            close(fd);
        }
    }
}

int user_init(void)
{
    GR_SCREEN_INFO si;
    GR_SIZE w, h, b;
    if (GrOpen() < 0) {
        wlog("w16: cannot connect to the Nano-X server (start nano-color first)\n");
        return -1;
    }
    GrGetScreenInfo(&si);
    vlog("w16: Nano-X screen %dx%d, %ld colours\n", si.cols, si.rows, (long)si.ncolors);
    memset(wins, 0, sizeof(wins));
    memset(classes, 0, sizeof(classes));
    if (gdi_init())
        return -1;
    GrGetGCTextSize(gdi_gc, "Ag", 2, GR_TFASCII, &w, &h, &b);
    if (h > 6 && h < 20)
        font_h = h;
    return 0;
}

void user_shutdown(void)
{
    int i, n = 0;
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used) {
            GrDestroyWindow(wins[i].frame);
            wins[i].used = 0;
            n++;
        }
    if (n)
        vlog("w16: destroyed %d window(s) the program left open\n", n);
    gdi_shutdown();
    GrClose();
}
