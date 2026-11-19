#include <windows.h>
#include <string.h>
static HFILE log;
static int armed;
static void mark(char *s){_lwrite(log,s,strlen(s));}
LRESULT CALLBACK __export Child(HWND w,UINT m,WPARAM p,LPARAM l){
 if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);TextOut(dc,6,8,"hello - show state 4",20);EndPaint(w,&ps);mark("CHILD-PAINT\n");if(!armed){armed=1;SetTimer(w,1,6000,0);}return 0;}
 if(m==WM_TIMER){KillTimer(w,1);DestroyWindow(w);return 0;}
 if(m==WM_DESTROY){mark("CHILD-DESTROY\n");PostQuitMessage(0);return 0;}
 return DefWindowProc(w,m,p,l);
}
int PASCAL WinMain(HANDLE inst,HANDLE prev,LPSTR cmd,int show){WNDCLASS wc;MSG msg;HWND w;(void)prev;
 log=_lcreat("wxchild.log",0);mark("CHILD-START\n");
 if(lstrcmp(cmd,"hello")||show!=SW_SHOWNOACTIVATE){mark("CHILD-ARGS-BAD\n");_lclose(log);return 5;}
 mark("CHILD-ARGS-OK\n");memset(&wc,0,sizeof(wc));wc.hInstance=inst;wc.lpfnWndProc=Child;wc.lpszClassName="WXChild";wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
 if(!RegisterClass(&wc))return 2;w=CreateWindow("WXChild","WinExec child: hello",WS_OVERLAPPEDWINDOW,20,110,280,80,0,0,inst,0);if(!w)return 3;
 ShowWindow(w,show);UpdateWindow(w);while(GetMessage(&msg,0,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}
 mark("CHILD-EXIT\n");_lclose(log);return 0;
}
