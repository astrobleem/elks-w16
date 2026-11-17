/* MM_TEXT and MM_ANISOTROPIC coordinate state. Keep the current position in
 * logical units; map only at the drawing boundary. Never narrow overflow. */
#include "wapi.h"
void gdi_map_init(struct dc *d)
{
    d->mapmode=1;
    d->wx=d->wy=d->vx=d->vy=0;
    d->wex=d->wey=d->vex=d->vey=1;
}
static int axis(int logical,i16 wo,i16 vo,i16 we,i16 ve,int mode,int *out)
{
    long n=(long)logical-wo,den=we,v;
    if(mode==8) {
        if(!den)return 0;
        n*=ve;
        if(den<0){n=-n;den=-den;}
        v=n<0?-((-n+den/2)/den):(n+den/2)/den;
    } else v=n;
    v+=vo;
    if(v < -32768L || v > 32767L)return 0;
    *out=(int)v;return 1;
}
int gdi_map_point(struct dc *d,int *x,int *y)
{
    int a,b;
    if(!axis(*x,d->wx,d->vx,d->wex,d->vex,d->mapmode,&a)||
       !axis(*y,d->wy,d->vy,d->wey,d->vey,d->mapmode,&b))return 0;
    *x=a;*y=b;return 1;
}
int gdi_map_rect(struct dc*d,int*l,int*t,int*r,int*b)
{
    int a=*l,c=*r,y=*t,z=*b,k;
    if(!gdi_map_point(d,&a,&y)||!gdi_map_point(d,&c,&z))return 0;
    if(a>c){k=a;a=c;c=k;}if(y>z){k=y;y=z;z=k;}
    if((long)c-a>32767L||(long)z-y>32767L)return 0;
    *l=a;*r=c;*t=y;*b=z;return 1;
}
static u32 pack(i16 x,i16 y){return ((u32)(u16)y<<16)|(u16)x;}
u32 g_SetMapMode(u16 ss,u16 ap)
{
    struct dc*d=dc_get(AW(2));u16 mode=AW(0),old;
    if(!d||(mode!=1&&mode!=8))return 0;
    old=d->mapmode;d->mapmode=mode;
    if(mode==1)d->wex=d->wey=d->vex=d->vey=1;
    return old;
}
static u32 set_pair(u16 ss,u16 ap,int viewport,int extent)
{
    struct dc*d=dc_get(AW(4));i16 x=(i16)AW(2),y=(i16)AW(0),*px,*py;u32 old;
    if(!d)return extent?0:bitmap_origin(AW(4),1,viewport,x,y);
    if(extent){px=viewport?&d->vex:&d->wex;py=viewport?&d->vey:&d->wey;}
    else{px=viewport?&d->vx:&d->wx;py=viewport?&d->vy:&d->wy;}
    old=pack(*px,*py);
    if(extent&&d->mapmode==1)return old;
    if(extent&&(!x||!y))return 0;
    *px=x;*py=y;return old;
}
u32 g_SetWindowOrg(u16 ss,u16 ap){return set_pair(ss,ap,0,0);}
u32 g_SetViewportOrg(u16 ss,u16 ap){return set_pair(ss,ap,1,0);}
u32 g_SetWindowExt(u16 ss,u16 ap){return set_pair(ss,ap,0,1);}
u32 g_SetViewportExt(u16 ss,u16 ap){return set_pair(ss,ap,1,1);}
u32 g_GetWindowOrg(u16 ss,u16 ap){struct dc*d=dc_get(AW(0));return d?pack(d->wx,d->wy):bitmap_origin(AW(0),0,0,0,0);}

static int unmap_point(struct dc*d,int*x,int*y)
{
    int a,b;
    if(!axis(*x,d->vx,d->wx,d->vex,d->wex,d->mapmode,&a)||
       !axis(*y,d->vy,d->wy,d->vey,d->wey,d->mapmode,&b))return 0;
    *x=a;*y=b;return 1;
}
static u32 points(u16 ss,u16 ap,int inverse)
{
    struct dc*d=dc_get(AW(6));u16 n=AW(0),off=AW(2),seg=AW(4),i;int x,y;
    if(!d||(i16)n<0)return 0;
    if(!n)return 1;
    if((!seg&&!off)||(u32)off+(u32)n*4>65536UL||
       ((u32)seg<<4)+off+(u32)n*4>0x100000UL)return 0;
    /* Validate every result before mutating the caller's array. */
    for(i=0;i<n;i++){
        x=(i16)rd16(seg,off+4*i);y=(i16)rd16(seg,off+4*i+2);
        if(!(inverse?unmap_point(d,&x,&y):gdi_map_point(d,&x,&y)))return 0;
    }
    for(i=0;i<n;i++){
        x=(i16)rd16(seg,off+4*i);y=(i16)rd16(seg,off+4*i+2);
        if(inverse)unmap_point(d,&x,&y);else gdi_map_point(d,&x,&y);
        wr16(seg,off+4*i,(u16)x);wr16(seg,off+4*i+2,(u16)y);
    }
    return 1;
}
u32 g_DPtoLP(u16 ss,u16 ap){return points(ss,ap,1);}
u32 g_LPtoDP(u16 ss,u16 ap){return points(ss,ap,0);}
