#include <windows.h>
#include <string.h>
static HFILE log;
static HWND frame;
static HANDLE instance;
static FARPROC proc;
static char before[80],after[80];
static volatile WORD cookie=0x2468;
static int phase,ticks;
static void mark(char *s){_lwrite(log,s,strlen(s));}
LONG FAR PASCAL __export Probe(void){return cookie;}
static int alive(void){return ((LONG (FAR PASCAL *)(void))proc)()==0x2468L;}
LRESULT CALLBACK __export Frame(HWND w,UINT m,WPARAM p,LPARAM l){
 if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);TextOut(dc,6,8,"Parent callback and launch test",31);EndPaint(w,&ps);return 0;}
 if(m==WM_TIMER){
  if(!phase){UINT x;phase=1;mark("PARENT-BEFORE\n");
   if(!alive()){mark("PARENT-CALLBACK-BAD\n");DestroyWindow(w);return 0;}
   x=WinExec("missing.exe",SW_SHOWNORMAL);if(x!=2){mark("MISSING-BAD\n");DestroyWindow(w);return 0;}mark("MISSING-OK\n");
   x=WinExec("wxbad.exe",SW_SHOWNORMAL);if(x!=11){mark("REJECT-BAD\n");DestroyWindow(w);return 0;}mark("REJECT-OK\n");
   GetModuleFileName(0,after,sizeof(after));if(strcmp(before,after)||!alive()){mark("PARENT-AFTER-REJECT-BAD\n");DestroyWindow(w);return 0;}mark("PARENT-AFTER-REJECT-OK\n");
   x=WinExec("wxchild.exe hello",SW_SHOWNOACTIVATE);
   if(x<=31){char b[40];wsprintf(b,"LAUNCH-ERROR %u\n",x);mark(b);DestroyWindow(w);return 0;}
   mark("LAUNCH-ACK\n");GetModuleFileName(0,after,sizeof(after));
   if(strcmp(before,after)||!alive()){mark("PARENT-AFTER-LAUNCH-BAD\n");DestroyWindow(w);return 0;}
   mark("PARENT-AFTER-LAUNCH-OK\n");
  }else if(GetNumTasks()==1){mark("CHILD-GONE\n");if(alive())mark("PARENT-FINAL-OK\n");DestroyWindow(w);}
  else if(++ticks>30){mark("CHILD-TIMEOUT\n");DestroyWindow(w);}
  return 0;
 }
 if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
 return DefWindowProc(w,m,p,l);
}
int PASCAL WinMain(HANDLE inst,HANDLE prev,LPSTR cmd,int show){
 WNDCLASS wc;MSG msg;(void)prev;(void)cmd;instance=inst;
 log=_lcreat("wxparent.log",0);mark("PARENT-START\n");GetModuleFileName(0,before,sizeof(before));
 proc=MakeProcInstance((FARPROC)Probe,inst);if(!proc)return 2;
 memset(&wc,0,sizeof(wc));wc.hInstance=inst;wc.lpfnWndProc=Frame;wc.lpszClassName="WXParent";wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
 if(!RegisterClass(&wc))return 3;
 frame=CreateWindow("WXParent","WinExec parent survives",WS_OVERLAPPEDWINDOW,8,6,280,110,0,0,inst,0);
 if(!frame)return 4;ShowWindow(frame,show);UpdateWindow(frame);SetTimer(frame,1,1500,0);
 while(GetMessage(&msg,0,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}
 FreeProcInstance(proc);mark("PARENT-EXIT\n");_lclose(log);return 0;
}
