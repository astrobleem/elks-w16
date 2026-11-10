/*
 * menu.c - USER menus. A window's class menu (a MENU resource) becomes a menu bar drawn in
 * the window frame under the caption; its popups drop down as Nano-X windows of their own.
 * Mouse: click a bar item, then an item. Keyboard: F10 or Alt+mnemonic opens a popup,
 * Up/Down move, Left/Right change popup, Enter or the item's mnemonic chooses, Esc closes.
 * The chosen item reaches the program as WM_COMMAND(id, 0), after WM_INITMENU and
 * WM_INITMENUPOPUP, as in Windows.
 * The system menu (the box at the left of the caption; Alt+Space, or F10 in a window without a
 * menu bar) is one more popup of the same kind, shared by every window. It holds only the
 * commands this layer carries out: Close, which reaches the window as WM_SYSCOMMAND SC_CLOSE.
 * Left/Right move between it and the menu bar, and a double-click on the box closes the window.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "wapi.h"

#define MAXMENU 2
#define SYSMENU MAXMENU                 /* menus[SYSMENU]: the system menu, never a program's menu */
struct menu {
    u8   used, idx;
    u16  hwnd;
    i16  bar_y;                         /* frame row of the bar, set when drawn */
    struct menu_tmpl t;
    i16  x0[MENU_MAXITEMS], x1[MENU_MAXITEMS];  /* bar items: frame columns */
    i16  tw[MENU_MAXITEMS], aw[MENU_MAXITEMS];  /* label and accelerator widths */
    i16  mx[MENU_MAXITEMS], mw[MENU_MAXITEMS];  /* mnemonic underline offset and width, mw 0 if none */
};
/* about 1 KB each, so allocated when first needed: most programs have one menu bar, many none */
static struct menu *menus[MAXMENU + 1];
#define USED(i) (menus[i] && menus[i]->used)
static int scr_w = 320, scr_h = 200, asc = 10;
#define DBLCLICK_MS 800                 /* system menu box double-click (the 8088 draws the popup in between) */

#define ITEMH (font_h + 3)
#define SEPH  5
static struct {
    u8   open;
    u8   mi, top;                       /* menu, and the bar item whose popup is open */
    int  sel;                           /* row selected, -1 none */
    GR_WINDOW_ID wid;
    int  x, y, w, h;
    u8   rows, item[MENU_MAXITEMS], shown;
    i16  ry[MENU_MAXITEMS + 1];
    u32  opened;                        /* server time of the click that opened the system menu, 0 by key */
} pop;

static struct menu *menu_of(u16 h, int *sub)
{
    u16 k = (u16)(h - HMENU_BASE), mi = k / 0x20, r = k % 0x20;
    if (h < HMENU_BASE || mi > SYSMENU || (r & 1) || !USED(mi))
        return 0;
    *sub = r ? r / 2 - 1 : -1;
    if (*sub >= menus[mi]->t.n || (*sub >= 0 && !(menus[mi]->t.it[*sub].flags & MF_POPUP)))
        return 0;
    return menus[mi];
}

/* label without '&' and without the accelerator after a tab; *mn: index of the mnemonic, -1 none */
static int label(const char *s, char *out, int *mn, const char **accel)
{
    int n = 0;
    *mn = -1;
    *accel = 0;
    for (; *s; s++) {
        if (*s == '\t') {
            *accel = s + 1;
            break;
        }
        if (*s == '&' && s[1] != '&') {
            if (s[1])
                *mn = n;
            continue;
        }
        if (*s == '&')
            s++;
        out[n++] = *s;
    }
    out[n] = 0;
    return n;
}

static char upper(char c) { return c >= 'a' && c <= 'z' ? (char)(c - 32) : c; }

static char mnemonic(const struct menu_item *it)
{
    char buf[32];
    int mn;
    const char *a;
    label(it->text, buf, &mn, &a);
    return mn >= 0 ? upper(buf[mn]) : 0;
}

static int text_w(const char *s, int n)
{
    GR_SIZE w, h, b;
    if (n <= 0)
        return 0;
    gdi_text_size((void *)s, n, &w, &h, &b);
    asc = b;
    return w;
}

static int border_w(u32 style)
{
    return (style & WS_THICKFRAME) ? 3 : (style & (WS_BORDER | WS_CAPTION)) ? 1 : 0;
}

/* label, accelerator and mnemonic widths of every item, once */
static void measure(struct menu *mn, int x)
{
    u16 k;
    for (k = 0; k < mn->t.n; k++) {
        char lb[32];
        int m;
        const char *a;
        int n = label(mn->t.it[k].text, lb, &m, &a);
        mn->tw[k] = (i16)text_w(lb, n);
        mn->aw[k] = (i16)(a ? text_w(a, (int)strlen(a)) : 0);
        mn->mx[k] = (i16)(m > 0 ? text_w(lb, m) : 0);
        mn->mw[k] = (i16)(m >= 0 ? text_w(lb + m, 1) : 0);
        if (mn->t.it[k].parent == 0xFF) {
            mn->x0[k] = (i16)x;
            x += mn->tw[k] + 12;
            mn->x1[k] = (i16)x;
        }
    }
}

/* slot i, allocated on first use (zeroed) */
static struct menu *slot(int i)
{
    if (!menus[i] && (menus[i] = calloc(1, sizeof(struct menu))) != 0)
        menus[i]->idx = (u8)i;
    if (!menus[i])
        wlog("w16: no memory for a menu\n");
    return menus[i];
}

/* attach a MENU resource to w as its menu bar; widths are measured once, here */
u16 menu_load(struct wnd *w, const char *name, u16 id)
{
    u8 *buf = w16_scratch;             /* 512 bytes of the shared scratch */
    struct menu *mn;
    GR_SCREEN_INFO si;
    u32 off;
    u16 len;
    int i;
    for (i = 0; i < MAXMENU && USED(i); i++)
        ;
    if (i == MAXMENU) {
        wlog("w16: menu table full (%d menus)\n", MAXMENU);
        return 0;
    }
    if (ne_res_find(w16_mod, RT_MENU, name, id, &off, &len)) {
        wlog("w16: menu %s not found in the program's resources\n", name ? name : "(ordinal)");
        return 0;
    }
    if (!(mn = slot(i)))
        return 0;
    if (len > 512)
        len = 512;
    if (ne_read(w16_mod, off, buf, len) || menu_parse(buf, len, &mn->t)) {
        wlog("w16: menu %s is malformed or has more than %d items\n", name ? name : "(ordinal)", MENU_MAXITEMS);
        return 0;
    }
    GrGetScreenInfo(&si);
    if (si.cols > 0) {
        scr_w = si.cols;
        scr_h = si.rows;
    }
    mn->used = 1;
    mn->hwnd = w->hwnd;
    mn->bar_y = (i16)(border_w(w->style) + ((w->style & WS_CAPTION) == WS_CAPTION ? font_h + 2 : 0));
    measure(mn, border_w(w->style) + 2);
    w->hmenu = (u16)(HMENU_BASE + 0x20 * i);
    vlog("w16: menu %s: %u items on window %04x -> %04x\n", name ? name : "(ordinal)", mn->t.n, w->hwnd, w->hmenu);
    return w->hmenu;
}

void menu_free(u16 h)
{
    int sub;
    struct menu *mn = menu_of(h, &sub);
    if (mn && sub < 0)
        mn->used = 0;
}

void menu_census(u16 *n)
{
    int i;
    *n = 0;
    for (i = 0; i < MAXMENU; i++)
        if (USED(i))
            (*n)++;
}

static void draw_label(GR_WINDOW_ID wid, struct menu *mn, int k, int x, int y, GR_COLOR c)
{
    char lb[32];
    int m;
    const char *a;
    int n = label(mn->t.it[k].text, lb, &m, &a);
    gdi_fg(c);
    gdi_usebg(0);
    if (n)
        GrText(wid, gdi_gc, x, y, lb, n, GR_TFASCII | GR_TFTOP);
    if (mn->mw[k])
        GrLine(wid, gdi_gc, x + mn->mx[k], y + asc + 1, x + mn->mx[k] + mn->mw[k] - 1, y + asc + 1);
}

void menu_draw_bar(struct wnd *w, int y)
{
    int sub, k, b = border_w(w->style);
    struct menu *mn = menu_of(w->hmenu, &sub);
    GR_COLOR black = GR_RGB(0, 0, 0), white = GR_RGB(255, 255, 255);
    if (!mn || sub >= 0)
        return;
    mn->bar_y = (i16)y;
    gdi_fill(w->frame, white, b, y, w->w - 2 * b, MENUH - 1);
    gdi_fg(black);
    GrLine(w->frame, gdi_gc, b, y + MENUH - 1, w->w - b - 1, y + MENUH - 1);
    for (k = 0; k < mn->t.n; k++) {
        int hi = pop.open && menus[pop.mi] == mn && pop.top == k, gray = (mn->t.it[k].flags & MF_GRAYED) != 0;
        if (mn->t.it[k].parent != 0xFF)
            continue;
        if (hi)
            gdi_fill(w->frame, GR_RGB(0, 0, 170), mn->x0[k], y, mn->x1[k] - mn->x0[k], MENUH - 1);
        draw_label(w->frame, mn, k, mn->x0[k] + 6, y + 1, hi ? white : gray ? GR_RGB(128, 128, 128) : black);
    }
}

static void draw_row(int r)
{
    struct menu *mn = menus[pop.mi];
    int k = pop.item[r], y = pop.ry[r], hi = r == pop.sel;
    const struct menu_item *it = &mn->t.it[k];
    GR_COLOR black = GR_RGB(0, 0, 0), white = GR_RGB(255, 255, 255), gray = GR_RGB(128, 128, 128), c;
    if (it->flags & MF_SEPARATOR) {
        gdi_fill(pop.wid, white, 1, y, pop.w - 2, SEPH);
        gdi_fg(black);
        GrLine(pop.wid, gdi_gc, 1, y + SEPH / 2, pop.w - 2, y + SEPH / 2);
        return;
    }
    gdi_fill(pop.wid, hi ? GR_RGB(0, 0, 170) : white, 1, y, pop.w - 2, ITEMH);
    c = hi ? white : (it->flags & MF_GRAYED) ? gray : black;
    if (it->flags & MF_CHECKED) {               /* check mark */
        int cy = y + ITEMH / 2;
        gdi_fg(c);
        GrLine(pop.wid, gdi_gc, 3, cy, 5, cy + 2);
        GrLine(pop.wid, gdi_gc, 5, cy + 2, 9, cy - 3);
        GrLine(pop.wid, gdi_gc, 3, cy + 1, 5, cy + 3);
        GrLine(pop.wid, gdi_gc, 5, cy + 3, 9, cy - 2);
    }
    draw_label(pop.wid, mn, k, 13, y + 1, c);
    if (mn->aw[k]) {
        const char *a = strchr(it->text, '\t') + 1;
        GrText(pop.wid, gdi_gc, pop.w - 6 - mn->aw[k], y + 1, (void *)a, (int)strlen(a), GR_TFASCII | GR_TFTOP);
    }
}

static void draw_popup(void)
{
    int r;
    gdi_fg(GR_RGB(0, 0, 0));
    GrRect(pop.wid, gdi_gc, 0, 0, pop.w, pop.h);
    for (r = 0; r < pop.rows; r++)
        draw_row(r);
}

static int usable(int r)
{
    const struct menu_item *it = &menus[pop.mi]->t.it[pop.item[r]];
    return !(it->flags & (MF_SEPARATOR | MF_GRAYED | MF_DISABLED));
}

static void select_row(int r)
{
    int old = pop.sel;
    if (r == old)
        return;
    pop.sel = r;
    if (old >= 0)
        draw_row(old);
    if (r >= 0)
        draw_row(r);
}

static int row_at(int y)
{
    int r;
    for (r = 0; r < pop.rows; r++)
        if (y >= pop.ry[r] && y < pop.ry[r + 1])
            return r;
    return -1;
}

/* the window's own menu bar, redrawn (no bar item highlighted unless its popup is open) */
static void redraw_bar(struct wnd *w)
{
    int sub;
    struct menu *b = menu_of(w->hmenu, &sub);
    if (b && sub < 0 && w->visible)
        menu_draw_bar(w, b->bar_y);
}

/* redraw 0: another popup opens next, which draws the bar itself */
static void close_popup(int redraw)
{
    struct wnd *w;
    if (!pop.open)
        return;
    GrDestroyWindow(pop.wid);
    pop.open = 0;
    w = win_get(menus[pop.mi]->hwnd);
    if (!w)
        return;
    if (pop.mi == SYSMENU)
        nc_sysbox(w, 0);
    if (redraw) {
        if (pop.mi != SYSMENU)
            redraw_bar(w);
        GrSetFocus(w->client);
        ready_write();
    }
}

/* the window is being destroyed: drop its popup without drawing */
void menu_close_for(u16 hwnd)
{
    if (pop.open && menus[pop.mi]->hwnd == hwnd) {
        GrDestroyWindow(pop.wid);
        pop.open = 0;
    }
}

static void choose(int r)
{
    const struct menu_item *it = &menus[pop.mi]->t.it[pop.item[r]];
    u16 hwnd = menus[pop.mi]->hwnd, id = it->id;
    int sys = pop.mi == SYSMENU;
    if (!usable(r) || (it->flags & MF_POPUP))
        return;
    close_popup(1);
    vlog("w16: %s item %u \"%s\" chosen -> %s\n", sys ? "system menu" : "menu", id, it->text,
         sys ? "WM_SYSCOMMAND" : "WM_COMMAND");
    w16_post(hwnd, sys ? WM_SYSCOMMAND : WM_COMMAND, id, 0);
}

/* open the popup of bar item top of menu mi; sel_first selects its first usable row (keyboard) */
static void open_popup(int mi, int top, int sel_first)
{
    struct menu *mn = menus[mi];
    struct wnd *w = win_get(mn->hwnd);
    int k, y = 1, wmax = 0, entering = !pop.open;
    if (!w)
        return;
    if (pop.open)
        close_popup(0);
    if (entering)
        win_send(w, WM_INITMENU, (u16)(HMENU_BASE + 0x20 * mi), 0);
    {
        int pos = 0;
        for (k = 0; k < top; k++)
            if (mn->t.it[k].parent == 0xFF)
                pos++;
        /* the high word of lParam is TRUE for the system menu */
        win_send(w, WM_INITMENUPOPUP, (u16)(HMENU_BASE + 0x20 * mi + 2 * (top + 1)),
                 (u32)pos | (mi == SYSMENU ? 0x10000ul : 0));
    }
    if (!win_get(mn->hwnd) || !mn->used)
        return;                             /* the program destroyed its window meanwhile */
    memset(&pop, 0, sizeof(pop));
    pop.mi = (u8)mi;
    pop.top = (u8)top;
    pop.sel = -1;
    for (k = 0; k < mn->t.n && pop.rows < MENU_MAXITEMS; k++) {
        int iw;
        if (mn->t.it[k].parent != top)
            continue;
        pop.item[pop.rows] = (u8)k;
        pop.ry[pop.rows++] = (i16)y;
        y += (mn->t.it[k].flags & MF_SEPARATOR) ? SEPH : ITEMH;
        iw = mn->tw[k] + (mn->aw[k] ? mn->aw[k] + 12 : 0);
        if (iw > wmax)
            wmax = iw;
    }
    pop.ry[pop.rows] = (i16)y;
    pop.w = wmax + 13 + 8;
    pop.h = y + 1;
    pop.x = w->x + mn->x0[top];
    pop.y = w->y + mn->bar_y + MENUH - 1;
    if (pop.x + pop.w > scr_w) pop.x = scr_w - pop.w;
    if (pop.y + pop.h > scr_h) pop.y = scr_h - pop.h;
    if (pop.x < 0) pop.x = 0;
    if (pop.y < 0) pop.y = 0;
    if (sel_first)
        for (k = 0; k < pop.rows; k++)
            if (usable(k)) {
                pop.sel = k;
                break;
            }
    pop.wid = GrNewWindowEx(GR_WM_PROPS_NODECORATE | GR_WM_PROPS_NOMOVE, "menu", GR_ROOT_WINDOW_ID, pop.x, pop.y,
                            pop.w, pop.h, GR_RGB(255, 255, 255));
    GrSelectEvents(pop.wid, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_BUTTON_DOWN | GR_EVENT_MASK_BUTTON_UP |
                   GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP);
    pop.open = 1;
    GrMapWindow(pop.wid);
    GrSetFocus(pop.wid);
    if (mi == SYSMENU)
        nc_sysbox(w, 1);
    redraw_bar(w);                          /* highlights the open bar item, or none */
}

/* open the system menu of w (top-level window or dialog); 0 if w has no system menu box */
int menu_sys_open(struct wnd *w, int sel_first, u32 click_time)
{
    static const struct menu_item close_item = { 0, SC_CLOSE, 0, "&Close\tAlt+F4" };
    struct menu *mn;
    int b = border_w(w->style);
    if (!(w->style & WS_SYSMENU) || (w->style & WS_CAPTION) != WS_CAPTION || w->ctl || !(mn = slot(SYSMENU)))
        return 0;
    if (!mn->used) {
        mn->used = 1;
        mn->t.n = 2;
        mn->t.it[0].flags = MF_POPUP;
        mn->t.it[0].parent = 0xFF;
        mn->t.it[1] = close_item;
        measure(mn, 0);
    }
    if (pop.open && pop.mi == SYSMENU) {
        if (mn->hwnd == w->hwnd)
            return 1;
        close_popup(0);                     /* another window's system menu */
    }
    mn->hwnd = w->hwnd;
    mn->x0[0] = (i16)b;                     /* the box, caption-high: the popup opens under the caption */
    mn->x1[0] = (i16)(b + font_h + 1);
    mn->bar_y = (i16)b;                     /* + MENUH - 1 = the line under the caption */
    vlog("w16: system menu of %04x opened\n", w->hwnd);
    open_popup(SYSMENU, 0, sel_first);
    pop.opened = click_time;
    return 1;
}

static int bar_item_at(struct menu *mn, int x)
{
    int k;
    for (k = 0; k < mn->t.n; k++)
        if (mn->t.it[k].parent == 0xFF && x >= mn->x0[k] && x < mn->x1[k])
            return k;
    return -1;
}

/* a bar item chosen: open its popup, or send a command for a plain bar item */
static void bar_choose(int mi, int k, int sel_first)
{
    struct menu *mn = menus[mi];
    if (mn->t.it[k].flags & (MF_GRAYED | MF_DISABLED))
        return;
    if (mn->t.it[k].flags & MF_POPUP) {
        open_popup(mi, k, sel_first);
        return;
    }
    close_popup(1);
    w16_post(mn->hwnd, WM_COMMAND, mn->t.it[k].id, 0);
}

int menu_bar_click(struct wnd *w, int x, int y)
{
    int sub, k;
    struct menu *mn = menu_of(w->hmenu, &sub);
    if (!mn || sub >= 0 || y < mn->bar_y || y >= mn->bar_y + MENUH)
        return 0;
    k = bar_item_at(mn, x);
    if (k >= 0)
        bar_choose(mn->idx, k, 0);
    return 1;
}

static int next_bar_item(struct menu *mn, int k, int dir)
{
    int i, n = mn->t.n;
    for (i = 1; i <= n; i++) {
        int j = ((k + dir * i) % n + n) % n;
        if (mn->t.it[j].parent == 0xFF)
            return j;
    }
    return k;
}

int menu_key(struct wnd *w, GR_KEY ch, u32 modifiers)
{
    int sub, k;
    struct menu *mn = menu_of(w->hmenu, &sub);
    if (!mn || sub >= 0)
        return 0;
    if (ch == MWKEY_F10) {
        for (k = 0; k < mn->t.n && mn->t.it[k].parent != 0xFF; k++)
            ;
        if (k < mn->t.n)
            bar_choose(mn->idx, k, 1);
        return 1;
    }
    if ((modifiers & MWKMOD_ALT) && ch > ' ' && ch < 127) {
        for (k = 0; k < mn->t.n; k++)
            if (mn->t.it[k].parent == 0xFF && mnemonic(&mn->t.it[k]) == upper((char)ch)) {
                bar_choose(mn->idx, k, 1);
                return 1;
            }
    }
    return 0;
}

int menu_event(GR_EVENT *ev)
{
    struct menu *mn;
    struct wnd *w;
    int r, k;
    if (!pop.open)
        return 0;
    mn = menus[pop.mi];
    switch (ev->type) {
    case GR_EVENT_TYPE_EXPOSURE:
        if (ev->exposure.wid != pop.wid)
            return 0;
        draw_popup();
        lat_shown("menu", 0);
        if (!pop.shown) {                   /* test hooks after the measurement */
            pop.shown = 1;
            vlog("w16: %s popup %u (%u rows) open at %d,%d %dx%d\n", pop.mi == SYSMENU ? "system menu" : "menu",
                 pop.top, pop.rows, pop.x, pop.y, pop.w, pop.h);
            if (opt_dump[0] && pop.mi == SYSMENU) {     /* -d: the system menu as drawn */
                static int nsys;
                char tag[12];
                GR_SCREEN_INFO si;
                GrGetScreenInfo(&si);       /* round trip: the server has drawn it */
                sprintf(tag, "sys%d", ++nsys);
                w16_dump_frame(tag);
            }
            ready_write();
        }
        return 1;
    case GR_EVENT_TYPE_BUTTON_DOWN:
        if (ev->button.wid == pop.wid) {
            r = row_at(ev->button.y);
            select_row(r >= 0 && usable(r) ? r : -1);
            return 1;
        }
        w = win_get(mn->hwnd);
        if (w && ev->button.wid == w->frame && ev->button.y >= mn->bar_y && ev->button.y < mn->bar_y + MENUH) {
            k = bar_item_at(mn, ev->button.x);
            if (k >= 0 && pop.mi == SYSMENU && pop.opened && ev->button.time - pop.opened < DBLCLICK_MS) {
                u16 hwnd = w->hwnd;             /* double-click on the system menu box */
                close_popup(1);
                vlog("w16: system menu double-click -> WM_SYSCOMMAND SC_CLOSE\n");
                w16_post(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
            } else if (k < 0 || k == pop.top)
                close_popup(1);
            else
                bar_choose(pop.mi, k, 0);
            return 1;
        }
        close_popup(1);                     /* a click elsewhere only closes the menu */
        return 1;
    case GR_EVENT_TYPE_BUTTON_UP:
        if (ev->button.wid == pop.wid) {
            r = row_at(ev->button.y);
            if (r >= 0 && r == pop.sel)
                choose(r);
        }
        return 1;
    case GR_EVENT_TYPE_KEY_DOWN: {
        GR_KEY ch = ev->keystroke.ch;
        int n = pop.rows, i;
        if (ch == MWKEY_F4 && (ev->keystroke.modifiers & MWKMOD_ALT)) {
            close_popup(1);                 /* Alt+F4 still closes the window */
            return 0;
        }
        if (ch == 27 || ch == MWKEY_F10)
            close_popup(1);
        else if (ch == 13) {
            if (pop.sel >= 0)
                choose(pop.sel);
        } else if (ch == MWKEY_UP || ch == MWKEY_DOWN) {
            int dir = ch == MWKEY_UP ? -1 : 1, s = pop.sel < 0 ? (dir > 0 ? -1 : 0) : pop.sel;
            for (i = 1; i <= n; i++) {
                r = ((s + dir * i) % n + n) % n;
                if (usable(r)) {
                    select_row(r);
                    break;
                }
            }
        } else if (ch == MWKEY_LEFT || ch == MWKEY_RIGHT) {
            /* like Windows, the system menu sits before the first bar item and after the last */
            int dir = ch == MWKEY_LEFT ? -1 : 1, sub;
            struct menu *bar;
            w = win_get(mn->hwnd);
            bar = w ? menu_of(w->hmenu, &sub) : 0;
            if (bar && sub >= 0)
                bar = 0;
            if (pop.mi == SYSMENU) {
                if (bar && bar->t.n)
                    bar_choose(bar->idx, next_bar_item(bar, dir > 0 ? bar->t.n - 1 : 0, dir), 1);
            } else {
                k = next_bar_item(mn, pop.top, dir);
                if (w && (dir > 0 ? k <= pop.top : k >= pop.top) && menu_sys_open(w, 1, 0))
                    ;                       /* wrapped past the end of the bar: the system menu */
                else
                    bar_choose(pop.mi, k, 1);
            }
        } else if (ch > ' ' && ch < 127) {
            for (r = 0; r < n; r++)
                if (usable(r) && mnemonic(&mn->t.it[pop.item[r]]) == upper((char)ch)) {
                    select_row(r);
                    choose(r);
                    break;
                }
        }
        return 1;
    }
    case GR_EVENT_TYPE_KEY_UP:
        return 1;
    }
    return 0;
}

int menu_ready_lines(char *buf, int size)
{
    int i, k, n = 0, nb, dialog = 0;
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used && wins[i].isdialog)
            dialog = 1;
    for (i = 0; i < MAXMENU && !dialog; i++) {
        struct wnd *w = USED(i) ? win_get(menus[i]->hwnd) : 0;
        if (!w || !w->visible)
            continue;
        for (k = 0, nb = 0; k < menus[i]->t.n; k++)
            if (menus[i]->t.it[k].parent == 0xFF && n < size - 40)
                n += sprintf(buf + n, "button %d %d %d\n", 1000 + nb++, w->x + (menus[i]->x0[k] + menus[i]->x1[k]) / 2,
                             w->y + menus[i]->bar_y + MENUH / 2);
    }
    if (pop.open)
        for (k = 0; k < pop.rows; k++)
            if (usable(k) && n < size - 40)
                n += sprintf(buf + n, "button %u %d %d\n", menus[pop.mi]->t.it[pop.item[k]].id, pop.x + pop.w / 2,
                             pop.y + (pop.ry[k] + pop.ry[k + 1]) / 2);
    return n;
}

/* ---------------------------------------------------------------- USER entry points */
/* GetMenu(hWnd) */
u32 u_GetMenu(u16 ss, u16 ap)
{
    struct wnd *w = win_get(AW(0));
    return w ? w->hmenu : 0;
}

/* GetSubMenu(hMenu, nPos) */
u32 u_GetSubMenu(u16 ss, u16 ap)
{
    int sub, k, pos = 0, want = (i16)AW(0);
    struct menu *mn = menu_of(AW(2), &sub);
    if (!mn)
        return 0;
    for (k = 0; k < mn->t.n; k++)
        if (mn->t.it[k].parent == (sub < 0 ? 0xFF : sub) && pos++ == want)
            return (mn->t.it[k].flags & MF_POPUP) ? (u16)(HMENU_BASE + 0x20 * mn->idx + 2 * (k + 1)) : 0;
    return 0;
}

/* the item of menu h (bar or popup) named by id or by position */
static struct menu_item *find_item(u16 h, u16 idpos, u16 flags, struct menu **pmn)
{
    int sub, k, pos = 0;
    struct menu *mn = menu_of(h, &sub);
    if (!mn)
        return 0;
    *pmn = mn;
    for (k = 0; k < mn->t.n; k++) {
        struct menu_item *it = &mn->t.it[k];
        if (flags & MF_BYPOSITION) {
            if (it->parent == (sub < 0 ? 0xFF : sub) && pos++ == idpos)
                return it;
        } else if (!(it->flags & (MF_POPUP | MF_SEPARATOR)) && it->id == idpos)
            return it;
    }
    return 0;
}

static void menu_changed(struct menu *mn)
{
    struct wnd *w = win_get(mn->hwnd);
    if (pop.open && menus[pop.mi] == mn)
        draw_popup();
    else if (w && w->visible)
        menu_draw_bar(w, mn->bar_y);
}

/* CheckMenuItem(hMenu, wIDCheckItem, wCheck): previous state, or -1 */
u32 u_CheckMenuItem(u16 ss, u16 ap)
{
    struct menu *mn;
    struct menu_item *it = find_item(AW(4), AW(2), AW(0), &mn);
    u16 prev;
    if (!it)
        return 0xFFFF;
    prev = it->flags & MF_CHECKED;
    it->flags = (u16)((it->flags & ~MF_CHECKED) | (AW(0) & MF_CHECKED));
    if (pop.open && menus[pop.mi] == mn)
        draw_popup();
    return prev;
}

/* EnableMenuItem(hMenu, wIDEnableItem, wEnable): previous state, or -1 */
u32 u_EnableMenuItem(u16 ss, u16 ap)
{
    struct menu *mn;
    struct menu_item *it = find_item(AW(4), AW(2), AW(0), &mn);
    u16 prev;
    if (!it)
        return 0xFFFF;
    prev = it->flags & (MF_GRAYED | MF_DISABLED);
    it->flags = (u16)((it->flags & ~(MF_GRAYED | MF_DISABLED)) | (AW(0) & (MF_GRAYED | MF_DISABLED)));
    menu_changed(mn);
    return prev;
}

/* DrawMenuBar(hWnd) */
u32 u_DrawMenuBar(u16 ss, u16 ap)
{
    int sub;
    struct wnd *w = win_get(AW(0));
    struct menu *mn = w ? menu_of(w->hmenu, &sub) : 0;
    if (mn && w->visible)
        menu_draw_bar(w, mn->bar_y);
    return 0;
}
