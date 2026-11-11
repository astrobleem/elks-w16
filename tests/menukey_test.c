/* Host test of menu.c's keyboard handling: Alt+F4 while a menu bar popup or the system menu is open
 * must close the popup and send the owner WM_SYSCOMMAND SC_CLOSE (the key event belongs to the popup,
 * so the normal Alt+F4 path, which looks the window up by the event's window, cannot see it).
 * Nano-X and the rest of USER are stubbed; menu.c and dlgtmpl.c (menu_parse) are the real ones.
 * Build: gcc -DW16_HOST -I$MWDIR/src/include -Isrc tests/menukey_test.c src/dlgtmpl.c */
#include "../src/menu.c"

/* ---- stubs: Nano-X */
static int next_wid = 100, destroyed, focus;
GR_WINDOW_ID GrNewWindowEx(GR_WM_PROPS p, const char *t, GR_WINDOW_ID par, GR_COORD x, GR_COORD y,
                           GR_SIZE w, GR_SIZE h, GR_COLOR bg) { return ++next_wid; }
void GrDestroyWindow(GR_WINDOW_ID wid) { destroyed = wid; }
void GrMapWindow(GR_WINDOW_ID wid) {}
void GrSelectEvents(GR_WINDOW_ID wid, GR_EVENT_MASK m) {}
void GrSetFocus(GR_WINDOW_ID wid) { focus = wid; }
void GrGetScreenInfo(GR_SCREEN_INFO *si) { memset(si, 0, sizeof(*si)); si->cols = 320; si->rows = 200; }
void GrLine(GR_DRAW_ID d, GR_GC_ID gc, GR_COORD x1, GR_COORD y1, GR_COORD x2, GR_COORD y2) {}
void GrRect(GR_DRAW_ID d, GR_GC_ID gc, GR_COORD x, GR_COORD y, GR_SIZE w, GR_SIZE h) {}
void GrText(GR_DRAW_ID d, GR_GC_ID gc, GR_COORD x, GR_COORD y, void *s, GR_COUNT n, GR_TEXTFLAGS f) {}

/* ---- stubs: the rest of w16 */
struct wnd wins[MAXWIN];
int font_h = 10;
GR_GC_ID gdi_gc;
char opt_dump[40];
u8 w16_scratch[W16_SCRATCH];
struct ne_module *w16_mod;
static u8 fake_mem[65536];
u8 *host_ptr(u16 seg, u16 off, u16 n) { return fake_mem + (u16)(seg * 16 + off); }   /* never reached */
static int nposts;
static u16 post_hwnd, post_msg, post_wp;
struct wnd *win_get(u16 hwnd)
{
    int i;
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used && wins[i].hwnd == hwnd)
            return &wins[i];
    return 0;
}
u32 win_send(struct wnd *w, u16 msg, u16 wp, u32 lp) { return 0; }
void w16_post(u16 hwnd, u16 msg, u16 wp, u32 lp) { nposts++; post_hwnd = hwnd; post_msg = msg; post_wp = wp; }
void nc_sysbox(struct wnd *w, int open) {}
void ready_write(void) {}
void lat_shown(const char *what, int synced) {}
int w16_dump_frame(const char *tag) { return 0; }
void gdi_fg(GR_COLOR c) {}
void gdi_fill(GR_WINDOW_ID wid, GR_COLOR c, int x, int y, int w, int h) {}
void gdi_usebg(int on) {}
void gdi_text_size(const char *s, int n, GR_SIZE *w, GR_SIZE *h, GR_SIZE *b) { *w = 8 * n; *h = 10; *b = 8; }
u32 gdi_sys_color(u16 idx) { return 0; }
void wlog(const char *f, ...) {}
void vlog(const char *f, ...) {}
/* a MENU resource: popup "&Settings" holding "&Analog" (1) and "&Digital" (2) */
static const u8 menu_res[] = { 0, 0, 0, 0,
    0x90, 0, '&', 'S', 'e', 't', 't', 'i', 'n', 'g', 's', 0,
    0x00, 0, 1, 0, '&', 'A', 'n', 'a', 'l', 'o', 'g', 0,
    0x80, 0, 2, 0, '&', 'D', 'i', 'g', 'i', 't', 'a', 'l', 0 };
int ne_res_find(struct ne_module *m, u16 type, const char *name, u16 id, u32 *off, u16 *len)
{
    *off = 0; *len = sizeof(menu_res); return 0;
}
int ne_read(struct ne_module *m, u32 off, void *buf, u16 n) { memcpy(buf, menu_res + off, n); return 0; }

/* ---- the test */
static int fails, cases;
static void check(int ok, const char *what)
{
    cases++;
    if (!ok)
        fails++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
}
static int key(GR_KEY ch, int mods)
{
    GR_EVENT ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = GR_EVENT_TYPE_KEY_DOWN;
    ev.keystroke.wid = pop.open ? pop.wid : 0;
    ev.keystroke.ch = ch;
    ev.keystroke.modifiers = mods;
    return menu_event(&ev);
}
static struct wnd *mkwin(int i, u16 hwnd)
{
    struct wnd *w = &wins[i];
    memset(w, 0, sizeof(*w));
    w->used = w->visible = 1;
    w->hwnd = hwnd;
    w->style = WS_CAPTION | WS_SYSMENU;
    w->x = 10; w->y = 10; w->w = 200; w->h = 150;
    w->frame = 10 + i; w->client = 20 + i;
    return w;
}
int main(void)
{
    struct wnd *w = mkwin(0, 0x100), *box = mkwin(1, 0x102);
    w->hmenu = menu_load(w, 0, 1);
    check(w->hmenu != 0, "menu bar loaded from a MENU resource");

    /* menu bar popup, opened with F10 */
    nposts = 0;
    check(menu_key(w, MWKEY_F10, 0) && pop.open, "F10 opens the Settings popup");
    check(key(MWKEY_F4, MWKMOD_ALT) == 1, "Alt+F4 with the popup open is taken by the menu code");
    check(!pop.open && destroyed, "the popup is closed");
    check(nposts == 1 && post_hwnd == 0x100 && post_msg == WM_SYSCOMMAND && post_wp == SC_CLOSE,
          "the window gets WM_SYSCOMMAND SC_CLOSE");

    /* system menu, opened as Alt+Space does */
    nposts = 0;
    check(menu_sys_open(w, 1, 0) && pop.open && pop.mi == SYSMENU, "Alt+Space opens the system menu");
    check(key(MWKEY_F4, MWKMOD_ALT) == 1 && !pop.open, "Alt+F4 with the system menu open closes it");
    check(nposts == 1 && post_hwnd == 0x100 && post_msg == WM_SYSCOMMAND && post_wp == SC_CLOSE,
          "the window gets WM_SYSCOMMAND SC_CLOSE");

    /* the system menu of a second window (a message box): SC_CLOSE goes to that window */
    nposts = 0;
    menu_sys_open(box, 1, 0);
    key(MWKEY_F4, MWKMOD_ALT);
    check(nposts == 1 && post_hwnd == 0x102 && post_wp == SC_CLOSE, "a message box's system menu: SC_CLOSE to the box");

    /* controls: Esc and a plain F4 close the popup without closing anything */
    nposts = 0;
    menu_key(w, MWKEY_F10, 0);
    key(27, 0);
    check(!pop.open && nposts == 0, "Esc closes the popup and sends nothing");
    menu_sys_open(w, 1, 0);
    key(MWKEY_F4, 0);
    check(pop.open && nposts == 0, "F4 without Alt does not close the window");
    key(27, 0);
    check(key(MWKEY_F4, MWKMOD_ALT) == 0 && nposts == 0, "with no menu open Alt+F4 is left to the window");

    printf("== menus: %d passed, %d failed\n", cases - fails, fails);
    return fails != 0;
}
