/*
 * cursor.c - USER cursors, accelerator tables and WinHelp.
 *
 * Cursors: the standard arrow is Nano-X's own; the wait and cross cursors are drawn here; a
 * program's CURSOR resource (a Windows DIB with AND and XOR masks) becomes a Nano-X cursor,
 * halved from 32x32 to 16x16 because the screen is 320x200. SetCursor applies to every
 * top-level window of the program, so the shape holds wherever the pointer is over it.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "wapi.h"

/* ---------------------------------------------------------------- cursors */
#define MAXCUR 6
static struct { u8 used; GR_CURSOR_ID id; } cursors[MAXCUR];   /* HCURSOR_BASE + 2 * i; id 0 = the arrow */
static u16 cur_hcur;                /* the program's current cursor, 0 = the arrow */
static int cur_show;                /* ShowCursor count: hidden while negative */
static GR_CURSOR_ID cur_blank, cur_shown = (GR_CURSOR_ID)-1;
static u16 cur_std[3];              /* handles of the arrow, wait and cross cursors once made */

/* 16x16 hourglass and cross: fg bits are black, bg bits white (MSB = left) */
static const GR_BITMAP wait_fg[16] = { 0x7FFE, 0x7FFE, 0x2004, 0x1008, 0x0810, 0x0420, 0x0240, 0x0180,
                                       0x0180, 0x0240, 0x0420, 0x0810, 0x1008, 0x2004, 0x7FFE, 0x7FFE };
static const GR_BITMAP wait_bg[16] = { 0x0000, 0x0000, 0x1FF8, 0x0FF0, 0x07E0, 0x03C0, 0x0180, 0x0000,
                                       0x0000, 0x0180, 0x03C0, 0x07E0, 0x0FF0, 0x1FF8, 0x0000, 0x0000 };
static const GR_BITMAP cross_fg[16] = { 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0xFFFF,
                                        0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100 };
static const GR_BITMAP cross_bg[16] = { 0x0280, 0x0280, 0x0280, 0x0280, 0x0280, 0x0280, 0xFEFF, 0x0000,
                                        0xFEFF, 0x0280, 0x0280, 0x0280, 0x0280, 0x0280, 0x0280, 0x0280 };

static u16 cur_handle(GR_CURSOR_ID id)
{
    int i;
    for (i = 0; i < MAXCUR && cursors[i].used; i++)
        ;
    if (i == MAXCUR) {
        wlog("w16: cursor table full (%d)\n", MAXCUR);
        return 0;
    }
    cursors[i].used = 1;
    cursors[i].id = id;
    return (u16)(HCURSOR_BASE + 2 * i);
}

static GR_CURSOR_ID cur_id(u16 h)
{
    u16 i = (u16)(h - HCURSOR_BASE) >> 1;
    return (h >= HCURSOR_BASE && !(h & 1) && i < MAXCUR && cursors[i].used) ? cursors[i].id : 0;
}

static GR_CURSOR_ID current_id(void)
{
    static GR_BITMAP none[1];
    if (cur_show >= 0)
        return cur_id(cur_hcur);
    if (!cur_blank)
        cur_blank = GrNewCursor(1, 1, 0, 0, GR_RGB(0, 0, 0), GR_RGB(0, 0, 0), none, none);
    return cur_blank;
}

/* the cursor for every top-level window of the program (sent only when it changes) */
static void cur_apply(void)
{
    GR_CURSOR_ID id = current_id();
    int i;
    if (id == cur_shown)
        return;
    cur_shown = id;
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used && !wins[i].ctl) {
            GrSetWindowCursor(wins[i].frame, id);
            GrSetWindowCursor(wins[i].client, id);
        }
}

/* a new top-level window takes the current cursor */
void cursor_attach(GR_WINDOW_ID frame, GR_WINDOW_ID client)
{
    GR_CURSOR_ID id = current_id();
    if (id) {
        GrSetWindowCursor(frame, id);
        GrSetWindowCursor(client, id);
    }
}

/* a program CURSOR resource: WORD hot x, WORD hot y, BITMAPINFOHEADER (height doubled), two colours,
 * XOR rows, AND rows; rows bottom-up. AND 1 is transparent, else XOR 1 is white and 0 black. */
static GR_CURSOR_ID res_cursor(const char *name, u16 id)
{
    u16 glen, len, cid = 0, w, h, bpp, stride, x, y, hx, hy, scale;
    u8 *g = res_load(RT_GROUP_CURSOR, name, id, &glen), *d;
    GR_BITMAP fg[16], bg[16];
    GR_CURSOR_ID r;
    if (!g)
        return 0;
    if (glen >= 20)                                     /* first entry: WORD w, h, planes, bits, DWORD size, WORD id */
        cid = (u16)(g[18] | (g[19] << 8));
    free(g);
    if (!cid || !(d = res_load(RT_CURSOR, 0, cid, &len)))
        return 0;
    hx = (u16)(d[0] | (d[1] << 8)); hy = (u16)(d[2] | (d[3] << 8));
    w = (u16)(d[8] | (d[9] << 8)); h = (u16)((d[12] | (d[13] << 8)) / 2); bpp = (u16)(d[18] | (d[19] << 8));
    stride = (u16)(((w + 31) / 32) * 4);
    if (bpp != 1 || (w != 32 && w != 16) || h != w || len < 52 || (u32)len - 52 < 2ul * stride * h) {
        wlog("w16: cursor %u: %ux%u at %u bits is not supported (32x32 or 16x16 monochrome)\n", cid, w, h, bpp);
        free(d);
        return 0;
    }
    scale = w / 16;
    memset(fg, 0, sizeof(fg));
    memset(bg, 0, sizeof(bg));
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            u16 sx = x * scale, sy = y * scale;
            const u8 *xr = d + 52 + stride * (h - 1 - sy), *ar = xr + stride * h;
            int a = (ar[sx >> 3] >> (7 - (sx & 7))) & 1, v = (xr[sx >> 3] >> (7 - (sx & 7))) & 1;
            if (!a && v)
                fg[y] |= (GR_BITMAP)(0x8000u >> x);     /* white */
            else if (!a || v)
                bg[y] |= (GR_BITMAP)(0x8000u >> x);     /* black, and the inverting pixels */
        }
    free(d);
    r = GrNewCursor(16, 16, hx / scale, hy / scale, GR_RGB(255, 255, 255), GR_RGB(0, 0, 0), fg, bg);
    vlog("w16: cursor %u: %ux%u resource, hot spot %u,%u -> Nano-X cursor %d at 16x16\n", cid, w, h, hx, hy, (int)r);
    return r;
}

/* LoadCursor(hInstance, lpCursorName): IDC_WAIT and IDC_CROSS are drawn here, every other standard
 * cursor is the arrow; a program's own cursor comes from its resources */
u32 u_LoadCursor(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2), hinst = AW(4), id;
    char name[34];
    GR_CURSOR_ID cid;
    int which = 0;
    if (!hinst) {
        which = off == 0x7F02 ? 1 : off == 0x7F03 ? 2 : 0;
        if (!cur_std[which])
            cur_std[which] = cur_handle(!which ? 0 : GrNewCursor(16, 16, 7, 7, GR_RGB(0, 0, 0), GR_RGB(255, 255, 255),
                                                                (GR_BITMAP *)(which == 1 ? wait_fg : cross_fg),
                                                                (GR_BITMAP *)(which == 1 ? wait_bg : cross_bg)));
        return cur_std[which];
    }
    res_name(off, seg, name, &id);
    cid = res_cursor(name[0] ? name : 0, id);
    if (!cid) {
        wlog("w16: LoadCursor: cursor %s%s is not usable; the arrow stands in\n", name[0] ? name : "#", name[0] ? "" : "(ordinal)");
        if (!cur_std[0])
            cur_std[0] = cur_handle(0);
        return cur_std[0];
    }
    return cur_handle(cid);
}

/* SetCursor(hCursor): returns the previous cursor */
u32 u_SetCursor(u16 ss, u16 ap)
{
    u16 prev = cur_hcur;
    cur_hcur = AW(0);
    cur_apply();
    return prev;
}

/* ShowCursor(bShow): a display count, hidden while negative */
u32 u_ShowCursor(u16 ss, u16 ap)
{
    cur_show += AW(0) ? 1 : -1;
    cur_apply();
    return (u16)cur_show;
}

/* SetCursorPos(X, Y) */
u32 u_SetCursorPos(u16 ss, u16 ap)
{
    vlog("w16: SetCursorPos(%d,%d)\n", (i16)AW(2), (i16)AW(0));
    GrMoveCursor((i16)AW(2), (i16)AW(0));
    return 1;
}

void cursor_census(u16 *n)
{
    int i;
    *n = 0;
    for (i = 0; i < MAXCUR; i++)
        if (cursors[i].used && cursors[i].id)
            (*n)++;
}

/* ---------------------------------------------------------------- accelerators */
#define MAXACC 2
struct accel { u8 fl; u16 key, cmd; };          /* FVIRTKEY 1, FNOINVERT 2, FSHIFT 4, FCONTROL 8, FALT 16, last 0x80 */
static struct { u8 used, n; struct accel e[16]; } accels[MAXACC];

/* LoadAccelerators(hInstance, lpTableName) */
u32 u_LoadAccelerators(u16 ss, u16 ap)
{
    char name[34];
    u16 id, len, k;
    u8 *d;
    int i;
    res_name(AW(0), AW(2), name, &id);
    for (i = 0; i < MAXACC && accels[i].used; i++)
        ;
    if (i == MAXACC) {
        wlog("w16: LoadAccelerators: table full (%d)\n", MAXACC);
        return 0;
    }
    d = res_load(RT_ACCELERATOR, name[0] ? name : 0, id, &len);
    if (!d) {
        wlog("w16: LoadAccelerators: table %s%s not found\n", name[0] ? name : "#", name[0] ? "" : "(ordinal)");
        return 0;
    }
    accels[i].used = 1;
    accels[i].n = 0;
    for (k = 0; k + 5 <= len && accels[i].n < 16; k += 5) {
        struct accel *a = &accels[i].e[accels[i].n++];
        a->fl = d[k];
        a->key = (u16)(d[k + 1] | (d[k + 2] << 8));
        a->cmd = (u16)(d[k + 3] | (d[k + 4] << 8));
        if (a->fl & 0x80)
            break;
    }
    free(d);
    vlog("w16: LoadAccelerators %s: %u entries\n", name[0] ? name : "#", accels[i].n);
    return (u16)(HACCEL_BASE + 2 * i);
}

/* TranslateAccelerator(hWnd, hAccel, lpMsg): a matching key goes to the window as WM_COMMAND with 1 in
 * the high word of lParam; the key-up of a matched key is swallowed as well */
u32 u_TranslateAccelerator(u16 ss, u16 ap)
{
    u16 off = AW(0), seg = AW(2), h = AW(4), i = (u16)(h - HACCEL_BASE) >> 1;
    u16 msg = rd16(seg, off + 2), key = rd16(seg, off + 4), mods = user_key_mods(), k;
    struct wnd *w = win_get(AW(6));
    if (h < HACCEL_BASE || (h & 1) || i >= MAXACC || !accels[i].used || !w)
        return 0;
    if (msg != WM_KEYDOWN && msg != WM_KEYUP && msg != WM_SYSKEYDOWN && msg != WM_CHAR)
        return 0;
    for (k = 0; k < accels[i].n; k++) {
        const struct accel *a = &accels[i].e[k];
        int virt = a->fl & 1;
        if (a->key != key || (virt ? msg == WM_CHAR : msg != WM_CHAR))
            continue;
        if (virt && (((a->fl & 4) != 0) != ((mods & MWKMOD_SHIFT) != 0) ||
                     ((a->fl & 8) != 0) != ((mods & MWKMOD_CTRL) != 0) ||
                     ((a->fl & 16) != 0) != ((mods & MWKMOD_ALT) != 0)))
            continue;
        if (msg != WM_KEYUP) {
            vlog("w16: accelerator key %04x -> WM_COMMAND %u\n", key, a->cmd);
            win_send(w, WM_COMMAND, a->cmd, 0x00010000ul);
        }
        return 1;
    }
    return 0;
}

/* WinHelp(hWnd, lpHelpFile, wCommand, dwData): there is no help viewer; HELP_QUIT (2) is silent */
u32 u_WinHelp(u16 ss, u16 ap)
{
    char file[40];
    u16 cmd = AW(4);
    if (cmd == 2)
        return 1;
    if (AW(8))
        fstrncpy_in(file, AW(8), AW(6), sizeof(file));
    else
        strcpy(file, "(none)");
    wlog("w16: WinHelp(\"%s\", command %u): no help viewer is available\n", file, cmd);
    message_box("Help is not available under w16.", "Help", 0x40);     /* MB_ICONASTERISK */
    return 0;
}
