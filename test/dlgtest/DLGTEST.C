/*
 * DLGTEST - Win16 dialog-box test program for the ELKS Win16 layer.
 *
 * Exercises the same paths as Windows 3.0's WINVER.EXE with source that can be shared:
 * GetVersion, GetWinFlags, GetSystemMetrics, LockSegment/UnlockSegment, MakeProcInstance,
 * DialogBox from a named resource, a named icon, SetDlgItemText, EndDialog, FreeProcInstance.
 * The exit code is the dialog result: 1 for OK (Enter or click), 2 for Cancel (Esc or click).
 */
#include <windows.h>
#include "dlgtest.h"

static char szInfo[80];

BOOL CALLBACK __export TestDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemText(hDlg, ID_INFO, szInfo);
        return TRUE;
    case WM_COMMAND:
        if (wParam == IDOK || wParam == IDCANCEL) {
            EndDialog(hDlg, wParam);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

int PASCAL WinMain(HANDLE hInstance, HANDLE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    FARPROC lpfn;
    DWORD flags;
    WORD ver;
    int r;

    (void)lpszCmdLine; (void)nCmdShow;
    if (hPrevInstance)
        return 0;
    LockSegment((UINT)-1);
    ver = LOWORD(GetVersion());
    flags = GetWinFlags();
    wsprintf(szInfo, "Windows %d.%02d %s, screen %dx%d", LOBYTE(ver), HIBYTE(ver),
             (LPSTR)((flags & WF_PMODE) ? "protected mode" : "real mode"),
             GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    UnlockSegment((UINT)-1);

    lpfn = MakeProcInstance((FARPROC)TestDlgProc, hInstance);
    r = DialogBox(hInstance, "TestDlg", NULL, (DLGPROC)lpfn);
    FreeProcInstance(lpfn);
    return r;
}
