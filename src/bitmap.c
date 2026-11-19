/* Bounded far-owned bitmaps and memory DCs. Unsupported raster operations
 * fail; screen capture commits only after a complete negotiated snapshot. */
#include <string.h>
#include <stdlib.h>
#include "wapi.h"
#include "polygon.h"
#include "dib.h"
#define BBASE 0x0a00
#define DBASE 0x0c00
#define NB 16
#define ND 8
#define DEFAULT_BITMAP 0x0aff
#define BITMAP_BANK_BYTES 16384UL
#define BITMAP_BANKS 10
struct bitmap {u16 w,h,dimx,dimy,bank[BITMAP_BANKS],stride;u8 used,bpp;struct dib_rgb palette[16];};
static struct bitmap bitmaps[NB];
struct memory_dc {u16 bitmap,pen,brush,font,palette,bkmode,textalign;u8 used,stretchmode,rop2;i16 brushx,brushy,curx,cury,wx,wy,vx,vy;u32 textcolor,bkcolor;};
static struct memory_dc memory[ND];
#define MEMORY_SAVE_DEPTH 4
static struct memory_dc memory_saved[ND][MEMORY_SAVE_DEPTH];
static u8 memory_saved_count[ND];
static int bi(u16 h){u16 n=(h-BBASE)/2;return h>=BBASE&&!(h&1)&&n<NB&&bitmaps[n].used?n:-1;}
static int di(u16 h){u16 n=(h-DBASE)/2;return h>=DBASE&&!(h&1)&&n<ND&&memory[n].used?n:-1;}
static int bitmap_referenced(u16 h,int exclude)
{
 int i,j;
 if(h==DEFAULT_BITMAP)return 0;
 for(i=0;i<ND;i++)if(i!=exclude&&memory[i].used){
  if(memory[i].bitmap==h)return 1;
  for(j=0;j<memory_saved_count[i];j++)if(memory_saved[i][j].bitmap==h)return 1;
 }
 return 0;
}
u32 bitmap_origin(u16 h,int set,int viewport,i16 x,i16 y)
{
 int m=di(h);i16 *px,*py;u32 old;if(m<0)return 0;
 px=viewport?&memory[m].vx:&memory[m].wx;py=viewport?&memory[m].vy:&memory[m].wy;
 old=(u16)*px|((u32)(u16)*py<<16);if(set){*px=x;*py=y;}return old;
}
static int memory_point(int m,int*x,int*y)
{
 long a=(long)*x-memory[m].wx+memory[m].vx,b=(long)*y-memory[m].wy+memory[m].vy;
 if(a< -32768L||a>32767L||b< -32768L||b>32767L)return 0;
 *x=a;*y=b;return 1;
}
u16 bitmap_savedc(u16 h)
{
 int i=di(h),n;if(i<0)return 0;n=memory_saved_count[i];
 if(n==MEMORY_SAVE_DEPTH)return 0;
 memory_saved[i][n]=memory[i];memory_saved_count[i]++;return n+1;
}
u16 bitmap_restoredc(u16 h,i16 level)
{
 int i=di(h),target;if(i<0)return 0;
 target=level<0?(long)memory_saved_count[i]+level:(long)level-1;
 if(!level||target<0||target>=memory_saved_count[i])return 0;
 memory[i]=memory_saved[i][target];memory_saved_count[i]=target;return 1;
}
u16 bitmap_rop2(u16 h,u16 mode)
{
 int i=di(h);u16 old;if(i<0)return 0;old=memory[i].rop2;
 if(mode>=1&&mode<=16)memory[i].rop2=mode;return old;
}
u16 bitmap_object(u16 h,u16 type,u16 object)
{
 int i=di(h);u16 *slot,old;if(i<0)return 0;
 slot=type==1?&memory[i].pen:type==2?&memory[i].brush:type==3?&memory[i].font:type==5?&memory[i].palette:0;
 if(!slot)return 0;old=*slot;if(object)*slot=object;return old;
}
int bitmap_object_used(u16 object)
{
 int i,j;
 for(i=0;i<ND;i++)if(memory[i].used){
  if(memory[i].pen==object||memory[i].brush==object||memory[i].font==object||memory[i].palette==object)return 1;
  for(j=0;j<memory_saved_count[i];j++)
   if(memory_saved[i][j].pen==object||memory_saved[i][j].brush==object||memory_saved[i][j].font==object||memory_saved[i][j].palette==object)return 1;
 }
 return 0;
}
static void release_pixels(struct bitmap*b)
{int j;for(j=0;j<BITMAP_BANKS;j++)if(b->bank[j]){seg_free(b->bank[j]);b->bank[j]=0;}}
static void release_bitmap(int i){release_pixels(&bitmaps[i]);memset(&bitmaps[i],0,sizeof(bitmaps[i]));}
static int allocate_pixels(struct bitmap*b,u32 bytes)
{
 int j;u16 n,seg;
 if(!bytes||bytes>BITMAP_BANKS*BITMAP_BANK_BYTES)return 0;
 for(j=0;bytes;j++){
  n=bytes>BITMAP_BANK_BYTES?BITMAP_BANK_BYTES:(u16)bytes;
  if(seg_alloc((n+15)/16,&seg)){release_pixels(b);return 0;}
  b->bank[j]=seg;
  fzero(b->bank[j],0,n);bytes-=n;
 }
 return 1;
}
static void bitmap_write(struct bitmap*b,u32 at,const u8*src,u16 n)
{
 u16 off,part;unsigned bank;
 while(n){
  bank=(unsigned)(at/BITMAP_BANK_BYTES);off=(u16)(at%BITMAP_BANK_BYTES);
  part=(u16)(BITMAP_BANK_BYTES-off);if(part>n)part=n;
  fcopy_out(b->bank[bank],off,src,part);src+=part;at+=part;n-=part;
 }
}
void bitmap_shutdown(void){int i;for(i=0;i<NB;i++)release_bitmap(i);memset(memory,0,sizeof(memory));memset(memory_saved_count,0,sizeof(memory_saved_count));}
/* Clipboard transfer/release is refused while a live or saved DC selects it. */
int bitmap_clipboard(u16 h,int release)
{int i=bi(h);if(i<0||bitmap_referenced(h,-1))return 0;if(release)release_bitmap(i);return 1;}
int bitmap_delete(u16 h){int i=bi(h);if(i<0)return -1;if(bitmap_referenced(h,-1))return 0;release_bitmap(i);return 1;}
int bitmap_deletedc(u16 h){int i=di(h);if(i<0)return -1;memset(&memory[i],0,sizeof(memory[i]));memory_saved_count[i]=0;return 1;}
u16 bitmap_select(u16 dc,u16 h){int i=di(dc),j=bi(h);u16 old;if(i<0||(j<0&&h!=DEFAULT_BITMAP))return 0;if(bitmap_referenced(h,i))return 0;old=memory[i].bitmap;memory[i].bitmap=h;return old;}
u32 g_CreateCompatibleDC(u16 ss,u16 ap){u16 src=AW(0);int i;if(src&&!dc_get(src)&&di(src)<0)return 0;for(i=0;i<ND&&memory[i].used;i++);if(i==ND)return 0;memory_saved_count[i]=0;memory[i].wx=memory[i].wy=memory[i].vx=memory[i].vy=0;memory[i].used=1;memory[i].bkmode=2;memory[i].textalign=0;memory[i].rop2=13;memory[i].stretchmode=1;memory[i].pen=0x030e;memory[i].brush=0x0300;memory[i].font=0x031a;memory[i].palette=0x031e;memory[i].bitmap=DEFAULT_BITMAP;memory[i].bkcolor=0xffffffUL;return DBASE+2*i;}
/* All owned pixels use bounded far storage, never the server near heap. */
static int allocate_bitmap(u16 w,u16 h,u16 bpp)
{
 int i;u16 stride=((w*bpp+15)/16)*2;
 if(!w||!h||w>640||h>480||(bpp!=1&&bpp!=4))return -1;
 for(i=0;i<NB&&bitmaps[i].used;i++);if(i==NB)return -1;
 if(!allocate_pixels(&bitmaps[i],(u32)stride*h))return -1;
 bitmaps[i].w=w;bitmaps[i].h=h;bitmaps[i].stride=stride;bitmaps[i].bpp=bpp;
 return i;
}
static u8 bitmap_index(struct bitmap*b,u16 x,u16 y)
{
 u32 at=(u32)y*b->stride+(b->bpp==1?x/8:x/2);
 u8 v=rd8(b->bank[(unsigned)(at/BITMAP_BANK_BYTES)],(u16)(at%BITMAP_BANK_BYTES));
 return b->bpp==1?(v>>(7-(x&7)))&1:(v>>((x&1)?0:4))&15;
}
static void bitmap_put(struct bitmap*b,u16 x,u16 y,u8 index)
{
 u32 at=(u32)y*b->stride+(b->bpp==1?x/8:x/2);
 u16 seg=b->bank[(unsigned)(at/BITMAP_BANK_BYTES)],off=(u16)(at%BITMAP_BANK_BYTES);
 u8 shift=b->bpp==1?7-(x&7):(x&1)?0:4;
 u8 mask=(b->bpp==1?1:15)<<shift;
 wr8(seg,off,(rd8(seg,off)&~mask)|(index<<shift));
}
static u32 bitmap_rgb(struct bitmap*b,u8 index)
{struct dib_rgb*c=&b->palette[index];return b->bpp==1?(index?0xffffffUL:0):(u32)c->r|((u32)c->g<<8)|((u32)c->b<<16);}
static u8 bitmap_nearest(struct bitmap*b,u32 rgb)
{
 long closest=0x7fffffffL,dr,dg,db,dist;u32 c;u8 i,best=0;
 for(i=0;i<(b->bpp==1?2:16);i++){
  c=bitmap_rgb(b,i);dr=(long)(rgb&255)-(long)(c&255);dg=(long)((rgb>>8)&255)-(long)((c>>8)&255);db=(long)((rgb>>16)&255)-(long)((c>>16)&255);
  dist=dr*dr+dg*dg+db*db;if(dist<closest){closest=dist;best=i;}
 }return best;
}
int bitmap_patblt(u16 h,i16 x,i16 y,i16 w,i16 height,u32 rop,u32 color,u16 style,int nullbrush)
{
 int m=di(h),i,row,col;u8 index=0,mask;struct bitmap*b;
 long l=x,t=y,r,bot,ww=w,hh=height;
 if(m<0||(i=bi(memory[m].bitmap))<0)return 0;b=&bitmaps[i];
 if(rop!=0x00f00021UL&&rop!=0x005a0049UL&&rop!=0x00550009UL&&rop!=0x00000042UL&&rop!=0x00ff0062UL)return 0;
 if(rop==0x00f00021UL||rop==0x005a0049UL){
  if(nullbrush)return 1;if(style!=0)return 0;index=bitmap_nearest(b,color);
 }
 l-=memory[m].wx;l+=memory[m].vx;t-=memory[m].wy;t+=memory[m].vy;
 if(ww<0){l+=ww;ww=-ww;}if(hh<0){t+=hh;hh=-hh;}r=l+ww;bot=t+hh;
 if(l<0)l=0;if(t<0)t=0;if(r>b->w)r=b->w;if(bot>b->h)bot=b->h;
 mask=b->bpp==1?1:15;
 if(rop==0x00ff0062UL)index=mask;
 for(row=t;row<bot;row++)for(col=l;col<r;col++){
  u8 out=index;
  if(rop==0x00550009UL)out=bitmap_index(b,col,row)^mask;
  else if(rop==0x005a0049UL)out=bitmap_index(b,col,row)^index;
  bitmap_put(b,col,row,out);
 }
 return 1;
}
static void memory_rop_pixel(struct bitmap*b,int x,int y,u8 source,u8 rop)
{
 u8 dst,result=0,bit,table=rop-1;
 if(x<0||y<0||x>=b->w||y>=b->h)return;
 dst=bitmap_index(b,x,y);
 for(bit=0;bit<(b->bpp==1?1:4);bit++)
  result|=((table>>((((source>>bit)&1)<<1)|((dst>>bit)&1)))&1)<<bit;
 bitmap_put(b,x,y,result);
}
u32 bitmap_moveto(u16 h,i16 x,i16 y)
{
 int m=di(h);u32 old;if(m<0)return 0;
 old=(u16)memory[m].curx|((u32)(u16)memory[m].cury<<16);
 memory[m].curx=x;memory[m].cury=y;return old;
}
int bitmap_lineto(u16 h,i16 x,i16 y,u32 color,u16 style,u16 width,int nullpen)
{
 int m=di(h),i,ax,ay,bx=x,by=y;long xx,yy,dx,dy,err,e2,sx,sy;u8 index;struct bitmap*b;
 if(m<0||(i=bi(memory[m].bitmap))<0)return 0;
 if(!nullpen&&(style!=0||width>1))return 0;
 b=&bitmaps[i];index=bitmap_nearest(b,color);
 ax=memory[m].curx;ay=memory[m].cury;
 if(!memory_point(m,&ax,&ay)||!memory_point(m,&bx,&by))return 0;
 memory[m].curx=x;memory[m].cury=y;x=bx;y=by;
 xx=ax;yy=ay;dx=(long)x-xx;dy=(long)y-yy;
 sx=dx<0?-1:1;sy=dy<0?-1:1;if(dx<0)dx=-dx;if(dy>0)dy=-dy;err=dx+dy;
 if(!nullpen)while(xx!=x||yy!=y){
  memory_rop_pixel(b,(int)xx,(int)yy,index,memory[m].rop2);
  e2=2*err;if(e2>=dy){err+=dy;xx+=sx;}if(e2<=dx){err+=dx;yy+=sy;}
 }
 return 1;
}
struct polygon_memory {struct bitmap *b;u8 pen,brush,rop,gray;i16 brushx,brushy;};
static void polygon_memory_span(void *ctx,int x,int y,int width,int kind)
{
 struct polygon_memory *s=ctx;int end=x+width;
 for(;x<end;x++){
  u8 color=kind==2?s->pen:s->brush;
  if(kind==1&&s->gray)color=(s->gray>>((((unsigned)y-s->brushy)&1)*2+(((unsigned)x-s->brushx)&1)))&1;
  memory_rop_pixel(s->b,x,y,color,s->rop);
 }
}
int bitmap_polygon(u16 h,struct poly_point*p,int n,u32 pen,int stroke,u32 brush,int nb)
{
 int m=di(h),b,i,count=n<0?-n:n;struct polygon_memory out;
 if(m<0||(b=bi(memory[m].bitmap))<0)return 0;
 for(i=0;i<(count?count:2);i++)if(!memory_point(m,&p[i].x,&p[i].y))return 0;
 out.b=&bitmaps[b];out.pen=bitmap_nearest(out.b,pen);
 out.brush=bitmap_nearest(out.b,brush);out.rop=memory[m].rop2;
 /* Stock gray brushes realize as 2x2 stipples on monochrome targets, not
  * nearest black/white. Paintbrush uses DKGRAY_BRUSH for its spray mask. */
 out.gray=0;out.brushx=memory[m].brushx;out.brushy=memory[m].brushy;
 if(out.b->bpp==1)switch(memory[m].brush){
  case 0x0302:out.gray=13;break; /* LTGRAY: rows 01,11 (LSB first) */
  case 0x0304:out.gray=6;break;  /* GRAY:   rows 10,01 */
  case 0x0306:out.gray=1;break;  /* DKGRAY: rows 01,00 */
 }
 return n?polygon_raster(p,n,out.b->w,out.b->h,stroke,!nb,polygon_memory_span,&out):

          oval_raster(p,out.b->w,out.b->h,stroke,!nb,polygon_memory_span,&out);
}
int bitmap_rectangle(u16 h,i16 l,i16 t,i16 r,i16 bot,u32 pen,u16 ps,u16 pw,int np,u32 brush,u16 bs,int nb)
{
 int m=di(h),i,x,y,x0,y0,x1,y1;u8 pi,bindex;struct bitmap*b;
 if(m<0||(i=bi(memory[m].bitmap))<0||r<=l||bot<=t)return 0;
 if((!np&&(ps!=0||pw>1))||(!nb&&bs!=0))return 0;
 x0=l;y0=t;x1=r;y1=bot;
 if(!memory_point(m,&x0,&y0)||!memory_point(m,&x1,&y1))return 0;
 l=x0;t=y0;r=x1;bot=y1;
 b=&bitmaps[i];pi=bitmap_nearest(b,pen);bindex=bitmap_nearest(b,brush);
 x0=l<0?0:l;y0=t<0?0:t;x1=(long)r>b->w?b->w:r;y1=(long)bot>b->h?b->h:bot;
 for(y=y0;y<y1;y++)for(x=x0;x<x1;x++){
  int edge=x==l||x==(long)r-1||y==t||y==(long)bot-1;
  if(!np&&edge)memory_rop_pixel(b,x,y,pi,memory[m].rop2);
  else if(!nb)memory_rop_pixel(b,x,y,bindex,memory[m].rop2);
 }
 return 1;
}
u32 bitmap_getpixel(u16 h,i16 x,i16 y)
{
 int m=di(h),i;if(m<0||(i=bi(memory[m].bitmap))<0||x<0||y<0||x>=bitmaps[i].w||y>=bitmaps[i].h)return 0xffffffffUL;
 return bitmap_rgb(&bitmaps[i],bitmap_index(&bitmaps[i],x,y));
}
u32 bitmap_setpixel(u16 h,i16 x,i16 y,u32 rgb)
{
 int m=di(h),i;u8 index;
 if(m<0||(i=bi(memory[m].bitmap))<0||x<0||y<0||x>=bitmaps[i].w||y>=bitmaps[i].h||(rgb&0xff000000UL))return 0xffffffffUL;
 index=bitmap_nearest(&bitmaps[i],rgb);bitmap_put(&bitmaps[i],x,y,index);return bitmap_rgb(&bitmaps[i],index);
}
u32 bitmap_getcolor(u16 h,int bk)
{
 int m=di(h);if(m<0)return 0xffffffffUL;
 return bk?memory[m].bkcolor:memory[m].textcolor;
}
u32 bitmap_setcolor(u16 h,u32 rgb,int bk)
{
 int m=di(h);u32 old;if(m<0||(rgb&0xff000000UL))return 0xffffffffUL;
 old=bk?memory[m].bkcolor:memory[m].textcolor;if(bk)memory[m].bkcolor=rgb;else memory[m].textcolor=rgb;return old;
}
struct resource {u32 off;u16 len;};
static int read_bitmap(void *ctx,u32 off,void *dst,u16 n){struct resource*r=ctx;if(off>r->len||n>r->len-off||r->off+off<r->off)return 0;return ne_read(w16_mod,r->off+off,dst,n)==0;}
u32 u_LoadBitmap(u16 ss,u16 ap)
{
 struct resource r;struct dib_info d;char name[34];u16 id,x,y,n,j,bpp;u8 pixels[32];int i;
 if(!w16_mod||AW(4)!=w16_mod->dgroup)return 0;
 res_name(AW(0),AW(2),name,&id);
 if(ne_res_find(w16_mod,2,name[0]?name:0,id,&r.off,&r.len)||!dib_open(&d,read_bitmap,&r,r.len))return 0;
 bpp=d.bpp;
 if(bpp==1){
  int black0=!d.palette[0].r&&!d.palette[0].g&&!d.palette[0].b;
  int white1=d.colors==2&&d.palette[1].r==255&&d.palette[1].g==255&&d.palette[1].b==255;
  /* A colored 1bpp DIB is not a monochrome DDB. Preserve its RGBs in
   * color storage; only black/white resources use text/background expansion. */
  if(!(black0&&white1))bpp=4;
 }
 i=allocate_bitmap(d.width,d.height,bpp);if(i<0)return 0;
 memcpy(bitmaps[i].palette,d.palette,d.colors*sizeof(d.palette[0]));
 for(y=0;y<d.height;y++)for(x=0;x<d.width;x+=n){
  n=d.width-x;if(n>32)n=32;if(!dib_read_indices(&d,read_bitmap,&r,x,y,pixels,n)){release_bitmap(i);return 0;}
  for(j=0;j<n;j++)bitmap_put(&bitmaps[i],x+j,y,pixels[j]);
 }
 bitmaps[i].used=1;return BBASE+2*i;
}
/* Clip both rectangles together in widened arithmetic, preserving source phase. */
static int clip_axis(long *dst,long *src,long *n,long limit,long slimit){long q;if(*dst<0){q=-*dst;*dst=0;*src+=q;*n-=q;}if(*src<0){q=-*src;*src=0;*dst+=q;*n-=q;}if(*n>limit-*dst)*n=limit-*dst;if(*n>slimit-*src)*n=slimit-*src;return *n>0;}
/* Prevalidated clipped memory copy. Direction handles same-bitmap overlap;
 * conversion tables are completed before the first destination mutation. */
/* Keep the operation word-sized: ia16 -Os miscompiles the narrowed
 * high-word argument (u8)(rop >> 16), passing the low ROP word instead. */
static int memory_copy(struct bitmap*d,int dm,int dx,int dy,struct bitmap*s,int sm,int sx,int sy,int w,int h,u16 rop)
{
 u8 map[16],i,index;int row,col,yy,xx;u32 rgb;struct dc pattern;
 if(rop==0xe2){
  memset(&pattern,0,sizeof pattern);pattern.brush=memory[dm].brush;
  pattern.brushx=memory[dm].brushx;pattern.brushy=memory[dm].brushy;
  pattern.textcolor=memory[dm].textcolor;pattern.bkcolor=memory[dm].bkcolor;
  if(!gdi_pattern_color(&pattern,dx,dy,&rgb))return 0;
 }
 for(i=0;i<(s->bpp==1?2:16);i++){
  if(d==s|| (s->bpp==1&&d->bpp==1))map[i]=i;
  else if(d->bpp==1)map[i]=bitmap_rgb(s,i)==memory[sm].bkcolor;
  else{rgb=s->bpp==1?(i?memory[dm].bkcolor:memory[dm].textcolor):bitmap_rgb(s,i);map[i]=bitmap_nearest(d,rgb);}
 }
 for(row=0;row<h;row++){
  yy=d==s&&dy>sy?h-1-row:row;
  for(col=0;col<w;col++){
   xx=d==s&&dy==sy&&dx>sx?w-1-col:col;
   index=map[bitmap_index(s,sx+xx,sy+yy)];
   if(rop==0x22)index=(~index)&bitmap_index(d,dx+xx,dy+yy);
   else if(rop==0x66)index^=bitmap_index(d,dx+xx,dy+yy);
   else if(rop==0xe2){
    u8 dest=bitmap_index(d,dx+xx,dy+yy);
    gdi_pattern_color(&pattern,dx+xx,dy+yy,&rgb);
    index=(index&bitmap_nearest(d,rgb))|((~index)&dest);
   }
   bitmap_put(d,dx+xx,dy+yy,index);
  }
 }
 return 1;
}
#ifdef GR_MODE_HAVE_CAPTUREPIXELS
struct capture_stage {struct bitmap image;u8 *near_pixels;u16 rows,base;int valid;};
static void capture_row(void *ctx,unsigned row,const unsigned char *packed,unsigned len)
{
 struct capture_stage*c=ctx;row+=c->base;
 if(!c->valid||row!=c->rows||row>=c->image.h||len!=c->image.stride){c->valid=0;return;}
 if(c->near_pixels)memcpy(c->near_pixels+row*c->image.stride,packed,len);
 else bitmap_write(&c->image,(u32)row*c->image.stride,packed,len);
 c->rows++;
}
#endif
static int capture_copy(struct dc*src,int dm,int dx,int dy,int sx,int sy,int w,int h)
{
#ifdef GR_MODE_HAVE_CAPTUREPIXELS
 struct capture_stage c;struct bitmap*d;GR_PALETTE*p=(GR_PALETTE*)w16_scratch;
 GR_SCREEN_INFO si;long xx=dx,yy=dy,ax=sx,ay=sy,ww=w,hh=h;int db=bi(memory[dm].bitmap),sw,sh,j,ok,rows,limit;
 if(db<0||bitmaps[db].bpp!=4)return 0;d=&bitmaps[db];
 if(src->w){sw=src->w->cw;sh=src->w->ch;}else{memset(&si,0,sizeof(si));GrGetScreenInfo(&si);sw=si.cols;sh=si.rows;}
 if(!clip_axis(&xx,&ax,&ww,d->w,sw)||!clip_axis(&yy,&ay,&hh,d->h,sh))return 1;
 memset(&c,0,sizeof(c));c.image.w=ww;c.image.h=hh;c.image.stride=(ww+1)/2;c.image.bpp=4;c.valid=1;

 memset(p,0,sizeof(*p));GrGetSystemPalette(p);if(p->count<1||p->count>16)return 0;
 for(j=0;j<16;j++){c.image.palette[j].r=p->palette[j].r;c.image.palette[j].g=p->palette[j].g;c.image.palette[j].b=p->palette[j].b;}
 /* Small selection captures can outlive the last free far block. Keep the
  * same atomic staging contract using bounded near storage in that case. */
 if(!allocate_pixels(&c.image,(u32)c.image.stride*c.image.h)){
  u32 bytes=(u32)c.image.stride*c.image.h;
  if(bytes>2048||!(c.near_pixels=malloc((u16)bytes)))return 0;
 }
 ok=1;limit=16384/c.image.stride;
 while(c.rows<c.image.h){
  c.base=c.rows;rows=c.image.h-c.rows;if(rows>limit)rows=limit;
  if(!GrCapturePixels(src->wid,src->gc,ax,ay+c.base,ww,rows,capture_row,&c)||
     !c.valid||c.rows!=c.base+rows){ok=0;break;}
 }
 if(ok&&c.near_pixels){
  u8 map[16];int x,y;
  for(j=0;j<16;j++)map[j]=bitmap_nearest(d,bitmap_rgb(&c.image,j));
  for(y=0;y<hh;y++)for(x=0;x<ww;x++){
   u8 packed=c.near_pixels[y*c.image.stride+x/2];
   bitmap_put(d,xx+x,yy+y,map[(packed>>((x&1)?0:4))&15]);
  }
 }else if(ok)ok=memory_copy(d,dm,xx,yy,&c.image,0,0,0,ww,hh,0xcc);
 free(c.near_pixels);release_pixels(&c.image);return ok;
#else
 (void)src;(void)dm;(void)dx;(void)dy;(void)sx;(void)sy;(void)w;(void)h;return 0;
#endif
}
/* Discover connected scanline spans before drawing. The bounded near worklist
 * avoids any far mask allocation; capture or complexity failures are atomic. */
#define FLOOD_SPANS 512
struct flood_span {i16 l,r,y;};
struct flood_row {u8 bits[80],accept[16];u16 w;int valid,rows;};
static int flood_bit(struct flood_row*c,int x){return (c->bits[x>>3]>>(x&7))&1;}
#ifdef GR_MODE_HAVE_CAPTUREPIXELS
static void flood_row_data(void*ctx,unsigned row,const unsigned char*data,unsigned len)
{
 struct flood_row*c=ctx;unsigned x;
 if(!c->valid||row||c->rows||len!=(c->w+1)/2){c->valid=0;return;}
 for(x=0;x<c->w;x++)if(c->accept[(data[x/2]>>((x&1)?0:4))&15])c->bits[x>>3]|=1<<(x&7);
 c->rows=1;
}
#endif
static int flood_read_row(struct flood_row*c,struct dc*d,struct bitmap*b,int y,u32 boundary,int mode)
{
 int x;memset(c->bits,0,sizeof c->bits);c->valid=1;c->rows=0;
 if(d){
#ifdef GR_MODE_HAVE_CAPTUREPIXELS
  return GrCapturePixels(d->wid,d->gc,0,y,c->w,1,flood_row_data,c)&&c->valid&&c->rows==1;
#else
  return 0;
#endif
 }
 for(x=0;x<c->w;x++)if((bitmap_rgb(b,bitmap_index(b,x,y))==boundary)==mode)c->bits[x>>3]|=1<<(x&7);
 return 1;
}
int bitmap_flood(u16 h,struct dc*d,int x,int y,u32 boundary,int mode,u32 brush,
                 void (*output)(void*,int,int,int,int),void *ctx)
{
 struct bitmap*dest=0;struct flood_row row;struct flood_span*spans;
 int m=di(h),i,j,w,hgt,head,count=1,limit,l,r,ny,xx,known,ok=0;u8 pi=0;
 GR_SCREEN_INFO si;GR_PALETTE*palette=(GR_PALETTE*)w16_scratch;
 if(mode<0||mode>1||(boundary&0xff000000UL))return 0;
 if(d){
  if(d->w){w=d->w->cw;hgt=d->w->ch;}
  else{memset(&si,0,sizeof(si));GrGetScreenInfo(&si);w=si.cols;hgt=si.rows;}
 }else{
  if(m<0||(i=bi(memory[m].bitmap))<0||!memory_point(m,&x,&y))return 0;
  dest=&bitmaps[i];w=dest->w;hgt=dest->h;pi=bitmap_nearest(dest,brush);
 }
 if(w<1||hgt<1||w>640||hgt>480||x<0||y<0||x>=w||y>=hgt)return 0;
 memset(&row,0,sizeof row);row.w=w;
 if(d){
  memset(palette,0,sizeof(*palette));GrGetSystemPalette(palette);if(palette->count<1||palette->count>16)return 0;
  for(i=0;i<16;i++){
   u32 rgb=(u32)palette->palette[i].r|((u32)palette->palette[i].g<<8)|((u32)palette->palette[i].b<<16);
   row.accept[i]=(rgb==boundary)==mode;
  }
 }
 for(limit=FLOOD_SPANS;limit>=32;limit/=2){spans=malloc(limit*sizeof(*spans));if(spans)break;}
 if(!spans){wlog("w16: flood near worklist unavailable\n");return 0;}
 if(!flood_read_row(&row,d,dest,y,boundary,mode)||!flood_bit(&row,x))goto done;
 l=r=x;while(l>0&&flood_bit(&row,l-1))l--;while(r+1<w&&flood_bit(&row,r+1))r++;
 spans[0].l=l;spans[0].r=r;spans[0].y=y;
 for(head=0;head<count;head++)for(j=-1;j<=1;j+=2){
  ny=spans[head].y+j;if(ny<0||ny>=hgt)continue;
  if(!flood_read_row(&row,d,dest,ny,boundary,mode))goto done;
  for(xx=spans[head].l;xx<=spans[head].r;){
   if(!flood_bit(&row,xx)){xx++;continue;}
   l=r=xx;while(l>0&&flood_bit(&row,l-1))l--;while(r+1<w&&flood_bit(&row,r+1))r++;
   known=0;for(i=0;i<count;i++)if(spans[i].y==ny&&spans[i].l==l&&spans[i].r==r){known=1;break;}
   if(!known){
    if(count==limit)goto done;
    spans[count].l=l;spans[count].r=r;spans[count++].y=ny;
   }
   xx=r+1;
  }
 }
 for(i=0;i<count;i++){
  l=spans[i].l;r=spans[i].r;y=spans[i].y;
  if(d)output(ctx,l,y,r-l+1,1);
  else for(x=l;x<=r;x++)memory_rop_pixel(dest,x,y,pi,memory[m].rop2);
 }
 ok=1;
 done:if(opt_trace)wlog("w16: flood result %d discovered spans %d\n",ok,count);free(spans);return ok;
}

u32 g_BitBlt(u16 ss,u16 ap)
{
 struct dc*d=dc_get(AW(18));int m=di(AW(8)),dm=di(AW(18)),db=-1,b,row,col,end,bit;struct bitmap*s;long dx=(i16)AW(16),dy=(i16)AW(14),w=(i16)AW(12),h=(i16)AW(10),sx=(i16)AW(6),sy=(i16)AW(4);int dw,dh;GR_SCREEN_INFO screen;GR_GC_INFO old;u32 rop=AL(0);
 if((!d&&dm<0)||(rop!=0x00cc0020UL&&rop!=0x008800c6UL&&rop!=0x00e20746UL&&rop!=0x00220326UL&&rop!=0x00660046UL)||w<=0||h<=0)return 0;
 {
  int ax=dx,ay=dy,bx=sx,by=sy;struct dc*src=dc_get(AW(8));
  if(!(dm>=0?memory_point(dm,&ax,&ay):gdi_map_point(d,&ax,&ay)))return 0;
  if(m>=0){if(!memory_point(m,&bx,&by))return 0;}
  else if(!src||!gdi_map_point(src,&bx,&by))return 0;
  dx=ax;dy=ay;sx=bx;sy=by;
  if(m<0)return dm>=0&&rop==0x00cc0020UL?capture_copy(src,dm,dx,dy,sx,sy,w,h):0;
 }
 b=bi(memory[m].bitmap);if(b<0)return 0;s=&bitmaps[b];
 if(dm>=0){db=bi(memory[dm].bitmap);if(db<0||(rop!=0x00cc0020UL&&rop!=0x00220326UL&&rop!=0x00660046UL&&rop!=0x00e20746UL))return 0;dw=bitmaps[db].w;dh=bitmaps[db].h;}
 else if(d->w){dw=d->w->cw;dh=d->w->ch;}else{GrGetScreenInfo(&screen);dw=screen.cols;dh=screen.rows;}
 if(!clip_axis(&dx,&sx,&w,dw,s->w)||!clip_axis(&dy,&sy,&h,dh,s->h))return 1;
 if(dm>=0)return memory_copy(&bitmaps[db],dm,dx,dy,s,m,sx,sy,w,h,(u16)(rop>>16));
 if(rop==0x00220326UL||rop==0x00660046UL)return 0;
 /* E2 is S ? P : D. A monochrome source expands using the destination
  * text/background colours; admit only full-bit masks (also their inverse).
  * No readback or destination rewrite is needed for the zero-mask pixels. */
 if(rop==0x00e20746UL){
  u32 rgb;int invert;
  if(s->bpp!=1||!((d->textcolor==0&&d->bkcolor==0xffffffUL)||
     (d->textcolor==0xffffffUL&&d->bkcolor==0))||
     !gdi_pattern_color(d,dx,dy,&rgb))return 0;
  invert=d->textcolor!=0;
  GrGetGCInfo(d->gc,&old);GrSetGCMode(d->gc,GR_MODE_COPY);
  for(row=0;row<h;row++)for(col=0;col<w;col++){
   if(bitmap_index(s,sx+col,sy+row)==invert)continue;
   gdi_pattern_color(d,dx+col,dy+row,&rgb);
   GrSetGCForeground(d->gc,rgb_to_gr(rgb));
   GrFillRect(d->wid,d->gc,dx+col,dy+row,1,1);
  }
  GrSetGCForeground(d->gc,old.foreground);GrSetGCMode(d->gc,old.mode);return 1;
 }
 GrGetGCInfo(d->gc,&old);
 /* Query-only native screen-AND capability; old/Tandy backends refuse. */
 if(rop==0x008800c6UL&&!(old.mode&0x2000))return 0;
 GrSetGCMode(d->gc,rop==0x008800c6UL?GR_MODE_AND:GR_MODE_COPY);
 for(row=0;row<h;row++)for(col=0;col<w;col=end){
  u32 rgb;bit=bitmap_index(s,sx+col,sy+row);
  for(end=col+1;end<w&&bitmap_index(s,sx+end,sy+row)==bit;end++);
  if(s->bpp==1)rgb=bit?d->bkcolor:d->textcolor;
  else{struct dib_rgb*c=&s->palette[bit];rgb=(u32)c->r|((u32)c->g<<8)|((u32)c->b<<16);}
  GrSetGCForeground(d->gc,rgb_to_gr(rgb));GrFillRect(d->wid,d->gc,dx+col,dy+row,end-col,1);
 }
 GrSetGCForeground(d->gc,old.foreground);GrSetGCMode(d->gc,old.mode);return 1;
}

/* Win16 BITMAP: four WORDs, two BYTEs, then a null far bmBits pointer.
 * Owned backing pixels are not a caller-accessible DIB section. */
u32 g_GetObject(u16 ss,u16 ap)
{
 int i=bi(AW(6));u16 off=AW(0),seg=AW(2),bpp;u8 out[14];
 if(i<0)return palette_object(AW(6),(i16)AW(4),seg,off);
 if((i16)AW(4)<14||!seg||(u32)off+14>65536UL)return 0;
 memset(out,0,sizeof(out));bpp=bitmaps[i].bpp;
 out[2]=bitmaps[i].w;out[3]=bitmaps[i].w>>8;
 out[4]=bitmaps[i].h;out[5]=bitmaps[i].h>>8;
 {u16 stride=((bitmaps[i].w*bpp+15)/16)*2;out[6]=stride;out[7]=stride>>8;}
 out[8]=1;out[9]=bpp;fcopy_out(seg,off,out,14);return 14;
}

/* Physical size metadata in 0.1 mm; pixels and raster operations are unchanged. */
u32 g_SetBitmapDimension(u16 ss,u16 ap)
{
 int i=bi(AW(4));u32 old;if(i<0)return 0;
 old=(u32)bitmaps[i].dimx|((u32)bitmaps[i].dimy<<16);
 bitmaps[i].dimx=AW(2);bitmaps[i].dimy=AW(0);return old;
}

/* DDB byte order is top-down, WORD-aligned packed rows, matching GetObject.
 * A single far span must fit its segment; larger images use DIB scan bands. */
u32 bitmap_bits(u16 h,u32 count,u16 seg,u16 off,int set)
{
 int i=bi(h);u32 size,at;struct bitmap*b;
 if(i<0 || (count & 0x80000000UL))return 0;
 b=&bitmaps[i];size=(u32)b->stride*b->h;
 if(!seg&&!off)return set?0:size;
 if(count>size)count=size;
 if((u32)off+count>65536UL || ((u32)seg<<4)+off+count>0x100000UL)return 0;
 for(at=0;at<count;at++){
  u16 bank=b->bank[(unsigned)(at/BITMAP_BANK_BYTES)],bo=(u16)(at%BITMAP_BANK_BYTES);
  if(set)wr8(bank,bo,rd8(seg,(u16)(off+at)));
  else wr8(seg,(u16)(off+at),rd8(bank,bo));
 }
 return count;
}

/* Snapshot mono or two-colour DDBs without retaining the source handle.
 * A third colour is rejected, never approximated into a monochrome pattern. */
int bitmap_pattern(u16 h,u8*w,u8*height,u16*rows,u32*colors)
{
 int i=bi(h),x,y,two=0;struct bitmap*b;u16 row;u8 v;u32 rgb;
 if(i<0)return 0;b=&bitmaps[i];
 if(b->w>16||b->h>16)return 0;
 colors[0]=colors[1]=bitmap_rgb(b,bitmap_index(b,0,0));
 for(y=0;y<b->h;y++){
  row=0;
  for(x=0;x<b->w;x++){
   v=bitmap_index(b,x,y);
   if(b->bpp!=1){
    rgb=bitmap_rgb(b,v);v=rgb!=colors[0];
    if(v){if(!two){colors[1]=rgb;two=1;}else if(rgb!=colors[1])return 0;}
   }
   if(v)row|=0x8000u>>x;
  }
  rows[y]=row;
 }
 *w=b->w;*height=b->h;return b->bpp==1?1:2;
}

/* Compatibility follows the supplied DC's selected bitmap, not merely the
 * display from which a memory DC was created. The default memory bitmap is mono. */
typedef char bitmap_palette_fits_scratch[(sizeof(GR_PALETTE)<=W16_SCRATCH)?1:-1];
u32 g_CreateCompatibleBitmap(u16 ss,u16 ap)
{
 u16 hdc=AW(4);int w=(i16)AW(2),h=(i16)AW(0),m=di(hdc),i,b,j,mono=0;
 GR_PALETTE*p=(GR_PALETTE*)w16_scratch;
 if(w<0||h<0||w>640||h>480)return 0;
 if(m>=0){b=bi(memory[m].bitmap);mono=b<0||bitmaps[b].bpp==1;}
 else if(!dc_get(hdc))return 0;
 if(!w||!h){w=h=1;mono=1;}
 if(!mono){memset(p,0,sizeof(*p));GrGetSystemPalette(p);if(p->count<1||p->count>16)return 0;}
 i=allocate_bitmap(w,h,mono?1:4);if(i<0)return 0;
 if(!mono)for(j=0;j<16;j++){
  bitmaps[i].palette[j].r=p->palette[j].r;bitmaps[i].palette[j].g=p->palette[j].g;bitmaps[i].palette[j].b=p->palette[j].b;
 }
 bitmaps[i].used=1;return BBASE+2*i;
}

/* General 1-plane DDBs; small monochrome brush patterns retain their old path.
 * lpBits is a bounded single far span, WORD-aligned rows, top row first. */
u16 bitmap_create(u16 w,u16 h,u16 planes,u16 bpp,u16 seg,u16 off)
{
 int i,j;u16 stride,x,y;u32 bytes;GR_PALETTE*p=(GR_PALETTE*)w16_scratch;
 if(planes!=1||(bpp!=1&&bpp!=4)||!w||!h||w>640||h>480)return 0;
 stride=((w*bpp+15)/16)*2;bytes=(u32)stride*h;
 if((seg||off)&&((u32)off+bytes>65536UL||((u32)seg<<4)+off+bytes>0x100000UL))return 0;
 if(bpp==4){memset(p,0,sizeof(*p));GrGetSystemPalette(p);if(p->count<1||p->count>16)return 0;}
 i=allocate_bitmap(w,h,bpp);if(i<0)return 0;
 if(bpp==4)for(j=0;j<16;j++){
  bitmaps[i].palette[j].r=p->palette[j].r;bitmaps[i].palette[j].g=p->palette[j].g;bitmaps[i].palette[j].b=p->palette[j].b;
 }
 if(seg||off)for(y=0;y<h;y++)for(x=0;x<stride;x++){
  u8 v=rd8(seg,(u16)(off+(u32)y*stride+x));
  bitmap_write(&bitmaps[i],(u32)y*stride+x,&v,1);
 }
 bitmaps[i].used=1;return BBASE+2*i;
}

int bitmap_brushorg(u16 h,i16 x,i16 y,u32 *old)
{
 int i=di(h);if(i<0)return 0;
 *old=(u16)memory[i].brushx|((u32)(u16)memory[i].brushy<<16);
 memory[i].brushx=x;memory[i].brushy=y;return 1;
}

/* SetStretchBltMode: state belongs to the destination DC, including memory DCs.
 * Unsupported modes leave the old value untouched. */
u32 g_SetStretchBltMode(u16 ss,u16 ap)
{
 u16 h=AW(2),mode=AW(0);struct dc*d=dc_get(h);int m=di(h);u8 old;
 if((!d&&m<0)||mode<1||mode>3)return 0;
 if(d){old=d->stretchmode;d->stretchmode=mode;}
 else{old=memory[m].stretchmode;memory[m].stretchmode=mode;}
 return old;
}

/* Bounded memory-to-display scaling. COLORONCOLOR uses nearest-neighbour
 * sampling; BLACKONWHITE supports the exact vertical row pair below. Destination
 * clipping preserves the sampling phase. Other stretch paths fail before drawing. */
/* BLACKONWHITE for the observed 2:1 vertical reduction combines both rows.
 * The source is an owned 1/4bpp DDB: combine its stored pixel bits before RGB expansion. */
static u8 stretch_index(struct bitmap *s,u16 x,u16 y,int pair)
{
 u8 c=bitmap_index(s,x,y);
 return pair?(c&bitmap_index(s,x,y+1)):c;
}
u32 g_StretchBlt(u16 ss,u16 ap)
{
 struct dc*d=dc_get(AW(22));int m=di(AW(12)),b,x,y,end,limitx,limity,pair;u32 rop=AL(0);
 long dx=(i16)AW(20),dy=(i16)AW(18),dw=(i16)AW(16),dh=(i16)AW(14);
 long sx=(i16)AW(10),sy=(i16)AW(8),sw=(i16)AW(6),sh=(i16)AW(4),left,top,right,bottom;
 struct bitmap*s;GR_SCREEN_INFO si;GR_GC_INFO old;
 int ax,ay,bx,by;
 if(!d||m<0||(b=bi(memory[m].bitmap))<0)return 0;
 if((rop!=0x00cc0020UL&&rop!=0x008800c6UL)||dw<=0||dh<=0||sw<=0||sh<=0)return 0;
 pair=d->stretchmode==1;
 if(d->stretchmode!=3&&!pair)return 0;
 /* Logical destination extents are transformed without losing the source phase.
  * Mirrored destination extents remain explicitly unsupported here. */
 if(dx+dw>32767L||dy+dh>32767L)return 0;
 ax=(int)dx;ay=(int)dy;bx=(int)(dx+dw);by=(int)(dy+dh);
 if(!gdi_map_point(d,&ax,&ay)||!gdi_map_point(d,&bx,&by))return 0;
 dx=ax;dy=ay;dw=(long)bx-ax;dh=(long)by-ay;
 if(dw<=0||dh<=0)return 0;
 if(pair&&(dw!=sw||sh!=2*dh))return 0;
 s=&bitmaps[b];
 if(sx<0||sy<0||sx+sw>s->w||sy+sh>s->h)return 0;
 if(d->w){limitx=d->w->cw;limity=d->w->ch;}
 else{GrGetScreenInfo(&si);limitx=si.cols;limity=si.rows;}
 left=dx<0?0:dx;top=dy<0?0:dy;right=dx+dw;bottom=dy+dh;
 if(right>limitx)right=limitx;if(bottom>limity)bottom=limity;
 if(left>=right||top>=bottom)return 1;
 GrGetGCInfo(d->gc,&old);
 if(rop==0x008800c6UL&&!(old.mode&0x2000))return 0;
 GrSetGCMode(d->gc,rop==0x008800c6UL?GR_MODE_AND:GR_MODE_COPY);
 for(y=(int)top;y<bottom;y++){
  u16 yy=(u16)(sy+((long)y-dy)*sh/dh);
  for(x=(int)left;x<right;x=end){
   u8 index=stretch_index(s,(u16)(sx+((long)x-dx)*sw/dw),yy,pair);u32 rgb;
   for(end=x+1;end<right;end++)
    if(stretch_index(s,(u16)(sx+((long)end-dx)*sw/dw),yy,pair)!=index)break;
   rgb=s->bpp==1?(index?d->bkcolor:d->textcolor):bitmap_rgb(s,index);
   GrSetGCForeground(d->gc,rgb_to_gr(rgb));
   GrFillRect(d->wid,d->gc,x,y,end-x,1);
  }
 }
 GrSetGCForeground(d->gc,old.foreground);GrSetGCMode(d->gc,old.mode);
 return 1;
}

/* DIB scan bands use bottom-up numbering and a band-local far buffer. No
 * full-image staging allocation: a 640x480 DDB remains in its existing banks.
 * BI_RGB indexed RGB and SetDIBits logical-palette tables are supported. */
static int dib_span(u16 s,u16 o,u32 n)
{return s && (u32)o+n<=65536UL && ((u32)s<<4)+o+n<=0x100000UL;}
static u32 dib_long(u16 s,u16 o)
{return (u32)rd16(s,o)|((u32)rd16(s,o+2)<<16);}
static void dib_wlong(u16 s,u16 o,u32 v)
{wr16(s,o,(u16)v);wr16(s,o+2,(u16)(v>>16));}
static u32 bitmap_dib_transfer(u16 hdc,u16 hb,u16 start,u16 count,
                               u16 bs,u16 bo,u16 is,u16 io,u16 usage,int get)
{
 int i=bi(hb),core,query;struct bitmap*b;
 u32 size,w,h,colors,stride,bytes,rgb;u16 depth,step,x,y,row,at,j;
 u8 map[16],v,index,shift,entry[4];u16 pal=0;struct dc*dc;
 GR_PALETTE*p=(GR_PALETTE*)w16_scratch;
 if(i<0||(!dc_get(hdc)&&di(hdc)<0)||usage>1||(get&&usage)||!dib_span(is,io,4))return 0;
 b=&bitmaps[i];size=dib_long(is,io);core=size==12;
 if((size!=40&&!core)||!dib_span(is,io,size))return 0;
 depth=rd16(is,io+(core?10:14));query=get&&!bs&&!bo&&!depth;
 if(query){
  if(core){wr16(is,io+4,b->w);wr16(is,io+6,b->h);wr16(is,io+8,1);wr16(is,io+10,b->bpp);}
  else{
   dib_wlong(is,io+4,b->w);dib_wlong(is,io+8,b->h);wr16(is,io+12,1);wr16(is,io+14,b->bpp);
   dib_wlong(is,io+16,0);dib_wlong(is,io+20,(u32)(((b->w*b->bpp+31)/32)*4)*b->h);
   dib_wlong(is,io+24,0);dib_wlong(is,io+28,0);dib_wlong(is,io+32,1U<<b->bpp);dib_wlong(is,io+36,0);
  }return b->h;
 }
 if(depth!=1&&depth!=4)return 0;
 w=core?rd16(is,io+4):dib_long(is,io+4);h=core?rd16(is,io+6):dib_long(is,io+8);
 if(w!=b->w||h!=b->h||rd16(is,io+(core?8:12))!=1||(!core&&dib_long(is,io+16)))return 0;
 if(get&&depth!=b->bpp)return 0;
 colors=core?1U<<depth:dib_long(is,io+32);if(!colors)colors=1U<<depth;
 if(colors>(1U<<depth))return 0;step=usage?2:core?3:4;
 /* Get emits the complete native palette, not a silently truncated table. */
 if(get)colors=1U<<depth;
 if(!dib_span(is,io,size+colors*step))return 0;
 stride=((w*depth+31)/32)*4;
 if(start>=h)return 0;if(count>h-start)count=(u16)(h-start);
 bytes=stride*count;
 if((bs||bo)&&!dib_span(bs,bo,bytes))return 0;
 if(!get&&(!count||(!bs&&!bo)))return 0;
 if(get){
  for(j=0;j<colors;j++){
   rgb=bitmap_rgb(b,j);at=io+(u16)size+j*step;
   wr8(is,at,(u8)(rgb>>16));wr8(is,at+1,(u8)(rgb>>8));wr8(is,at+2,(u8)rgb);if(!core)wr8(is,at+3,0);
  }
  if(!core){dib_wlong(is,io+20,stride*h);dib_wlong(is,io+32,colors);}
  if(!bs&&!bo)return b->h;
 }else{
  if(usage){
   dc=dc_get(hdc);pal=dc?dc->palette:memory[di(hdc)].palette;
   memset(p,0,sizeof(*p));GrGetSystemPalette(p);
  }
  for(j=0;j<colors;j++){
   at=io+(u16)size+j*step;
   if(usage){
    if(!palette_entry(pal,rd16(is,at),entry))return 0;
    if(entry[3]&2){u16 k=entry[0]|((u16)entry[1]<<8);
     if(k>=p->count)return 0;rgb=(u32)p->palette[k].r|((u32)p->palette[k].g<<8)|((u32)p->palette[k].b<<16);
    }else rgb=(u32)entry[0]|((u32)entry[1]<<8)|((u32)entry[2]<<16);
   }else rgb=((u32)rd8(is,at)<<16)|((u32)rd8(is,at+1)<<8)|rd8(is,at+2);map[j]=bitmap_nearest(b,rgb);
  }
  /* Validate every source index before modifying any destination pixel. */
  for(y=0;y<count;y++)for(x=0;x<w;x++){
   v=rd8(bs,bo+(u16)((u32)y*stride)+x/(8/depth));shift=depth==1?7-(x&7):(x&1)?0:4;
   if(((v>>shift)&((1U<<depth)-1))>=colors)return 0;
  }
 }
 for(y=0;y<count;y++){
  row=b->h-1-start-y;at=bo+(u16)((u32)y*stride);
  if(get){
   /* Zero alignment and unused tail bits, then pack high nibble/bit first. */
   for(j=0;j<stride;j++)wr8(bs,at+j,0);
   for(x=0;x<w;x++){
    j=at+x/(8/depth);shift=depth==1?7-(x&7):(x&1)?0:4;
    index=bitmap_index(b,x,row);wr8(bs,j,rd8(bs,j)|(index<<shift));
   }
  }else for(x=0;x<w;x++){
   v=rd8(bs,at+x/(8/depth));shift=depth==1?7-(x&7):(x&1)?0:4;
   bitmap_put(b,x,row,map[(v>>shift)&((1U<<depth)-1)]);
  }
 }
 return count;
}
static u32 bitmap_dib(u16 ss,u16 ap,int get)
{return bitmap_dib_transfer(AW(16),AW(14),AW(12),AW(10),AW(8),AW(6),AW(4),AW(2),AW(0),get);}
u32 g_SetDIBits(u16 ss,u16 ap){return bitmap_dib(ss,ap,0);}
u32 g_GetDIBits(u16 ss,u16 ap){return bitmap_dib(ss,ap,1);}

/* CreateDIBitmap produces a device-colour DDB. lpHeader and lpInfo can differ;
 * the former supplies dimensions, the latter supplies the initialization data.
 * Unsupported compression, depth, direction, or far span fails atomically. */
static int dib_dimensions(u16 seg,u16 off,u16 *w,u16 *h)
{
 u32 size,x,y;int core;
 if(!dib_span(seg,off,4))return 0;
 size=dib_long(seg,off);core=size==12;
 if((!core&&size!=40)||!dib_span(seg,off,size))return 0;
 x=core?rd16(seg,off+4):dib_long(seg,off+4);
 y=core?rd16(seg,off+6):dib_long(seg,off+8);
 if(!x||!y||x>640||y>480||rd16(seg,off+(core?8:12))!=1)return 0;
 if(rd16(seg,off+(core?10:14))!=1&&rd16(seg,off+(core?10:14))!=4)return 0;
 if(!core&&dib_long(seg,off+16))return 0;
 *w=x;*h=y;return 1;
}
static int dib_device_bitmap(u16 hdc,u16 width,u16 height)
{
 GR_PALETTE*p=(GR_PALETTE*)w16_scratch;int i,j;
 if(!dc_get(hdc)&&di(hdc)<0)return -1;
 memset(p,0,sizeof(*p));GrGetSystemPalette(p);if(p->count<1||p->count>16)return -1;
 i=allocate_bitmap(width,height,4);if(i<0)return -1;
 for(j=0;j<16;j++){
  bitmaps[i].palette[j].r=p->palette[j].r;
  bitmaps[i].palette[j].g=p->palette[j].g;
  bitmaps[i].palette[j].b=p->palette[j].b;
 }
 bitmaps[i].used=1;return i;
}
u32 g_CreateDIBitmap(u16 ss,u16 ap)
{
 u16 width,height,hdc=AW(18),usage=AW(0);u32 init=AL(10);int i;
 if(usage>1||(init!=0&&init!=4)||!dib_dimensions(AW(16),AW(14),&width,&height))return 0;
 i=dib_device_bitmap(hdc,width,height);if(i<0)return 0;
 if(init==4&&bitmap_dib_transfer(hdc,BBASE+2*i,0,height,AW(8),AW(6),AW(4),AW(2),usage,0)!=height){
  release_bitmap(i);return 0;
 }
 return BBASE+2*i;
}

/* SetDIBitsToDevice consumes scan-band-local bits, with bottom-up source
 * coordinates, and never changes a DC's selected bitmap or colours. */
u32 g_SetDIBitsToDevice(u16 ss,u16 ap)
{
 u16 hdc=AW(26),width,height,count=AW(10),start=AW(12);
 struct dc*d=dc_get(hdc);int dm=di(hdc),i,db=-1,row,col,end,x,y,dw,dh;
 long dx=(i16)AW(24),dy=(i16)AW(22),sx=(i16)AW(16),sy,ww=AW(20),hh=AW(18),top,bottom,q;
 GR_SCREEN_INFO screen;GR_GC_INFO old;struct bitmap*s;
 if((!d&&dm<0)||!ww||!hh||AW(0)>1||!dib_dimensions(AW(4),AW(2),&width,&height)||start>=height||!count)return 0;
 if(count>height-start)count=height-start;
 sy=(long)height-(i16)AW(14)-hh;
 x=dx;y=dy;if(!(dm>=0?memory_point(dm,&x,&y):gdi_map_point(d,&x,&y)))return 0;
 dx=x;dy=y;
 if(dm>=0){db=bi(memory[dm].bitmap);if(db<0)return 0;dw=bitmaps[db].w;dh=bitmaps[db].h;}
 else if(d->w){dw=d->w->cw;dh=d->w->ch;}
 else{GrGetScreenInfo(&screen);dw=screen.cols;dh=screen.rows;}
 /* Only the supplied scan band is valid; clipping keeps destination phase. */
 top=(long)height-start-count;bottom=(long)height-start;
 if(sy<top){q=top-sy;sy+=q;dy+=q;hh-=q;}
 if(sy+hh>bottom)hh=bottom-sy;
 if(hh<=0||!clip_axis(&dx,&sx,&ww,dw,width)||!clip_axis(&dy,&sy,&hh,dh,height))return 0;
 i=dib_device_bitmap(hdc,width,height);if(i<0)return 0;s=&bitmaps[i];
 if(bitmap_dib_transfer(hdc,BBASE+2*i,start,count,AW(8),AW(6),AW(4),AW(2),AW(0),0)!=count){
  release_bitmap(i);return 0;
 }
 if(dm>=0){
  u8 map[16];for(col=0;col<16;col++)map[col]=bitmap_nearest(&bitmaps[db],bitmap_rgb(s,col));
  for(row=0;row<hh;row++)for(col=0;col<ww;col++)
   bitmap_put(&bitmaps[db],dx+col,dy+row,map[bitmap_index(s,sx+col,sy+row)]);
 }else{
  GrGetGCInfo(d->gc,&old);GrSetGCMode(d->gc,GR_MODE_COPY);
  for(row=0;row<hh;row++)for(col=0;col<ww;col=end){
   u8 index=bitmap_index(s,sx+col,sy+row);
   for(end=col+1;end<ww&&bitmap_index(s,sx+end,sy+row)==index;end++);
   GrSetGCForeground(d->gc,rgb_to_gr(bitmap_rgb(s,index)));
   GrFillRect(d->wid,d->gc,dx+col,dy+row,end-col,1);
  }
  GrSetGCForeground(d->gc,old.foreground);GrSetGCMode(d->gc,old.mode);
 }
 release_bitmap(i);return hh;
}

/* Losslessly packed from the same proportional font used by screen DCs. */

u16 bitmap_textmode(u16 h,u16 value,int align)
{
 int m=di(h);u16 old;if(m<0)return 0;
 if(align){old=memory[m].textalign;memory[m].textalign=value;}
 else{if(value!=1&&value!=2)return 0;old=memory[m].bkmode;memory[m].bkmode=value;}
 return old;
}
int bitmap_text(u16 h,int x,int y,u16 seg,u16 off,int n)
{
 struct bitmap *b;struct memory_dc *d;int m=di(h),bb,k,row,col,width=0,cw;
 u16 a;u8 fg,bg,c;long px,py,drawx,tx,ty;static int checked,compatible;
 if(m<0||(bb=bi(memory[m].bitmap))<0||n<0||n>128||(u32)off+n>65536UL)return 0;
 if(!checked){
  int i;GR_SIZE sw,sh,sb;char ch;
  compatible=1;
  for(i=0;i<256;i++){ch=(char)i;gdi_text_size(&ch,1,&sw,&sh,&sb);
   if(sw!=rasterfont_width(i)||sh!=13||sb!=11){
    wlog("w16: memory text font mismatch char %u native %d/%d/%d expected %u/13/11\n",i,sw,sh,sb,rasterfont_width(i));compatible=0;break;}}
  checked=1;
 }
 if(!compatible)return 0;
 d=&memory[m];b=&bitmaps[bb];a=d->textalign;
 for(k=0;k<n;k++){c=rd8(seg,off+k);width+=rasterfont_width(c);}
 if(a&1){x=d->curx;y=d->cury;}
 tx=x;ty=y;
 if((a&6)==6)tx-=width/2;else if(a&2)tx-=width;
 if((a&24)==24)ty-=11;else if(a&8)ty-=13;
 if(tx< -32768L||tx>32767L||ty< -32768L||ty>32767L)return 0;
 x=tx;y=ty;
 if(!memory_point(m,&x,&y))return 0;
 fg=bitmap_nearest(b,d->textcolor);bg=bitmap_nearest(b,d->bkcolor);drawx=x;
 for(k=0;k<n;k++){
  c=rd8(seg,off+k);if(c<32)c=32;cw=rasterfont_width(c);
  for(row=0;row<13;row++){
   u16 bits=rasterfont_row(c,row);py=(long)y+row;
   if(py<0||py>=b->h)continue;
   for(col=0;col<cw;col++){
    int ink=(bits&(0x8000u>>col))!=0;px=drawx+col;
    if(px>=0&&px<b->w&&(ink||d->bkmode==2))bitmap_put(b,(int)px,(int)py,ink?fg:bg);
   }
  }
  drawx+=cw;
 }
 if(a&1)d->curx=(i16)((a&2)?(long)d->curx-width:(long)d->curx+width);
 return 1;
}
