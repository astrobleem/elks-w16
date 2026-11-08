/*
 * api.c - the entry points this layer provides to Win16 programs, and the
 * dispatcher that the per-entry thunks in thunk.S call.
 *
 * Each implemented entry gets an 8-byte stub at w16_stubs + 8 * index; the
 * program's import relocations are bound to those far addresses. argbytes is
 * the Pascal argument size the callee pops on return.
 */
#include <string.h>
#include "w16.h"

const char *const w16_modnames[NMODS] = { "KERNEL", "USER", "GDI" };

#ifdef W16_HOST
#define H(f) 0
#else
#define H(f) f
u32 k_GetVersion(u16, u16), k_WaitEvent(u16, u16), k_GetModuleFileName(u16, u16), k_InitTask(u16, u16);
u32 u_InitApp(u16, u16), u_PostQuitMessage(u16, u16), u_GetClientRect(u16, u16), u_BeginPaint(u16, u16);
u32 u_EndPaint(u16, u16), u_CreateWindow(u16, u16), u_ShowWindow(u16, u16), u_DestroyWindow(u16, u16);
u32 u_RegisterClass(u16, u16), u_DefWindowProc(u16, u16), u_GetMessage(u16, u16), u_TranslateMessage(u16, u16);
u32 u_DispatchMessage(u16, u16), u_UpdateWindow(u16, u16), u_InvalidateRect(u16, u16), u_LoadCursor(u16, u16);
u32 u_LoadIcon(u16, u16), u_wsprintf(u16, u16);
u32 g_SetBkColor(u16, u16), g_SetBkMode(u16, u16), g_SetTextColor(u16, u16), g_LineTo(u16, u16), g_MoveTo(u16, u16);
u32 g_Ellipse(u16, u16), g_Rectangle(u16, u16), g_TextOut(u16, u16), g_SelectObject(u16, u16), g_CreatePen(u16, u16);
u32 g_CreateSolidBrush(u16, u16), g_DeleteObject(u16, u16), g_GetStockObject(u16, u16);
#endif

#define K MOD_KERNEL
#define U MOD_USER
#define G MOD_GDI
const struct api api_table[] = {
    /* KERNEL */
    { K,   3, "GetVersion",        0, 0, H(k_GetVersion) },
    { K,  30, "WaitEvent",         2, 0, H(k_WaitEvent) },
    { K,  49, "GetModuleFileName", 8, 0, H(k_GetModuleFileName) },
    { K,  91, "InitTask",          0, 0, H(k_InitTask) },
    { K, 102, "DOS3Call",          0, API_RAW, 0 },
    { K, 113, "__AHSHIFT",         0, API_CONST, 0, 12 },
    { K, 114, "__AHINCR",          0, API_CONST, 0, 0x1000 },
    { K, 174, "__A000H",           0, API_CONST, 0, 0xA000 },
    { K, 178, "__WINFLAGS",        0, API_CONST, 0, 0x0040 },     /* WF_CPU086: real mode on an 8086/8088 */
    { K, 179, "__D000H",           0, API_CONST, 0, 0xD000 },
    { K, 181, "__B000H",           0, API_CONST, 0, 0xB000 },
    { K, 182, "__B800H",           0, API_CONST, 0, 0xB800 },
    { K, 183, "__0000H",           0, API_CONST, 0, 0x0000 },
    { K, 190, "__E000H",           0, API_CONST, 0, 0xE000 },
    { K, 193, "__0040H",           0, API_CONST, 0, 0x0040 },
    { K, 194, "__F000H",           0, API_CONST, 0, 0xF000 },
    { K, 195, "__C000H",           0, API_CONST, 0, 0xC000 },
    /* USER */
    { U,   5, "InitApp",           2, 0, H(u_InitApp) },
    { U,   6, "PostQuitMessage",   2, 0, H(u_PostQuitMessage) },
    { U,  33, "GetClientRect",     6, 0, H(u_GetClientRect) },
    { U,  39, "BeginPaint",        6, 0, H(u_BeginPaint) },
    { U,  40, "EndPaint",          6, 0, H(u_EndPaint) },
    { U,  41, "CreateWindow",     30, 0, H(u_CreateWindow) },
    { U,  42, "ShowWindow",        4, 0, H(u_ShowWindow) },
    { U,  53, "DestroyWindow",     2, 0, H(u_DestroyWindow) },
    { U,  57, "RegisterClass",     4, 0, H(u_RegisterClass) },
    { U, 107, "DefWindowProc",    10, 0, H(u_DefWindowProc) },
    { U, 108, "GetMessage",       10, 0, H(u_GetMessage) },
    { U, 113, "TranslateMessage",  4, 0, H(u_TranslateMessage) },
    { U, 114, "DispatchMessage",   4, 0, H(u_DispatchMessage) },
    { U, 124, "UpdateWindow",      2, 0, H(u_UpdateWindow) },
    { U, 125, "InvalidateRect",    8, 0, H(u_InvalidateRect) },
    { U, 173, "LoadCursor",        6, 0, H(u_LoadCursor) },
    { U, 174, "LoadIcon",          6, 0, H(u_LoadIcon) },
    { U, 420, "wsprintf",          0, API_CDECL, H(u_wsprintf) },
    /* GDI */
    { G,   1, "SetBkColor",        6, 0, H(g_SetBkColor) },
    { G,   2, "SetBkMode",         4, 0, H(g_SetBkMode) },
    { G,   9, "SetTextColor",      6, 0, H(g_SetTextColor) },
    { G,  19, "LineTo",            6, 0, H(g_LineTo) },
    { G,  20, "MoveTo",            6, 0, H(g_MoveTo) },
    { G,  24, "Ellipse",          10, 0, H(g_Ellipse) },
    { G,  27, "Rectangle",        10, 0, H(g_Rectangle) },
    { G,  33, "TextOut",          12, 0, H(g_TextOut) },
    { G,  45, "SelectObject",      4, 0, H(g_SelectObject) },
    { G,  61, "CreatePen",         8, 0, H(g_CreatePen) },
    { G,  66, "CreateSolidBrush",  4, 0, H(g_CreateSolidBrush) },
    { G,  69, "DeleteObject",      2, 0, H(g_DeleteObject) },
    { G,  87, "GetStockObject",    2, 0, H(g_GetStockObject) },
};
const int api_count = sizeof(api_table) / sizeof(api_table[0]);

/* Names of common Windows 3.0 entry points this layer does NOT implement, for diagnostics only. */
struct known { u8 mod; u16 ord; const char *name; };
static const struct known known_names[] = {
    { K, 1, "FatalExit" }, { K, 4, "LocalInit" }, { K, 5, "LocalAlloc" }, { K, 6, "LocalReAlloc" },
    { K, 7, "LocalFree" }, { K, 8, "LocalLock" }, { K, 9, "LocalUnlock" }, { K, 10, "LocalSize" },
    { K, 15, "GlobalAlloc" }, { K, 16, "GlobalReAlloc" }, { K, 17, "GlobalFree" }, { K, 18, "GlobalLock" },
    { K, 19, "GlobalUnlock" }, { K, 20, "GlobalSize" }, { K, 23, "LockSegment" }, { K, 24, "UnlockSegment" },
    { K, 36, "GetCurrentTask" }, { K, 47, "GetModuleHandle" }, { K, 48, "GetModuleUsage" },
    { K, 50, "GetProcAddress" }, { K, 51, "MakeProcInstance" }, { K, 52, "FreeProcInstance" },
    { K, 60, "FindResource" }, { K, 61, "LoadResource" }, { K, 62, "LockResource" }, { K, 74, "OpenFile" },
    { K, 81, "_lclose" }, { K, 82, "_lread" }, { K, 83, "_lcreat" }, { K, 84, "_llseek" }, { K, 85, "_lopen" },
    { K, 86, "_lwrite" }, { K, 88, "lstrcpy" }, { K, 89, "lstrcat" }, { K, 90, "lstrlen" },
    { K, 95, "LoadLibrary" }, { K, 96, "FreeLibrary" }, { K, 131, "GetDOSEnvironment" }, { K, 137, "FatalAppExit" },
    { U, 1, "MessageBox" }, { U, 10, "SetTimer" }, { U, 12, "KillTimer" }, { U, 13, "GetTickCount" },
    { U, 15, "GetCurrentTime" }, { U, 17, "GetCursorPos" }, { U, 18, "SetCapture" }, { U, 19, "ReleaseCapture" },
    { U, 22, "SetFocus" }, { U, 23, "GetFocus" }, { U, 32, "GetWindowRect" }, { U, 34, "EnableWindow" },
    { U, 36, "GetWindowText" }, { U, 37, "SetWindowText" }, { U, 46, "GetParent" }, { U, 56, "MoveWindow" },
    { U, 66, "GetDC" }, { U, 68, "ReleaseDC" }, { U, 81, "FillRect" }, { U, 85, "DrawText" },
    { U, 87, "DialogBox" }, { U, 88, "EndDialog" }, { U, 89, "CreateDialog" }, { U, 104, "MessageBeep" },
    { U, 106, "GetKeyState" }, { U, 109, "PeekMessage" }, { U, 110, "PostMessage" }, { U, 111, "SendMessage" },
    { U, 150, "LoadMenu" }, { U, 175, "LoadBitmap" }, { U, 176, "LoadString" }, { U, 177, "LoadAccelerators" },
    { U, 178, "TranslateAccelerator" }, { U, 179, "GetSystemMetrics" }, { U, 421, "wvsprintf" },
    { G, 3, "SetMapMode" }, { G, 4, "SetROP2" }, { G, 11, "SetWindowOrg" }, { G, 13, "SetViewportOrg" },
    { G, 28, "RoundRect" }, { G, 29, "PatBlt" }, { G, 30, "SaveDC" }, { G, 31, "SetPixel" }, { G, 34, "BitBlt" },
    { G, 35, "StretchBlt" }, { G, 36, "Polygon" }, { G, 37, "Polyline" }, { G, 39, "RestoreDC" },
    { G, 48, "CreateBitmap" }, { G, 50, "CreateBrushIndirect" }, { G, 52, "CreateCompatibleDC" },
    { G, 53, "CreateDC" }, { G, 56, "CreateFont" }, { G, 57, "CreateFontIndirect" }, { G, 58, "CreateHatchBrush" },
    { G, 68, "DeleteDC" }, { G, 80, "GetDeviceCaps" }, { G, 91, "GetTextExtent" }, { G, 93, "GetTextMetrics" },
};

int api_find_ord(int mod, u16 ord)
{
    int i;
    for (i = 0; i < api_count; i++)
        if (api_table[i].mod == mod && api_table[i].ord == ord)
            return i;
    return -1;
}

static int ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a >= 'a' && *a <= 'z' ? *a - 32 : *a, y = *b >= 'a' && *b <= 'z' ? *b - 32 : *b;
        if (x != y)
            return 0;
    }
    return *a == *b;
}

int api_find_name(int mod, const char *name)
{
    int i;
    for (i = 0; i < api_count; i++)
        if (api_table[i].mod == mod && ieq(api_table[i].name, name))
            return i;
    return -1;
}

const char *api_known_name(int mod, u16 ord)
{
    unsigned i;
    for (i = 0; i < sizeof(known_names) / sizeof(known_names[0]); i++)
        if (known_names[i].mod == mod && known_names[i].ord == ord)
            return known_names[i].name;
    return 0;
}

#ifdef W16_HOST
/* host tests: stubs live in a pretend segment so bindings can be checked */
int api_target(int idx, u16 *seg, u16 *off)
{
    if (idx < 0 || idx >= api_count || (api_table[idx].flags & API_CONST))
        return -1;
    *seg = 0xE000;
    *off = (u16)(8 * idx);
    return 0;
}
#else
int api_target(int idx, u16 *seg, u16 *off)
{
    if (idx < 0 || idx >= api_count || (api_table[idx].flags & API_CONST))
        return -1;
    *seg = w16_get_cs();
    *off = (api_table[idx].flags & API_RAW) ? (u16)(unsigned)w16_dos3call : (u16)((unsigned)w16_stubs + 8 * idx);
    return 0;
}

/* Called from thunk.S on the loader stack. The app frame at w16_app_ss:w16_app_sp is
 * DI SI DS BP IP CS, then the arguments. Pops Pascal arguments by moving the frame up. */
u32 w16_api_dispatch(u16 idx)
{
    const struct api *a = &api_table[idx];
    u16 ss = w16_app_ss, sp = w16_app_sp, n, k;
    u32 r;

    w16_ret_bx = 0;
    w16_ret_cx = 0;
    w16_ret_es = ss;
    if (opt_trace)
        wlog("w16: call %s.%s\n", w16_modnames[a->mod], a->name);
    r = a->fn(ss, (u16)(sp + 12));
    n = a->argbytes;
    if (n) {
        sp = w16_app_sp;                       /* restored by any nested calls */
        for (k = 12; k; ) {
            k -= 2;
            wr16(ss, (u16)(sp + n + k), rd16(ss, (u16)(sp + k)));
        }
        w16_app_sp = sp + n;
    }
    return r;
}
#endif
