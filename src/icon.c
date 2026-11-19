/* Native Win3 icon/cursor storage: a lockable global block containing the
 * 12-byte CURSORICONINFO followed by word-aligned AND and device XOR rows.
 * This software device supports one plane, at 1 or 4 bits per pixel. */
#include <stdlib.h>
#include <string.h>
#include "w16.h"
#include "dib.h"
static u16 iw(const u8 *p){return p[0]|((u16)p[1]<<8);}
static void putw(u8 *p,u16 v){p[0]=(u8)v;p[1]=(u8)(v>>8);}
static int range(u16 s,u16 o,u16 n)
{return (s||o)&&(u32)o+n<=65536UL&&((u32)s<<4)+o+n<=0x100000UL;}
static int layout(const u8 *p,u16 *andlen,u16 *xorlen)
{
 u16 w=iw(p+4),h=iw(p+6),stride=iw(p+8);
 if(!w||w>32||!h||h>32||p[10]!=1||(p[11]!=1&&p[11]!=4)||stride!=(u16)(((w+15)/16)*2))return 0;
 *andlen=stride*h;*xorlen=((w*p[11]+15)/16)*2*h;return 1;
}
static void copy_far(u16 ds,u16 doff,u16 ss,u16 soff,u16 len)
{
 u8 buf[64];u16 n;
 while(len){n=len>sizeof(buf)?sizeof(buf):len;fcopy_in(buf,ss,soff,n);fcopy_out(ds,doff,buf,n);soff+=n;doff+=n;len-=n;}
}
/* Device colour indices use the conventional sixteen RGBI colours. */
static void color(u16 i,u8 *r,u8 *g,u8 *b)
{
 u8 lo=i&8?255:128;
 *r=i&4?lo:0;*g=i&2?lo:0;*b=i&1?lo:0;
 if(i==7)*r=*g=*b=192;
 if(i==8)*r=*g=*b=128;
}
static u8 nearest(const struct dib_rgb *rgb)
{
 u16 i,best=0;u32 distance=0xffffffffUL;
 for(i=0;i<16;i++){
  u8 r,g,b;long dr,dg,db;u32 d;color(i,&r,&g,&b);
  dr=(long)rgb->r-r;dg=(long)rgb->g-g;db=(long)rgb->b-b;d=dr*dr+dg*dg+db*db;
  if(d<distance){distance=d;best=i;}
 }
 return (u8)best;
}
u16 image_copy(u16 h)
{
 u16 out,n;u32 src,dst;
 if(!global_image(h,0)||!(n=(u16)global_size(h))||!global_pointer(h,&src)||!src)return 0;
 out=global_alloc(n,0x42);
 if(out){global_pointer(out,&dst);copy_far(dst>>16,0,src>>16,0,n);global_unlock(out);global_image(out,1);}
 global_unlock(h);return out;
}
u16 image_destroy(u16 h)
{return global_image(h,0)&&!global_free(h);}
/* Return an owned DIB snapshot. Re-reading native bytes each time preserves
 * changes applications make through GlobalLock; no stale raster cache. */
u8 *image_snapshot(u16 h,u16 *len)
{
 u32 ptr;u16 seg,a,xr,w,ht,bpp,as, xs,ds,off,x,y,i;u8 head[12],*d;
 if(!global_image(h,0)||!global_pointer(h,&ptr)||!ptr)return 0;
 seg=ptr>>16;
 if(global_size(h)<12)goto bad;
 fcopy_in(head,seg,0,12);
 if(!layout(head,&a,&xr)||12UL+a+xr>global_size(h))goto bad;
 w=iw(head+4);ht=iw(head+6);bpp=head[11];as=iw(head+8);xs=xr/ht;ds=((w*bpp+31)/32)*4;
 off=40+(bpp==1?8:64);*len=off+ds*ht+((w+31)/32)*4*ht;
 d=calloc(1,*len);if(!d)goto bad;
 d[0]=40;putw(d+4,w);putw(d+8,ht*2);d[12]=1;d[14]=(u8)bpp;
 for(i=0;i<(bpp==1?2:16);i++){
  u8 r,g,b;if(bpp==1)r=g=b=i?255:0;else color(i,&r,&g,&b);
  d[40+4*i]=b;d[41+4*i]=g;d[42+4*i]=r;
 }
 for(y=0;y<ht;y++){
  u16 row=ht-1-y;u8 *mask=d+off+ds*ht+((w+31)/32)*4*row;
  fcopy_in(mask,seg,12+as*y,as);
  fcopy_in(d+off+ds*row,seg,12+a+xs*y,xs);
  /* The bytes beyond the visible right edge remain padding, never pixels. */
  for(x=w;x<((w+31)/32)*32;x++)mask[x>>3]&=(u8)~(0x80>>(x&7));
 }
 global_unlock(h);return d;
bad:
 global_unlock(h);return 0;
}
/* Convert a Win3 DIB into native rows, replacing a supplied global block
 * only after the entire image is validated. A zero handle allocates anew. */
u16 image_from_dib(u16 h,const u8 *data,u16 len)
{
 struct dib_info info;struct dib_memory mem={data,len};u32 mask,ptr;
 u16 x,y,a,xr,as,xs,n,bpp;u8 *raw,map[16],indices[32];
 if(!dib_icon_open(&info,dib_read_memory,&mem,len,&mask))return 0;
 bpp=info.bpp;as=((info.width+15)/16)*2;xs=((info.width*bpp+15)/16)*2;
 a=as*info.height;xr=xs*info.height;n=12+a+xr;raw=calloc(1,n);if(!raw)return 0;
 putw(raw,info.width/2);putw(raw+2,info.height/2);
 putw(raw+4,info.width);putw(raw+6,info.height);putw(raw+8,as);raw[10]=1;raw[11]=(u8)bpp;
 for(x=0;x<info.colors;x++){
  if(bpp==1){
   /* Mono DDBs have fixed black/white pixels, not an attached palette. */
   const struct dib_rgb *c=&info.palette[x];
   if(c->r!=c->g||c->r!=c->b||(c->r!=0&&c->r!=255))goto bad;
   map[x]=c->r==255;
  }else map[x]=nearest(&info.palette[x]);
 }
 for(y=0;y<info.height;y++){
  const u8 *ar=data+mask+((info.width+31)/32)*4*(info.height-1-y);
  if(!dib_read_indices(&info,dib_read_memory,&mem,0,y,indices,info.width))goto bad;
  memcpy(raw+12+as*y,ar,as);
  for(x=0;x<info.width;x++){
   if(indices[x]>=info.colors)goto bad;
   if(bpp==1)raw[12+a+xs*y+(x>>3)]|=map[indices[x]]<<(7-(x&7));
   else raw[12+a+xs*y+(x>>1)]|=map[indices[x]]<<((x&1)?0:4);
  }
 }
 if(h){if(!global_realloc(h,n,0))goto bad;}
 else h=global_alloc(n,0x42);
 if(!h)goto bad;
 global_pointer(h,&ptr);fcopy_out(ptr>>16,0,raw,n);global_unlock(h);global_image(h,1);free(raw);return h;
bad:
 free(raw);return 0;
}
/* HINSTANCE, info far pointer, AND far pointer, XOR far pointer: 14 bytes. */
u32 u_CreateCursorIconIndirect(u16 ss,u16 ap)
{
 u8 p[12];u16 a,x,n,h;u32 ptr;
 if(ap>0xfff2u||(AW(12)&&(!w16_mod||AW(12)!=w16_mod->dgroup))||!range(AW(10),AW(8),12))return 0;
 fcopy_in(p,AW(10),AW(8),12);
 if(!layout(p,&a,&x)||!range(AW(6),AW(4),a)||!range(AW(2),AW(0),x))return 0;
 n=12+a+x;h=global_alloc(n,0x42);if(!h)return 0;
 global_pointer(h,&ptr);fcopy_out(ptr>>16,0,p,12);
 copy_far(ptr>>16,12,AW(6),AW(4),a);copy_far(ptr>>16,12+a,AW(2),AW(0),x);
 global_unlock(h);global_image(h,1);return h;
}
/* DumpIcon(raw far pointer, WORD far *size, far *AND, far *XOR), 16 bytes.
 * AX is AND bytes, DX is XOR bytes. Output pointers alias the input image. */
u32 u_DumpIcon(u16 ss,u16 ap)
{
 u8 p[12];u16 a,x,off,seg;
 if(ap>0xfff0u||!range(AW(10),AW(8),2))return 0;
 wr16(AW(10),AW(8),12);off=AW(12);seg=AW(14);
 if(!range(seg,off,12)||!range(AW(6),AW(4),4)||!range(AW(2),AW(0),4))return 0;
 fcopy_in(p,seg,off,12);
 if(!layout(p,&a,&x)||!range(seg,off,12+a+x))return 0;
 wr32(AW(6),AW(4),((u32)seg<<16)|(off+12));
 wr32(AW(2),AW(0),((u32)seg<<16)|(off+12+a));return a|((u32)x<<16);
}
u32 u_DestroyCursor(u16 ss,u16 ap)
{return ap<=0xfffeu?image_destroy(AW(0)):0;}
