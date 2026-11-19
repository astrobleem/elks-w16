/* Win3 DragObject protocol. DRAGINFO16 is 18 guest bytes; two records hold
 * current/previous target coordinates. Payload words are opaque guest data,
 * never native pointers. No screen-sized buffers or per-window state. */
#include <string.h>
#include "wapi.h"
#define DRAG_BYTES 18
#define WM_DROPOBJECT 0x022a
#define WM_QUERYDROPOBJECT 0x022b
#define WM_DRAGLOOP 0x022d
#define WM_DRAGSELECT 0x022e
#define WM_DRAGMOVE 0x022f
static u8 dragging;

/* Return the query's WORD result: 0 rejects, 1 uses the caller's cursor,
 * other nonzero values select a target-supplied cursor. */
static u16 drag_query(u16 hwnd,u16 seg,int sx,int sy,int depth)
{
 struct wnd *w=win_get(hwnd),*c;u16 child,next,reply;GR_WINDOW_ID frame;int ox,oy,n;long x,y;
 if(!w||depth>=MAXWIN||!w->visible||user_input_blocked(w))return 0;
 frame=w->frame;win_client_origin(w,&ox,&oy);x=(long)sx-ox;y=(long)sy-oy;
 if((long)x+w->cx<0||(long)y+w->cy<0||(long)x+w->cx>=w->w||(long)y+w->cy>=w->h)return 0;
 if(x>=0&&y>=0&&x<w->cw&&y<w->ch&&!(w->style&WS_MINIMIZE)){
  child=user_window_relative(hwnd,5);
  for(n=0;child&&n<MAXWIN;n++,child=next){
   c=win_get(child);if(!c)break;next=user_window_relative(child,2);
   reply=drag_query(child,seg,sx,sy,depth+1);if(reply)return reply;
   if(win_get(hwnd)!=w||w->frame!=frame)return 0;
  }
 }
 wr16(seg,2,hwnd);wr16(seg,10,(u16)x);wr16(seg,12,(u16)y);
 reply=(u16)win_send(w,WM_QUERYDROPOBJECT,!(x>=0&&y>=0&&x<w->cw&&y<w->ch),((u32)seg<<16));
 if(win_get(hwnd)!=w||w->frame!=frame||!win_get(rd16(seg,2)))reply=0;
 if(!reply){wr16(seg,2,0);wr16(seg,10,(u16)sx);wr16(seg,12,(u16)sy);}
 return reply;
}
u32 u_DragObject(u16 ss,u16 ap)
{
 struct wnd *source,*scope,*target;GR_WINDOW_ID source_frame,scope_frame,selected_frame=0;
 struct drag_cursor cursor;struct drag_event event;
 u16 root,from,h,seg,selected=0,reply=0,target_h;u32 ptr,result=0;u8 previous[DRAG_BYTES];int complete=0;
 if(ap>0xfff4u||dragging)return 0;
 root=AW(10);from=AW(8);scope=win_get(root);source=win_get(from);
 if(!scope||!source||user_input_blocked(source))return 0;
 source_frame=source->frame;scope_frame=scope->frame;
 h=global_alloc(DRAG_BYTES*2,0x42);if(!h)return 0;
 global_pointer(h,&ptr);seg=(u16)(ptr>>16);
 wr16(seg,0,from);wr16(seg,4,AW(6));wr16(seg,6,AW(2));wr16(seg,8,AW(4));
 if(!cursor_drag_start(&cursor,AW(0)))goto free_info;
 if(!user_drag_capture(from)){cursor_drag_end(&cursor);goto free_info;}
 dragging=1;
 while(user_drag_alive(from,source_frame)){
  if(!user_drag_next(from,source_frame,&event))break;
  if(win_get(root)!=scope||scope->frame!=scope_frame)break;
  fcopy_in(previous,seg,0,DRAG_BYTES);fcopy_out(seg,DRAG_BYTES,previous,DRAG_BYTES);
  wr16(seg,10,(u16)event.x);wr16(seg,12,(u16)event.y);wr16(seg,2,0);
  reply=drag_query(root,seg,event.x,event.y,0);
  if(!user_drag_alive(from,source_frame))break;
  source=win_get(from);win_send(source,WM_DRAGLOOP,reply!=0,ptr);
  if(!user_drag_alive(from,source_frame))break;
  cursor_drag_choose(&cursor,reply);
  target_h=reply?rd16(seg,2):0;target=win_get(target_h);
  if(target_h&&!target){target_h=0;reply=0;}
  if(selected!=target_h||(target&&target->frame!=selected_frame)){
   target=win_get(selected);
   if(target&&target->frame==selected_frame)win_send(target,WM_DRAGSELECT,0,ptr+DRAG_BYTES);
   selected=target_h;target=win_get(selected);selected_frame=target?target->frame:0;
   if(target)win_send(target,WM_DRAGSELECT,1,ptr);
  }else if(target)win_send(target,WM_DRAGMOVE,0,ptr);
  if(!user_drag_alive(from,source_frame))break;
  if(event.released){complete=1;break;}
 }
 /* Restore capture/cursor before WM_DROPOBJECT, matching Win3's callback order. */
 user_drag_release(from);cursor_drag_end(&cursor);
 target=win_get(selected);
 if(complete&&reply&&target&&target->frame==selected_frame)result=win_send(target,WM_DROPOBJECT,from,ptr);
 else if(target&&target->frame==selected_frame)win_send(target,WM_DRAGSELECT,0,rd16(seg,DRAG_BYTES+2)==selected?ptr+DRAG_BYTES:ptr);
 dragging=0;
free_info:
 global_unlock(h);global_free(h);return result;
}
/* Win3's basic beep policy has one enabled/disabled switch; this backend
 * requests the system bell, rather than claiming support for sound schemes. */
u32 u_MessageBeep(u16 ss,u16 ap)
{
 char value[8];int n;(void)ss;
 if(ap>0xfffeu)return 0;
 n=prof_get(prof_path(),"windows","Beep",value,sizeof(value));
 if(n<0||!(value[0]=='n'||value[0]=='N'||value[0]=='0'))GrBell();
 return 0; /* Win3 void Pascal */
}
