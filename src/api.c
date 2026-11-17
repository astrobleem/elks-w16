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

int opt_missing; /* explicit diagnostic mode; never returns success for missing APIs */

const char *const w16_modnames[NMODS] = { "KERNEL", "USER", "GDI", "PBRUSH" };

#ifdef W16_HOST
#define H(f) 0
#else
#define H(f) f
u32 u_lstrcmpi(u16,u16),u_lstrcmp(u16,u16),u_AnsiNext(u16,u16),u_AnsiPrev(u16,u16);
u32 u_InvertRect(u16,u16);
u32 u_GetMenuString(u16,u16),u_ModifyMenu(u16,u16),u_GetMenuState(u16,u16);
u32 g_EnumFonts(u16,u16);
u32 g_CreateIC(u16,u16),g_EnumObjects(u16,u16);
u32 k_GlobalAlloc(u16,u16), k_GlobalFree(u16,u16), k_GlobalReAlloc(u16,u16), k_GlobalSize(u16,u16);
u32 k_GetTempDrive(u16,u16),k_GetTempFileName(u16,u16);
u32 k_OpenFile(u16,u16),k_lclose(u16,u16),k_lread(u16,u16),k_lcreat(u16,u16),k_llseek(u16,u16),k_lopen(u16,u16),k_lwrite(u16,u16);
u32 k_LocalInit(u16,u16),k_LocalReAlloc(u16,u16);
u32 k_GetFreeSpace(u16,u16);
u32 k_GlobalNotify(u16,u16), k_GlobalLRUNewest(u16,u16);
u32 k_GetVersion(u16, u16), k_WaitEvent(u16, u16), k_GetModuleFileName(u16, u16), k_InitTask(u16, u16);
u32 u_OpenClipboard(u16,u16),u_CloseClipboard(u16,u16),u_EmptyClipboard(u16,u16);
u32 u_SetClipboardData(u16,u16),u_GetClipboardData(u16,u16),u_GetPriorityClipboardFormat(u16,u16);
u32 u_GetDlgItemText(u16,u16),u_SendDlgItemMessage(u16,u16);
u32 u_DlgDirList(u16,u16),u_DlgDirSelect(u16,u16);
u32 u_GetDesktopHwnd(u16,u16);
u32 u_InitApp(u16, u16), u_PostQuitMessage(u16, u16), u_GetClientRect(u16, u16), u_BeginPaint(u16, u16);
u32 u_EndPaint(u16, u16), u_CreateWindow(u16, u16), u_ShowWindow(u16, u16), u_DestroyWindow(u16, u16);
u32 u_RegisterClass(u16, u16), u_DefWindowProc(u16, u16), u_GetMessage(u16, u16), u_TranslateMessage(u16, u16);
u32 u_DispatchMessage(u16, u16), u_UpdateWindow(u16, u16), u_InvalidateRect(u16, u16), u_LoadCursor(u16, u16);
u32 u_LoadIcon(u16, u16), u_wsprintf(u16, u16);
u32 u_LoadBitmap(u16,u16), g_CreateCompatibleDC(u16,u16), g_BitBlt(u16,u16);
u32 u_SetRectEmpty(u16,u16),u_IsRectEmpty(u16,u16),u_UnionRect(u16,u16);
u32 u_CopyRect(u16,u16),u_PtInRect(u16,u16),u_IntersectRect(u16,u16);
u32 g_GetBitmapBits(u16,u16),g_SetBitmapBits(u16,u16);
u32 g_GetObject(u16,u16),g_CreateCompatibleBitmap(u16,u16),g_SetBrushOrg(u16,u16),g_SetPixel(u16,u16),g_GetPixel(u16,u16),g_GetDCOrg(u16,u16),g_PtVisible(u16,u16),g_LineDDA(u16,u16);
u32 u_RegisterWindowMessage(u16,u16),u_DrawText(u16,u16);
u32 g_GetBkColor(u16,u16);
u32 g_SetBkColor(u16, u16), g_SetBkMode(u16, u16), g_SetTextColor(u16, u16), g_LineTo(u16, u16), g_MoveTo(u16, u16);
u32 g_Ellipse(u16, u16), g_Rectangle(u16, u16), g_TextOut(u16, u16), g_SelectObject(u16, u16), g_CreatePen(u16, u16);
u32 g_CreateSolidBrush(u16, u16), g_DeleteObject(u16, u16), g_GetStockObject(u16, u16);
u32 k_GetWinFlags(u16, u16), k_LockSegment(u16, u16), k_UnlockSegment(u16, u16), k_MakeProcInstance(u16, u16);
u32 k_FreeProcInstance(u16, u16), u_DialogBox(u16, u16), u_EndDialog(u16, u16), u_SetDlgItemText(u16, u16);
u32 u_GetDlgItem(u16, u16), u_GetSystemMetrics(u16, u16), u_EnableWindow(u16, u16);
u32 u_CheckRadioButton(u16,u16), u_CheckDlgButton(u16,u16), u_IsDlgButtonChecked(u16,u16);
u32 k_LocalAlloc(u16, u16), k_LocalFree(u16, u16), k_LocalLock(u16, u16), k_LocalUnlock(u16, u16), k_LocalSize(u16, u16);
u32 k_GlobalLock(u16, u16), k_GlobalUnlock(u16, u16), k_GetInstanceData(u16, u16), k_GetProfileInt(u16, u16);
u32 k_GetPrivateProfileInt(u16,u16), k_GetPrivateProfileString(u16,u16);
u32 k_GetProfileString(u16, u16), k_WriteProfileString(u16, u16), k_FindResource(u16, u16), k_LoadResource(u16, u16);
u32 k_FreeResource(u16, u16), k_lstrcpy(u16, u16), k_lstrcat(u16, u16), k_lstrlen(u16, u16);
u32 u_MessageBox(u16, u16), u_SetTimer(u16, u16), u_KillTimer(u16, u16), u_GetDC(u16, u16), u_ReleaseDC(u16, u16);
u32 u_SetRect(u16, u16), u_OffsetRect(u16, u16), u_InflateRect(u16, u16), u_FillRect(u16, u16);
u32 u_CheckMenuItem(u16, u16), u_EnableMenuItem(u16, u16), u_GetMenu(u16, u16), u_GetSubMenu(u16, u16);
u32 u_DrawMenuBar(u16, u16), u_LoadString(u16, u16), u_GetSysColor(u16, u16);
u32 g_SetStretchBltMode(u16,u16),g_StretchBlt(u16,u16);
u32 g_SetDIBits(u16,u16),g_GetDIBits(u16,u16);
u32 g_DPtoLP(u16,u16),g_LPtoDP(u16,u16);
u32 g_SetMapMode(u16,u16),g_SetWindowOrg(u16,u16),g_SetViewportOrg(u16,u16),g_SetWindowExt(u16,u16),g_SetViewportExt(u16,u16),g_GetWindowOrg(u16,u16);
u32 g_CreatePalette(u16,u16),g_GetPaletteEntries(u16,u16),g_GetNearestPaletteIndex(u16,u16);
u32 u_SelectPalette(u16,u16),u_RealizePalette(u16,u16);
u32 g_SaveDC(u16,u16),g_RestoreDC(u16,u16),g_GetROP2(u16,u16);
u32 g_SetROP2(u16, u16), g_CreateDC(u16, u16), g_CreateFontIndirect(u16, u16), g_DeleteDC(u16, u16);
u32 g_GetDeviceCaps(u16, u16), g_GetTextMetrics(u16, u16), g_AddFontResource(u16, u16), g_RemoveFontResource(u16, u16);
u32 g_GetNearestColor(u16, u16), g_SetTextAlign(u16, u16), g_ExtTextOut(u16, u16);
u32 k_GetModuleUsage(u16, u16);
u32 u_PeekMessage(u16, u16), u_PostMessage(u16, u16), u_SetCapture(u16, u16), u_ReleaseCapture(u16, u16);
u32 u_ScreenToClient(u16,u16), u_GetWindowRect(u16,u16);
u32 u_SetClassWord(u16,u16);
u32 u_GetWindowLong(u16,u16),u_SetWindowLong(u16,u16);
u32 u_IsWindow(u16,u16),u_IsWindowVisible(u16,u16);
u32 u_GetActiveWindow(u16,u16),u_SendMessage(u16,u16);
u32 u_MoveWindow(u16,u16),u_SetWindowPos(u16,u16);
u32 u_ScrollWindow(u16,u16);
u32 u_SetScrollRange(u16,u16),u_SetScrollPos(u16,u16),u_GetScrollPos(u16,u16),u_GetScrollRange(u16,u16);
u32 u_SetFocus(u16, u16), u_IsIconic(u16, u16), u_SetWindowText(u16, u16), u_ClientToScreen(u16, u16);
u32 u_ClipCursor(u16,u16),u_GetCursorPos(u16,u16);
u32 u_GetKeyState(u16, u16), u_SetCursor(u16, u16), u_ShowCursor(u16, u16), u_SetCursorPos(u16, u16);
u32 u_LoadAccelerators(u16, u16), u_TranslateAccelerator(u16, u16), u_WinHelp(u16, u16);
u32 g_PatBlt(u16, u16), g_CreateBitmap(u16, u16), g_CreatePatternBrush(u16, u16), g_GetTextExtent(u16, u16);
u32 g_UnrealizeObject(u16, u16);
#endif

#define K MOD_KERNEL
#define U MOD_USER
#define G MOD_GDI
#define P MOD_PBRUSH
const struct api api_table[] = {
    { U,175,"LoadBitmap",6,0,H(u_LoadBitmap) },
    { G,52,"CreateCompatibleDC",2,0,H(g_CreateCompatibleDC) },
    { G,34,"BitBlt",20,0,H(g_BitBlt) },
    /* Optional diagnostic traps for the remaining Solitaire imports.
     * Strict default lookup rejects these. -u binds a terminating thunk. */
    { G,31,"SetPixel",10,0,H(g_SetPixel) },
    { G,35,"StretchBlt",24,0,H(g_StretchBlt) },
    { G,51,"CreateCompatibleBitmap",6,0,H(g_CreateCompatibleBitmap) },
    { G,79,"GetDCOrg",2,0,H(g_GetDCOrg) },
    { G,82,"GetObject",8,0,H(g_GetObject) },
    { G,83,"GetPixel",6,0,H(g_GetPixel) },
    { G,100,"LineDDA",16,0,H(g_LineDDA) },
    { G,103,"PtVisible",6,0,H(g_PtVisible) },
    { G,148,"SetBrushOrg",6,0,H(g_SetBrushOrg) },
    { K,6,"LocalReAlloc",6,0,H(k_LocalReAlloc) },
    { K,15,"GlobalAlloc",6,0,H(k_GlobalAlloc) },
    { K,16,"GlobalReAlloc",8,0,H(k_GlobalReAlloc) },
    { K,17,"GlobalFree",2,0,H(k_GlobalFree) },
    { U,34,"EnableWindow",4,0,H(u_EnableWindow) },
    { U,56,"MoveWindow",12,0,H(u_MoveWindow) },
    { U,73,"SetRectEmpty",4,0,H(u_SetRectEmpty) },
    { U,75,"IsRectEmpty",4,0,H(u_IsRectEmpty) },
    { U,80,"UnionRect",12,0,H(u_UnionRect) },
    { U,74,"CopyRect",8,0,H(u_CopyRect) },
    { U,76,"PtInRect",8,0,H(u_PtInRect) },
    { U,79,"IntersectRect",12,0,H(u_IntersectRect) },
    { U,82,"InvertRect",6,0,H(u_InvertRect) },
    { U,83,"FrameRect",0,API_MISSING,0 },
    { U,85,"DrawText",14,0,H(u_DrawText) },
    { U,96,"CheckRadioButton",8,0,H(u_CheckRadioButton) },
    { U,97,"CheckDlgButton",6,0,H(u_CheckDlgButton) },
    { U,98,"IsDlgButtonChecked",4,0,H(u_IsDlgButtonChecked) },
    { U,118,"RegisterWindowMessage",4,0,H(u_RegisterWindowMessage) },

    { U,65,"GetScrollRange",12,0,H(u_GetScrollRange) },
    /* KERNEL */
    { K, 83, "_lcreat", 6, 0, H(k_lcreat) },
    { K,20,"GlobalSize",2,0,H(k_GlobalSize) },
    { K, 4, "LocalInit", 6, 0, H(k_LocalInit) },
    { K, 154, "GlobalNotify", 4, 0, H(k_GlobalNotify) },
    { K,   3, "GetVersion",        0, 0, H(k_GetVersion) },
    { K,   5, "LocalAlloc",        4, 0, H(k_LocalAlloc) },
    { K,   7, "LocalFree",         2, 0, H(k_LocalFree) },
    { K,   8, "LocalLock",         2, 0, H(k_LocalLock) },
    { K,   9, "LocalUnlock",       2, 0, H(k_LocalUnlock) },
    { K,  10, "LocalSize",         2, 0, H(k_LocalSize) },
    { K,  18, "GlobalLock",        2, 0, H(k_GlobalLock) },
    { K,  19, "GlobalUnlock",      2, 0, H(k_GlobalUnlock) },
    { K,  54, "GetInstanceData",   6, 0, H(k_GetInstanceData) },
    { K, 127, "GetPrivateProfileInt", 14, 0, H(k_GetPrivateProfileInt) },
    { K, 128, "GetPrivateProfileString", 22, 0, H(k_GetPrivateProfileString) },
    { K,  57, "GetProfileInt",    10, 0, H(k_GetProfileInt) },
    { K,  58, "GetProfileString", 18, 0, H(k_GetProfileString) },
    { K,  59, "WriteProfileString", 12, 0, H(k_WriteProfileString) },
    { K,  60, "FindResource",     10, 0, H(k_FindResource) },
    { K,  61, "LoadResource",      4, 0, H(k_LoadResource) },
    { K,  62, "LockResource",      2, 0, H(k_GlobalLock) },
    { K,  63, "FreeResource",      2, 0, H(k_FreeResource) },
    { K,  88, "lstrcpy",           8, 0, H(k_lstrcpy) },
    { K,  89, "lstrcat",           8, 0, H(k_lstrcat) },
    { K,  90, "lstrlen",           4, 0, H(k_lstrlen) },
    { K,  30, "WaitEvent",         2, 0, H(k_WaitEvent) },
    { K,  48, "GetModuleUsage",    2, 0, H(k_GetModuleUsage) },
    { K,  49, "GetModuleFileName", 8, 0, H(k_GetModuleFileName) },
    { K,  23, "LockSegment",       2, 0, H(k_LockSegment) },
    { K,  24, "UnlockSegment",     2, 0, H(k_UnlockSegment) },
    { K,  51, "MakeProcInstance",  6, 0, H(k_MakeProcInstance) },
    { K,  52, "FreeProcInstance",  4, 0, H(k_FreeProcInstance) },
    { K,  91, "InitTask",          0, 0, H(k_InitTask) },
    { K, 102, "DOS3Call",          0, API_RAW, 0 },
    { K, 132, "GetWinFlags",       0, 0, H(k_GetWinFlags) },
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
    { U,   1, "MessageBox",       12, 0, H(u_MessageBox) },
    { U,   5, "InitApp",           2, 0, H(u_InitApp) },
    { U,  10, "SetTimer",         10, 0, H(u_SetTimer) },
    { U,  12, "KillTimer",         4, 0, H(u_KillTimer) },
    { U,  18, "SetCapture",        2, 0, H(u_SetCapture) },
    { U,  19, "ReleaseCapture",    0, 0, H(u_ReleaseCapture) },
    { U,  22, "SetFocus",          2, 0, H(u_SetFocus) },
    { U,  29, "ScreenToClient",    6, 0, H(u_ScreenToClient) },
    { U,  32, "GetWindowRect",     6, 0, H(u_GetWindowRect) },
    { U,  28, "ClientToScreen",    6, 0, H(u_ClientToScreen) },
    { U,  31, "IsIconic",          2, 0, H(u_IsIconic) },
    { U,   6, "PostQuitMessage",   2, 0, H(u_PostQuitMessage) },
    { U,  33, "GetClientRect",     6, 0, H(u_GetClientRect) },
    { U,  39, "BeginPaint",        6, 0, H(u_BeginPaint) },
    { U,  40, "EndPaint",          6, 0, H(u_EndPaint) },
    { U,  41, "CreateWindow",     30, 0, H(u_CreateWindow) },
    { U,  42, "ShowWindow",        4, 0, H(u_ShowWindow) },
    { U,  53, "DestroyWindow",     2, 0, H(u_DestroyWindow) },
    { U,  37, "SetWindowText",     6, 0, H(u_SetWindowText) },
    { U,  57, "RegisterClass",     4, 0, H(u_RegisterClass) },
    { U,  69, "SetCursor",         2, 0, H(u_SetCursor) },
    { U, 16, "ClipCursor", 4, 0, H(u_ClipCursor) },
    { U, 17, "GetCursorPos", 4, 0, H(u_GetCursorPos) },
    { U,  70, "SetCursorPos",      4, 0, H(u_SetCursorPos) },
    { U,  71, "ShowCursor",        2, 0, H(u_ShowCursor) },
    { U,  66, "GetDC",             2, 0, H(u_GetDC) },
    { U,  68, "ReleaseDC",         4, 0, H(u_ReleaseDC) },
    { U,  72, "SetRect",          12, 0, H(u_SetRect) },
    { U,  77, "OffsetRect",        8, 0, H(u_OffsetRect) },
    { U,  78, "InflateRect",       8, 0, H(u_InflateRect) },
    { U,  81, "FillRect",          8, 0, H(u_FillRect) },
    { U,  87, "DialogBox",        12, 0, H(u_DialogBox) },
    { U,  88, "EndDialog",         4, 0, H(u_EndDialog) },
    { U, 101, "SendDlgItemMessage", 12, 0, H(u_SendDlgItemMessage) },
    { U,  91, "GetDlgItem",        4, 0, H(u_GetDlgItem) },
    { U,  92, "SetDlgItemText",    8, 0, H(u_SetDlgItemText) },
    { U, 107, "DefWindowProc",    10, 0, H(u_DefWindowProc) },
    { U, 106, "GetKeyState",       2, 0, H(u_GetKeyState) },
    { U, 108, "GetMessage",       10, 0, H(u_GetMessage) },
    { U, 109, "PeekMessage",      12, 0, H(u_PeekMessage) },
    { U, 110, "PostMessage",      10, 0, H(u_PostMessage) },
    { U, 113, "TranslateMessage",  4, 0, H(u_TranslateMessage) },
    { U, 114, "DispatchMessage",   4, 0, H(u_DispatchMessage) },
    { U, 124, "UpdateWindow",      2, 0, H(u_UpdateWindow) },
    { U, 125, "InvalidateRect",    8, 0, H(u_InvalidateRect) },
    { U, 154, "CheckMenuItem",     6, 0, H(u_CheckMenuItem) },
    { U, 155, "EnableMenuItem",    6, 0, H(u_EnableMenuItem) },
    { U, 157, "GetMenu",           2, 0, H(u_GetMenu) },
    { U, 159, "GetSubMenu",        4, 0, H(u_GetSubMenu) },
    { U, 160, "DrawMenuBar",       2, 0, H(u_DrawMenuBar) },
    { U, 171, "WinHelp",          12, 0, H(u_WinHelp) },
    { U, 173, "LoadCursor",        6, 0, H(u_LoadCursor) },
    { U, 174, "LoadIcon",          6, 0, H(u_LoadIcon) },
    { U, 176, "LoadString",       10, 0, H(u_LoadString) },
    { U, 177, "LoadAccelerators",  6, 0, H(u_LoadAccelerators) },
    { U, 178, "TranslateAccelerator", 8, 0, H(u_TranslateAccelerator) },
    { U, 179, "GetSystemMetrics",  2, 0, H(u_GetSystemMetrics) },
    { U, 180, "GetSysColor",       2, 0, H(u_GetSysColor) },
    { U, 420, "wsprintf",          0, API_CDECL, H(u_wsprintf) },
    /* GDI */
    { G,   1, "SetBkColor",        6, 0, H(g_SetBkColor) },
    { G,   2, "SetBkMode",         4, 0, H(g_SetBkMode) },
    { G,   4, "SetROP2",           4, 0, H(g_SetROP2) },
    { G,   9, "SetTextColor",      6, 0, H(g_SetTextColor) },
    { G,  19, "LineTo",            6, 0, H(g_LineTo) },
    { G,  20, "MoveTo",            6, 0, H(g_MoveTo) },
    { G,  24, "Ellipse",          10, 0, H(g_Ellipse) },
    { G,  27, "Rectangle",        10, 0, H(g_Rectangle) },
    { G,  29, "PatBlt",           14, 0, H(g_PatBlt) },
    { G,  33, "TextOut",          12, 0, H(g_TextOut) },
    { G,  45, "SelectObject",      4, 0, H(g_SelectObject) },
    { G,  48, "CreateBitmap",     12, 0, H(g_CreateBitmap) },
    { G,  53, "CreateDC",         16, 0, H(g_CreateDC) },
    { G,  57, "CreateFontIndirect", 4, 0, H(g_CreateFontIndirect) },
    { G,  60, "CreatePatternBrush", 2, 0, H(g_CreatePatternBrush) },
    { G,  61, "CreatePen",         8, 0, H(g_CreatePen) },
    { G,  66, "CreateSolidBrush",  4, 0, H(g_CreateSolidBrush) },
    { G,  68, "DeleteDC",          2, 0, H(g_DeleteDC) },
    { G,  69, "DeleteObject",      2, 0, H(g_DeleteObject) },
    { G,  80, "GetDeviceCaps",     4, 0, H(g_GetDeviceCaps) },
    { G,  87, "GetStockObject",    2, 0, H(g_GetStockObject) },
    { G,  91, "GetTextExtent",     8, 0, H(g_GetTextExtent) },
    { G,  93, "GetTextMetrics",    6, 0, H(g_GetTextMetrics) },
    { G, 119, "AddFontResource",   4, 0, H(g_AddFontResource) },
    { G, 136, "RemoveFontResource", 4, 0, H(g_RemoveFontResource) },
    { G, 150, "UnrealizeObject",   2, 0, H(g_UnrealizeObject) },
    { G, 154, "GetNearestColor",   6, 0, H(g_GetNearestColor) },
    { G, 346, "SetTextAlign",      4, 0,