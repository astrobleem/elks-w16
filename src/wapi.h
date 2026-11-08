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
#define WM_SYSCOMMAND    0x0112
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

#define MAXWIN   8
#define MAXCLASS 8
#define MAXDC    6

struct wclass {
    u8   used;
    char name[24];
    u16  style, procoff, procseg, hinst, hbrush, cbwndextra;
};

struct wnd {
    u8   used, visible, shown, invalid, erase, active;
    u16  hwnd, hinst, procoff, procseg;
    int  cls;
    u32  style;
    int  x, y, w, h;            /* frame, screen coordinates */
    int  cx, cy, cw, ch;        /* client area inside the frame */
    int  ul, ut, ur, ub;        /* update rectangle, client coordinates */
    char title[32];
    GR_WINDOW_ID frame, client;
    u32  lastsysclick;
};

struct dc {
    u8   used, paint;
    u16  hdc;
    struct wnd *w;
    GR_WINDOW_ID wid;
    GR_GC_ID gc;
    u16  pen, brush, font;
    u32  textcolor, bkcolor;
    u16  bkmode;
    int  curx, cury;
};

#define HWND_BASE  0x0100
#define HDC_BASE   0x0200
#define HSTOCK     0x0300       /* stock objects: HSTOCK + 2 * index */
#define HOBJ_BASE  0x0400
#define HCURSOR    0x0500
#define HICON      0x0502

extern struct wnd wins[MAXWIN];
struct wnd *win_get(u16 hwnd);
u32  win_send(struct wnd *w, u16 msg, u16 wp, u32 lp);
void win_invalidate_all(struct wnd *w, int erase);

/* GDI */
GR_COLOR  rgb_to_gr(u32 colorref);
u32       brush_color(u16 hbrush, int *isnull);
u16       dc_create(struct wnd *w, int paint);
void      dc_release(u16 hdc);
struct dc *dc_get(u16 hdc);
void      gdi_fill(GR_WINDOW_ID wid, GR_COLOR c, int x, int y, int w, int h);
extern GR_GC_ID gdi_gc;     /* scratch GC for USER drawing */

/* frame dumps / readiness */
void      paint_done(struct wnd *w);
#endif
