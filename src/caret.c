/* One task caret, in client device coordinates. Never changes a caller's DC.
 * Bitmap carets are not supported yet; reject them rather than invent pixels. */
#include "wapi.h"
static struct { u16 owner; i16 x,y,w,h; unsigned hide,painting;
    u8 drawn,gray; GR_GC_ID gc; u32 due; } caret;

static void caret_toggle(void)
{
    struct wnd *w=win_get(caret.owner);
    int x,y;
    if(!w || !caret.gc) return;
    if(caret.gray) {
        for(y=0;y<caret.h;y++) for(x=0;x<caret.w;x++)
            if((x+y)&1) GrPoint(w->client,caret.gc,caret.x+x,caret.y+y);
    } else GrFillRect(w->client,caret.gc,caret.x,caret.y,caret.w,caret.h);
    caret.drawn=!caret.drawn;
}
void caret_destroy(u16 hwnd)
{
    if(hwnd && hwnd!=caret.owner) return;
    if(caret.drawn) caret_toggle();
    if(caret.gc) GrDestroyGC(caret.gc);
    caret.gc=0;caret.owner=0;caret.drawn=0;caret.painting=0;
}
u32 u_CreateCaret(u16 ss,u16 ap)
{
    u16 hwnd=AW(6),bitmap=AW(4); i16 w=AW(2),h=AW(0);
    if(!win_get(hwnd)||bitmap>1||w<0||h<0) return 0;
    caret_destroy(0);
    caret.gc=GrNewGC();if(!caret.gc)return 0;
    GrSetGCMode(caret.gc,GR_MODE_INVERT);
    GrSetGCForeground(caret.gc,GR_RGB(255,255,255));
    caret.owner=hwnd;caret.x=caret.y=0;caret.w=w?w:1;caret.h=h?h:1;
    caret.gray=bitmap==1;caret.hide=1;caret.due=now_ms()+500;
    return 1;
}
u32 u_DestroyCaret(u16 ss,u16 ap)
{ (void)ss;(void)ap;caret_destroy(0);return 1; }
u32 u_SetCaretPos(u16 ss,u16 ap)
{
    if(!caret.owner)return 0;
    if(caret.drawn)caret_toggle();
    caret.x=(i16)AW(2);caret.y=(i16)AW(0);
    if(!caret.hide&&!caret.painting)caret_toggle();
    caret.due=now_ms()+500;return 1;
}
u32 u_HideCaret(u16 ss,u16 ap)
{
    if(!caret.owner||(AW(0)&&AW(0)!=caret.owner))return 0;
    if(caret.drawn)caret_toggle();
    if(caret.hide<65535u)caret.hide++;
    return 1;
}
u32 u_ShowCaret(u16 ss,u16 ap)
{
    if(!caret.owner||(AW(0)&&AW(0)!=caret.owner))return 0;
    if(caret.hide && !--caret.hide) {
        if(!caret.painting)caret_toggle();
        caret.due=now_ms()+500;
    }
    return 1;
}
void caret_paint(u16 hwnd,int begin)
{
    if(hwnd!=caret.owner)return;
    if(begin) {if(caret.drawn)caret_toggle();caret.painting++;}
    else if(caret.painting && !--caret.painting && !caret.hide) {
        caret_toggle();caret.due=now_ms()+500;
    }
}
u32 caret_wait(u32 now)
{
    long left;
    if(!caret.owner||caret.hide||caret.painting)return 0xfffffffful;
    left=(long)(caret.due-now);
    if(left<=0) {caret_toggle();caret.due=now+500;return 500;}
    return (u32)left;
}
