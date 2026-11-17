#include "wapi.h"
#define BAR 16
static u16 drag_hwnd;
static int drag_bar,drag_offset;
static i16 drag_pos;
static int enabled(struct wnd*w,int bar)
{return (w->style&(bar?WS_VSCROLL:WS_HSCROLL))!=0;}
/* Rectangles are in the window frame; client layout reserves these strips. */
static int geometry(struct wnd*w,int bar,int*x,int*y,int*len,int*thumb)
{
 long range=(long)w->scroll_max[bar]-w->scroll_min[bar],travel;
 if(!enabled(w,bar))return 0;
 *x=bar?w->cx+w->cw:w->cx;*y=bar?w->cy:w->cy+w->ch;
 *len=bar?w->ch:w->cw;
 if(*len<3*BAR)return 0;
 travel=*len-3*BAR;
 *thumb=BAR+(range>0?((long)w->scroll_pos[bar]-w->scroll_min[bar])*travel/range:0);
 return 1;
}
static void box(struct wnd*w,int bar,int x,int y,int at,int len,GR_COLOR c)
{gdi_fill(w->frame,c,x+(bar?0:at),y+(bar?at:0),bar?BAR:len,bar?len:BAR);}
void scrollbar_draw(struct wnd*w)
{
 int bar,x,y,len,thumb,k;GR_COLOR gray=GR_RGB(170,170,170),black=GR_RGB(0,0,0),white=GR_RGB(255,255,255);
 for(bar=0;bar<2;bar++)if(geometry(w,bar,&x,&y,&len,&thumb)){
  box(w,bar,x,y,0,len,gray);box(w,bar,x,y,1,BAR-2,white);
  box(w,bar,x,y,len-BAR+1,BAR-2,white);
  box(w,bar,x,y,thumb,BAR,white);
  gdi_fg(black);
  GrRect(w->frame,gdi_gc,x,y,bar?BAR:len,bar?len:BAR);
  for(k=0;k<4;k++){
   if(bar){
    GrLine(w->frame,gdi_gc,x+7-k,y+5+k,x+7+k,y+5+k);
    GrLine(w->frame,gdi_gc,x+7-k,y+len-6-k,x+7+k,y+len-6-k);
   }else{
    GrLine(w->frame,gdi_gc,x+5+k,y+7-k,x+5+k,y+7+k);
    GrLine(w->frame,gdi_gc,x+len-6-k,y+7-k,x+len-6-k,y+7+k);
   }
  }
 }
}
static void notify(struct wnd*w,int bar,u16 code,i16 pos)
{w16_post(w->hwnd,bar?WM_VSCROLL:WM_HSCROLL,code,(u16)pos);}
static void frame_point(struct wnd*w,int rx,int ry,int*x,int*y)
{int ox,oy;win_client_origin(w,&ox,&oy);*x=rx-ox+w->cx;*y=ry-oy+w->cy;}
/* kind: 0 motion, 1 left-down, 2 left-up. No synthetic changes to guest position:
 * the application handles the notification and calls SetScrollPos itself. */
int scrollbar_input(struct wnd*w,int kind,int rx,int ry)
{
 int bar,x,y,len,thumb,px,py,at,cross;long range,travel,v;
 if(drag_hwnd){
  w=win_get(drag_hwnd);
  if(!w){drag_hwnd=0;return 0;}
  bar=drag_bar;
  if(!geometry(w,bar,&x,&y,&len,&thumb)){drag_hwnd=0;return 1;}
  frame_point(w,rx,ry,&px,&py);at=(bar?py-y:px-x)-drag_offset-BAR;
  travel=len-3*BAR;if(at<0)at=0;if(at>travel)at=travel;
  range=(long)w->scroll_max[bar]-w->scroll_min[bar];
  v=w->scroll_min[bar]+(travel?((long)at*range+travel/2)/travel:0);
  drag_pos=(i16)v;
  notify(w,bar,kind==2?4:5,drag_pos);
  if(kind==2){drag_hwnd=0;notify(w,bar,8,0);}
  return 1;
 }
 if(!w||kind!=1)return 0;
 frame_point(w,rx,ry,&px,&py);
 for(bar=0;bar<2;bar++){
  if(!geometry(w,bar,&x,&y,&len,&thumb))continue;
  at=bar?py-y:px-x;cross=bar?px-x:py-y;
  if(at<0||at>=len||cross<0||cross>=BAR)continue;
  if(w->scroll_min[bar]>=w->scroll_max[bar])return 1;
  if(at>=thumb&&at<thumb+BAR){drag_hwnd=w->hwnd;drag_bar=bar;drag_offset=at-thumb;drag_pos=w->scroll_pos[bar];}
  else {notify(w,bar,at<BAR?0:at>=len-BAR?1:at<thumb?2:3,0);notify(w,bar,8,0);}
  return 1;
 }
 return 0;
}
void scrollbar_forget(u16 hwnd){if(drag_hwnd==hwnd)drag_hwnd=0;}
