/* Bounded resource bitmaps and source-only memory DCs. Unsupported raster
 * operations and memory destinations fail; no memory DC aliases the screen. */
#include <string.h>
#include "wapi.h"
#include "dib.h"
#define BBASE 0x0a00
#define DBASE 0x0c00
#define NB 16
#define ND 8
#define DEFAULT_BITMAP 0x0aff
struct bitmap {u16 w,h,seg,stride;GR_WINDOW_ID pix;u8 used;};
static struct bitmap bitmaps[NB];
static struct {u16 bitmap;u8 used;} memory[ND];
static int bi(u16 h){u16 n=(h-BBASE)/2;return h>=BBASE&&!(h&1)&&n<NB&&bitmaps[n].used?n:-1;}
static int di(u16 h){u16 n=(h-DBASE)/2;return h>=DBASE&&!(h&1)&&n<ND&&memory[n].used?n:-1;}
static void release_bitmap(int i){if(bitmaps[i].seg)seg_free(bitmaps[i].seg);if(bitmaps[i].pix)GrDestroyWindow(bitmaps[i].pix);memset(&bitmaps[i],0,sizeof(bitmaps[i]));}
void bitmap_shutdown(void){int i;for(i=0;i<NB;i++)release_bitmap(i);memset(memory,0,sizeof(memory));}
int bitmap_delete(u16 h){int i=bi(h),j;if(i<0)return -1;for(j=0;j<ND;j++)if(memory[j].used&&memory[j].bitmap==h)return 0;release_bitmap(i);return 1;}
int bitmap_deletedc(u16 h){int i=di(h);if(i<0)return -1;memset(&memory[i],0,sizeof(memory[i]));return 1;}
u16 bitmap_select(u16 dc,u16 h){int i=di(dc),j=bi(h),k;u16 old;if(i<0||(j<0&&h!=DEFAULT_BITMAP))return 0;for(k=0;k<ND;k++)if(k!=i&&memory[k].used&&memory[k].bitmap==h&&h!=DEFAULT_BITMAP)return 0;old=memory[i].bitmap;memory[i].bitmap=h;return old;}
u32 g_CreateCompatibleDC(u16 ss,u16 ap){u16 src=AW(0);int i;if(src&&!dc_get(src)&&di(src)<0)return 0;for(i=0;i<ND&&memory[i].used;i++);if(i==ND)return 0;memory[i].used=1;memory[i].bitmap=DEFAULT_BITMAP;return DBASE+2*i;}
struct resource {u32 off;u16 len;};
static int read_bitmap(void *ctx,u32 off,void *dst,u16 n){struct resource*r=ctx;if(off>r->len||n>r->len-off||r->off+off<r->off)return 0;return ne_read(w16_mod,r->off+off,dst,n)==0;}
u32 u_LoadBitmap(u16 ss,u16 ap)
{
 struct resource r;struct dib_info d;char name[34];u16 id,x,y,n,j,end,stride;u8 pixels[32];int i;GR_GC_ID gc=0;
 if(!w16_mod||AW(4)!=w16_mod->dgroup)return 0;
 res_name(AW(0),AW(2),name,&id);
 if(ne_res_find(w16_mod,2,name[0]?name:0,id,&r.off,&r.len)||!dib_open(&d,read_bitmap,&r,r.len)||d.width>640||d.height>480)return 0;
 stride=d.bpp==1?((d.width+15)/16)*2:(((d.width+1)/2+3)&~3);
 if((u32)stride*d.height>16384UL)return 0;
 if(d.bpp==1&&(d.colors!=2||d.palette[0].r||d.palette[0].g||d.palette[0].b||d.palette[1].r!=255||d.palette[1].g!=255||d.palette[1].b!=255))return 0;
 for(i=0;i<NB&&bitmaps[i].used;i++);if(i==NB)return 0;
 bitmaps[i].w=d.width;bitmaps[i].h=d.height;bitmaps[i].stride=stride;
 if(d.bpp==1){if(seg_alloc(((u32)stride*d.height+15)/16,&bitmaps[i].seg))goto fail;fzero(bitmaps[i].seg,0,stride*d.height);}
 else{bitmaps[i].pix=GrNewPixmap(d.width,d.height,0);if(!bitmaps[i].pix)goto fail;gc=GrNewGC();if(!gc)goto fail;GrSetGCMode(gc,GR_MODE_COPY);}
 for(y=0;y<d.height;y++)for(x=0;x<d.width;x+=n){
  n=d.width-x;if(n>32)n=32;if(!dib_read_indices(&d,read_bitmap,&r,x,y,pixels,n))goto fail;
  for(j=0;j<n;j=end){
   end=j+1;
   if(d.bpp==1){if(pixels[j]){u16 at=y*stride+(x+j)/8;wr8(bitmaps[i].seg,at,rd8(bitmaps[i].seg,at)|(0x80>>((x+j)&7)));}}
   else{struct dib_rgb*c=&d.palette[pixels[j]];while(end<n&&pixels[end]==pixels[j])end++;GrSetGCForeground(gc,GR_RGB(c->r,c->g,c->b));GrFillRect(bitmaps[i].pix,gc,x+j,y,end-j,1);}
  }
 }
 if(gc)GrDestroyGC(gc);bitmaps[i].used=1;return BBASE+2*i;
fail: if(gc)GrDestroyGC(gc);release_bitmap(i);return 0;
}
/* Clip both rectangles together in widened arithmetic, preserving source phase. */
static int clip_axis(long *dst,long *src,long *n,long limit,long slimit){long q;if(*dst<0){q=-*dst;*dst=0;*src+=q;*n-=q;}if(*src<0){q=-*src;*src=0;*dst+=q;*n-=q;}if(*n>limit-*dst)*n=limit-*dst;if(*n>slimit-*src)*n=slimit-*src;return *n>0;}
u32 g_BitBlt(u16 ss,u16 ap)
{
 struct dc*d=dc_get(AW(18));int m=di(AW(8)),b,row,col,end,bit;struct bitmap*s;long dx=(i16)AW(16),dy=(i16)AW(14),w=(i16)AW(12),h=(i16)AW(10),sx=(i16)AW(6),sy=(i16)AW(4);int dw,dh;GR_SCREEN_INFO screen;GR_GC_INFO old;
 if(!d||m<0||AL(0)!=0x00cc0020UL||w<=0||h<=0)return 0;b=bi(memory[m].bitmap);if(b<0)return 0;s=&bitmaps[b];
 if(d->w){dw=d->w->cw;dh=d->w->ch;}else{GrGetScreenInfo(&screen);dw=screen.cols;dh=screen.rows;}
 if(!clip_axis(&dx,&sx,&w,dw,s->w)||!clip_axis(&dy,&sy,&h,dh,s->h))return 1;
 if(s->pix){GrCopyArea(d->wid,d->gc,dx,dy,w,h,s->pix,sx,sy,MWROP_COPY);return 1;}
 GrGetGCInfo(d->gc,&old);GrSetGCMode(d->gc,GR_MODE_COPY);
 for(row=0;row<h;row++)for(col=0;col<w;col=end){
  bit=rd8(s->seg,(sy+row)*s->stride+(sx+col)/8)&(0x80>>((sx+col)&7));
  for(end=col+1;end<w;end++)if(!!(rd8(s->seg,(sy+row)*s->stride+(sx+end)/8)&(0x80>>((sx+end)&7)))!=!!bit)break;
  GrSetGCForeground(d->gc,rgb_to_gr(bit?d->bkcolor:d->textcolor));GrFillRect(d->wid,d->gc,dx+col,dy+row,end-col,1);
 }
 GrSetGCForeground(d->gc,old.foreground);GrSetGCMode(d->gc,old.mode);return 1;
}

/* Win16 BITMAP: four WORDs, two BYTEs, then a null far bmBits pointer.
 * Native backing pixels are not a caller-accessible DIB section. */
u32 g_GetObject(u16 ss,u16 ap)
{
 int i=bi(AW(6));u16 off=AW(0),seg=AW(2),bpp;u8 out[14];
 if(i<0||(i16)AW(4)<14||!seg||(u32)off+14>65536UL)return 0;
 memset(out,0,sizeof(out));bpp=bitmaps[i].seg?1:4;
 out[2]=bitmaps[i].w;out[3]=bitmaps[i].w>>8;
 out[4]=bitmaps[i].h;out[5]=bitmaps[i].h>>8;
 {u16 stride=((bitmaps[i].w*bpp+15)/16)*2;out[6]=stride;out[7]=stride>>8;}
 out[8]=1;out[9]=bpp;fcopy_out(seg,off,out,14);return 14;
}
