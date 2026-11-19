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
#define WM_ENABLE        0x000A
#define WM_PAINT         0x000F
#define WM_CLOSE         0x0010
#define WM_QUERYENDSESSION 0x0011
#define WM_ENDSESSION    0x0016
#define WM_QUIT          0x0012
#define WM_ERASEBKGND    0x0014
#define WM_SHOWWINDOW    0x0018
#define WM_NCDESTROY     0x0082
#define WM_KEYDOWN       0x0100
#define WM_KEYUP         0x0101
#define WM_CHAR          0x0102
#define WM_SYSKEYDOWN    0x0104
#define WM_SYSKEYUP      0x0105
#define WM_SYSCHAR       0x0106
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

#define WS_MINIMIZE      0x20000000ul
#define WS_MAXIMIZE      0x01000000ul
#define WM_QUERYOPEN     0x0013
#define ICON_WIDTH 80
#define ICON_HEIGHT (32+CAPH+4)
#define ICON_STEP_Y 64

#define WS_HSCROLL       0x00100000ul
#define WS_VSCROLL       0x00200000ul
#define SCROLLBAR_SIZE 16
#define WM_HSCROLL 0x0114
#define WM_VSCROLL 0x0115
#define WS_CHILD         0x40000000ul
#define WS_DISABLED      0x08000000ul
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

#define MAXWIN   32
#define MAXCLASS 8
#define MAXDC    6

/* built-in control kinds (dialog items) */
#define CTL_NONE    0
#define CTL_STATIC  1
#define CTL_BUTTON  2
#define CTL_EDIT    4
#define CTL_LIST    5
#define CTL_OTHER   3           /* class not provided: drawn as a placeholder */

struct wclass {
    u8   used;
    char name[24];
    u16  style, procoff, procseg, hinst, hbrush, hcursor, hicon, cbwndextra;
    char menu[16];              /* lpszMenuName: a name, "#n", or empty */
};

struct wnd {
    u8   used, visible, shown, invalid, erase, active;
    u16  hwnd, hinst, procoff, procseg;
    i16 scroll_min[2], scroll_max[2], scroll_pos[2];
    u8 scroll_init; /* per-axis state, cleared with the window */
    int  cls;
    u32  style;
    int  x, y, w, h;            /* frame: screen coordinates (controls: parent client coordinates) */
    int  cx, cy, cw, ch;        /* client area inside the frame */
    int  ul, ut, ur, ub;        /* update rectangle, client coordinates */
    char title[100];
    struct filelist *list;
    GR_WINDOW_ID frame, client; /* controls: one Nano-X window, frame == client */
    u16  mdi_active;            /* active child HWND, MDICLIENT only */
    u16  owner;                 /* owned top-level window; distinct from a child parent */
    u16  parent;                /* controls: the dialog */
    u8   ctl, pressed, isdialog, ended, checked;
    u16  id, result, defid;
    u8 edit_cursor, edit_anchor, edit_limit;
    u8  *extra;                 /* zeroed class cbWndExtra bytes */
    u8  *icon;                  /* SS_ICON: DIB of the chosen icon image (malloc) */
    u16  iconlen;
    u16  hmenu;                 /* menu bar, or 0 */
    u8   readied;               /* test hook: ready file written after its first paint */
    u8   stdicon;               /* SS_ICON of a message box: 1 hand, 2 question, 3 exclamation, 4 asterisk */
};

struct dc {
    u8   used, paint;
    u16  hdc;
    struct wnd *w;
    GR_WINDOW_ID wid;
    GR_GC_ID gc;
    u16  pen, brush, font, palette;
    u32  textcolor, bkcolor;
    u16  bkmode, textalign;
    u8   rop2, stretchmode;
    u16 mapmode;
    i16 wx,wy,vx,vy,wex,wey,vex,vey;
    int  curx, cury;
    i16 brushx, brushy;
};

#define HWND_BASE  0x0100
#define HDC_BASE   0x0200
#define HSTOCK     0x0300       /* stock objects: HSTOCK + 2 * index */
#define HOBJ_BASE  0x0400
#define HCURSOR_BASE 0x0500     /* cursors: HCURSOR_BASE + 2 * index (cursor.c) */
#define HICON      0x0590
#define HACCEL_BASE 0x0800      /* accelerator tables */
#define HBMP_BASE  0x0900       /* monochrome bitmaps (gdi.c) */
#define HMENU_BASE 0x2000       /* disjoint from object, resource and bitmap handle ranges */
#define HMENU_STRIDE 0x0200    /* room for 128 item-indexed popup handles per menu */

u32 u_CreateCaret(u16 ss,u16 ap);
u32 u_DestroyCaret(u16 ss,u16 ap);
u32 u_SetCaretPos(u16 ss,u16 ap);
u32 u_HideCaret(u16 ss,u16 ap);
u32 u_ShowCaret(u16 ss,u16 ap);
void caret_destroy(u16 hwnd);
void caret_paint(u16 hwnd,int begin);
u32 caret_wait(u32 now);
struct drag_event { i16 x,y;u8 released; };
struct drag_cursor {u16 old,alternate_handle;GR_CURSOR_ID good,bad,alternate;u8 owned;};
int cursor_drag_start(struct drag_cursor *,u16 h);
void cursor_drag_choose(struct drag_cursor *,u16 reply);
void cursor_drag_end(struct drag_cursor *);
int user_input_blocked(struct wnd *);
u16 user_window_relative(u16 hwnd,u16 cmd);
int user_drag_capture(u16 hwnd);
int user_drag_alive(u16 hwnd,GR_WINDOW_ID frame);
void user_drag_release(u16 hwnd);
int user_drag_next(u16 hwnd,GR_WINDOW_ID frame,struct drag_event *);
extern struct wnd wins[MAXWIN];
struct wnd *win_get(u16 hwnd);
void win_client_origin(struct wnd *w,int *x,int *y);
u32  win_send(struct wnd *w, u16 msg, u16 wp, u32 lp);
void win_invalidate_all(struct wnd *w, int erase);

u8       *res_load(u16 type, const char *name, u16 id, u16 *len);   /* malloc'd copy of a resource */
u16       message_box(const char *text, const char *caption, u16 type);
u16       user_key_mods(void);        /* MWKMOD_* at the latest key event */

/* cursors and accelerators (cursor.c) */
void      cursor_class(GR_WINDOW_ID frame,GR_WINDOW_ID client,u16 h);
void      cursor_attach(GR_WINDOW_ID frame, GR_WINDOW_ID client);
void      pointer_event(GR_EVENT *ev);
void      cursor_census(u16 *n);

/* GDI */
GR_COLOR  rgb_to_gr(u32 colorref);
u32       brush_color(u16 hbrush, int *isnull);
u16       dc_create(struct wnd *w, int paint);
void      dc_release(u16 hdc);
struct dc *dc_get(u16 hdc);
void gdi_map_init(struct dc *d);
int gdi_map_point(struct dc *d,int *x,int *y);
int gdi_map_rect(struct dc *d,int *l,int *t,int *r,int *b);
void      gdi_fill(GR_WINDOW_ID wid, GR_COLOR c, int x, int y, int w, int h);
void      gdi_fg(GR_COLOR c);           /* scratch GC colour, sent only when it changes */
void      gdi_usebg(int on);
void      gdi_text_size(const char *s, int n, GR_SIZE *w, GR_SIZE *h, GR_SIZE *base);  /* no round trip */
int       gdi_draw_icon(GR_WINDOW_ID wid, int x, int y, const u8 *dib, u16 len);
void gdi_draw_text(struct dc *d, int x, int y, const char *text, int n);
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
int       menu_attach(struct wnd *w,u16 hmenu);
void      menu_draw_bar(struct wnd *w, int y);                 /* in the frame, at frame row y */
int       menu_bar_click(struct wnd *w, int x, int y);          /* frame coordinates; 1 if handled */
int       menu_event(GR_EVENT *ev);                            /* events while a menu is open; 1 if taken */
int       menu_key(struct wnd *w, GR_KEY ch, u32 modifiers);   /* F10 / Alt+letter; 1 if taken */
int       menu_ready_lines(char *buf, int size);                /* "button" lines for the open popup */
void      menu_close_for(u16 hwnd);
int       menu_sys_open(struct wnd *w, int sel_first, u32 click_time);   /* 0 if w has no system menu box */
void      nc_sysbox(struct wnd *w, int open);  /* the caption's system menu box, highlighted while open */
void      menu_census(u16 *nmenus);

/* frame dumps / readiness */
void      paint_done(struct wnd *w);

/* Resource bitmap/source-only memory DC support. */
void registered_reset(void);
void bitmap_shutdown(void);
u32 bitmap_moveto(u16 h,i16 x,i16 y);
int bitmap_lineto(u16 h,i16 x,i16 y,u32 color,u16 style,u16 width,int nullpen);
int bitmap_rectangle(u16 h,i16 l,i16 t,i16 r,i16 bot,u32 pen,u16 ps,u16 pw,int np,u32 brush,u16 bs,int nb);
u16 bitmap_object(u16 h,u16 type,u16 object);
int bitmap_object_used(u16 object);
int bitmap_patblt(u16 h,i16 x,i16 y,i16 w,i16 height,u32 rop,u32 color,u16 style,int nullbrush);
u16 bitmap_savedc(u16 h);
u16 bitmap_restoredc(u16 h,i16 level);
u16 bitmap_rop2(u16 h,u16 mode);
u16 bitmap_create(u16 w,u16 h,u16 planes,u16 bpp,u16 seg,u16 off);
int bitmap_brushorg(u16 dc,i16 x,i16 y,u32 *old);
u32 bitmap_getpixel(u16 dc,i16 x,i16 y);
u32 bitmap_setpixel(u16 dc,i16 x,i16 y,u32 rgb);
u32 bitmap_origin(u16 h,int set,int viewport,i16 x,i16 y);
u32 bitmap_getcolor(u16 h,int bk);
u32 bitmap_setcolor(u16 dc,u32 rgb,int bk);
int bitmap_delete(u16 h);
int bitmap_deletedc(u16 h);
u16 bitmap_select(u16 dc,u16 h);

#endif

void scrollbar_draw(struct wnd *w);
int scrollbar_input(struct wnd *w,int kind,int rx,int ry);
void scrollbar_forget(u16 hwnd);

u32 bitmap_bits(u16 h,u32 count,u16 seg,u16 off,int set);

struct poly_point;
/* Pen argument is stroke width (0 = null); polygons accept widths up to32. */
int bitmap_polygon(u16,struct poly_point *,int,u32,int,u32,int);

int bitmap_flood(u16,struct dc*,int,int,u32,int,u32,void (*)(void*,int,int,int,int),void*);
int bitmap_pattern(u16,u8 *,u8 *,u16 *,u32 *);

int gdi_pattern_color(struct dc*,int,int,u32*);

u16 bitmap_textmode(u16,u16,int);
int bitmap_text(u16,int,int,u16,u16,int);

u8 rasterfont_width(u8);
u16 rasterfont_row(u8,int);
