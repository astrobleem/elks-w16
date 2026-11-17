#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "polygon.h"
static unsigned char pixels[80][80],seen[80][80];
static void span(void*c,int x,int y,int w,int kind){int i;(void)c;assert(x>=0&&y>=0&&x+w<=80&&y<80);for(i=x;i<x+w;i++){assert(!seen[y][i]++);pixels[y][i]=kind;}}
static int inside(int x,int y,int w,int h,int a,int b){
 int dx,dy;if(x<0||y<0||x>=w||y>=h)return 0;
 if(!a||!b)return 1;
 dx=2*x-w+1;if(dx<0)dx=-dx;dx-=w-a;if(dx<0)dx=0;
 dy=2*y-h+1;if(dy<0)dy=-dy;dy-=h-b;if(dy<0)dy=0;
 return (int64_t)dx*dx*b*b+(int64_t)dy*dy*a*a<=(int64_t)a*a*b*b;
}
int main(void){
 int w,h,a,b,x,y,pen,brush,k;unsigned long count=0;struct poly_point p[3];
 for(w=1;w<=64;w+=3)for(h=1;h<=64;h+=3)for(k=0;k<3;k++){
  a=k==0?w:k==1?w/2:0;b=k==0?h:k==1?h/2:0;
  p[0].x=p[0].y=3;p[1].x=3+w;p[1].y=3+h;p[2].x=a;p[2].y=b;
  for(pen=0;pen<2;pen++)for(brush=0;brush<2;brush++){
   memset(pixels,0,sizeof pixels);memset(seen,0,sizeof seen);
   assert(oval_raster(p,80,80,pen,brush,span,0));
   for(y=0;y<80;y++)for(x=0;x<80;x++){
    int xx=x-3,yy=y-3,in=inside(xx,yy,w,h,a,b),edge=in&&(!inside(xx-1,yy,w,h,a,b)||!inside(xx+1,yy,w,h,a,b)||!inside(xx,yy-1,w,h,a,b)||!inside(xx,yy+1,w,h,a,b));
    assert(pixels[y][x]==(pen&&edge?2:brush&&in?1:0));count++;
   }
  }
 }
 p[0].x=-3;p[0].y=-5;p[1].x=12;p[1].y=10;p[2].x=p[2].y=10;
 memset(seen,0,sizeof seen);assert(oval_raster(p,80,80,1,1,span,0));
 p[1].x=700;assert(!oval_raster(p,80,80,1,1,span,0));
 printf("Ellipse/RoundRect: %lu independent fill/outline pixel checks passed\n",count);return 0;
}
