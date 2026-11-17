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
 int i,j,x,y,l,r,t,b,start,kind; unsigned char *row;
 if(n<2||n>128||width<1||width>2048||height<1||height>2048)return 0;
 l=r=p[0].x;t=b=p[0].y;
 for(i=0;i<n;i++){
  if(p[i].x< -8191||p[i].x>8191||p[i].y< -8191||p[i].y>8191)return 0;
  if(p[i].x<l)l=p[i].x;
  if(p[i].x>r)r=p[i].x;
  if(p[i].y<t)t=p[i].y;
  if(p[i].y>b)b=p[i].y;
 }
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
  if(pen)for(i=0,j=n-1;i<n;j=i++){
   /* Bounds above keep differences and doubled error within signed 16 bits. */
   int xx=p[j].x,yy=p[j].y,dx=p[i].x-xx,dy=p[i].y-yy;
   int sx=dx<0?-1:1,sy=dy<0?-1:1,err,e2;
   if(dx<0)dx=-dx;
   if(dy>0)dy=-dy;
   err=dx+dy;
   /* Endpoints excluded; the next edge owns the shared vertex. */
   while(xx!=p[i].x||yy!=p[i].y){
    if(yy==y&&xx>=l&&xx<=r)row[(int)xx-l]=2;
    e2=2*err;
   if(e2>=dy){err+=dy;xx+=sx;}if(e2<=dx){err+=dx;yy+=sy;}
   }
  }
  for(x=l;x<=r;){
   start=x;kind=row[x-l];while(x<=r&&row[x-l]==kind)x++;
   if(kind)output(ctx,start,y,x-start,kind);
  }
 }
 free(row);return 1;
}
