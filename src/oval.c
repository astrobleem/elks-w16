/* Solid hairline rounded boxes/ellipses. Pixel-centre coverage uses an integer
 * ellipse equation advanced by differences, never the overflowing a*a*b*b.
 * Each destination pixel is emitted once, including the filled outline. */
#include <stdlib.h>
#include "polygon.h"
int oval_raster(struct poly_point*p,int width,int height,int pen,int brush,
                poly_span output,void*ctx)
{
 int l=p[0].x,t=p[0].y,r=p[1].x,b=p[1].y,w,h,a=p[2].x,c=p[2].y;
 int x,y,k,start,end,inner,left,right,up,down;int *margin;long aa,cc,error;
 if(l< -8191||t< -8191||r>8191||b>8191||r<=l||b<=t)return 0;
 w=r-l;h=b-t;
 if(w>640||h>480||width<1||height<1||width>2048||height>2048||a<0||c<0)return 0;
 if(a>w)a=w;
 if(c>h)c=h;
 margin=malloc(h*sizeof(*margin));if(!margin)return 0;
 for(y=0;y<h;y++)margin[y]=0;
 if(a&&c){
  aa=(long)a*a;cc=(long)c*c;x=a-1;y=(c-1)&1;
  error=cc*(1L-2L*a)+aa*y*y;
  for(;y<c;y+=2){
   while(x>0&&error>0){error+=cc*(4L-4L*x);x-=2;}
   k=(c-1-y)/2;margin[k]=margin[h-1-k]=(a-1-x)/2;
   error+=aa*(4L*y+4);
  }
 }
 start=t<0?-t:0;end=b>height?height-t:h;
 for(y=start;y<end;y++){
  left=l+margin[y];right=r-margin[y];
  up=y?margin[y-1]:w;down=y+1<h?margin[y+1]:w;
  inner=margin[y]+1;if(up>inner)inner=up;if(down>inner)inner=down;
  for(x=left;x<right;){
   int kind=pen&&(x<l+inner||x>=r-inner)?2:brush?1:0;
   k=x+1;
   while(k<right&&(pen&&(k<l+inner||k>=r-inner)?2:brush?1:0)==kind)k++;
   if(kind){int first=x<0?0:x,last=k>width?width:k;
    if(last>first)output(ctx,first,t+y,last-first,kind);
   }
   x=k;
  }
 }
 free(margin);return 1;
}
