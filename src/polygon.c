/* Rasterize fill and outline together: a destination pixel is visited once,
 * even at shared vertices, crossings and overlapping edges (important for R2).
 * Limits are checked before output. Extreme coordinates and complex styles
 * are rejected rather than silently drawing an approximation. */
#include <stdlib.h>
#include <string.h>
#include "polygon.h"
int polygon_raster(struct poly_point *p,int n,int width,int height,
                   int pen,int brush,poly_span output,void *ctx)
{
 int i,j,x,y,l,r,t,b,start,kind,nib[32],lo,hi,phase,closed=n>0; unsigned char *row;
 if(n< -128||n>128||pen<0||pen>32)return 0;
 lo=pen/2;hi=(pen-1)/2;phase=!(pen&1);
 if(pen>1)for(i=-lo;i<=hi;i++){
  int yy=2*i+phase;
  for(j=-lo;(2*j+phase)*(2*j+phase)+yy*yy>pen*pen;j++);
  nib[i+lo]=j;
 }
 if(n<0)n=-n;
 if(!closed)brush=0;
 if(n<2||n>128||width<1||width>2048||height<1||height>2048)return 0;
 l=r=p[0].x;t=b=p[0].y;
 for(i=0;i<n;i++){
  if(p[i].x< -8191||p[i].x>8191||p[i].y< -8191||p[i].y>8191)return 0;
  if(p[i].x<l)l=p[i].x;
  if(p[i].x>r)r=p[i].x;
  if(p[i].y<t)t=p[i].y;
  if(p[i].y>b)b=p[i].y;
 }
 if(pen>1){l-=lo;t-=lo;r+=hi;b+=hi;}
 if(l<0)l=0;
 if(t<0)t=0;
 if(r>=width)r=width-1;
 if(b>=height)b=height-1;
 if(l>r||t>b||(!pen&&!brush))return 1;
 row=malloc(r-l+1);
 if(!row)return 0;
 for(y=t;y<=b;y++){
  memset(row,0,r-l+1);
  if(brush)for(x=l;x<=r;x++){
   int inside=0;
   for(i=0,j=n-1;i<n;j=i++){
    long dy=(long)p[j].y-p[i].y;
    if((p[i].y>y)!=(p[j].y>y)){
     long a=((long)x-p[i].x)*dy;
     long c=((long)y-p[i].y)*((long)p[j].x-p[i].x);
     if(dy>0?a<c:a>c)inside=!inside;
    }
   }
   row[x-l]=inside?1:0;
  }
  if(pen)for(i=closed?0:1,j=closed?n-1:0;i<n;j=i++){
   /* Differences and error fit 16 bits; doubled error needs 32 bits. */
   int xx=p[j].x,yy=p[j].y,dx=p[i].x-xx,dy=p[i].y-yy;
   int sx=dx<0?-1:1,sy=dy<0?-1:1,err;long e2;
   if(dx<0)dx=-dx;
   if(dy>0)dy=-dy;
   err=dx+dy;
   /* A circular pixel footprint gives genuine width and round joins/caps.
    * Union it into the scanline mask before applying any destination ROP.
    * Cosmetic edges preserve their established excluded final endpoint. */
   for(;;){
    if(pen>1){
     int oy=y-yy;
     if(oy>=-lo&&oy<=hi){
      int first=xx+nib[oy+lo],last=xx-nib[oy+lo]-phase,q;
      if(first<l)first=l;
      if(last>r)last=r;
      for(q=first;q<=last;q++)row[q-l]=2;
     }
    }else if(xx==p[i].x&&yy==p[i].y)break;
    else if(yy==y&&xx>=l&&xx<=r)row[(int)xx-l]=2;
    if(xx==p[i].x&&yy==p[i].y)break;
    e2=2L*err;
    if(e2>=dy){err+=dy;xx+=sx;}
    if(e2<=dx){err+=dx;yy+=sy;}
   }
  }
  for(x=l;x<=r;){
   start=x;kind=row[x-l];while(x<=r&&row[x-l]==kind)x++;
   if(kind)output(ctx,start,y,x-start,kind);
  }
 }
 free(row);return 1;
}
