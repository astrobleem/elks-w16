/* Win16 screen-coordinate cursor restriction. Nano-X exposes cursor warping,
 * not a server confinement region: clamp every received pointer event and warp
 * the real pointer as well as the guest coordinates. Capture ownership is never
 * changed by clipping. RECT right/bottom are exclusive. */
#include "wapi.h"
static int clip_on,clip_left,clip_top,clip_right,clip_bottom;
static void pointer_bounds(int*x,int*y)
{
 GR_SCREEN_INFO si;int l=0,t=0,r,b;
 GrGetScreenInfo(&si);r=si.cols;b=si.rows;
 if(clip_on){l=clip_left;t=clip_top;r=clip_right;b=clip_bottom;}
 if(*x<l)*x=l;if(*x>=r)*x=r-1;
 if(*y<t)*y=t;if(*y>=b)*y=b-1;
}
void pointer_event(GR_EVENT*ev)
{
 int x,y,oldx,oldy;
 if(ev->type==GR_EVENT_TYPE_BUTTON_DOWN||ev->type==GR_EVENT_TYPE_BUTTON_UP){
  oldx=x=ev->button.rootx;oldy=y=ev->button.rooty;pointer_bounds(&x,&y);
  ev->button.x+=x-oldx;ev->button.y+=y-oldy;ev->button.rootx=x;ev->button.rooty=y;
 }else if(ev->type==GR_EVENT_TYPE_MOUSE_POSITION){
  oldx=x=ev->mouse.rootx;oldy=y=ev->mouse.rooty;pointer_bounds(&x,&y);
  ev->mouse.x+=x-oldx;ev->mouse.y+=y-oldy;ev->mouse.rootx=x;ev->mouse.rooty=y;
 }else return;
 if(x!=oldx||y!=oldy)GrMoveCursor(x,y);
}
u32 u_ClipCursor(u16 ss,u16 ap)
{
 u16 s=AW(2),o=AW(0);GR_SCREEN_INFO si;int l,t,r,b,x,y;
 GrGetScreenInfo(&si);
 if(s||o){
  if(!s||(u32)o+8>65536UL||((u32)s<<4)+o+8>0x100000UL)return 0;
  l=(i16)rd16(s,o);t=(i16)rd16(s,o+2);r=(i16)rd16(s,o+4);b=(i16)rd16(s,o+6);
  if(l<0)l=0;if(t<0)t=0;if(r>si.cols)r=si.cols;if(b>si.rows)b=si.rows;
  if(l>=r||t>=b)return 0;
  clip_left=l;clip_top=t;clip_right=r;clip_bottom=b;clip_on=1;
 }else clip_on=0;
 x=si.xpos;y=si.ypos;pointer_bounds(&x,&y);
 if(x!=si.xpos||y!=si.ypos)GrMoveCursor(x,y);
 return 1;
}
u32 u_GetCursorPos(u16 ss,u16 ap)
{
 GR_SCREEN_INFO si;int x,y;u16 s=AW(2),o=AW(0);
 if(!s||(u32)o+4>65536UL||((u32)s<<4)+o+4>0x100000UL)return 0;
 GrGetScreenInfo(&si);x=si.xpos;y=si.ypos;pointer_bounds(&x,&y);
 if(x!=si.xpos||y!=si.ypos)GrMoveCursor(x,y);
 wr16(s,o,(u16)x);wr16(s,o+2,(u16)y);return 1;
}
u32 u_SetCursorPos(u16 ss,u16 ap)
{
 int x=(i16)AW(2),y=(i16)AW(0);pointer_bounds(&x,&y);GrMoveCursor(x,y);return 1;
}
