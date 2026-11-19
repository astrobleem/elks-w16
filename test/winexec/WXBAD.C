#include <windows.h>
int PASCAL WinMain(HANDLE i,HANDLE p,LPSTR c,int s){(void)i;(void)p;(void)c;(void)s;return GetProcAddress(0,"unused")?1:0;}
