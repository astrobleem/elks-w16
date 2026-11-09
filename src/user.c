/*
 * user.c - USER: window classes, windows, the message queue and painting,
 * mapped onto Nano-X. Each Win16 top-level window is an undecorated Nano-X
 * window (frame, caption and border drawn here) with a child Nano-X window
 * for the client area, so GDI output is clipped to the client like on Windows.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include "wapi.h"

struct wnd wins[MAXWIN];
static struct wclass classes[MAXCLASS];

#define QSIZE 24
struct qmsg { u16 hwnd, msg, wp; u32 lp; i16 x, y; u32 time; u8 ch; };
static struct qmsg queue[QSIZE];
static int qhead, qcount;
static int quit_pending;
static u16 quit_code;
static u32 msgclock;
/* characters of the key-down messages most recently handed to the program, for TranslateMessage;
 * the character travels in the queue entry until then, so a burst of keys cannot overwrite it */
static struct { u32 time; u8 ch; } keychars[8];
static int kcnext;
static u16 keyup_hwnd, keyup_vk;
static u32 keyup_lp;
static int keyup_pending, saw_real_keyup;
static int npaints;
int font_h = 13;
static int sysclick_armed;
static u32 sysclick_ms;
char opt_dump[40];
int  opt_ready;
static char readyfile[40] = "/tmp/w16.ready";
static u16 capture_hwnd, focus_hwnd;    /* SetCapture / SetFocus */
static u16 key_mods;                    /* MWKMOD_* of the latest key event (GetKeyState, accelerators) */
static u16 mouse_buttons;               /* MWBUTTON_* of the latest mouse event */

#define CAPH  (font_h + 1)

u32 now_ms(void)
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
    if (!w->procoff && !w->procseg)     /* built-in controls have no program window procedure */
        return 0;
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

    /* Nano-X does not clip a window's drawing against its own children, so paint only the
     * non-client strips and never the client area (or the controls inside it). */
    {
        int top = b + ((w->style & WS_CAPTION) == WS_CAPTION ? CAPH + 1 : 0);
        gdi_fill(f, gray, 0, 0, W, top);
        gdi_fill(f, gray, 0, H - b, W, b);
        gdi_fill(f, gray, 0, top, b, H - b - top);
        gdi_fill(f, gray, W - b, top, b, H - b - top);
    }
    gdi_fg(black);
    GrRect(f, gc, 0, 0, W, H);
    if (b > 1)
        GrRect(f, gc, b - 1, b - 1, W - 2 * (b - 1), H - 2 * (b - 1));
    if ((w->style & WS_CAPTION) == WS_CAPTION) {
        int x0 = b, x1 = W - b, y = b;
        gdi_fill(f, GR_RGB(0, 0, 170), x0, y, x1 - x0, CAPH);
        if (w->style & WS_SYSMENU) {
            gdi_fill(f, gray, x0, y, CAPH, CAPH);
            gdi_fill(f, white, x0 + 3, y + CAPH / 2 - 1, CAPH - 6, 2);
            gdi_fg(black);
            GrLine(f, gc, x0 + 4, y + CAPH / 2 + 1, x0 + CAPH - 3, y + CAPH / 2 + 1);
            GrLine(f, gc, x0 + CAPH, y, x0 + CAPH, y + CAPH - 1);
            x0 += CAPH + 1;
        }
        if (w->style & WS_MAXIMIZEBOX) {
            int bx = x1 - CAPH, k;
            gdi_fill(f, gray, bx, y, CAPH, CAPH);
            gdi_fg(black);
            GrLine(f, gc, bx - 1, y, bx - 1, y + CAPH - 1);
            for (k = 0; k < 4; k++)                    /* up arrow */
                GrLine(f, gc, bx + CAPH / 2 - k, y + 4 + k, bx + CAPH / 2 + k, y + 4 + k);
            x1 = bx - 1;
        }
        if (w->style & WS_MINIMIZEBOX) {
            int bx = x1 - CAPH, k;
            gdi_fill(f, gray, bx, y, CAPH, CAPH);
            gdi_fg(black);
            GrLine(f, gc, bx - 1, y, bx - 1, y + CAPH - 1);
            for (k = 0; k < 4; k++)                    /* down arrow */
                GrLine(f, gc, bx + CAPH / 2 - k, y + CAPH - 5 - k, bx + CAPH / 2 + k, y + CAPH - 5 - k);
            x1 = bx - 1;
        }
        GrGetGCTextSize(gc, w->title, n, GR_TFASCII, &tw, &th, &tb);
        gdi_fg(white);
        gdi_usebg(0);
        GrText(f, gc, x0 + ((x1 - x0) - tw) / 2 > x0 ? x0 + ((x1 - x0) - tw) / 2 : x0, y + 1, w->title, n,
               GR_TFASCII | GR_TFTOP);
        gdi_fg(black);
        GrLine(f, gc, b, b + CAPH, W - b - 1, b + CAPH);
    }
    if (w->hmenu)
        menu_draw_bar(w, b + ((w->style & WS_CAPTION) == WS_CAPTION ? CAPH + 1 : 0));
}

/* ---------------------------------------------------------------- queue */
static int post(u16 hwnd, u16 msg, u16 wp, u32 lp, int x, int y)
{
    struct qmsg *m;
    if (qcount == QSIZE) {
        wlog("w16: message queue full, dropping message %04x\n", msg);
        return 0;
    }
    m = &queue[(qhead + qcount++) % QSIZE];
    m->hwnd = hwnd; m->msg = msg; m->wp = wp; m->lp = lp; m->x = x; m->y = y; m->ch = 0;
    m->time = ++msgclock;
    return 1;
}

/* take the oldest queued message; a key-down's character is kept for TranslateMessage */
static void dequeue(struct qmsg *m)
{
    *m = queue[qhead];
    qhead = (qhead + 1) % QSIZE;
    qcount--;
    if (m->msg == WM_KEYDOWN) {
        keychars[kcnext].time = m->time;
        keychars[kcnext].ch = m->ch;
        kcnext = (kcnext + 1) & 7;
    }
}

void w16_post(u16 hwnd, u16 msg, u16 wp, u32 lp)
{
    post(hwnd, msg, wp, lp, 0, 0);
}

/* ---------------------------------------------------------------- timers */
#define MAXTIMER 8
static struct { u8 used; u16 hwnd, id, procoff, procseg; u32 period, due; } timers[MAXTIMER];

static void kill_timers(u16 hwnd)
{
    int i;
    for (i = 0; i < MAXTIMER; i++)
        if (timers[i].used && timers[i].hwnd == hwnd)
            timers[i].used = 0;
}

/* SetTimer(hWnd, nIDEvent, wElapse, lpTimerFunc) */
u32 u_SetTimer(u16 ss, u16 ap)
{
    u16 poff = AW(0), pseg = AW(2), ms = AW(4), id = AW(6), hwnd = AW(8);
    int i, f = -1;
    if (hwnd && !win_get(hwnd))
        return 0;
    for (i = 0; i < MAXTIMER; i++) {
        if (timers[i].used && hwnd && timers[i].hwnd == hwnd && timers[i].id == id)
            break;                              /* same window and id: reset that timer */
        if (!timers[i].used && f < 0)
            f = i;
    }
    if (i == MAXTIMER) {
        if (f < 0) {
            wlog("w16: SetTimer: all %d timers are in use\n", MAXTIMER);
            return 0;
        }
        i = f;
    }
    if (!hwnd)
        id = (u16)(0x100 + i);                  /* windowless timers get an id of their own */
    timers[i].used = 1;
    timers[i].hwnd = hwnd; timers[i].id = id;
    timers[i].procoff = poff; timers[i].procseg = pseg;
    timers[i].period = ms ? ms : 1;
    timers[i].due = now_ms() + timers[i].period;
    vlog("w16: SetTimer(%04x, %u, %u ms%s) -> %u\n", hwnd, id, ms, pseg ? ", callback" : "", id);
    return id;
}

/* KillTimer(hWnd, nIDEvent) */
u32 u_KillTimer(u16 ss, u16 ap)
{
    u16 id = AW(0), hwnd = AW(2);
    int i;
    for (i = 0; i < MAXTIMER; i++)
        if (timers[i].used && timers[i].hwnd == hwnd && timers[i].id == id) {
            timers[i].used = 0;
            vlog("w16: KillTimer(%04x, %u)\n", hwnd, id);
            return 1;
        }
    return 0;
}

/* ---------------------------------------------------------------- measurements (-m) */
int opt_metrics, opt_quit_s;
u32 w16_t0;
static u32 lat_t;
static u8 lat_on;
static const char *lat_what;
static u32 tk_n, tk_sum, tk_max, tk_hist[6];

static void lat_input(const char *what)
{
    if (!opt_metrics)
        return;
    lat_t = now_ms();
    lat_on = 1;
    lat_what = what;
}

/* the screen shows the response to the last input: report the delay once */
void lat_shown(const char *what, int synced)
{
    GR_SCREEN_INFO si;
    char line[64];
    u32 dt;
    if (!lat_on)
        return;
    if (!synced) {                          /* round trip: the server has drawn everything */
        GrFlush();
        GrGetScreenInfo(&si);
    }
    lat_on = 0;
    dt = now_ms() - lat_t;
    sprintf(line, "@@LAT %s %s %lu\n", lat_what, what, (unsigned long)dt);
    if (opt_debugport)
        e9puts(line);
    wlog("w16: latency from %s to %s: %lu ms\n", lat_what, what, (unsigned long)dt);
}

static void tick_done(u32 t)
{
    GR_SCREEN_INFO si;
    static const u16 lim[5] = { 10, 50, 100, 200, 500 };
    u32 dt;
    int k;
    GrFlush();
    GrGetScreenInfo(&si);
    dt = now_ms() - t;
    tk_n++;
    tk_sum += dt;
    if (dt > tk_max)
        tk_max = dt;
    for (k = 0; k < 5 && dt >= lim[k]; k++)
        ;
    tk_hist[k]++;
    if (!(tk_n & 63))                       /* about every half minute at CLOCK's 450 ms */
        wlog("w16: timer messages %lu, %lu ms in all, longest %lu ms; under 10/50/100/200/500 ms and over: "
             "%lu %lu %lu %lu %lu %lu\n", (unsigned long)tk_n, (unsigned long)tk_sum, (unsigned long)tk_max,
             (unsigned long)tk_hist[0], (unsigned long)tk_hist[1], (unsigned long)tk_hist[2],
             (unsigned long)tk_hist[3], (unsigned long)tk_hist[4], (unsigned long)tk_hist[5]);
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
    case MWKEY_INSERT: return 0x2D;
    case MWKEY_HOME:   return 0x24;
    case MWKEY_END:    return 0x23;
    case MWKEY_PAGEUP: return 0x21;
    case MWKEY_PAGEDOWN: return 0x22;
    }
    if (ch >= MWKEY_F1 && ch <= MWKEY_F12)
        return (u16)(0x70 + (ch - MWKEY_F1));           /* VK_F1.. */
    if (ch > ' ' && ch < 127) { *ascii = (u8)ch; return 0xBA; }   /* punctuation: an OEM key */
    return 0;
}

static void paint_control(struct wnd *w);

static void post_button(struct wnd *w, int down, u16 changed, u16 buttons, int x, int y, int rx, int ry)
{
    u16 msg = (changed & MWBUTTON_R) ? (down ? WM_RBUTTONDOWN : WM_RBUTTONUP) : (down ? WM_LBUTTONDOWN : WM_LBUTTONUP);
    u16 mk = (buttons & MWBUTTON_L ? 1 : 0) | (buttons & MWBUTTON_R ? 2 : 0);
    post(w->hwnd, msg, mk, ((u32)(u16)y << 16) | (u16)x, rx, ry);
    if (opt_verbose)
        wlog("w16: button %s at %d,%d -> %s\n", down ? "down" : "up", x, y,
             msg == WM_LBUTTONDOWN ? "WM_LBUTTONDOWN" : msg == WM_LBUTTONUP ? "WM_LBUTTONUP" : "right button");
}

static void translate_event(GR_EVENT *ev)
{
    struct wnd *w;
    int isclient = 0;

    if (ev->type == GR_EVENT_TYPE_BUTTON_DOWN)
        lat_input("click");
    else if (ev->type == GR_EVENT_TYPE_KEY_DOWN)
        lat_input("key");
    if (menu_event(ev))                    /* an open menu takes the input */
        return;
    switch (ev->type) {
    case GR_EVENT_TYPE_EXPOSURE:
        w = win_by_nx(ev->exposure.wid, &isclient);
        if (!w)
            break;
        if (w->ctl)
            paint_control(w);
        else if (isclient) {
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
        key_mods = ev->keystroke.modifiers;
        w = win_by_nx(ev->keystroke.wid, &isclient);
        if (w && !w->ctl && w->hmenu && menu_key(w, ev->keystroke.ch, ev->keystroke.modifiers))
            break;
        if (w && ev->keystroke.ch == MWKEY_F4 && (ev->keystroke.modifiers & MWKMOD_ALT)) {
            post(w->ctl ? w->parent : w->hwnd, WM_SYSCOMMAND, SC_CLOSE, 0, 0, 0);
            break;
        }
        if (!w || !vk)
            break;
        if (keyup_pending)                 /* previous key never got a real key-up */
            post(keyup_hwnd, WM_KEYUP, keyup_vk, keyup_lp | 0xC0000000ul, 0, 0);
        if (post(w->hwnd, WM_KEYDOWN, vk, lp, 0, 0))
            queue[(qhead + qcount - 1) % QSIZE].ch = ascii;
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
        mouse_buttons = ev->button.buttons;
        if (capture_hwnd && (!w || !w->ctl)) {  /* SetCapture: button events go to that window */
            struct wnd *cw = win_get(capture_hwnd);
            if (cw) {
                post_button(cw, down, ev->button.changebuttons, ev->button.buttons,
                            ev->button.rootx - (cw->x + cw->cx), ev->button.rooty - (cw->y + cw->cy),
                            ev->button.rootx, ev->button.rooty);
                break;
            }
        }
        if (!w)
            break;
        if (w->ctl) {                   /* built-in control: push buttons notify their dialog */
            if (w->ctl == CTL_BUTTON && (ev->button.changebuttons & MWBUTTON_L)) {
                if (down) {
                    w->pressed = 1;
                    paint_control(w);
                } else if (w->pressed) {
                    w->pressed = 0;
                    paint_control(w);
                    vlog("w16: button control %u clicked -> WM_COMMAND to %04x\n", w->id, w->parent);
                    post(w->parent, WM_COMMAND, w->id, ((u32)BN_CLICKED << 16) | w->hwnd, ev->button.rootx, ev->button.rooty);
                }
            }
        } else if (isclient) {
            post_button(w, down, ev->button.changebuttons, ev->button.buttons, ev->button.x, ev->button.y,
                        ev->button.rootx, ev->button.rooty);
        } else if (down && w->hmenu && menu_bar_click(w, ev->button.x, ev->button.y)) {
            ;                               /* the menu bar opened or chose an item */
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
    case GR_EVENT_TYPE_MOUSE_POSITION: {
        struct wnd *t;
        int x = ev->mouse.x, y = ev->mouse.y;
        u16 mk;
        w = win_by_nx(ev->mouse.wid, &isclient);
        mouse_buttons = ev->mouse.buttons;
        if (capture_hwnd && (t = win_get(capture_hwnd)) != 0) {
            x = ev->mouse.rootx - (t->x + t->cx);
            y = ev->mouse.rooty - (t->y + t->cy);
        } else if (!w || !isclient || w->ctl)
            break;
        else
            t = w;
        mk = (mouse_buttons & MWBUTTON_L ? 1 : 0) | (mouse_buttons & MWBUTTON_R ? 2 : 0);
        /* like Windows, only the latest position is kept while the program has not read the previous one */
        if (qcount && queue[(qhead + qcount - 1) % QSIZE].msg == WM_MOUSEMOVE &&
            queue[(qhead + qcount - 1) % QSIZE].hwnd == t->hwnd) {
            struct qmsg *m = &queue[(qhead + qcount - 1) % QSIZE];
            m->wp = mk; m->lp = ((u32)(u16)y << 16) | (u16)x; m->x = ev->mouse.rootx; m->y = ev->mouse.rooty;
        } else
            post(t->hwnd, WM_MOUSEMOVE, mk, ((u32)(u16)y << 16) | (u16)x, ev->mouse.rootx, ev->mouse.rooty);
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
    {
        char mn[34];
        u16 mid;
        res_name(rd16(seg, off + 18), rd16(seg, off + 20), mn, &mid);
        if (mn[0])
            strncpy(c->menu, mn, sizeof(c->menu) - 1);
        else if (mid)
            sprintf(c->menu, "#%u", mid);
        else
            c->menu[0] = 0;
    }
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
        wlog("w16: CreateWindow: menu handle %04x ignored (only class menus are supported)\n", hmenu);
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
    if (c->menu[0] && !hmenu) {
        u16 mid = c->menu[0] == '#' ? (u16)atoi(c->menu + 1) : 0;
        if (menu_load(w, mid ? 0 : c->menu, mid) && w->ch > MENUH) {
            w->cy += MENUH;
            w->ch -= MENUH;
        }
    }
    if (tseg)
        fstrncpy_in(w->title, tseg, toff, sizeof(w->title));
    bg = rgb_to_gr(brush_color(c->hbrush, &isnull));
    w->frame = GrNewWindowEx(GR_WM_PROPS_NODECORATE | GR_WM_PROPS_NOMOVE, w->title, GR_ROOT_WINDOW_ID,
                             x, y, wd, h, GR_RGB(170, 170, 170));
    w->client = GrNewWindow(w->frame, w->cx, w->cy, w->cw, w->ch, 0, bg, 0);
    GrSelectEvents(w->frame, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_CLOSE_REQ |
                   GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP);
    GrSelectEvents(w->client, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_BUTTON_UP |
                   GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP | GR_EVENT_MASK_MOUSE_POSITION);
    cursor_attach(w->frame, w->client);
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

/* forget the built-in controls of a destroyed dialog (their Nano-X windows go with the parent) */
static void free_controls(u16 parent)
{
    int i;
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used && wins[i].ctl && wins[i].parent == parent) {
            purge(wins[i].hwnd);
            if (keyup_hwnd == wins[i].hwnd)
                keyup_pending = 0;
            if (wins[i].icon)
                free(wins[i].icon);
            wins[i].icon = 0;
            wins[i].used = 0;
        }
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
    menu_close_for(hwnd);
    GrDestroyWindow(w->frame);          /* also destroys the Nano-X windows of its controls */
    kill_timers(hwnd);
    if (capture_hwnd == hwnd)
        capture_hwnd = 0;
    if (focus_hwnd == hwnd)
        focus_hwnd = 0;
    if (w->hmenu)
        menu_free(w->hmenu);
    w->hmenu = 0;
    purge(hwnd);
    if (keyup_hwnd == hwnd)
        keyup_pending = 0;
    free_controls(hwnd);
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
        if (w->cls < 0) {               /* dialogs and controls: white background */
            gdi_fill(w->client, GR_RGB(255, 255, 255), 0, 0, w->cw, w->ch);
            return 1;
        }
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

/* The next message for this task, in Windows order: posted and input messages, WM_QUIT,
 * then WM_PAINT for invalid windows; blocks in the Nano-X client library when idle. */
static int next_msg(struct qmsg *m, int block)
{
    GR_EVENT ev;
    u16 i;
    static u8 quit_sent;

    for (;;) {
        if (opt_quit_s && !quit_sent && now_ms() - w16_t0 >= (u32)opt_quit_s * 1000ul) {
            quit_sent = 1;                  /* -q: close the main window as the system menu would */
            for (i = 0; i < MAXWIN; i++)
                if (wins[i].used && !wins[i].ctl && !wins[i].isdialog) {
                    vlog("w16: -q: closing window %04x after %d s\n", wins[i].hwnd, opt_quit_s);
                    post(wins[i].hwnd, WM_SYSCOMMAND, SC_CLOSE, 0, 0, 0);
                    break;
                }
        }
        for (;;) {                      /* take whatever Nano-X already has */
            GrGetNextEventTimeout(&ev, GR_TIMEOUT_POLL);
            if (ev.type == GR_EVENT_TYPE_NONE || ev.type == GR_EVENT_TYPE_TIMEOUT)
                break;
            translate_event(&ev);
        }
        if (qcount) {
            dequeue(m);
            return 1;
        }
        if (quit_pending) {
            memset(m, 0, sizeof(*m));
            m->msg = WM_QUIT; m->wp = quit_code; m->time = ++msgclock;
            quit_pending = 0;
            return 1;
        }
        if (keyup_pending) {
            keyup_pending = 0;
            post(keyup_hwnd, WM_KEYUP, keyup_vk, keyup_lp | 0xC0000000ul, 0, 0);
            continue;
        }
        for (i = 0; i < MAXWIN; i++)
            if (wins[i].used && wins[i].ctl && wins[i].invalid) {   /* controls paint themselves */
                wins[i].invalid = 0;
                if (wins[i].visible)
                    paint_control(&wins[i]);
            }
        for (i = 0; i < MAXWIN; i++)
            if (wins[i].used && wins[i].visible && wins[i].invalid) {
                memset(m, 0, sizeof(*m));
                m->hwnd = wins[i].hwnd; m->msg = WM_PAINT; m->time = ++msgclock;
                return 1;
            }
        {                               /* timers: one WM_TIMER per elapsed timer, after paints, like Windows */
            static int rr;
            u32 now = now_ms(), wait = 0xFFFFFFFFul;
            int k;
            for (k = 0; k < MAXTIMER; k++) {
                int t = (rr + k) % MAXTIMER;
                long left;
                if (!timers[t].used)
                    continue;
                left = (long)(timers[t].due - now);
                if (left <= 0) {
                    timers[t].due = now + timers[t].period;
                    rr = t + 1;
                    memset(m, 0, sizeof(*m));
                    m->hwnd = timers[t].hwnd; m->msg = WM_TIMER; m->wp = timers[t].id;
                    m->lp = ((u32)timers[t].procseg << 16) | timers[t].procoff;
                    m->time = ++msgclock;
                    return 1;
                }
                if ((u32)left < wait)
                    wait = (u32)left;
            }
            if (!block)                         /* PeekMessage: nothing to report */
                return 0;
            if (wait != 0xFFFFFFFFul) {         /* sleep in the client library until input or the next timer */
                GrGetNextEventTimeout(&ev, wait);
                if (ev.type != GR_EVENT_TYPE_NONE && ev.type != GR_EVENT_TYPE_TIMEOUT)
                    translate_event(&ev);
                continue;
            }
        }
        GrGetNextEvent(&ev);            /* nothing to do: block in the Nano-X client library */
        translate_event(&ev);
    }
}

/* deliver one message to its window procedure; WM_TIMER with a callback calls the callback */
static u32 dispatch(u16 hwnd, u16 msg, u16 wp, u32 lp)
{
    struct wnd *w = win_get(hwnd);
    u32 r, t = 0;
    if (msg == WM_TIMER && lp) {
        u16 words[5];
        u32 tm = now_ms();
        words[0] = hwnd; words[1] = msg; words[2] = wp; words[3] = (u16)(tm >> 16); words[4] = (u16)tm;
        if (opt_metrics)
            t = now_ms();
        r = w16_call16((u16)lp, (u16)(lp >> 16), w16_mod->dgroup, 5, words);
        if (opt_metrics)
            tick_done(t);
        return r;
    }
    if (!w)
        return 0;
    if (msg == WM_TIMER && opt_metrics)
        t = now_ms();
    r = win_send(w, msg, wp, lp);
    if (msg == WM_TIMER && opt_metrics)
        tick_done(t);
    if (msg == WM_COMMAND && !lp)           /* a menu command has run */
        w16_census("menu command");
    return r;
}

/* GetMessage(lpMsg, hwnd, wMsgFilterMin, wMsgFilterMax) */
u32 u_GetMessage(u16 ss, u16 ap)
{
    u16 off = AW(6), seg = AW(8);
    struct qmsg m;
    next_msg(&m, 1);
    write_msg(seg, off, &m);
    return m.msg != WM_QUIT;
}

/* PeekMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg): like GetMessage, but returns 0
 * instead of waiting. PM_NOREMOVE looks at the queue without taking from it; the WM_PAINT and WM_TIMER
 * that are made up on demand are only reported when removing (a look would cost a timer its tick). */
u32 u_PeekMessage(u16 ss, u16 ap)
{
    u16 off = AW(8), seg = AW(10), flags = AW(0);
    struct qmsg m;
    GR_EVENT ev;
    if (!(flags & 1)) {                 /* PM_NOREMOVE */
        for (;;) {
            GrGetNextEventTimeout(&ev, GR_TIMEOUT_POLL);
            if (ev.type == GR_EVENT_TYPE_NONE || ev.type == GR_EVENT_TYPE_TIMEOUT)
                break;
            translate_event(&ev);
        }
        if (!qcount)
            return 0;
        write_msg(seg, off, &queue[qhead]);
        return 1;
    }
    if (!next_msg(&m, 0))
        return 0;
    write_msg(seg, off, &m);
    return 1;
}

/* PostMessage(hWnd, wMsg, wParam, lParam) */
u32 u_PostMessage(u16 ss, u16 ap)
{
    u16 hwnd = AW(8);
    if (hwnd == 0xFFFF) {
        wlog("w16: PostMessage to every window is not supported\n");
        return 0;
    }
    if (hwnd && !win_get(hwnd))
        return 0;
    return post(hwnd, AW(6), AW(4), AL(0), 0, 0);
}

/* SetCapture(hWnd) / ReleaseCapture(): mouse events go to the capture window wherever the pointer is */
u32 u_SetCapture(u16 ss, u16 ap)
{
    u16 prev = capture_hwnd;
    capture_hwnd = win_get(AW(0)) ? AW(0) : 0;
    return prev;
}

u32 u_ReleaseCapture(u16 ss, u16 ap)
{
    (void)ss; (void)ap;
    capture_hwnd = 0;
    return 0;
}

/* SetFocus(hWnd): returns the window that had the focus */
u32 u_SetFocus(u16 ss, u16 ap)
{
    struct wnd *w = win_get(AW(0));
    u16 prev = focus_hwnd;
    if (!w)
        return 0;
    focus_hwnd = w->hwnd;
    GrSetFocus(w->ctl ? w->frame : w->client);
    return prev;
}

u32 u_IsIconic(u16 ss, u16 ap) { (void)ss; (void)ap; return 0; }   /* windows are never minimised here */

/* SetWindowText(hWnd, lpString) */
u32 u_SetWindowText(u16 ss, u16 ap)
{
    struct wnd *w = win_get(AW(4));
    if (!w)
        return 0;
    fstrncpy_in(w->title, AW(2), AW(0), sizeof(w->title));
    vlog("w16: SetWindowText(%04x) \"%s\"\n", AW(4), w->title);
    if (w->visible && w->ctl)
        paint_control(w);
    else if (w->visible)
        draw_nc(w);
    return 0;
}

/* ClientToScreen(hWnd, lpPoint) */
u32 u_ClientToScreen(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2);
    struct wnd *w = win_get(AW(4));
    if (!w)
        return 0;
    wr16(seg, off, (u16)(rd16(seg, off) + w->x + w->cx));
    wr16(seg, off + 2, (u16)(rd16(seg, off + 2) + w->y + w->cy));
    return 0;
}

/* GetKeyState(nVirtKey): the state at the latest input event; 0x8000 when down */
u32 u_GetKeyState(u16 ss, u16 ap)
{
    u16 down = 0;
    switch (AW(0)) {
    case 0x01: down = (mouse_buttons & MWBUTTON_L) != 0; break;   /* VK_LBUTTON */
    case 0x02: down = (mouse_buttons & MWBUTTON_R) != 0; break;   /* VK_RBUTTON */
    case 0x04: down = (mouse_buttons & MWBUTTON_M) != 0; break;   /* VK_MBUTTON */
    case 0x10: down = (key_mods & MWKMOD_SHIFT) != 0; break;      /* VK_SHIFT */
    case 0x11: down = (key_mods & MWKMOD_CTRL) != 0; break;       /* VK_CONTROL */
    case 0x12: down = (key_mods & MWKMOD_ALT) != 0; break;        /* VK_MENU */
    }
    return down ? 0x8000 : 0;
}

u16 user_key_mods(void) { return key_mods; }

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
    return dispatch(rd16(seg, off), rd16(seg, off + 2), rd16(seg, off + 4), rd32(seg, off + 6));
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

/* LoadIcon: a predefined icon, or the program's own if the resource exists (the icon drawn in the
 * frame stands in for either; LoadCursor is in cursor.c) */
static u32 load_resource(u16 ss, u16 ap, u16 handle, u16 type, const char *what)
{
    u16 off = AW(0), seg = AW(2), hinst = AW(4), id, len;
    char name[34];
    u32 foff;
    if (!hinst)
        return handle;                  /* predefined IDC_ / IDI_ */
    res_name(off, seg, name, &id);
    if (!ne_res_find(w16_mod, type, name[0] ? name : 0, id, &foff, &len)) {
        vlog("w16: %s %s%s: the program's resource stands in as the standard one\n", what, name[0] ? name : "#",
             name[0] ? "" : "(ordinal)");
        return handle;
    }
    wlog("w16: %s: resource %s not found\n", what, name[0] ? name : "(ordinal)");
    return 0;
}

u32 u_LoadIcon(u16 ss, u16 ap)   { return load_resource(ss, ap, HICON, RT_GROUP_ICON, "LoadIcon"); }

/* GetDC(hWnd): the client area, or the screen for hWnd 0 */
u32 u_GetDC(u16 ss, u16 ap)
{
    u16 hwnd = AW(0);
    struct wnd *w = win_get(hwnd);
    if (hwnd && !w)
        return 0;
    return dc_create(w, 0);
}

/* ReleaseDC(hWnd, hDC) */
u32 u_ReleaseDC(u16 ss, u16 ap)
{
    struct dc *d = dc_get(AW(0));
    if (!d || d->paint)
        return 0;
    dc_release(AW(0));
    return 1;
}

/* SetRect(lpRect, left, top, right, bottom) / OffsetRect(lpRect, x, y) / InflateRect(lpRect, x, y) */
u32 u_SetRect(u16 ss, u16 ap)
{
    u16 off = AW(8), seg = AW(10);
    wr16(seg, off, AW(6)); wr16(seg, off + 2, AW(4)); wr16(seg, off + 4, AW(2)); wr16(seg, off + 6, AW(0));
    return 0;
}

static void adjust_rect(u16 ss, u16 ap, int inflate)
{
    u16 off = AW(4), seg = AW(6), dx = AW(2), dy = AW(0);
    wr16(seg, off, (u16)(rd16(seg, off) + (inflate ? -dx : dx)));
    wr16(seg, off + 2, (u16)(rd16(seg, off + 2) + (inflate ? -dy : dy)));
    wr16(seg, off + 4, (u16)(rd16(seg, off + 4) + dx));
    wr16(seg, off + 6, (u16)(rd16(seg, off + 6) + dy));
}

u32 u_OffsetRect(u16 ss, u16 ap)  { adjust_rect(ss, ap, 0); return 0; }
u32 u_InflateRect(u16 ss, u16 ap) { adjust_rect(ss, ap, 1); return 0; }

/* GetSysColor(nIndex) */
u32 u_GetSysColor(u16 ss, u16 ap) { return gdi_sys_color(AW(0)); }

/* LoadString(hInstance, wID, lpBuffer, nBufferMax) */
u32 u_LoadString(u16 ss, u16 ap)
{
    char s[256];
    u16 max = AW(0), off = AW(2), seg = AW(4), id = AW(6);
    int n;
    if (!max)
        return 0;
    n = ne_load_string(w16_mod, id, s, sizeof(s));
    if (n < 0) {
        vlog("w16: LoadString %u: no such string\n", id);
        wr8(seg, off, 0);
        return 0;
    }
    if (n > max - 1)
        n = max - 1;
    fcopy_out(seg, off, s, (u16)n);
    wr8(seg, (u16)(off + n), 0);
    return (u16)n;
}

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

/* ---------------------------------------------------------------- resources, icons, dialog boxes */
static int scr_w = 320, scr_h = 200;
static struct dlg_tmpl dtmpl;               /* one template is parsed at a time */
static long dbx_num, dbx_den, dby_num, dby_den;
#define DX(v) ((int)((long)(v) * dbx_num / dbx_den))
#define DY(v) ((int)((long)(v) * dby_num / dby_den))

u8 *res_load(u16 type, const char *name, u16 id, u16 *len)
{
    u32 off;
    u16 n;
    u8 *p;
    if (ne_res_find(w16_mod, type, name && name[0] ? name : 0, id, &off, &n))
        return 0;
    if (n > 4096)
        n = 4096;
    p = malloc(n);
    if (!p)
        return 0;
    if (ne_read(w16_mod, off, p, n)) {
        free(p);
        return 0;
    }
    *len = n;
    return p;
}

/* GROUP_ICON -> the image best suited to a 16-colour screen: 32x32 at 4 bits, then 1 bit */
static u8 *load_icon(const char *name, u16 id, u16 *len)
{
    u16 glen, n, k, best = 0;
    int bestscore = -1;
    u8 *g = res_load(RT_GROUP_ICON, name, id, &glen), *img;
    if (!g)
        return 0;
    n = glen >= 6 ? (u16)(g[4] | (g[5] << 8)) : 0;
    for (k = 0; k < n && 6 + 14u * k + 14 <= glen; k++) {
        const u8 *e = g + 6 + 14 * k;
        int w = e[0] ? e[0] : 256, h = e[1] ? e[1] : 256, bits = e[6] | (e[7] << 8);
        int score = (w == 32 && h == 32 ? 100 : w <= 32 && h <= 32 ? 50 : 0) + (bits == 4 ? 20 : bits == 1 ? 10 : -100);
        if (score > bestscore) {
            bestscore = score;
            best = (u16)(e[12] | (e[13] << 8));
        }
    }
    free(g);
    if (!best || bestscore < 0)
        return 0;
    img = res_load(RT_ICON, 0, best, len);
    return img;
}

static struct wnd *win_alloc(void)
{
    int i;
    for (i = 0; i < MAXWIN && wins[i].used; i++)
        ;
    if (i == MAXWIN) {
        wlog("w16: window table full (%d)\n", MAXWIN);
        return 0;
    }
    memset(&wins[i], 0, sizeof(wins[i]));
    wins[i].used = 1;
    wins[i].hwnd = HWND_BASE + 4 * i;
    wins[i].cls = -1;
    return &wins[i];
}

static struct wnd *find_ctl(u16 hdlg, u16 id)
{
    int i;
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used && wins[i].ctl && wins[i].parent == hdlg && wins[i].id == id)
            return &wins[i];
    return 0;
}

static void paint_control(struct wnd *w)
{
    GR_GC_ID gc = gdi_gc;
    GR_WINDOW_ID f = w->frame;
    GR_COLOR black = GR_RGB(0, 0, 0), white = GR_RGB(255, 255, 255), gray = GR_RGB(170, 170, 170), dark = GR_RGB(85, 85, 85);
    GR_SIZE tw = 0, th = 0, tb;
    char txt[100];
    int n = 0, x, y, k, kind = (int)(w->style & 0xF);
    const char *s;

    for (s = w->title; *s && n < 99; s++) {      /* "&" marks a mnemonic; "&&" is a literal ampersand */
        if (*s == '&' && s[1] != '&')
            continue;
        if (*s == '&')
            s++;
        txt[n++] = *s;
    }
    txt[n] = 0;
    if (n)
        GrGetGCTextSize(gc, txt, n, GR_TFASCII, &tw, &th, &tb);
    gdi_usebg(0);
    switch (w->ctl) {
    case CTL_STATIC:
        gdi_fill(f, white, 0, 0, w->w, w->h);
        if (kind == 3) {                        /* SS_ICON: drawn at the control's top-left */
            if (w->icon)
                gdi_draw_icon(f, 0, 0, w->icon, w->iconlen);
            break;
        }
        gdi_fg(black);
        if (tw > w->w && kind <= 2) {           /* SS_LEFT/CENTER/RIGHT wrap at spaces, like Windows */
            int st = 0, row = 0;
            while (st < n && (row + 1) * font_h <= w->h + font_h / 2) {
                int end = n, brk = -1, j;
                GR_SIZE lw;
                for (j = st; j <= n; j++) {
                    if (j < n && txt[j] != ' ')
                        continue;
                    GrGetGCTextSize(gc, txt + st, j - st, GR_TFASCII, &lw, &th, &tb);
                    if (lw > w->w && brk >= 0)
                        break;
                    brk = j;
                    if (lw > w->w)
                        break;
                }
                end = brk > st ? brk : n;
                GrGetGCTextSize(gc, txt + st, end - st, GR_TFASCII, &lw, &th, &tb);
                x = kind == 1 ? (w->w - lw) / 2 : kind == 2 ? w->w - lw : 0;
                GrText(f, gc, x < 0 ? 0 : x, row * font_h, txt + st, end - st, GR_TFASCII | GR_TFTOP);
                for (st = end; st < n && txt[st] == ' '; st++)
                    ;
                row++;
            }
            break;
        }
        x = kind == 1 ? (w->w - tw) / 2 : kind == 2 ? w->w - tw : 0;
        if (n)
            GrText(f, gc, x < 0 ? 0 : x, 0, txt, n, GR_TFASCII | GR_TFTOP);
        break;
    case CTL_BUTTON:
        gdi_fill(f, gray, 0, 0, w->w, w->h);
        gdi_fg(black);
        GrRect(f, gc, 0, 0, w->w, w->h);
        k = 1;
        if (kind == 1) {                        /* BS_DEFPUSHBUTTON: heavier border */
            GrRect(f, gc, 1, 1, w->w - 2, w->h - 2);
            k = 2;
        }
        gdi_fg(w->pressed ? dark : white);
        GrLine(f, gc, k, k, w->w - k - 2, k);
        GrLine(f, gc, k, k, k, w->h - k - 2);
        if (!w->pressed) {
            gdi_fg(dark);
            GrLine(f, gc, k, w->h - k - 1, w->w - k - 1, w->h - k - 1);
            GrLine(f, gc, w->w - k - 1, k, w->w - k - 1, w->h - k - 1);
        }
        x = (w->w - tw) / 2 + w->pressed;
        y = (w->h - th) / 2 + w->pressed;
        gdi_fg(black);
        if (n)
            GrText(f, gc, x < 0 ? 0 : x, y < 0 ? 0 : y, txt, n, GR_TFASCII | GR_TFTOP);
        break;
    default:                                    /* control class not provided */
        gdi_fill(f, white, 0, 0, w->w, w->h);
        gdi_fg(dark);
        GrRect(f, gc, 0, 0, w->w, w->h);
        break;
    }
}

static struct wnd *ctl_create(struct wnd *d, const struct dlg_item *it, const char *text)
{
    static const char *const names[6] = { "BUTTON", "EDIT", "STATIC", "LISTBOX", "SCROLLBAR", "COMBOBOX" };
    struct wnd *w = win_alloc();
    if (!w)
        return 0;
    w->parent = d->hwnd;
    w->hinst = d->hinst;
    w->id = it->id;
    w->style = it->style;
    w->ctl = it->cls == 0x80 ? CTL_BUTTON : it->cls == 0x82 ? CTL_STATIC : CTL_OTHER;
    if (w->ctl == CTL_OTHER)
        wlog("w16: dialog control %u of class %s is not provided; drawn as an empty box\n", it->id,
             it->cls >= 0x80 && it->cls <= 0x85 ? names[it->cls - 0x80] : it->clsname);
    if (w->ctl == CTL_BUTTON && (it->style & 0xF) > 1)
        wlog("w16: button control %u style %lx drawn as a push button\n", it->id, (unsigned long)(it->style & 0xF));
    strncpy(w->title, text ? text : it->text, sizeof(w->title) - 1);
    w->x = DX(it->x); w->y = DY(it->y); w->w = DX(it->cx); w->h = DY(it->cy);
    if (w->w < 1) w->w = 1;
    if (w->h < 1) w->h = 1;
    w->cw = w->w; w->ch = w->h;
    if (w->ctl == CTL_STATIC && (it->style & 0xF) == 3) {
        u16 id = 0;
        const char *nm = it->text;
        if (nm[0] == '#') {
            id = (u16)atoi(nm + 1);
            nm = "";
        }
        w->icon = load_icon(nm, id, &w->iconlen);
        if (!w->icon)
            wlog("w16: dialog icon \"%s\" not found in the program's resources\n", it->text);
        else {
            /* like Windows, an SS_ICON control takes the size of its icon */
            int iw = w->iconlen >= 12 ? w->icon[4] : 32, ih = w->iconlen >= 12 ? w->icon[8] / 2 : 32;
            if (iw > 0 && iw <= 64 && ih > 0 && ih <= 64) {
                w->w = w->cw = iw;
                w->h = w->ch = ih;
            }
            vlog("w16: dialog icon \"%s\" loaded (%u bytes, %dx%d)\n", it->text, w->iconlen, w->w, w->h);
        }
    }
    w->frame = w->client = GrNewWindow(d->client, w->x, w->y, w->w, w->h, 0, GR_RGB(255, 255, 255), 0);
    GrSelectEvents(w->frame, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_BUTTON_UP |
                   GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP);
    if (it->style & WS_VISIBLE) {
        GrMapWindow(w->frame);
        w->visible = 1;
    }
    return w;
}

static void dlg_command(struct wnd *d, u16 id)
{
    struct wnd *c = find_ctl(d->hwnd, id);
    vlog("w16: dialog command %u\n", id);
    if (!d->procoff && !d->procseg) {           /* message box: its buttons end it */
        if (id == IDCANCEL && !c) {             /* Esc without a Cancel button works only with one button */
            int i, n = 0;
            for (i = 0; i < MAXWIN; i++)
                if (wins[i].used && wins[i].ctl == CTL_BUTTON && wins[i].parent == d->hwnd) {
                    n++;
                    id = wins[i].id;
                }
            if (n != 1)
                return;
        }
        d->ended = 1;
        d->result = id;
        return;
    }
    win_send(d, WM_COMMAND, id, ((u32)BN_CLICKED << 16) | (c ? c->hwnd : 0));
}

static u16 run_dialog(const char *name, u16 procoff, u16 procseg, u16 hinst, int pixels, const char *text0);

/* DialogBox(hInstance, lpTemplateName, hWndParent, lpDialogFunc): modal dialog from a DIALOG resource */
u32 u_DialogBox(u16 ss, u16 ap)
{
    u16 procoff = AW(0), procseg = AW(2), noff = AW(6), nseg = AW(8), hinst = AW(10);
    char name[34];
    u16 id, len, k;
    u8 *res;

    res_name(noff, nseg, name, &id);
    res = res_load(RT_DIALOG, name, id, &len);
    if (!res) {
        wlog("w16: DialogBox: dialog resource %s%s not found\n", name[0] ? name : "#", name[0] ? "" : "(ordinal)");
        return 0xFFFF;
    }
    k = dlg_parse(res, len, &dtmpl);
    free(res);
    if (k) {
        wlog("w16: DialogBox: dialog template %s is malformed or has more than %d controls\n", name, DLG_MAXITEMS);
        return 0xFFFF;
    }
    if (dtmpl.clsname[0]) {
        wlog("w16: DialogBox: private dialog class %s is not supported\n", dtmpl.clsname);
        return 0xFFFF;
    }
    if (dtmpl.menu[0])
        wlog("w16: DialogBox: dialog menu %s ignored (dialog menus are not supported)\n", dtmpl.menu);
    return run_dialog(name[0] ? name : "#", procoff, procseg, hinst, 0, 0);
}

/* MessageBox(hWndParent, lpText, lpCaption, wType): a dialog built here, run like DialogBox */
u16 message_box(const char *text, const char *caption, u16 type)
{
    static const u8 nbtn[6] = { 1, 2, 3, 3, 2, 2 };
    static const u16 btn[6][3] = { { 1 }, { 1, 2 }, { 3, 4, 5 }, { 6, 7, 2 }, { 6, 7 }, { 4, 2 } };
    static const char *const bname[8] = { "", "OK", "Cancel", "&Abort", "&Retry", "&Ignore", "&Yes", "&No" };
    u16 kind = type & 0xF, k, r;
    GR_SIZE tw, th, tb;
    int maxw = scr_w - 32, textw, lines, nb, bw = 44, bh = font_h + 8, gap = 8, W, H, bx;
    if (kind > 5)
        kind = 0;
    nb = nbtn[kind];
    memset(&dtmpl, 0, sizeof(dtmpl));
    strncpy(dtmpl.caption, caption, sizeof(dtmpl.caption) - 1);
    GrGetGCTextSize(gdi_gc, (void *)text, strlen(text), GR_TFASCII, &tw, &th, &tb);
    textw = tw <= maxw ? tw : maxw;
    lines = tw <= maxw ? 1 : (tw + maxw - 1) / maxw + 1;      /* room for word wrap */
    W = textw + 2 * gap;
    if (W < nb * (bw + gap) + gap)
        W = nb * (bw + gap) + gap;
    H = gap + lines * font_h + gap + bh + gap;
    dtmpl.style = WS_POPUP | WS_CAPTION | DS_MODALFRAME | WS_VISIBLE;
    dtmpl.cx = W; dtmpl.cy = H;
    dtmpl.x = (scr_w - W - 6) / 2; dtmpl.y = (scr_h - H - 6 - CAPH) / 2;
    dtmpl.nitems = 1 + nb;
    dtmpl.item[0].cls = 0x82;
    dtmpl.item[0].style = WS_CHILD | WS_VISIBLE | (lines == 1 ? 1 : 0);    /* SS_CENTER or SS_LEFT */
    dtmpl.item[0].x = gap; dtmpl.item[0].y = gap; dtmpl.item[0].cx = W - 2 * gap; dtmpl.item[0].cy = lines * font_h;
    dtmpl.item[0].id = 0xFFFF;
    bx = (W - nb * bw - (nb - 1) * gap) / 2;
    for (k = 0; k < nb; k++) {
        struct dlg_item *it = &dtmpl.item[1 + k];
        it->cls = 0x80;
        it->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | (k == 0 ? 1 : 0);   /* first is BS_DEFPUSHBUTTON */
        it->x = bx + k * (bw + gap); it->y = H - gap - bh; it->cx = bw; it->cy = bh;
        it->id = btn[kind][k];
        strcpy(it->text, bname[it->id]);
    }
    if (type & 0xF0)
        vlog("w16: MessageBox icon style %02x is not drawn\n", type & 0xF0);
    r = run_dialog("MessageBox", 0, 0, w16_mod->dgroup, 1, text);
    vlog("w16: MessageBox \"%s\" -> %u\n", text, r);
    return r;
}

/* MessageBox(hWndParent, lpText, lpCaption, wType) */
u32 u_MessageBox(u16 ss, u16 ap)
{
    char text[100], caption[48];
    if (AW(4))
        fstrncpy_in(caption, AW(4), AW(2), sizeof(caption));
    else
        strcpy(caption, "Error");
    fstrncpy_in(text, AW(8), AW(6), sizeof(text));
    return message_box(text, caption, AW(0));
}

/* Create the dialog in dtmpl (dialog units, or pixels), run its modal loop, return EndDialog's result.
 * text0, if given, replaces the text of the first control (message boxes have long texts). */
static u16 run_dialog(const char *name, u16 procoff, u16 procseg, u16 hinst, int pixels, const char *text0)
{
    u16 hfocus = 0, k;
    struct wnd *d, *w;
    struct qmsg m;
    int b, cap, W, H, x, y;
    GR_SIZE tw, th, tb;
    u32 r;

    /* dialog units: base units from the Nano-X system font, shrunk if the dialog would not fit */
    GrGetGCTextSize(gdi_gc, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, GR_TFASCII, &tw, &th, &tb);
    dbx_num = tw; dbx_den = 52 * 4; dby_num = font_h; dby_den = 8;
    if (pixels)
        dbx_num = dbx_den = dby_num = dby_den = 1;
    b = (dtmpl.style & (DS_MODALFRAME | WS_THICKFRAME)) ? 3 : (dtmpl.style & (WS_BORDER | WS_CAPTION)) ? 1 : 0;
    cap = (dtmpl.style & WS_CAPTION) == WS_CAPTION ? CAPH + 1 : 0;
    if (dtmpl.cx > 0 && DX(dtmpl.cx) + 2 * b > scr_w) { dbx_num = scr_w - 2 * b; dbx_den = dtmpl.cx; }
    if (dtmpl.cy > 0 && DY(dtmpl.cy) + 2 * b + cap > scr_h) { dby_num = scr_h - 2 * b - cap; dby_den = dtmpl.cy; }
    W = DX(dtmpl.cx) + 2 * b;
    H = DY(dtmpl.cy) + 2 * b + cap;
    x = DX(dtmpl.x); y = DY(dtmpl.y);
    if (x + W > scr_w) x = scr_w - W;
    if (y + H > scr_h) y = scr_h - H;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    d = win_alloc();
    if (!d)
        return 0xFFFF;
    d->isdialog = 1;
    d->style = dtmpl.style | ((dtmpl.style & DS_MODALFRAME) ? WS_THICKFRAME : 0);
    d->hinst = hinst ? hinst : w16_mod->dgroup;
    d->procoff = procoff; d->procseg = procseg;
    d->defid = IDOK;
    strncpy(d->title, dtmpl.caption, sizeof(d->title) - 1);
    d->x = x; d->y = y; d->w = W; d->h = H;
    d->cx = b; d->cy = b + cap; d->cw = W - 2 * b; d->ch = H - 2 * b - cap;
    d->frame = GrNewWindowEx(GR_WM_PROPS_NODECORATE | GR_WM_PROPS_NOMOVE, d->title, GR_ROOT_WINDOW_ID, x, y, W, H,
                             GR_RGB(170, 170, 170));
    d->client = GrNewWindow(d->frame, d->cx, d->cy, d->cw, d->ch, 0, GR_RGB(255, 255, 255), 0);
    GrSelectEvents(d->frame, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_CLOSE_REQ |
                   GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP);
    GrSelectEvents(d->client, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_BUTTON_UP |
                   GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP);
    for (k = 0; k < dtmpl.nitems; k++) {
        const struct dlg_item *it = &dtmpl.item[k];
        w = ctl_create(d, it, k == 0 ? text0 : 0);
        if (!w)
            break;
        if (!hfocus && (it->style & WS_TABSTOP))
            hfocus = w->hwnd;
        if (w->ctl == CTL_BUTTON && (it->style & 0xF) == 1)
            d->defid = it->id;
    }
    vlog("w16: DialogBox \"%s\": %u controls, %dx%d at %d,%d, default button %u\n", name, dtmpl.nitems,
         W, H, x, y, d->defid);

    r = win_send(d, WM_INITDIALOG, hfocus, 0);
    (void)r;
    GrMapWindow(d->client);
    GrMapWindow(d->frame);
    GrSetFocus(d->client);
    d->visible = 1;

    while (!d->ended) {
        next_msg(&m, 1);
        if (m.msg == WM_QUIT) {                 /* leave the loop and repost the quit */
            quit_pending = 1;
            quit_code = m.wp;
            d->result = 0xFFFF;
            break;
        }
        w = win_get(m.hwnd);
        if (!w) {
            if (m.msg == WM_TIMER)
                dispatch(m.hwnd, m.msg, m.wp, m.lp);    /* windowless timer callbacks */
            continue;
        }
        if (w == d || (w->ctl && w->parent == d->hwnd)) {
            switch (m.msg) {
            case WM_KEYDOWN:                    /* the dialog manager's keyboard interface */
                if (m.wp == 0x0D)
                    dlg_command(d, d->defid);
                else if (m.wp == 0x1B)
                    dlg_command(d, IDCANCEL);
                break;
            case WM_CHAR:
            case WM_KEYUP:
                break;
            case WM_COMMAND:                    /* a button: through dlg_command, which also ends message boxes */
                dlg_command(d, m.wp);
                break;
            case WM_SYSCOMMAND:
                if ((m.wp & 0xFFF0) == SC_CLOSE)
                    dlg_command(d, IDCANCEL);
                break;
            case WM_PAINT:
                if (w == d) {
                    /* no erase here: the server already cleared exposed parts of the client to white,
                     * and filling it would paint over the controls (Nano-X does not clip by children) */
                    win_send(d, WM_PAINT, 0, 0);
                    d->invalid = 0;
                    d->erase = 0;
                    for (k = 0; k < MAXWIN; k++)    /* controls may not have had their exposure yet */
                        if (wins[k].used && wins[k].ctl && wins[k].parent == d->hwnd && wins[k].visible)
                            paint_control(&wins[k]);
                    paint_done(d);
                }
                break;
            default:
                if (w == d)
                    win_send(d, m.msg, m.wp, m.lp);
                break;
            }
        } else
            dispatch(m.hwnd, m.msg, m.wp, m.lp);    /* other windows of the program keep running */
    }
    r = d->result;
    vlog("w16: dialog ended with result %u\n", (u16)r);
    win_send(d, WM_DESTROY, 0, 0);
    GrDestroyWindow(d->frame);
    purge(d->hwnd);
    free_controls(d->hwnd);
    d->used = 0;
    {                                   /* the keyboard goes back to the program's window */
        struct wnd *o = win_get(focus_hwnd);
        if (!o || o->isdialog || o->ctl)
            for (k = 0, o = 0; k < MAXWIN; k++)
                if (wins[k].used && !wins[k].ctl && !wins[k].isdialog && wins[k].visible) {
                    o = &wins[k];
                    break;
                }
        if (o)
            GrSetFocus(o->client);
    }
    ready_write();                      /* the dialog's buttons are gone; menus are clickable again */
    w16_census("dialog end");
    return (u16)r;
}

/* EndDialog(hDlg, nResult) */
u32 u_EndDialog(u16 ss, u16 ap)
{
    struct wnd *d = win_get(AW(2));
    if (!d || !d->isdialog)
        return 0;
    d->ended = 1;
    d->result = AW(0);
    vlog("w16: EndDialog(%04x, %u)\n", d->hwnd, d->result);
    return 1;
}

/* SetDlgItemText(hDlg, nIDDlgItem, lpString) */
u32 u_SetDlgItemText(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2), id = AW(4), hdlg = AW(6);
    struct wnd *c = find_ctl(hdlg, id);
    if (!c) {
        wlog("w16: SetDlgItemText: no control %u in dialog %04x\n", id, hdlg);
        return 0;
    }
    fstrncpy_in(c->title, seg, off, sizeof(c->title));
    vlog("w16: SetDlgItemText(%u, \"%s\")\n", id, c->title);
    if (c->visible)
        paint_control(c);
    return 0;
}

/* GetDlgItem(hDlg, nIDDlgItem) */
u32 u_GetDlgItem(u16 ss, u16 ap)
{
    struct wnd *c = find_ctl(AW(2), AW(0));
    return c ? c->hwnd : 0;
}

/* GetSystemMetrics(nIndex) for the 320x200 Tandy screen and the frames drawn here */
u32 u_GetSystemMetrics(u16 ss, u16 ap)
{
    switch (AW(0)) {
    case 0:  return scr_w;          /* SM_CXSCREEN */
    case 1:  return scr_h;          /* SM_CYSCREEN */
    case 4:  return CAPH;           /* SM_CYCAPTION */
    case 5: case 6: return 1;       /* SM_CXBORDER, SM_CYBORDER */
    case 7: case 8: return 3;       /* SM_CXDLGFRAME, SM_CYDLGFRAME */
    case 11: case 12: return 32;    /* SM_CXICON, SM_CYICON */
    case 13: case 14: return 16;    /* SM_CXCURSOR, SM_CYCURSOR */
    case 16: return scr_w;          /* SM_CXFULLSCREEN */
    case 17: return scr_h - CAPH;   /* SM_CYFULLSCREEN */
    case 19: return 1;              /* SM_MOUSEPRESENT */
    case 22: return 0;              /* SM_DEBUG: retail */
    case 32: case 33: return 3;     /* SM_CXFRAME, SM_CYFRAME */
    }
    return 0;
}

/* ---------------------------------------------------------------- paint completion hooks */
void paint_done(struct wnd *w)
{
    GR_SCREEN_INFO si;
    char line[48];
    u32 t;
    npaints++;
    GrFlush();
    GrGetScreenInfo(&si);               /* round trip: the server has drawn everything before this */
    t = now_ms();                       /* measurements first: the log and frame dump below take time */
    lat_shown("paint", 1);
    if (npaints == 1 && opt_metrics) {
        sprintf(line, "@@STARTUP %lu\n", (unsigned long)(t - w16_t0));
        if (opt_debugport)
            e9puts(line);
        wlog("w16: first paint complete %lu ms after w16 started\n", (unsigned long)(t - w16_t0));
    }
    sprintf(line, "@@PAINT %d hwnd %04x\n", npaints, w->hwnd);
    if (opt_debugport)
        e9puts(line);
    vlog("w16: paint %d of hwnd %04x complete\n", npaints, w->hwnd);
    if (npaints == 1)
        w16_census("first paint");
    if (opt_dump[0]) {
        char tag[12];
        sprintf(tag, "%d", npaints);
        w16_dump_frame(tag);
    }
    if (opt_ready && !w->readied) {
        w->readied = 1;
        ready_write();
    }
}

/* test hook (-r): "painted", then the screen centre of each click target: the push buttons of dialogs,
 * menu bar items as "button 1000+n", and the items of an open menu by command id */
void ready_write(void)
{
    char *buf = (char *)w16_scratch;    /* 600 bytes of the shared scratch */
    int n = 8, i, fd;
    if (!opt_ready)
        return;
    memcpy(buf, "painted\n", 8);
    for (i = 0; i < MAXWIN; i++) {
        struct wnd *c = &wins[i], *d = c->used && c->ctl == CTL_BUTTON ? win_get(c->parent) : 0;
        if (d && d->isdialog && n < 600 - 40)
            n += sprintf(buf + n, "button %u %d %d\n", c->id, d->x + d->cx + c->x + c->w / 2,
                         d->y + d->cy + c->y + c->h / 2);
    }
    n += menu_ready_lines(buf + n, 600 - n);
    fd = open(readyfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        write(fd, buf, n);              /* one write: the driver never sees half a file */
        close(fd);
    }
}

/* counts of everything a program can leak, and how far w16's own heap has grown */
void w16_census(const char *when)
{
    extern void lheap_census(u16 *used, u16 *blocks, u16 *size);
    extern void rsrc_census(u16 *n, u32 *bytes);
    u16 nw = 0, nc = 0, nt = 0, ndc, nobj, lu, lb, ls, nr, nm, i;
    u32 rb;
    if (!opt_verbose && !opt_metrics)
        return;
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used) {
            if (wins[i].ctl)
                nc++;
            else
                nw++;
        }
    for (i = 0; i < MAXTIMER; i++)
        if (timers[i].used)
            nt++;
    gdi_census(&ndc, &nobj);
    lheap_census(&lu, &lb, &ls);
    rsrc_census(&nr, &rb);
    menu_census(&nm);
    wlog("w16: census at %s: windows %u, controls %u, DCs %u, GDI objects %u, timers %u, menus %u, "
         "local heap %u bytes in %u blocks of %u, resources %u (%lu bytes), w16 break %04x\n",
         when, nw, nc, ndc, nobj, nt, nm, lu, lb, ls, nr, (unsigned long)rb, (unsigned)sbrk(0));
    if (opt_metrics && tk_n)
        wlog("w16: timer messages %lu, %lu ms in all, longest %lu ms; under 10/50/100/200/500 ms and over: "
             "%lu %lu %lu %lu %lu %lu\n", (unsigned long)tk_n, (unsigned long)tk_sum, (unsigned long)tk_max,
             (unsigned long)tk_hist[0], (unsigned long)tk_hist[1], (unsigned long)tk_hist[2],
             (unsigned long)tk_hist[3], (unsigned long)tk_hist[4], (unsigned long)tk_hist[5]);
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
    if (si.cols > 0 && si.rows > 0) {
        scr_w = si.cols;
        scr_h = si.rows;
    }
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
    w16_census("exit");
    for (i = 0; i < MAXTIMER; i++)
        timers[i].used = 0;
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used && wins[i].hmenu) {
            menu_close_for(wins[i].hwnd);
            menu_free(wins[i].hmenu);
            wins[i].hmenu = 0;
        }
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used && !wins[i].ctl) {
            GrDestroyWindow(wins[i].frame);
            free_controls(wins[i].hwnd);
            wins[i].used = 0;
            n++;
        }
    if (n)
        vlog("w16: destroyed %d window(s) the program left open\n", n);
    gdi_shutdown();
    GrClose();
}
