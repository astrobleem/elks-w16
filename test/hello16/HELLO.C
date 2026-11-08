/*
 * HELLO16 - tiny Win16 test program for the ELKS Win16 layer.
 *
 * Ordinary Windows 3.0 source: standard WinMain, RegisterClass/CreateWindow,
 * a GetMessage loop and a window procedure. Built with Open Watcom as a
 * medium-model, 8086-instruction NE executable (see build.sh).
 *
 * Interaction: any character key is counted and echoed in the window,
 * a left click moves the marker, Esc or Q closes the window.
 * The exit code is the number of character keys seen.
 */
#include <windows.h>
#include "hello.h"

static char szClass[] = "ElksHello16";

int  nKeys = 0;            /* character keys seen (also the exit code) */
int  nLastKey = 0;         /* last character, 0 if none                */
int  xMark = -1, yMark = -1;
int  nPaints = 0;

LRESULT CALLBACK __export WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (msg) {
    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        nPaints++;
        PaintScene(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CHAR:
        if (wParam == 'q' || wParam == 'Q') {
            DestroyWindow(hwnd);
            return 0;
        }
        nKeys++;
        nLastKey = wParam;
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_LBUTTONDOWN:
        xMark = LOWORD(lParam);
        yMark = HIWORD(lParam);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(nKeys);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int PASCAL WinMain(HANDLE hInstance, HANDLE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    WNDCLASS wc;
    MSG msg;
    HWND hwnd;

    (void)lpszCmdLine;
    if (!hPrevInstance) {
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = hInstance;
        wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = GetStockObject(WHITE_BRUSH);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = szClass;
        if (!RegisterClass(&wc))
            return 255;
    }

    hwnd = CreateWindow(szClass, "Win16 on ELKS", WS_OVERLAPPEDWINDOW,
                        16, 12, 288, 168, NULL, NULL, hInstance, NULL);
    if (!hwnd)
        return 254;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
