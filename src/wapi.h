/* wapi.h - USER/GDI internals shared by user.c and gdi.c (ELKS build only) */
#ifndef WAPI_H
#define WAPI_H
#include "nano-X.h"
#include "w16.h"

/* Win16 message numbers used here */
#define WM_CREATE        0x0001
#define WM_DESTROY       0x0002
#define WM_MOVE          0x0003
#define WM_SIZE          0x0005
#define WM_SETFOCUS      0x0007
#define WM_PAINT         0x000F
#define WM_CLOSE         0x0010
#define WM_QUIT          0x0012
#define WM_ERASEBKGND    0x0014
#define WM_SHOWWINDOW    0x0018
#define WM_NCDESTROY     0x0082
#define WM_KEYDOWN       0x0100
#define WM_KEYUP         0x0101
#define WM_CHAR          0x0102
#define WM_SYSKEYDOWN    0x0104
#define WM_SYSCOMMAND    0x0112
#define WM_TIMER         0x0113
#define WM_INITMENU      0x0116
#define WM_INITMENUPOPUP 0x0117
#define WM_MOUSEMOVE     0x0200
#define WM_LBUTTONDOWN   0x0201
#define WM_LBUTTONUP     0x0202
#define WM_RBUTTONDOWN   0x0204
#define WM_RBUTTONUP     0x0205
#define SC_CLOSE         0xF060

#define WS_CHILD         0x40000000ul
#define WS_VISIBLE       0x10000000ul
#define WS_CAPTION       0x00C00000ul
#define WS_BORDER        0x00800000ul
#define WS_THICKFRAME    0x00040000ul
#define WS_SYSMENU       0x00080000ul
#define WS_MINIMIZEBOX   0x00020000ul
#define WS_MAXIMIZEBOX   0x00010000ul

#define WM_INITDIALOG    0x0110
#define WM_COMMAND       0x0111
#define IDOK             1
#define IDCANCEL         2
#define BN_CLICKED       0
#define WS_POPUP         0x80000000ul
#define WS_TABSTOP       0x00010000ul
#define DS_MODALFRAME    0x0080

/* GC mode flags added to Nano-X by nanox-w16.patch; a stock server stores them without effect */
#ifndef GR_MODE_EXCLUDEENDPOINT
#define GR_MODE_EXCLUDEENDPOINT      0x0200      /* GrLine leaves out its end point (Windows LineTo) */
#define GR_MODE_HAVE_EXCLUDEENDPOINT 0x0400      /* set by the server when it honours the flag above */
#endif

/* SetROP2 binary raster operations */
#define R2_BLACK        1
#define R2_NOTMERGEPEN  2
#define R2_MASKNOTPEN   3
#define R2_NOTCOPYPEN   4
#define R2_MASKPENNOT   5
#define R2_NOT          6
#define R2_XORPEN       7
#define R2_NOTMASKPEN   8
#define R2_MASKPEN      9
#define R2_NOTXORPEN    10
#define R2_NOP          11
#define R2_MERGENOTPEN  12
#define R2_COPYPEN      13
#define R2_MERGEPENNOT  14
#define R2_MERGEPEN     15
#define R2_WHITE        16

#define MAXWIN   16
#define MAXCLASS 8
#define MAXDC    6

/* built-in control kinds (dialog items) */
#define CTL_NONE    0
#define CTL_STATIC  1
#define CTL_BUTTON  2
#define CTL_OTHER   3           /* class not provided: drawn as a placeholder */

struct wclass {
    u8   used;
    char name[24];
    u16  style, procoff, procseg, hinst, hbrush, cbwndextra;
    char menu[16];              /* lpszMenuName: a name, "#n", or empty */
};

struct wnd {
    u8   used, visible, shown, invalid, erase, active;
    u16  hwnd, hinst, procoff, procseg;
    int  cls;
    u32  style;
    int  x, y, w, h;            /* frame: screen coordinates (controls: parent client coordinates) */
    int  cx, cy, cw, ch;        /* client area inside the frame */
    int  ul, ut, ur, ub;        /* update rectangle, client coordinates */
    char title[100];
    GR_WINDOW_ID frame, client; /* controls: one Nano-X window, frame == client */
    u16  parent;                /* controls: the dialog */
    u8   ctl, pressed, isdialog, ended;
    u16  id, result, defid;
    u8  *icon;                  /* SS_ICON: DIB of the chosen icon image (malloc) */
    u16  iconlen;
    u16  hmenu;                 /* menu bar, or 0 */
    u8   readied;               /* test hook: ready file written after its first paint */
};

struct dc {
    u8   used, paint;
    u16  hdc;
    struct wnd *w;
    GR_WINDOW_ID wid;
    GR_GC_ID gc;
    u16  pen, brush, font;
    u32  textcolor, bkcolor;
    u16  bkmode, textalign;
    u8   rop2;
    int  curx, cury;
};

#define HWND_BASE  0x0100
#define HDC_BASE   0x0200
#define HSTOCK     0x0300       /* stock objects: HSTOCK + 2 * index */
#define HOBJ_BASE  0x0400
#define HCURSOR_BASE 0x0500     /* cursors: HCURSOR_BASE + 2 * index (cursor.c) */
#define HICON      0x0590
#define HACCEL_BASE 0x0800      /* accelerator tables */
#define HBMP_BASE  0x0900       /* monochrome bitmaps (gdi.c) */
#define HMENU_BASE 0x0600       /* menu bars: HMENU_BASE + 0x20 * n; their popups add 2 * (item + 1) */

extern struct wnd wins[MAXWIN];
struct wnd *win_get(u16 hwnd);
u32  win_send(struct wnd *w, u16 msg, u16 wp, u32 lp);
void win_invalidate_all(struct wnd *w, int erase);

u8       *res_load(u16 type, const char *name, u16 id, u16 *len);   /* malloc'd copy of a resource */
u16       message_box(const char *text, const char *caption, u16 type);
u16       user_key_mods(void);        /* MWKMOD_* at the latest key event */

/* cursors and accelerators (cursor.c) */
void      cursor_attach(GR_WINDOW_ID frame, GR_WINDOW_ID client);
void      cursor_census(u16 *n);

/* GDI */
GR_COLOR  rgb_to_gr(u32 colorref);
u32       brush_color(u16 hbrush, int *isnull);
u16       dc_create(struct wnd *w, int paint);
void      dc_release(u16 hdc);
struct dc *dc_get(u16 hdc);
void      gdi_fill(GR_WINDOW_ID wid, GR_COLOR c, int x, int y, int w, int h);
void      gdi_fg(GR_COLOR c);           /* scratch GC colour, sent only when it changes */
void      gdi_usebg(int on);
int       gdi_draw_icon(GR_WINDOW_ID wid, int x, int y, const u8 *dib, u16 len);
extern GR_GC_ID gdi_gc;     /* scratch GC for USER drawing */

u32       gdi_sys_color(u16 idx);
void      gdi_census(u16 *ndc, u16 *nobj);

/* USER internals shared with menu.c */
extern int font_h;
void      w16_post(u16 hwnd, u16 msg, u16 wp, u32 lp);
void      ready_write(void);
void      w16_census(const char *when);  /* windows, DCs, objects, timers, heap: leak checks */
void      lat_shown(const char *what, int synced);   /* -m: report input-to-screen delay */
u32       now_ms(void);
extern int opt_metrics;
extern u32 w16_t0;          /* test hook: rewrite the ready file with current click targets */

/* menus (menu.c) */
#define MENUH (font_h + 3)              /* menu bar height, including its bottom line */
u16       menu_load(struct wnd *w, const char *name, u16 id);   /* attach a MENU resource as w's menu bar */
void      menu_free(u16 hmenu);
void      menu_draw_bar(struct wnd *w, int y);                 /* in the frame, at frame row y */
int       menu_bar_click(struct wnd *w, int x, int y);          /* frame coordinates; 1 if handled */
int       menu_event(GR_EVENT *ev);                            /* events while a menu is open; 1 if taken */
int       menu_key(struct wnd *w, GR_KEY ch, u32 modifiers);   /* F10 / Alt+letter; 1 if taken */
int       menu_ready_lines(char *buf, int size);                /* "button" lines for the open popup */
void      menu_close_for(u16 hwnd);
void      menu_census(u16 *nmenus);

/* frame dumps / readiness */
void      paint_done(struct wnd *w);
#endif
