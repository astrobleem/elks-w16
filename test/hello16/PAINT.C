/*
 * PAINT.C - drawing for HELLO16, compiled into its own code segment so the
 * executable carries inter-segment far calls and relocations.
 */
#include <windows.h>
#include "hello.h"

void FAR PaintScene(HWND hwnd, HDC hdc)
{
    RECT rc;
    HBRUSH hbrBlue, hbrRed, hbrOld;
    HPEN hpenGreen, hpenOld;
    char buf[40];
    int n;

    GetClientRect(hwnd, &rc);

    /* blue filled rectangle with the default black pen */
    hbrBlue = CreateSolidBrush(RGB(0, 0, 255));
    hbrOld = SelectObject(hdc, hbrBlue);
    Rectangle(hdc, 8, 8, 120, 56);

    /* red ellipse */
    hbrRed = CreateSolidBrush(RGB(255, 0, 0));
    SelectObject(hdc, hbrRed);
    Ellipse(hdc, 136, 8, 248, 56);
    SelectObject(hdc, hbrOld);
    DeleteObject(hbrBlue);
    DeleteObject(hbrRed);

    /* green diagonal lines across the bottom band */
    hpenGreen = CreatePen(PS_SOLID, 1, RGB(0, 128, 0));
    hpenOld = SelectObject(hdc, hpenGreen);
    MoveTo(hdc, 8, rc.bottom - 8);
    LineTo(hdc, rc.right - 8, rc.bottom - 40);
    MoveTo(hdc, 8, rc.bottom - 40);
    LineTo(hdc, rc.right - 8, rc.bottom - 8);
    SelectObject(hdc, hpenOld);
    DeleteObject(hpenGreen);

    /* text: greeting and the interaction state */
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    TextOut(hdc, 8, 62, "Hello from Win16!", 17);
    if (nLastKey)
        n = wsprintf(buf, "Keys: %d  Last: %c", nKeys, nLastKey);
    else
        n = wsprintf(buf, "Keys: %d  (Q quits)", nKeys);
    SetTextColor(hdc, RGB(128, 0, 128));
    TextOut(hdc, 8, 78, buf, n);

    /* click marker: small black square at the last left click */
    if (xMark >= 0) {
        SelectObject(hdc, GetStockObject(BLACK_BRUSH));
        Rectangle(hdc, xMark - 3, yMark - 3, xMark + 4, yMark + 4);
    }
}
