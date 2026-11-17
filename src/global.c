/* Single-segment Win16 global storage. Movable handles are stable; memory is
 * never implicitly compacted/discarded. Requests over 65520 bytes fail. */
#include "w16.h"
#define GLOBAL_MAX 48
#define GLOBAL_BASE 0xf000
#define GM_MOVEABLE 2
#define GM_ZEROINIT 0x40
struct global_block { u16 seg, bytes, locks, flags; u8 used, clipboard_owned; };
static struct global_block blocks[GLOBAL_MAX];
/* GlobalLRUNewest is an advisory ordering request, not an allocation or lock.
 * Maintain the relative order without discarding data behind the guest. */
static u8 lru[GLOBAL_MAX], nlru;
static void mark_newest(u8 slot)
{
    u8 n;
    for(n=0;n<nlru && lru[n]!=slot;n++) ;
    if(n==nlru) nlru++;
    else for(;n+1<nlru;n++) lru[n]=lru[n+1];
    lru[nlru-1]=slot;
}
static int lookup(u16 h)
{
    int i;
    for(i=0;i<GLOBAL_MAX;i++)
        if(blocks[i].used &&
           h==((blocks[i].flags&GM_MOVEABLE)?GLOBAL_BASE+2*i:blocks[i].seg))
            return i;
    return -1;
}
static u16 handle(int i)
{ return blocks[i].flags&GM_MOVEABLE?GLOBAL_BASE+2*i:blocks[i].seg; }
int global_pointer(u16 h,u32 *ptr)
{
    int i=lookup(h);
    if(i<0) return 0;
    *ptr=0;
    if(blocks[i].seg) {
        if(blocks[i].flags&GM_MOVEABLE) {
            if(blocks[i].locks==0xffff) return 1;
            blocks[i].locks++;
        }
        *ptr=(u32)blocks[i].seg<<16;
    }
    return 1;
}
u32 k_GlobalLRUNewest(u16 ss,u16 ap)
{
    u16 h=AW(0);
    int i=lookup(h);
    if(i<0) return 0;
    mark_newest((u8)i);
    return h;
}
u32 global_unlock(u16 h)
{
    int i=lookup(h);
    if(i<0 || !(blocks[i].flags&GM_MOVEABLE) || !blocks[i].locks) return 0;
    return --blocks[i].locks!=0;
}
u32 global_size(u16 h)
{ int i=lookup(h); return i<0?0:blocks[i].bytes; }
u32 k_GlobalAlloc(u16 ss,u16 ap)
{
    u32 bytes=AL(0);
    u16 flags=AW(4),seg=0,n;
    int i;
    if(bytes>0xfff0UL) return 0;
    for(i=0;i<GLOBAL_MAX && blocks[i].used;i++);
    if(i==GLOBAL_MAX) return 0;
    if(!bytes && !(flags&GM_MOVEABLE)) bytes=1;
    n=(u16)((bytes+15)&~15UL);
    if(n && seg_alloc(n>>4,&seg)) return 0;
    if(n && (flags&GM_ZEROINIT)) fzero(seg,0,n);
    blocks[i].used=1;blocks[i].seg=seg;blocks[i].bytes=n;
    blocks[i].flags=flags;blocks[i].locks=0;blocks[i].clipboard_owned=0;
    mark_newest((u8)i);
    return handle(i);
}
u32 k_GlobalFree(u16 ss,u16 ap)
{
    u16 h=AW(0);
    int i=lookup(h);
    if(!h) return 0;
    if(i<0 || blocks[i].clipboard_owned) return h;
    if(blocks[i].seg) seg_free(blocks[i].seg);
    blocks[i].used=0; return 0;
}
u32 k_GlobalReAlloc(u16 ss,u16 ap)
{
    u16 flags=AW(0),h=AW(6),n,seg,copy,pos,part;
    u32 bytes=AL(2);
    int i=lookup(h);
    if(i<0 || blocks[i].clipboard_owned || bytes>0xfff0UL) return 0;
    /* GMEM_MODIFY changes allocation flags, not size or identity. */
    if(flags&0x80) {
        blocks[i].flags=(blocks[i].flags&GM_MOVEABLE)|(flags&~(GM_MOVEABLE|0x80));
        return h;
    }
    n=(u16)((bytes+15)&~15UL);
    if(n && n<=blocks[i].bytes) return h; /* retain allocation */
    if(!(blocks[i].flags&GM_MOVEABLE) || blocks[i].locks) return 0;
    seg=0;
    if(n && seg_alloc(n>>4,&seg)) return 0;
    copy=n<blocks[i].bytes?n:blocks[i].bytes;
    for(pos=0;pos<copy;pos+=part) {
        part=copy-pos; if(part>W16_SCRATCH) part=W16_SCRATCH;
        fcopy_in(w16_scratch,blocks[i].seg,pos,part);
        fcopy_out(seg,pos,w16_scratch,part);
    }
    if(n>copy && (flags&GM_ZEROINIT)) fzero(seg,copy,n-copy);
    if(blocks[i].seg) seg_free(blocks[i].seg);
    blocks[i].seg=seg;blocks[i].bytes=n;
    return h;
}
void global_shutdown(void)
{
    int i;
    nlru=0;
    for(i=0;i<GLOBAL_MAX;i++) {
        if(blocks[i].used && blocks[i].seg) seg_free(blocks[i].seg);
        blocks[i].used=0;
    }
}

/* Ownership is transferred, not copied: returned clipboard handles retain
 * their identity. Caller Free/ReAlloc must not recycle clipboard storage. */
int global_clipboard_take(u16 h)
{
 int i=lookup(h);
 if(i<0||!(blocks[i].flags&GM_MOVEABLE)||!blocks[i].seg||!blocks[i].bytes||blocks[i].locks||blocks[i].clipboard_owned)return 0;
 blocks[i].clipboard_owned=1;return 1;
}
void global_clipboard_release(u16 h)
{
 int i=lookup(h);if(i<0||!blocks[i].clipboard_owned)return;
 if(blocks[i].seg)seg_free(blocks[i].seg);
 blocks[i].used=0;blocks[i].clipboard_owned=0;
}
