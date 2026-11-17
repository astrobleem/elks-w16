/* A real single-task clipboard for movable global CF_TEXT, CF_OEMTEXT and
 * CF_DIB payloads. GDI-object/private/delayed-rendering formats are refused.
 * No system desktop clipboard bridge or implicit format conversion. */
#include "w16.h"
struct wnd;
extern struct wnd *win_get(u16 hwnd);
#define CLIP_FORMATS 3
static const u16 formats[CLIP_FORMATS]={1,7,8};
static u16 handles[CLIP_FORMATS],opened_by,owner;
static u8 opened;
static int format_slot(u16 format)
{unsigned i;for(i=0;i<CLIP_FORMATS;i++)if(formats[i]==format)return i;return -1;}
void clipboard_shutdown(void)
{
 unsigned i;for(i=0;i<CLIP_FORMATS;i++){
  if(handles[i])global_clipboard_release(handles[i]);
  handles[i]=0;
 }
 opened=0;opened_by=owner=0;
}
u32 u_OpenClipboard(u16 ss,u16 ap)
{
 if(opened||(AW(0)&&!win_get(AW(0))))return 0;
 opened=1;opened_by=AW(0);return 1;
}
u32 u_CloseClipboard(u16 ss,u16 ap)
{
 (void)ss;(void)ap;if(!opened)return 0;
 opened=0;opened_by=0;return 1;
}
u32 u_EmptyClipboard(u16 ss,u16 ap)
{
 unsigned i;(void)ss;(void)ap;if(!opened)return 0;
 for(i=0;i<CLIP_FORMATS;i++){
  if(handles[i])global_clipboard_release(handles[i]);
  handles[i]=0;
 }
 owner=opened_by;return 1;
}
u32 u_SetClipboardData(u16 ss,u16 ap)
{
 u16 h=AW(0);int i=format_slot(AW(2));
 if(!opened||!owner||i<0||!h)return 0;
 if(handles[i]==h)return h;
 if(!global_clipboard_take(h))return 0;
 if(handles[i])global_clipboard_release(handles[i]);
 handles[i]=h;return h;
}
u32 u_GetClipboardData(u16 ss,u16 ap)
{
 int i=format_slot(AW(0));return opened&&i>=0?handles[i]:0;
}
u32 u_GetPriorityClipboardFormat(u16 ss,u16 ap)
{
 u16 count=AW(0),off=AW(2),seg=AW(4),j;int i,any=0;
 for(i=0;i<CLIP_FORMATS;i++)if(handles[i])any=1;
 if(!any)return 0;
 if(!count||count>256||(!off&&!seg)||(u32)off+count*2UL>65536UL||((u32)seg<<4)+off+count*2UL>0x100000UL)return 0xffff;
 for(j=0;j<count;j++){u16 f=rd16(seg,off+j*2);i=format_slot(f);if(i>=0&&handles[i])return f;}
 return 0xffff;
}
