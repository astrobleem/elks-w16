/* shared state between HELLO.C and PAINT.C (separate code segments) */
extern int nKeys, nLastKey, xMark, yMark, nPaints;
void FAR PaintScene(HWND hwnd, HDC hdc);
