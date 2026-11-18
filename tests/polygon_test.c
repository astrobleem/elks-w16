#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "polygon.h"
static unsigned char actual[32][32],visits[32][32];
static void span(void *ctx,int x,int y,int width,int kind)
{
 int end=x+width;(void)ctx;assert(y>=0&&y<32&&x>=0&&end<=32);
 for(;x<end;x++){assert(!visits[y][x]++);actual[y][x]=kind;}
}
static void clear(void){memset(actual,0,sizeof actual);memset(visits,0,sizeof visits);}
static int draw(struct poly_point *p,int n,int pen,int brush)
{return polygon_raster(p,n,32,32,pen,brush,span,0);}
int main(void)
{
 struct poly_point box[]={{2,2},{8,2},{8,8},{2,8}};
 struct poly_point concave[]={{2,2},{10,2},{10,5},{5,5},{5,10},{2,10}};
 struct poly_point bow[]={{2,2},{10,10},{2,10},{10,2}};
 struct poly_point clipped[]={{-3,-3},{4,-3},{4,4},{-3,4}};
 struct poly_point extreme[]={{-8191,-8191},{8191,-8191},{8191,8191},{-8191,8191}};
 struct poly_point reverse[6],line[]={{2,2},{8,8}};
 struct poly_point shallow[]={{-8191,0},{8191,1}},steep[]={{0,-8191},{1,8191}};
 unsigned char saved[32][32];int x,y,i;
 clear();assert(draw(box,4,1,1));
 for(y=0;y<32;y++)for(x=0;x<32;x++){
  int expected=0;
  if(x>=2&&x<=8&&y>=2&&y<=8)expected=x==2||x==8||y==2||y==8?2:1;
  assert(actual[y][x]==expected);
 }
 clear();assert(draw(box,4,0,1));assert(actual[2][2]==1&&!actual[8][8]);
 clear();assert(draw(box,4,1,0));assert(actual[2][2]==2&&!actual[3][3]);
 clear();assert(draw(concave,6,1,1));assert(actual[3][8]==1&&actual[8][3]==1&&!actual[8][8]);
 memcpy(saved,actual,sizeof saved);for(i=0;i<6;i++)reverse[i]=concave[5-i];
 clear();assert(draw(reverse,6,1,1));assert(!memcmp(saved,actual,sizeof saved));
 clear();assert(draw(bow,4,1,1));assert(actual[3][6]==1&&actual[9][6]==1&&!actual[6][3]&&actual[6][6]==2);
 clear();assert(draw(clipped,4,1,1));assert(actual[0][0]==1&&actual[4][4]==2&&!actual[5][5]);
 clear();assert(draw(box,-4,1,1));assert(actual[2][2]==2&&actual[8][3]==2&&!actual[3][2]&&!actual[4][4]&&!actual[8][2]);
 clear();assert(draw(line,2,1,0));for(i=2;i<=8;i++)assert(actual[i][i]==2);
 clear();assert(draw(extreme,4,0,1));for(y=0;y<32;y++)for(x=0;x<32;x++)assert(actual[y][x]==1);
 clear();assert(draw(shallow,2,1,0));for(i=1;i<32;i++)assert(actual[1][i]==2&&!actual[0][i]);
 clear();assert(draw(steep,2,1,0));for(i=1;i<32;i++)assert(actual[i][1]==2&&!actual[i][0]);
 /* Wide round footprints: independent per-pixel union oracle on horizontal,
  * vertical and diagonal centerlines, including clipped caps and joins. */
 {
  int width,axis,cx,cy,step,hit,dx,dy,phase;
  struct poly_point path[3];
  for(width=2;width<=32;width++)for(axis=0;axis<3;axis++){
   path[0].x=path[0].y=-2;
   path[1].x=axis==1?-2:14;path[1].y=axis==0?-2:14;
   clear();assert(draw(path,-2,width,0));phase=!(width&1);
   for(y=0;y<32;y++)for(x=0;x<32;x++){
    hit=0;
    for(step=-2;step<=14;step++){
     cx=axis==1?-2:step;cy=axis==0?-2:step;
     dx=2*(x-cx)+phase;dy=2*(y-cy)+phase;
     if(dx*dx+dy*dy<=width*width)hit=1;
    }
    assert(actual[y][x]==(hit?2:0));
   }
  }
  path[0].x=4;path[0].y=10;path[1].x=20;path[1].y=10;path[2]=path[0];
  clear();assert(draw(path,-3,4,0));
  /* Traversing the same stroke twice must not apply XOR twice. */
  assert(actual[10][10]==2&&visits[10][10]==1);
  clear();assert(!draw(path,-3,33,0)&&!actual[10][10]);
 }
 extreme[0].x=-8192;clear();assert(!draw(extreme,4,1,1));assert(!actual[0][0]);
 assert(!draw(box,1,1,1));assert(!draw(box,129,1,1));
 assert(!polygon_raster(box,4,2049,32,1,1,span,0));
 clear();assert(draw(box,4,0,0));assert(!actual[2][2]);
 puts("Polygon: fill, outline, clipping, crossings, winding reversal and bounds passed");return 0;
}
