/* Bounded far-owned bitmaps and memory DCs. Unsupported raster operations
 * fail; screen capture commits only after a complete negotiated snapshot. */
#include <string.h>
#include "wapi.h"
#include "dib.h"
#define BBASE 0x0a00
#define DBASE 0x0c00
#define NB 16
#define ND 8
#define DEFAULT_BITMAP 0x0aff
#define BITMAP_BANK_BYTES 16384UL
#define BITMAP_BANKS 10
struct bitmap {u16 w,h,bank[BITMAP_BANKS],stride;u8 used,bpp;struct dib_rgb palette[16];};
static struct bitmap bitmaps[NB];
struct memory_dc {u16 bitmap,pen,brush,font,palette;u8 used,stretchmode,rop2;i16 brushx,brushy,curx,cury;u32 textcolor,bkcolor;};
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
int bitmap_delete(u16 h){int i=bi(h);if(i<0)return -1;if(bitmap_referenced(h,-1))return 0;release_bitmap(i);return 1;}
int bitmap_deletedc(u16 h){int i=di(h);if(i<0)return -1;memset(&memory[i],0,sizeof(memory[i]));memory_saved_count[i]=0;return 1;}
u16 bitmap_select(u16 dc,u16 h){int i=di(dc),j=bi(h);u16 old;if(i<0||(j<0&&h!=DEFAULT_BITMAP))return 0;if(bitmap_referenced(h,i))return 0;old=memory[i].bitmap;memory[i].bitmap=h;return old;}
u32 g_CreateCompatibleDC(u16 ss,u16 ap){u16 src=AW(0);int i;if(src&&!dc_get(src)&&di(src)<0)return 0;for(i=0;i<ND&&memory[i].used;i++);if(i==ND)return 0;memory_saved_count[i]=0;memory[i].used=1;memory[i].rop2=13;memory[i].stretchmode=1;memory[i].pen=0x030e;memory[i].brush=0x0300;memory[i].font=0x031a;memory[i].palette=0x031e;memory[i].bitmap=DEFAULT_BITMAP;memory[i].bkcolor=0xffffffUL;return DBASE+2*i;}
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
 int m=di(h),i;long xx,yy,dx,dy,err,e2,sx,sy;u8 index;struct bitmap*b;
 if(m<0||(i=bi(memory[m].bitmap))<0)return 0;
 if(!nullpen&&(style!=0||width>1))return 0;
 b=&bitmaps[i];index=bitmap_nearest(b,color);
 xx=memory[m].curx;yy=memory[m].cury;dx=(long)x-xx;dy=(long)y-yy;
 sx=dx<0?-1:1;sy=dy<0?-1:1;if(dx<0)dx=-dx;if(dy>0)dy=-dy;err=dx+dy;
 if(!nullpen)while(xx!=x||yy!=y){
  memory_rop_pixel(b,(int)xx,(int)yy,index,memory[m].rop2);
  e2=2*err;if(e2>=dy){err+=dy;xx+=sx;}if(e2<=dx){err+=dx;yy+=sy;}
 }
 memory[m].curx=x;memory[m].cury=y;return 1;
}
int bitmap_rectangle(u16 h,i16 l,i16 t,i16 r,i16 bot,u32 pen,u16 ps,u16 pw,int np,u32 brush,u16 bs,int nb)
{
 int m=di(h),i,x,y,x0,y0,x1,y1;u8 pi,bindex;struct bitmap*b;
 if(m<0||(i=bi(memory[m].bitmap))<0||r<=l||bot<=t)return 0;
 if((!np&&(ps!=0||pw>1))||(!nb&&bs!=0))return 0;
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
static void memory_copy(struct bitmap*d,int dm,int dx,int dy,struct bitmap*s,int sm,int sx,int sy,int w,int h)
{
 u8 map[16],i,index;int row,col,yy,xx;u32 rgb;
 for(i=0;i<(s->bpp==1?2:16);i++){
  if(d==s|| (s->bpp==1&&d->bpp==1))map[i]=i;
  else if(d->bpp==1)map[i]=bitmap_rgb(s,i)==memory[sm].bkcolor;
  else{rgb=s->bpp==1?(i?memory[dm].bkcolor:memory[dm].textcolor):bitmap_rgb(s,i);map[i]=bitmap_nearest(d,rgb);}
 }
 for(row=0;row<h;row++){
  yy=d==s&&dy>sy?h-1-row:row;
  for(col=0;col<w;col++){
   xx=d==s&&dy==sy&&dx>sx?w-1-col:col;
   index=bitmap_index(s,sx+xx,sy+yy);bitmap_put(d,dx+xx,dy+yy,map[index]);
  }
 }
}
#ifdef GR_MODE_HAVE_CAPTUREPIXELS
struct capture_stage {struct bitmap image;u16 rows;int valid;};
static void capture_row(void *ctx,unsigned row,const unsigned char *packed,unsigned len)
{
 struct capture_stage*c=ctx;
 if(!c->valid||row!=c->rows||row>=c->image.h||len!=c->image.stride){c->valid=0;return;}
 bitmap_write(&c->image,(u32)row*c->image.stride,packed,len);c->rows++;
}
#endif
static int capture_copy(struct dc*src,int dm,int dx,int dy,int sx,int sy,int w,int h)
{
#ifdef GR_MODE_HAVE_CAPTUREPIXELS
 struct capture_stage c;struct bitmap*d;GR_PALETTE*p=(GR_PALETTE*)w16_scratch;
 GR_SCREEN_INFO si;long xx=dx,yy=dy,ax=sx,ay=sy,ww=w,hh=h;int db=bi(memory[dm].bitmap),sw,sh,j,ok;
 if(db<0||bitmaps[db].bpp!=4)return 0;d=&bitmaps[db];
 if(src->w){sw=src->w->cw;sh=src->w->ch;}else{memset(&si,0,sizeof(si));GrGetScreenInfo(&si);sw=si.cols;sh=si.rows;}
 if(!clip_axis(&xx,&ax,&ww,d->w,sw)||!clip_axis(&yy,&ay,&hh,d->h,sh))return 1;
 memset(&c,0,sizeof(c));c.image.w=ww;c.image.h=hh;c.image.stride=(ww+1)/2;c.image.bpp=4;c.valid=1;

 memset(p,0,sizeof(*p));GrGetSystemPalette(p);if(p->count!=16)return 0;
 for(j=0;j<16;j++){c.image.palette[j].r=p->palette[j].r;c.image.palette[j].g=p->palette[j].g;c.image.palette[j].b=p->palette[j].b;}
 if(!allocate_pixels(&c.image,(u32)c.image.stride*c.image.h))return 0;
 ok=GrCapturePixels(src->wid,src->gc,ax,ay,ww,hh,capture_row,&c)&&c.valid&&c.rows==c.image.h;
 if(ok)memory_copy(d,dm,xx,yy,&c.image,0,0,0,ww,hh);
 release_pixels(&c.image);return ok;
#else
 (void)src;(void)dm;(void)dx;(void)dy;(void)sx;(void)sy;(void)w;(void)h;return 0;
#endif
}
u32 g_BitBlt(u16 ss,u16 ap)
{
 struct dc*d=dc_get(AW(18));int m=di(AW(8)),dm=di(AW(18)),db=-1,b,row,col,end,bit;struct bitmap*s;long dx=(i16)AW(16),dy=(i16)AW(14),w=(i16)AW(12),h=(i16)AW(10),sx=(i16)AW(6),sy=(i16)AW(4);int dw,dh;GR_SCREEN_INFO screen;GR_GC_INFO old;u32 rop=AL(0);
 if((!d&&dm<0)||(rop!=0x00cc0020UL&&rop!=0x008800c6UL)||w<=0||h<=0)return 0;
 if(m<0){struct dc*src=dc_get(AW(8));return src&&dm>=0&&rop==0x00cc0020UL?capture_copy(src,dm,dx,dy,sx,sy,w,h):0;}
 b=bi(memory[m].bitmap);if(b<0)return 0;s=&bitmaps[b];
 if(dm>=0){db=bi(memory[dm].bitmap);if(db<0||rop!=0x00cc0020UL)return 0;dw=bitmaps[db].w;dh=bitmaps[db].h;}
 else if(d->w){dw=d->w->cw;dh=d->w->ch;}else{GrGetScreenInfo(&screen);dw=screen.cols;dh=screen.rows;}
 if(!clip_axis(&dx,&sx,&w,dw,s->w)||!clip_axis(&dy,&sy,&h,dh,s->h))return 1;
 if(dm>=0){memory_copy(&bitmaps[db],dm,dx,dy,s,m,sx,sy,w,h);return 1;}
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
 if(i<0||(i16)AW(4)<14||!seg||(u32)off+14>65536UL)return 0;
 memset(out,0,sizeof(out));bpp=bitmaps[i].bpp;
 out[2]=bitmaps[i].w;out[3]=bitmaps[i].w>>8;
 out[4]=bitmaps[i].h;out[5]=bitmaps[i].h>>8;
 {u16 stride=((bitmaps[i].w*bpp+15)/16)*2;out[6]=stride;out[7]=stride>>8;}
 out[8]=1;out[9]=bpp;fcopy_out(seg,off,out,14);return 14;
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
 if(!mono){memset(p,0,sizeof(*p));GrGetSystemPalette(p);if(p->count!=16)return 0;}
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
 if(bpp==4){memset(p,0,sizeof(*p));GrGetSystemPalette(p);if(p->count!=16)return 0;}
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

/* Bounded COLORONCOLOR memory-to-display nearest-neighbour scaling. Destination
 * clipping never resets sampling phase. Reject unsupported paths before drawing;
 * in particular do not silently approximate BLACKONWHITE/WHITEONBLACK shrink. */
u32 g_StretchBlt(u16 ss,u16 ap)
{
 struct dc*d=dc_get(AW(22));int m=di(AW(12)),b,x,y,end,limitx,limity;
 long dx=(i16)AW(20),dy=(i16)AW(18),dw=(i16)AW(16),dh=(i16)AW(14);
 long sx=(i16)AW(10),sy=(i16)AW(8),sw=(i16)AW(6),sh=(i16)AW(4),left,top,right,bottom;
 struct bitmap*s;GR_SCREEN_INFO si;GR_GC_INFO old;
 int ax,ay,bx,by;
 if(!d||m<0||AL(0)!=0x00cc0020UL||d->stretchmode!=3||
    dw<=0||dh<=0||sw<=0||sh<=0||(b=bi(memory[m].bitmap))<0)return 0;
 /* Logical destination extents are transformed without losing the source phase.
  * Mirrored destination extents remain explicitly unsupported here. */
 if(dx+dw>32767L||dy+dh>32767L)return 0;
 ax=(int)dx;ay=(int)dy;bx=(int)(dx+dw);by=(int)(dy+dh);
 if(!gdi_map_point(d,&ax,&ay)||!gdi_map_point(d,&bx,&by))return 0;
 dx=ax;dy=ay;dw=(long)bx-ax;dh=(long)by-ay;
 if(dw<=0||dh<=0)return 0;
 s=&bitmaps[b];
 if(sx<0||sy<0||sx+sw>s->w||sy+sh>s->h)return 0;
 if(d->w){limitx=d->w->cw;limity=d->w->ch;}
 else{GrGetScreenInfo(&si);limitx=si.cols;limity=si.rows;}
 left=dx<0?0:dx;top=dy<0?0:dy;right=dx+dw;bottom=dy+dh;
 if(right>limitx)right=limitx;if(bottom>limity)bottom=limity;
 if(left>=right||top>=bottom)return 1;
 GrGetGCInfo(d->gc,&old);GrSetGCMode(d->gc,GR_MODE_COPY);
 for(y=(int)top;y<bottom;y++){
  u16 yy=(u16)(sy+((long)y-dy)*sh/dh);
  for(x=(int)left;x<right;x=end){
   u8 index=bitmap_index(s,(u16)(sx+((long)x-dx)*sw/dw),yy);u32 rgb;
   for(end=x+1;end<right;end++)
    if(bitmap_index(s,(u16)(sx+((long)end-dx)*sw/dw),yy)!=index)break;
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
 * Only BI_RGB indexed RGB tables are supported; reject PAL_COLORS/compression. */
static int dib_span(u16 s,u16 o,u32 n)
{return s && (u32)o+n<=65536UL && ((u32)s<<4)+o+n<=0x100000UL;}
static u32 dib_long(u16 s,u16 o)
{return (u32)rd16(s,o)|((u32)rd16(s,o+2)<<16);}
static void dib_wlong(u16 s,u16 o,u32 v)
{wr16(s,o,(u16)v);wr16(s,o+2,(u16)(v>>16));}
static u32 bitmap_dib(u16 ss,u16 ap,int get)
{
 u16 is=AW(4),io=AW(2),bs=AW(8),bo=AW(6),count=AW(10),start=AW(12);
 int i=bi(AW(14)),core,query;struct bitmap*b;
 u32 size,w,h,colors,stride,bytes,rgb;u16 depth,step,x,y,row,at,j;
 u8 map[16],v,index,shift;
 if(i<0||(!dc_get(AW(16))&&di(AW(16))<0)||AW(0)||!dib_span(is,io,4))return 0;
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
 if(colors>(1U<<depth))return 0;step=core?3:4;
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
  for(j=0;j<colors;j++){
   at=io+(u16)size+j*step;rgb=((u32)rd8(is,at)<<16)|((u32)rd8(is,at+1)<<8)|rd8(is,at+2);map[j]=bitmap_nearest(b,rgb);
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
u32 g_SetDIBits(u16 ss,u16 ap){return bitmap_dib(ss,ap,0);}
u32 g_GetDIBits(u16 ss,u16 ap){return bitmap_dib(ss,ap,1);}
