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
#include <stdlib.h>
#include "api-slots.h"

int opt_missing; /* explicit diagnostic mode; never returns success for missing APIs */

const char *const w16_modnames[NMODS] = { "KERNEL", "USER", "GDI", "PBRUSH", "SOUND", "SHELL", "KEYBOARD" };

#ifdef W16_HOST
#define H(f) 0
#else
#define H(f) f
u32 k_AnsiToOem(u16,u16),k_OemToAnsi(u16,u16);
u32 u_GetWindowText(u16,u16),u_GetWindowTextLength(u16,u16);
u32 u_CreateCaret(u16,u16),u_DestroyCaret(u16,u16),u_SetCaretPos(u16,u16),u_HideCaret(u16,u16),u_ShowCaret(u16,u16);
u32 u_AnsiLower(u16,u16),u_AnsiUpper(u16,u16),u_AnsiUpperBuff(u16,u16),u_IsCharAlpha(u16,u16),u_IsCharAlphaNumeric(u16,u16);
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
u32 u_SetDlgItemInt(u16,u16), u_GetDlgItemInt(u16,u16), sh_ShellAbout(u16,u16);
u32 g_CreateDIBitmap(u16,u16),g_SetDIBitsToDevice(u16,u16);
u32 u_FindWindow(u16,u16),u_BringWindowToTop(u16,u16);
u32 s_OpenSound(u16,u16), s_CloseSound(u16,u16), s_Unavailable(u16,u16);
u32 k_FatalExit(u16,u16), k_FatalAppExit(u16,u16);
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
u32 u_DrawIcon(u16,u16),u_CopyIcon(u16,u16),u_DestroyIcon(u16,u16);
u32 u_GetIconId(u16,u16),u_LoadIconHandler(u16,u16);
u32 u_CreateCursorIconIndirect(u16,u16),u_DumpIcon(u16,u16),u_DestroyCursor(u16,u16);
u32 u_LoadIcon(u16, u16), u_wsprintf(u16, u16);
u32 u_LoadBitmap(u16,u16), g_CreateCompatibleDC(u16,u16), g_BitBlt(u16,u16);
u32 u_SetRectEmpty(u16,u16),u_IsRectEmpty(u16,u16),u_UnionRect(u16,u16);
u32 u_CopyRect(u16,u16),u_PtInRect(u16,u16),u_IntersectRect(u16,u16);
u32 g_GetBitmapBits(u16,u16),g_SetBitmapBits(u16,u16);
u32 g_SetBitmapDimension(u16,u16);
u32 g_GetObject(u16,u16),g_CreateCompatibleBitmap(u16,u16),g_SetBrushOrg(u16,u16),g_SetPixel(u16,u16),g_GetPixel(u16,u16),g_GetDCOrg(u16,u16),g_PtVisible(u16,u16),g_LineDDA(u16,u16);
u32 u_RegisterWindowMessage(u16,u16),u_DrawText(u16,u16);
u32 g_GetBkColor(u16,u16);
u32 g_SetBkColor(u16, u16), g_SetBkMode(u16, u16), g_SetTextColor(u16, u16), g_LineTo(u16, u16), g_MoveTo(u16, u16);
u32 g_SetPolyFillMode(u16, u16), g_ExtFloodFill(u16, u16), g_Polyline(u16, u16), g_Polygon(u16, u16), g_RoundRect(u16, u16), g_Ellipse(u16, u16), g_Rectangle(u16, u16), g_TextOut(u16, u16), g_SelectObject(u16, u16), g_CreatePen(u16, u16);
u32 g_CreateSolidBrush(u16, u16), g_DeleteObject(u16, u16), g_GetStockObject(u16, u16);
u32 k_GetWinFlags(u16, u16), k_LockSegment(u16, u16), k_UnlockSegment(u16, u16), k_MakeProcInstance(u16, u16);
u32 k_FreeProcInstance(u16, u16), u_DialogBox(u16, u16), u_EndDialog(u16, u16), u_SetDlgItemText(u16, u16);
u32 u_GetDlgItem(u16, u16), u_GetSystemMetrics(u16, u16), u_EnableWindow(u16, u16);
u32 u_CheckRadioButton(u16,u16), u_CheckDlgButton(u16,u16), u_IsDlgButtonChecked(u16,u16);
u32 k_LocalAlloc(u16, u16), k_LocalFree(u16, u16), k_LocalLock(u16, u16), k_LocalUnlock(u16, u16), k_LocalSize(u16, u16);
u32 k_DirectResAlloc(u16,u16),k_GetHeapSpaces(u16,u16);
u32 k_GlobalLock(u16, u16), k_GlobalUnlock(u16, u16), k_GetInstanceData(u16, u16), k_GetProfileInt(u16, u16);
u32 k_WritePrivateProfileString(u16,u16);
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
u32 k_GetModuleHandle(u16,u16),k_GetNumTasks(u16,u16),k_GetDriveType(u16,u16);
u32 k_GetCurPID(u16,u16);
u32 k_GetWindowsDirectory(u16,u16),u_ExitWindows(u16,u16),k_WinExec(u16,u16);
u32 u_GlobalAddAtom(u16,u16),u_GlobalDeleteAtom(u16,u16),u_GlobalFindAtom(u16,u16);
u32 u_PeekMessage(u16, u16), u_PostMessage(u16, u16), u_SetCapture(u16, u16), u_ReleaseCapture(u16, u16);
u32 u_ScreenToClient(u16,u16), u_GetWindowRect(u16,u16);
u32 u_SetClassWord(u16,u16);
u32 u_DefMDIChildProc(u16,u16),u_TranslateMDISysAccel(u16,u16);
u32 u_GetParent(u16,u16),u_GetWindow(u16,u16),u_SetActiveWindow(u16,u16);
u32 u_GetWindowWord(u16,u16),u_SetWindowWord(u16,u16);
u32 u_GetWindowLong(u16,u16),u_SetWindowLong(u16,u16);
u32 u_IsWindow(u16,u16),u_IsWindowVisible(u16,u16);
u32 u_GetCurrentTime(u16,u16),u_GetLastActivePopup(u16,u16),u_LoadMenu(u16,u16),u_SetMenu(u16,u16);
u32 u_GetActiveWindow(u16,u16),u_SendMessage(u16,u16);
u32 u_DragObject(u16,u16),u_MessageBeep(u16,u16),u_SetSysModalWindow(u16,u16),u_GetSysModalWindow(u16,u16);
u32 u_DefFrameProc(u16,u16),u_GetInternalWindowPos(u16,u16),u_SetInternalWindowPos(u16,u16),u_ArrangeIconicWindows(u16,u16);
u32 u_ShowScrollBar(u16,u16),u_CalcChildScroll(u16,u16),u_ScrollChildren(u16,u16);
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
#define S MOD_SOUND
#define L MOD_SHELL
#define B MOD_KEYBOARD
const struct api W16_RO_FAR api_table[] = {
    { B,5,W16_RO_STRING("AnsiToOem"),8,0,H(k_AnsiToOem) },
    { B,6,W16_RO_STRING("OemToAnsi"),8,0,H(k_OemToAnsi) },
    { U,36,W16_RO_STRING("GetWindowText"),8,0,H(u_GetWindowText) },
    { U,38,W16_RO_STRING("GetWindowTextLength"),2,0,H(u_GetWindowTextLength) },
    { U,15,W16_RO_STRING("GetCurrentTime"),0,0,H(u_GetCurrentTime) },
    { U,150,W16_RO_STRING("LoadMenu"),6,0,H(u_LoadMenu) },
    { U,158,W16_RO_STRING("SetMenu"),4,0,H(u_SetMenu) },
    { U,286,W16_RO_STRING("GetDesktopWindow"),0,0,H(u_GetDesktopHwnd) },
    { U,287,W16_RO_STRING("GetLastActivePopup"),2,0,H(u_GetLastActivePopup) },
    { U,175,W16_RO_STRING("LoadBitmap"),6,0,H(u_LoadBitmap) },
    { G,52,W16_RO_STRING("CreateCompatibleDC"),2,0,H(g_CreateCompatibleDC) },
    { G,34,W16_RO_STRING("BitBlt"),20,0,H(g_BitBlt) },
    /* Optional diagnostic traps for the remaining Solitaire imports.
     * Strict default lookup rejects these. -u binds a terminating thunk. */
    { G,31,W16_RO_STRING("SetPixel"),10,0,H(g_SetPixel) },
    { G,35,W16_RO_STRING("StretchBlt"),24,0,H(g_StretchBlt) },
    { G,51,W16_RO_STRING("CreateCompatibleBitmap"),6,0,H(g_CreateCompatibleBitmap) },
    { G,79,W16_RO_STRING("GetDCOrg"),2,0,H(g_GetDCOrg) },
    { G,82,W16_RO_STRING("GetObject"),8,0,H(g_GetObject) },
    { G,83,W16_RO_STRING("GetPixel"),6,0,H(g_GetPixel) },
    { G,100,W16_RO_STRING("LineDDA"),16,0,H(g_LineDDA) },
    { G,103,W16_RO_STRING("PtVisible"),6,0,H(g_PtVisible) },
    { G,148,W16_RO_STRING("SetBrushOrg"),6,0,H(g_SetBrushOrg) },
    { K,6,W16_RO_STRING("LocalReAlloc"),6,0,H(k_LocalReAlloc) },
    { K,15,W16_RO_STRING("GlobalAlloc"),6,0,H(k_GlobalAlloc) },
    { K,16,W16_RO_STRING("GlobalReAlloc"),8,0,H(k_GlobalReAlloc) },
    { K,17,W16_RO_STRING("GlobalFree"),2,0,H(k_GlobalFree) },
    { U,34,W16_RO_STRING("EnableWindow"),4,0,H(u_EnableWindow) },
    { U,56,W16_RO_STRING("MoveWindow"),12,0,H(u_MoveWindow) },
    { U,73,W16_RO_STRING("SetRectEmpty"),4,0,H(u_SetRectEmpty) },
    { U,75,W16_RO_STRING("IsRectEmpty"),4,0,H(u_IsRectEmpty) },
    { U,80,W16_RO_STRING("UnionRect"),12,0,H(u_UnionRect) },
    { U,74,W16_RO_STRING("CopyRect"),8,0,H(u_CopyRect) },
    { U,76,W16_RO_STRING("PtInRect"),8,0,H(u_PtInRect) },
    { U,79,W16_RO_STRING("IntersectRect"),12,0,H(u_IntersectRect) },
    { U,82,W16_RO_STRING("InvertRect"),6,0,H(u_InvertRect) },
    { U,83,W16_RO_STRING("FrameRect"),0,API_MISSING,0 },
    { U,85,W16_RO_STRING("DrawText"),14,0,H(u_DrawText) },
    { U,96,W16_RO_STRING("CheckRadioButton"),8,0,H(u_CheckRadioButton) },
    { U,97,W16_RO_STRING("CheckDlgButton"),6,0,H(u_CheckDlgButton) },
    { U,98,W16_RO_STRING("IsDlgButtonChecked"),4,0,H(u_IsDlgButtonChecked) },
    { U,118,W16_RO_STRING("RegisterWindowMessage"),4,0,H(u_RegisterWindowMessage) },

    { U,65,W16_RO_STRING("GetScrollRange"),12,0,H(u_GetScrollRange) },
    /* KERNEL */
    { K, 83, W16_RO_STRING("_lcreat"), 6, 0, H(k_lcreat) },
    { K,20,W16_RO_STRING("GlobalSize"),2,0,H(k_GlobalSize) },
    { K, 4, W16_RO_STRING("LocalInit"), 6, 0, H(k_LocalInit) },
    { K, 154, W16_RO_STRING("GlobalNotify"), 4, 0, H(k_GlobalNotify) },
    { K,   3, W16_RO_STRING("GetVersion"),        0, 0, H(k_GetVersion) },
    { K,   5, W16_RO_STRING("LocalAlloc"),        4, 0, H(k_LocalAlloc) },
    { K,   7, W16_RO_STRING("LocalFree"),         2, 0, H(k_LocalFree) },
    { K,   8, W16_RO_STRING("LocalLock"),         2, 0, H(k_LocalLock) },
    { K,   9, W16_RO_STRING("LocalUnlock"),       2, 0, H(k_LocalUnlock) },
    { K,  10, W16_RO_STRING("LocalSize"),         2, 0, H(k_LocalSize) },
    { K,  18, W16_RO_STRING("GlobalLock"),        2, 0, H(k_GlobalLock) },
    { K,  19, W16_RO_STRING("GlobalUnlock"),      2, 0, H(k_GlobalUnlock) },
    { K,  54, W16_RO_STRING("GetInstanceData"),   6, 0, H(k_GetInstanceData) },
    { K, 129, W16_RO_STRING("WritePrivateProfileString"), 16, 0, H(k_WritePrivateProfileString) },
    { K, 127, W16_RO_STRING("GetPrivateProfileInt"), 14, 0, H(k_GetPrivateProfileInt) },
    { K, 128, W16_RO_STRING("GetPrivateProfileString"), 22, 0, H(k_GetPrivateProfileString) },
    { K,  57, W16_RO_STRING("GetProfileInt"),    10, 0, H(k_GetProfileInt) },
    { K,  58, W16_RO_STRING("GetProfileString"), 18, 0, H(k_GetProfileString) },
    { K,  59, W16_RO_STRING("WriteProfileString"), 12, 0, H(k_WriteProfileString) },
    { K,  60, W16_RO_STRING("FindResource"),     10, 0, H(k_FindResource) },
    { K,  61, W16_RO_STRING("LoadResource"),      4, 0, H(k_LoadResource) },
    { K,  62, W16_RO_STRING("LockResource"),      2, 0, H(k_GlobalLock) },
    { K,  63, W16_RO_STRING("FreeResource"),      2, 0, H(k_FreeResource) },
    { K,  88, W16_RO_STRING("lstrcpy"),           8, 0, H(k_lstrcpy) },
    { K,  89, W16_RO_STRING("lstrcat"),           8, 0, H(k_lstrcat) },
    { K,  90, W16_RO_STRING("lstrlen"),           4, 0, H(k_lstrlen) },
    { K,  30, W16_RO_STRING("WaitEvent"),         2, 0, H(k_WaitEvent) },
    { K,  48, W16_RO_STRING("GetModuleUsage"),    2, 0, H(k_GetModuleUsage) },
    { K,  49, W16_RO_STRING("GetModuleFileName"), 8, 0, H(k_GetModuleFileName) },
    { K,  23, W16_RO_STRING("LockSegment"),       2, 0, H(k_LockSegment) },
    { K,  24, W16_RO_STRING("UnlockSegment"),     2, 0, H(k_UnlockSegment) },
    { K,  51, W16_RO_STRING("MakeProcInstance"),  6, 0, H(k_MakeProcInstance) },
    { K,  52, W16_RO_STRING("FreeProcInstance"),  4, 0, H(k_FreeProcInstance) },
    { K,  91, W16_RO_STRING("InitTask"),          0, 0, H(k_InitTask) },
    { K, 102, W16_RO_STRING("DOS3Call"),          0, API_RAW, 0 },
    { K, 132, W16_RO_STRING("GetWinFlags"),       0, 0, H(k_GetWinFlags) },
    { K, 113, W16_RO_STRING("__AHSHIFT"),         0, API_CONST, 0, 12 },
    { K, 114, W16_RO_STRING("__AHINCR"),          0, API_CONST, 0, 0x1000 },
    { K, 174, W16_RO_STRING("__A000H"),           0, API_CONST, 0, 0xA000 },
    { K, 178, W16_RO_STRING("__WINFLAGS"),        0, API_CONST, 0, 0x0040 },     /* WF_CPU086: real mode on an 8086/8088 */
    { K, 179, W16_RO_STRING("__D000H"),           0, API_CONST, 0, 0xD000 },
    { K, 181, W16_RO_STRING("__B000H"),           0, API_CONST, 0, 0xB000 },
    { K, 182, W16_RO_STRING("__B800H"),           0, API_CONST, 0, 0xB800 },
    { K, 183, W16_RO_STRING("__0000H"),           0, API_CONST, 0, 0x0000 },
    { K, 190, W16_RO_STRING("__E000H"),           0, API_CONST, 0, 0xE000 },
    { K, 193, W16_RO_STRING("__0040H"),           0, API_CONST, 0, 0x0040 },
    { K, 194, W16_RO_STRING("__F000H"),           0, API_CONST, 0, 0xF000 },
    { K, 195, W16_RO_STRING("__C000H"),           0, API_CONST, 0, 0xC000 },
    /* USER */
    { U,   1, W16_RO_STRING("MessageBox"),       12, 0, H(u_MessageBox) },
    { U,   5, W16_RO_STRING("InitApp"),           2, 0, H(u_InitApp) },
    { U,  10, W16_RO_STRING("SetTimer"),         10, 0, H(u_SetTimer) },
    { U,  12, W16_RO_STRING("KillTimer"),         4, 0, H(u_KillTimer) },
    { U,  18, W16_RO_STRING("SetCapture"),        2, 0, H(u_SetCapture) },
    { U,  19, W16_RO_STRING("ReleaseCapture"),    0, 0, H(u_ReleaseCapture) },
    { U,  22, W16_RO_STRING("SetFocus"),          2, 0, H(u_SetFocus) },
    { U,  29, W16_RO_STRING("ScreenToClient"),    6, 0, H(u_ScreenToClient) },
    { U,  32, W16_RO_STRING("GetWindowRect"),     6, 0, H(u_GetWindowRect) },
    { U,  28, W16_RO_STRING("ClientToScreen"),    6, 0, H(u_ClientToScreen) },
    { U,  31, W16_RO_STRING("IsIconic"),          2, 0, H(u_IsIconic) },
    { U,   6, W16_RO_STRING("PostQuitMessage"),   2, 0, H(u_PostQuitMessage) },
    { U,  33, W16_RO_STRING("GetClientRect"),     6, 0, H(u_GetClientRect) },
    { U,  39, W16_RO_STRING("BeginPaint"),        6, 0, H(u_BeginPaint) },
    { U,  40, W16_RO_STRING("EndPaint"),          6, 0, H(u_EndPaint) },
    { U,  41, W16_RO_STRING("CreateWindow"),     30, 0, H(u_CreateWindow) },
    { U,  42, W16_RO_STRING("ShowWindow"),        4, 0, H(u_ShowWindow) },
    { U,  53, W16_RO_STRING("DestroyWindow"),     2, 0, H(u_DestroyWindow) },
    { U,  37, W16_RO_STRING("SetWindowText"),     6, 0, H(u_SetWindowText) },
    { U,  57, W16_RO_STRING("RegisterClass"),     4, 0, H(u_RegisterClass) },
    { U,  69, W16_RO_STRING("SetCursor"),         2, 0, H(u_SetCursor) },
    { U, 16, W16_RO_STRING("ClipCursor"), 4, 0, H(u_ClipCursor) },
    { U, 17, W16_RO_STRING("GetCursorPos"), 4, 0, H(u_GetCursorPos) },
    { U,  70, W16_RO_STRING("SetCursorPos"),      4, 0, H(u_SetCursorPos) },
    { U,  71, W16_RO_STRING("ShowCursor"),        2, 0, H(u_ShowCursor) },
    { U,  66, W16_RO_STRING("GetDC"),             2, 0, H(u_GetDC) },
    { U,  68, W16_RO_STRING("ReleaseDC"),         4, 0, H(u_ReleaseDC) },
    { U,  72, W16_RO_STRING("SetRect"),          12, 0, H(u_SetRect) },
    { U,  77, W16_RO_STRING("OffsetRect"),        8, 0, H(u_OffsetRect) },
    { U,  78, W16_RO_STRING("InflateRect"),       8, 0, H(u_InflateRect) },
    { U,  81, W16_RO_STRING("FillRect"),          8, 0, H(u_FillRect) },
    { U,  87, W16_RO_STRING("DialogBox"),        12, 0, H(u_DialogBox) },
    { U,  88, W16_RO_STRING("EndDialog"),         4, 0, H(u_EndDialog) },
    { U, 101, W16_RO_STRING("SendDlgItemMessage"), 12, 0, H(u_SendDlgItemMessage) },
    { U,  91, W16_RO_STRING("GetDlgItem"),        4, 0, H(u_GetDlgItem) },
    { U,  92, W16_RO_STRING("SetDlgItemText"),    8, 0, H(u_SetDlgItemText) },
    { U, 107, W16_RO_STRING("DefWindowProc"),    10, 0, H(u_DefWindowProc) },
    { U, 106, W16_RO_STRING("GetKeyState"),       2, 0, H(u_GetKeyState) },
    { U, 108, W16_RO_STRING("GetMessage"),       10, 0, H(u_GetMessage) },
    { U, 109, W16_RO_STRING("PeekMessage"),      12, 0, H(u_PeekMessage) },
    { U, 110, W16_RO_STRING("PostMessage"),      10, 0, H(u_PostMessage) },
    { U, 113, W16_RO_STRING("TranslateMessage"),  4, 0, H(u_TranslateMessage) },
    { U, 114, W16_RO_STRING("DispatchMessage"),   4, 0, H(u_DispatchMessage) },
    { U, 124, W16_RO_STRING("UpdateWindow"),      2, 0, H(u_UpdateWindow) },
    { U, 125, W16_RO_STRING("InvalidateRect"),    8, 0, H(u_InvalidateRect) },
    { U, 154, W16_RO_STRING("CheckMenuItem"),     6, 0, H(u_CheckMenuItem) },
    { U, 155, W16_RO_STRING("EnableMenuItem"),    6, 0, H(u_EnableMenuItem) },
    { U, 157, W16_RO_STRING("GetMenu"),           2, 0, H(u_GetMenu) },
    { U, 159, W16_RO_STRING("GetSubMenu"),        4, 0, H(u_GetSubMenu) },
    { U, 160, W16_RO_STRING("DrawMenuBar"),       2, 0, H(u_DrawMenuBar) },
    { U, 171, W16_RO_STRING("WinHelp"),          12, 0, H(u_WinHelp) },
    { U, 173, W16_RO_STRING("LoadCursor"),        6, 0, H(u_LoadCursor) },
    { U, 174, W16_RO_STRING("LoadIcon"),          6, 0, H(u_LoadIcon) },
    { U, 176, W16_RO_STRING("LoadString"),       10, 0, H(u_LoadString) },
    { U, 177, W16_RO_STRING("LoadAccelerators"),  6, 0, H(u_LoadAccelerators) },
    { U, 178, W16_RO_STRING("TranslateAccelerator"), 8, 0, H(u_TranslateAccelerator) },
    { U, 179, W16_RO_STRING("GetSystemMetrics"),  2, 0, H(u_GetSystemMetrics) },
    { U, 180, W16_RO_STRING("GetSysColor"),       2, 0, H(u_GetSysColor) },
    { U, 420, W16_RO_STRING("wsprintf"),          0, API_CDECL, H(u_wsprintf) },
    /* GDI */
    { G,   1, W16_RO_STRING("SetBkColor"),        6, 0, H(g_SetBkColor) },
    { G,   2, W16_RO_STRING("SetBkMode"),         4, 0, H(g_SetBkMode) },
    { G,   4, W16_RO_STRING("SetROP2"),           4, 0, H(g_SetROP2) },
    { G,   9, W16_RO_STRING("SetTextColor"),      6, 0, H(g_SetTextColor) },
    { G,  19, W16_RO_STRING("LineTo"),            6, 0, H(g_LineTo) },
    { G,  20, W16_RO_STRING("MoveTo"),            6, 0, H(g_MoveTo) },
    { G,  24, W16_RO_STRING("Ellipse"),          10, 0, H(g_Ellipse) },
    { G,  27, W16_RO_STRING("Rectangle"),        10, 0, H(g_Rectangle) },
    { G,  29, W16_RO_STRING("PatBlt"),           14, 0, H(g_PatBlt) },
    { G,  33, W16_RO_STRING("TextOut"),          12, 0, H(g_TextOut) },
    { G,  45, W16_RO_STRING("SelectObject"),      4, 0, H(g_SelectObject) },
    { G,  48, W16_RO_STRING("CreateBitmap"),     12, 0, H(g_CreateBitmap) },
    { G,  53, W16_RO_STRING("CreateDC"),         16, 0, H(g_CreateDC) },
    { G,  57, W16_RO_STRING("CreateFontIndirect"), 4, 0, H(g_CreateFontIndirect) },
    { G,  60, W16_RO_STRING("CreatePatternBrush"), 2, 0, H(g_CreatePatternBrush) },
    { G,  61, W16_RO_STRING("CreatePen"),         8, 0, H(g_CreatePen) },
    { G,  66, W16_RO_STRING("CreateSolidBrush"),  4, 0, H(g_CreateSolidBrush) },
    { G,  68, W16_RO_STRING("DeleteDC"),          2, 0, H(g_DeleteDC) },
    { G,  69, W16_RO_STRING("DeleteObject"),      2, 0, H(g_DeleteObject) },
    { G,  80, W16_RO_STRING("GetDeviceCaps"),     4, 0, H(g_GetDeviceCaps) },
    { G,  87, W16_RO_STRING("GetStockObject"),    2, 0, H(g_GetStockObject) },
    { G,  91, W16_RO_STRING("GetTextExtent"),     8, 0, H(g_GetTextExtent) },
    { G,  93, W16_RO_STRING("GetTextMetrics"),    6, 0, H(g_GetTextMetrics) },
    { G, 119, W16_RO_STRING("AddFontResource"),   4, 0, H(g_AddFontResource) },
    { G, 136, W16_RO_STRING("RemoveFontResource"), 4, 0, H(g_RemoveFontResource) },
    { G, 150, W16_RO_STRING("UnrealizeObject"),   2, 0, H(g_UnrealizeObject) },
    { G, 154, W16_RO_STRING("GetNearestColor"),   6, 0, H(g_GetNearestColor) },
    { G, 346, W16_RO_STRING("SetTextAlign"),      4, 0, H(g_SetTextAlign) },
    { G, 351, W16_RO_STRING("ExtTextOut"),       22, 0, H(g_ExtTextOut) },
    { G, 75, W16_RO_STRING("GetBkColor"), 2, 0, H(g_GetBkColor) },
    { G,97,W16_RO_STRING("GetWindowOrg"),2,0,H(g_GetWindowOrg) },
    { K,97,W16_RO_STRING("GetTempFileName"),12,0,H(k_GetTempFileName) },
    { K, 164, W16_RO_STRING("GlobalLRUNewest"), 2, 0, H(k_GlobalLRUNewest) },
    /* Paintbrush imports. Entries marked API_MISSING are for -u diagnostics:
     * these terminate if executed; ordinary loads reject them. */
    { G,3,W16_RO_STRING("SetMapMode"),4,0,H(g_SetMapMode) },
    { G,   6, W16_RO_STRING("SetPolyFillMode"), 4, 0, H(g_SetPolyFillMode) },
    { G,7,W16_RO_STRING("SetStretchBltMode"),4,0,H(g_SetStretchBltMode) },
    { G,11,W16_RO_STRING("SetWindowOrg"),6,0,H(g_SetWindowOrg) },
    { G,12,W16_RO_STRING("SetWindowExt"),6,0,H(g_SetWindowExt) },
    { G,13,W16_RO_STRING("SetViewportOrg"),6,0,H(g_SetViewportOrg) },
    { G,14,W16_RO_STRING("SetViewportExt"),6,0,H(g_SetViewportExt) },
    { G,  21, W16_RO_STRING("EXCLUDECLIPRECT"), 0, API_MISSING, 0 },
    { G,  28, W16_RO_STRING("RoundRect"), 14, 0, H(g_RoundRect) },
    { G, 30, W16_RO_STRING("SaveDC"), 2, 0, H(g_SaveDC) },
    { G,  36, W16_RO_STRING("Polygon"), 8, 0, H(g_Polygon) },
    { G,  37, W16_RO_STRING("Polyline"), 8, 0, H(g_Polyline) },
    { G,  38, W16_RO_STRING("ESCAPE"), 0, API_MISSING, 0 },
    { G, 39, W16_RO_STRING("RestoreDC"), 4, 0, H(g_RestoreDC) },
    { G,  44, W16_RO_STRING("SELECTCLIPRGN"), 0, API_MISSING, 0 },
    { G,  58, W16_RO_STRING("CREATEHATCHBRUSH"), 0, API_MISSING, 0 },
    { G,  63, W16_RO_STRING("CREATEPOLYGONRGN"), 0, API_MISSING, 0 },
    { G,  64, W16_RO_STRING("CREATERECTRGN"), 0, API_MISSING, 0 },
    { G,99,W16_RO_STRING("LPtoDP"),8,0,H(g_LPtoDP) },
    { G,67,W16_RO_STRING("DPtoLP"),8,0,H(g_DPtoLP) },
    { G,  70, W16_RO_STRING("EnumFonts"), 14, 0, H(g_EnumFonts) },
    { G,  71, W16_RO_STRING("EnumObjects"), 12, 0, H(g_EnumObjects) },
    { G,  74, W16_RO_STRING("GetBitmapBits"), 10, 0, H(g_GetBitmapBits) },
    { G, 85, W16_RO_STRING("GetROP2"), 2, 0, H(g_GetROP2) },
    { G, 106, W16_RO_STRING("SetBitmapBits"), 10, 0, H(g_SetBitmapBits) },
    { G, 149, W16_RO_STRING("GETBRUSHORG"), 0, API_MISSING, 0 },
    { G, 153, W16_RO_STRING("CreateIC"), 16, 0, H(g_CreateIC) },
    /* Discardability permits reclamation; retain these owned pixels until DeleteObject. */
    { G, 156, W16_RO_STRING("CreateDiscardableBitmap"), 6, 0, H(g_CreateCompatibleBitmap) },
    { G, 163, W16_RO_STRING("SetBitmapDimension"), 6, 0, H(g_SetBitmapDimension) },
    { G, 360, W16_RO_STRING("CREATEPALETTE"), 4, 0, H(g_CreatePalette) },
    { G, 363, W16_RO_STRING("GETPALETTEENTRIES"), 10, 0, H(g_GetPaletteEntries) },
    { G, 366, W16_RO_STRING("UPDATECOLORS"), 0, API_MISSING, 0 },
    { G, 370, W16_RO_STRING("GETNEARESTPALETTEINDEX"), 6, 0, H(g_GetNearestPaletteIndex) },
    { G, 372, W16_RO_STRING("ExtFloodFill"), 12, 0, H(g_ExtFloodFill) },
    { G, 440, W16_RO_STRING("SetDIBits"), 18, 0, H(g_SetDIBits) },
    { G, 441, W16_RO_STRING("GetDIBits"), 18, 0, H(g_GetDIBits) },
    { G, 443, W16_RO_STRING("SetDIBitsToDevice"), 28, 0, H(g_SetDIBitsToDevice) },
    { K,  50, W16_RO_STRING("GETPROCADDRESS"), 0, API_MISSING, 0 },
    { K, 74, W16_RO_STRING("OpenFile"), 10, 0, H(k_OpenFile) },
    { K, 81, W16_RO_STRING("_lclose"), 2, 0, H(k_lclose) },
    { K, 82, W16_RO_STRING("_lread"), 8, 0, H(k_lread) },
    { K, 84, W16_RO_STRING("_llseek"), 8, 0, H(k_llseek) },
    { K, 85, W16_RO_STRING("_lopen"), 6, 0, H(k_lopen) },
    { K, 86, W16_RO_STRING("_lwrite"), 8, 0, H(k_lwrite) },
    { K,  92, W16_RO_STRING("GetTempDrive"), 2, 0, H(k_GetTempDrive) },
    { K,  95, W16_RO_STRING("LOADLIBRARY"), 0, API_MISSING, 0 },
    { K,  96, W16_RO_STRING("FREELIBRARY"), 0, API_MISSING, 0 },

    { K, 169, W16_RO_STRING("GetFreeSpace"), 2, 0, H(k_GetFreeSpace) },
    { P,   2, W16_RO_STRING("VCREATEBITMAP"), 0, API_MISSING, 0 },
    { P,   3, W16_RO_STRING("GETVCACHEDC"), 0, API_MISSING, 0 },
    { P,   4, W16_RO_STRING("VPATBLT"), 0, API_MISSING, 0 },
    { P,   5, W16_RO_STRING("VBITBLT"), 0, API_MISSING, 0 },
    { P,   6, W16_RO_STRING("VSTRETCHBLT"), 0, API_MISSING, 0 },
    { P,   7, W16_RO_STRING("VDELETEOBJECT"), 0, API_MISSING, 0 },
    { P,   8, W16_RO_STRING("DISCARDBAND"), 0, API_MISSING, 0 },
    { U,  16, W16_RO_STRING("CLIPCURSOR"), 0, API_MISSING, 0 },
    { U,  17, W16_RO_STRING("GETCURSORPOS"), 0, API_MISSING, 0 },
    { U,  45, W16_RO_STRING("BringWindowToTop"), 2, 0, H(u_BringWindowToTop) },
    { U,  47, W16_RO_STRING("IsWindow"), 2, 0, H(u_IsWindow) },
    { U,  49, W16_RO_STRING("IsWindowVisible"), 2, 0, H(u_IsWindowVisible) },
    { U,  59, W16_RO_STRING("SetActiveWindow"), 2, 0, H(u_SetActiveWindow) },
    { U,  60, W16_RO_STRING("GetActiveWindow"), 0, 0, H(u_GetActiveWindow) },
    { U,61,W16_RO_STRING("ScrollWindow"),14,0,H(u_ScrollWindow) },
    { U,  62, W16_RO_STRING("SetScrollPos"), 8, 0, H(u_SetScrollPos) },
    { U,  63, W16_RO_STRING("GetScrollPos"), 4, 0, H(u_GetScrollPos) },
    { U,  64, W16_RO_STRING("SetScrollRange"), 10, 0, H(u_SetScrollRange) },
    { U,  89, W16_RO_STRING("CREATEDIALOG"), 0, API_MISSING, 0 },
    { U,  90, W16_RO_STRING("ISDIALOGMESSAGE"), 0, API_MISSING, 0 },
    { U,  93, W16_RO_STRING("GetDlgItemText"), 10, 0, H(u_GetDlgItemText) },
    { U,  94, W16_RO_STRING("SetDlgItemInt"), 8, 0, H(u_SetDlgItemInt) },
    { U,  95, W16_RO_STRING("GetDlgItemInt"), 10, 0, H(u_GetDlgItemInt) },
    { U,  99, W16_RO_STRING("DlgDirSelect"), 8, 0, H(u_DlgDirSelect) },
    { U, 100, W16_RO_STRING("DlgDirList"), 12, 0, H(u_DlgDirList) },
    { U, 104, W16_RO_STRING("MESSAGEBEEP"), 0, API_MISSING, 0 },
    { U, 111, W16_RO_STRING("SendMessage"), 10, 0, H(u_SendMessage) },
    { U, 119, W16_RO_STRING("GETMESSAGEPOS"), 0, API_MISSING, 0 },
    { U, 130, W16_RO_STRING("SetClassWord"), 6, 0, H(u_SetClassWord) },
    { U, 133, W16_RO_STRING("GetWindowWord"), 4, 0, H(u_GetWindowWord) },
    { U, 134, W16_RO_STRING("SetWindowWord"), 6, 0, H(u_SetWindowWord) },
    { U, 135, W16_RO_STRING("GetWindowLong"), 4, 0, H(u_GetWindowLong) },
    { U, 136, W16_RO_STRING("SetWindowLong"), 8, 0, H(u_SetWindowLong) },
    { U, 137, W16_RO_STRING("OPENCLIPBOARD"), 2, 0, H(u_OpenClipboard) },
    { U, 138, W16_RO_STRING("CLOSECLIPBOARD"), 0, 0, H(u_CloseClipboard) },
    { U, 139, W16_RO_STRING("EMPTYCLIPBOARD"), 0, 0, H(u_EmptyClipboard) },
    { U, 141, W16_RO_STRING("SETCLIPBOARDDATA"), 4, 0, H(u_SetClipboardData) },
    { U, 142, W16_RO_STRING("GETCLIPBOARDDATA"), 2, 0, H(u_GetClipboardData) },
    { U, 156, W16_RO_STRING("GETSYSTEMMENU"), 0, API_MISSING, 0 },
    { U, 161, W16_RO_STRING("GetMenuString"), 12, 0, H(u_GetMenuString) },
    { U, 163, W16_RO_STRING("CreateCaret"), 8, 0, H(u_CreateCaret) },
    { U, 164, W16_RO_STRING("DestroyCaret"), 0, 0, H(u_DestroyCaret) },
    { U, 165, W16_RO_STRING("SetCaretPos"), 4, 0, H(u_SetCaretPos) },
    { U, 166, W16_RO_STRING("HideCaret"), 2, 0, H(u_HideCaret) },
    { U, 167, W16_RO_STRING("ShowCaret"), 2, 0, H(u_ShowCaret) },
    { U, 232, W16_RO_STRING("SetWindowPos"), 14, 0, H(u_SetWindowPos) },
    { U, 250, W16_RO_STRING("GetMenuState"), 6, 0, H(u_GetMenuState) },
    { U, 278, W16_RO_STRING("GetDesktopHwnd"), 0, 0, H(u_GetDesktopHwnd) },
    { U, 282, W16_RO_STRING("SELECTPALETTE"), 6, 0, H(u_SelectPalette) },
    { U, 283, W16_RO_STRING("REALIZEPALETTE"), 2, 0, H(u_RealizePalette) },
    { U, 402, W16_RO_STRING("GETPRIORITYCLIPBOARDFORMAT"), 6, 0, H(u_GetPriorityClipboardFormat) },
    { U, 410, W16_RO_STRING("INSERTMENU"), 0, API_MISSING, 0 },
    { U, 413, W16_RO_STRING("DELETEMENU"), 0, API_MISSING, 0 },
    { U, 414, W16_RO_STRING("ModifyMenu"), 12, 0, H(u_ModifyMenu) },
    { U, 430, W16_RO_STRING("lstrcmp"), 8, 0, H(u_lstrcmp) },
    { U, 431, W16_RO_STRING("AnsiUpper"), 4, 0, H(u_AnsiUpper) },
    { U, 471, W16_RO_STRING("lstrcmpi"), 8, 0, H(u_lstrcmpi) },
    { U, 472, W16_RO_STRING("AnsiNext"), 4, 0, H(u_AnsiNext) },
    { U, 473, W16_RO_STRING("AnsiPrev"), 8, 0, H(u_AnsiPrev) },
    /* SOUND is a deliberately unavailable play device, not pretend playback.
     * WinMine disables sound when OpenSound reports fewer than one voice. */
    { S, 1, W16_RO_STRING("OpenSound"), 0, 0, H(s_OpenSound) },
    { S, 2, W16_RO_STRING("CloseSound"), 0, 0, H(s_CloseSound) },
    { S, 4, W16_RO_STRING("SetVoiceNote"), 8, 0, H(s_Unavailable) },
    { S, 5, W16_RO_STRING("SetVoiceAccent"), 10, 0, H(s_Unavailable) },
    { S, 9, W16_RO_STRING("StartSound"), 0, 0, H(s_Unavailable) },
    { S, 10, W16_RO_STRING("StopSound"), 0, 0, H(s_Unavailable) },
    { K, 1, W16_RO_STRING("FatalExit"), 2, 0, H(k_FatalExit) },
    { K, 137, W16_RO_STRING("FatalAppExit"), 6, 0, H(k_FatalAppExit) },
    { K, 115, W16_RO_STRING("OutputDebugString"), 4, API_RAW, 0 },
    /* These bind only with -u and terminate if called. */
    { L, 22, W16_RO_STRING("ShellAbout"), 12, 0, H(sh_ShellAbout) },
    { G, 442, W16_RO_STRING("CreateDIBitmap"), 20, 0, H(g_CreateDIBitmap) },
    { U, 50, W16_RO_STRING("FindWindow"), 8, 0, H(u_FindWindow) },

    { U, 447, W16_RO_STRING("DefMDIChildProc"), 10, 0, H(u_DefMDIChildProc) },
    { K, 157, W16_RO_STRING("GetCurPID"), 4, API_RAW, H(k_GetCurPID) },
    { U, 432, W16_RO_STRING("AnsiLower"), 4, 0, H(u_AnsiLower) },
    { U, 433, W16_RO_STRING("IsCharAlpha"), 2, 0, H(u_IsCharAlpha) },
    { U, 434, W16_RO_STRING("IsCharAlphaNumeric"), 2, 0, H(u_IsCharAlphaNumeric) },
    { U, 437, W16_RO_STRING("AnsiUpperBuff"), 6, 0, H(u_AnsiUpperBuff) },
    { U, 46, W16_RO_STRING("GetParent"), 2, 0, H(u_GetParent) },
    { U, 262, W16_RO_STRING("GetWindow"), 4, 0, H(u_GetWindow) },
    { U, 451, W16_RO_STRING("TranslateMDISysAccel"), 6, 0, H(u_TranslateMDISysAccel) },
    { K, 47, W16_RO_STRING("GetModuleHandle"), 4, 0, H(k_GetModuleHandle) },
    { K, 136, W16_RO_STRING("GetDriveType"), 2, 0, H(k_GetDriveType) },
    { K, 152, W16_RO_STRING("GetNumTasks"), 0, 0, H(k_GetNumTasks) },
    { U, 84, W16_RO_STRING("DrawIcon"), 8, 0, H(u_DrawIcon) },
    { U, 368, W16_RO_STRING("CopyIcon"), 4, 0, H(u_CopyIcon) },
    { U, 104, W16_RO_STRING("MessageBeep"), 2, 0, H(u_MessageBeep) },
    { U, 188, W16_RO_STRING("SetSysModalWindow"), 2, 0, H(u_SetSysModalWindow) },
    { U, 189, W16_RO_STRING("GetSysModalWindow"), 0, 0, H(u_GetSysModalWindow) },
    { U, 464, W16_RO_STRING("DragObject"), 12, 0, H(u_DragObject) },
    { U, 170, W16_RO_STRING("ArrangeIconicWindows"), 2, 0, H(u_ArrangeIconicWindows) },
    { U, 460, W16_RO_STRING("GetInternalWindowPos"), 10, 0, H(u_GetInternalWindowPos) },
    { U, 461, W16_RO_STRING("SetInternalWindowPos"), 12, 0, H(u_SetInternalWindowPos) },
    { U, 445, W16_RO_STRING("DefFrameProc"), 12, 0, H(u_DefFrameProc) },
    { U, 462, W16_RO_STRING("CalcChildScroll"), 4, 0, H(u_CalcChildScroll) },
    { U, 463, W16_RO_STRING("ScrollChildren"), 10, 0, H(u_ScrollChildren) },
    { U, 267, W16_RO_STRING("ShowScrollBar"), 6, 0, H(u_ShowScrollBar) },
    { K, 138, W16_RO_STRING("GetHeapSpaces"), 2, 0, H(k_GetHeapSpaces) },
    { K, 168, W16_RO_STRING("DirectResAlloc"), 6, 0, H(k_DirectResAlloc) },
    { U, 408, W16_RO_STRING("CreateCursorIconIndirect"), 14, 0, H(u_CreateCursorIconIndirect) },
    { U, 458, W16_RO_STRING("DestroyCursor"), 2, 0, H(u_DestroyCursor) },
    { U, 459, W16_RO_STRING("DumpIcon"), 16, 0, H(u_DumpIcon) },
    { U, 456, W16_RO_STRING("LoadIconHandler"), 4, 0, H(u_LoadIconHandler) },
    { U, 455, W16_RO_STRING("GetIconId"), 6, 0, H(u_GetIconId) },
    { U, 457, W16_RO_STRING("DestroyIcon"), 2, 0, H(u_DestroyIcon) },
    { U, 268, W16_RO_STRING("GlobalAddAtom"), 4, 0, H(u_GlobalAddAtom) },
    { U, 269, W16_RO_STRING("GlobalDeleteAtom"), 2, 0, H(u_GlobalDeleteAtom) },
    { U, 270, W16_RO_STRING("GlobalFindAtom"), 4, 0, H(u_GlobalFindAtom) },
    { K, 134, W16_RO_STRING("GetWindowsDirectory"), 6, 0, H(k_GetWindowsDirectory) },
    { K, 166, W16_RO_STRING("WinExec"), 6, 0, H(k_WinExec) },
    { U, 7, W16_RO_STRING("ExitWindows"), 6, 0, H(u_ExitWindows) },
};
const int api_count = sizeof(api_table) / sizeof(api_table[0]);
typedef char api_stub_capacity[(sizeof(api_table)/sizeof(api_table[0])<=W16_API_SLOTS)?1:-1];

/* Names of common Windows 3.0 entry points this layer does NOT implement, for diagnostics only. */
struct known { u8 mod; u16 ord; const char W16_RO_FAR *name; };
static const struct known W16_RO_FAR known_names[] = {
    { K, 1, W16_RO_STRING("FatalExit") }, { K, 4, W16_RO_STRING("LocalInit") }, { K, 6, W16_RO_STRING("LocalReAlloc") },

    { K, 15, W16_RO_STRING("GlobalAlloc") }, { K, 16, W16_RO_STRING("GlobalReAlloc") }, { K, 17, W16_RO_STRING("GlobalFree") },
    { K, 20, W16_RO_STRING("GlobalSize") },
    { K, 36, W16_RO_STRING("GetCurrentTask") }, { K, 47, W16_RO_STRING("GetModuleHandle") },
    { K, 50, W16_RO_STRING("GetProcAddress") },
    { K, 74, W16_RO_STRING("OpenFile") },
    { K, 81, W16_RO_STRING("_lclose") }, { K, 82, W16_RO_STRING("_lread") }, { K, 83, W16_RO_STRING("_lcreat") }, { K, 84, W16_RO_STRING("_llseek") }, { K, 85, W16_RO_STRING("_lopen") },
    { K, 86, W16_RO_STRING("_lwrite") },
    { K, 95, W16_RO_STRING("LoadLibrary") }, { K, 96, W16_RO_STRING("FreeLibrary") }, { K, 131, W16_RO_STRING("GetDOSEnvironment") }, { K, 137, W16_RO_STRING("FatalAppExit") },
    { U, 13, W16_RO_STRING("GetTickCount") },
    { U, 15, W16_RO_STRING("GetCurrentTime") }, { U, 17, W16_RO_STRING("GetCursorPos") },
    { U, 23, W16_RO_STRING("GetFocus") }, { U, 32, W16_RO_STRING("GetWindowRect") }, { U, 34, W16_RO_STRING("EnableWindow") },
    { U, 36, W16_RO_STRING("GetWindowText") }, { U, 46, W16_RO_STRING("GetParent") }, { U, 56, W16_RO_STRING("MoveWindow") },
    { U, 85, W16_RO_STRING("DrawText") },
    { U, 89, W16_RO_STRING("CreateDialog") }, { U, 104, W16_RO_STRING("MessageBeep") },
    { U, 111, W16_RO_STRING("SendMessage") },
    { U, 150, W16_RO_STRING("LoadMenu") }, { U, 175, W16_RO_STRING("LoadBitmap") },
    { U, 421, W16_RO_STRING("wvsprintf") },
    { G, 3, W16_RO_STRING("SetMapMode") }, { G, 11, W16_RO_STRING("SetWindowOrg") }, { G, 13, W16_RO_STRING("SetViewportOrg") },
    { G, 28, W16_RO_STRING("RoundRect") }, { G, 30, W16_RO_STRING("SaveDC") }, { G, 31, W16_RO_STRING("SetPixel") }, { G, 34, W16_RO_STRING("BitBlt") },
    { G, 35, W16_RO_STRING("StretchBlt") }, { G, 36, W16_RO_STRING("Polygon") }, { G, 37, W16_RO_STRING("Polyline") }, { G, 39, W16_RO_STRING("RestoreDC") },
    { G, 50, W16_RO_STRING("CreateBrushIndirect") }, { G, 52, W16_RO_STRING("CreateCompatibleDC") },
    { G, 56, W16_RO_STRING("CreateFont") }, { G, 58, W16_RO_STRING("CreateHatchBrush") },

};

int api_find_ord(int mod, u16 ord)
{
    int i;
    for (i = 0; i < api_count; i++)
        if (api_table[i].mod == mod && api_table[i].ord == ord &&
            (!(api_table[i].flags & API_MISSING) || opt_missing))
            return i;
    return -1;
}

static int ieq(const char W16_RO_FAR *a, const char *b)
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
        if (api_table[i].mod == mod && ieq(api_table[i].name, name) &&
            (!(api_table[i].flags & API_MISSING) || opt_missing))
            return i;
    return -1;
}

static const char *near_name(const char W16_RO_FAR *name)
{
#if defined(W16_SHARED_RODATA) && !defined(W16_HOST)
    /* Names are diagnostic/lookup text, never guest pointers. Each task owns
     * this buffer; no shared mutable storage or borrowed far-to-near casts. */
    static char buf[64];
    unsigned i;
    for (i = 0; i < sizeof(buf); i++) {
        buf[i] = name[i];
        if (!buf[i]) return buf;
    }
    abort(); /* all built-in names must fit, including their terminating NUL */
#else
    return name;
#endif
}
const char *api_name(int idx)
{
    return idx >= 0 && idx < api_count ? near_name(api_table[idx].name) : 0;
}

const char *api_known_name(int mod, u16 ord)
{
    unsigned i;
    for (i = 0; i < sizeof(known_names) / sizeof(known_names[0]); i++)
        if (known_names[i].mod == mod && known_names[i].ord == ord)
            return near_name(known_names[i].name);
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
    *seg = FSEG(w16_stubs);                 /* the stubs' own code segment */
    if(api_table[idx].mod==MOD_KERNEL && api_table[idx].ord==157) {
        wr16(*seg,FOFF(w16_getcurpid_index),idx);
        *off=FOFF(w16_getcurpid);return 0;
    }
    *off = (api_table[idx].flags & API_RAW) ?
        (api_table[idx].mod == MOD_KERNEL && api_table[idx].ord == 115 ? FOFF(w16_debugstring) : FOFF(w16_dos3call)) :
        (u16)(FOFF(w16_stubs) + 8 * idx);
    return 0;
}

/* Called from thunk.S on the loader stack. The app frame at w16_app_ss:w16_app_sp is
 * DI SI DS BP IP CS, then the arguments. Pops Pascal arguments by moving the frame up. */
u32 W16_CDECL w16_api_dispatch(u16 idx)
{
    const struct api W16_RO_FAR *a;
    u16 ss = w16_app_ss, sp = w16_app_sp, n, k;
    u32 r;

    if (idx >= api_count) { w16_exit16(W16_EIMPORT); return W16_EIMPORT; }
    a = &api_table[idx];
    if (a->flags & API_MISSING) {
        wlog("w16: MISSING API TRAP: %s.%u at %04x:%04x; stopping\n",
             w16_modnames[a->mod],a->ord,rd16(ss,sp+10),rd16(ss,sp+8));
        if (opt_trace && sp <= 0xffe8u)
            wlog("w16: missing args %04x %04x %04x %04x %04x %04x\n",
                 rd16(ss,sp+12),rd16(ss,sp+14),rd16(ss,sp+16),
                 rd16(ss,sp+18),rd16(ss,sp+20),rd16(ss,sp+22));
        w16_exit16(W16_EIMPORT); return W16_EIMPORT;
    }
    w16_ret_bx = 0;
    w16_ret_cx = 0;
    w16_ret_es = ss;
    if (opt_trace)
        wlog("w16: call %s.%s\n", w16_modnames[a->mod], api_name(idx));
#ifndef W16_COMPACT_LOG
    if (opt_trace && a->mod == MOD_GDI && a->ord == 51)
        wlog("w16: compatible bitmap dc=%04x w=%d h=%d\n", rd16(ss,sp+16), (i16)rd16(ss,sp+14), (i16)rd16(ss,sp+12));
#endif
#if defined(W16_SHARED_RODATA) && !defined(W16_HOST)
    /* IA-16 6.3 may issue lcall through a far object's field with DS still
     * selecting that object. Materialize the pointer in private stack storage
     * so the call uses the ordinary native ABI (DS == SS), never the RO bank.
     * Volatile prevents folding this back into the unsafe far-memory call. */
    {
        api_fn volatile fn = a->fn;
        r = fn(ss, (u16)(sp + 12));
    }
#else
    r = a->fn(ss, (u16)(sp + 12));
#endif
#ifndef W16_COMPACT_LOG
    if (opt_trace && !r && a->mod == MOD_USER && a->ord == 175 && sp <= 0xffe8u)
        wlog("w16: LoadBitmap failed name=%04x:%04x instance=%04x\n",rd16(ss,sp+14),rd16(ss,sp+12),rd16(ss,sp+16));
    if (opt_trace && r == 0xffffffffUL && a->mod == MOD_GDI && (a->ord == 31 || a->ord == 83) && sp <= 0xffe8u)
        wlog("w16: pixel failed ord=%u args=%04x %04x %04x %04x %04x\n", a->ord,
             rd16(ss,sp+12),rd16(ss,sp+14),rd16(ss,sp+16),rd16(ss,sp+18),rd16(ss,sp+20));
    if (opt_trace && !r && a->mod == MOD_GDI && (a->ord == 29 || a->ord == 34) && sp <= 0xffdcu)
        wlog("w16: raster failed ord=%u args=%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n", a->ord,
             rd16(ss,sp+12),rd16(ss,sp+14),rd16(ss,sp+16),rd16(ss,sp+18),rd16(ss,sp+20),
             rd16(ss,sp+22),rd16(ss,sp+24),rd16(ss,sp+26),rd16(ss,sp+28),rd16(ss,sp+30));
    if(opt_trace && ((a->mod==MOD_KERNEL && (a->ord==15 || a->ord==16 || a->ord==74 || a->ord==81 || a->ord==82 || a->ord==86)) ||
       (a->mod==MOD_GDI && (a->ord==48 || a->ord==74 || a->ord==106 || a->ord==440)))) {
        wlog("w16: result %s.%u = %08lx",w16_modnames[a->mod],a->ord,(unsigned long)r);
        for(k=0;k<a->argbytes && k<12;k+=2)wlog(" %04x",rd16(ss,sp+12+k));
        wlog("\n");
    }
#endif
#ifdef W16_TRACE_STRETCH
    if(opt_trace && a->mod==MOD_GDI && a->ord==35 && sp<=0xffdcu)
        wlog("w16: stretch xy=%d,%d d=%dx%d rop=%08lx result=%lu\n",
             (i16)rd16(ss,sp+32),(i16)rd16(ss,sp+30),(i16)rd16(ss,sp+28),(i16)rd16(ss,sp+26),
             (unsigned long)rd32(ss,sp+12),(unsigned long)r);
#endif
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
