#include "dib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static u8 data[200000];
static u32 length;
static int fail, calls;
static int reader(void *ctx,u32 off,void *dst,u16 n)
{
    (void)ctx; assert(n<=40); calls++;
    if (calls==fail) return 0;
    assert(off<=length && n<=length-off);
    memcpy(dst,data+off,n); return 1;
}
static void put(u16 off,u32 v,u16 n)
{ while(n--) { data[off++]=(u8)v; v>>=8; } }
static void verify(void)
{
    struct dib_info d;
    u8 out[300]; u16 x,y,n,i, pp; u32 row;
    calls=0; fail=0; assert(dib_open(&d,reader,0,length));
    pp=8/d.bpp;
    for(y=0;y<d.height;y++) for(x=0;x<d.width;x+=17) {
        n=d.width-x; if(n>257)n=257;
        memset(out,0xad,sizeof(out));
        assert(dib_read_indices(&d,reader,0,x,y,out,n));
        row=d.pixels+(u32)(d.height-1-y)*d.stride;
        for(i=0;i<n;i++) assert(out[i]==((data[row+(x+i)/pp] >>
            (8-d.bpp-((x+i)%pp)*d.bpp))&((1<<d.bpp)-1)));
        assert(out[n]==0xad);
    }
    assert(!dib_read_indices(&d,reader,0,d.width,0,out,1));
    assert(!dib_read_indices(&d,reader,0,0,d.height,out,1));
    calls=0; fail=1; assert(!dib_read_indices(&d,reader,0,0,0,out,1));
    calls=0; assert(!dib_open(&d,reader,0,length)); fail=0;
}
int main(int argc,char **argv)
{
    u16 hdr,bpp,w,entry,colors,x,y,pp; u32 pixels,stride; int i;
    struct dib_info d;
    for(hdr=12;hdr<=40;hdr+=28) for(bpp=1;bpp<=4;bpp+=3)
    for(w=1;w<=259;w+=2) {
        memset(data,0,sizeof(data)); colors=1<<bpp; entry=hdr==12?3:4;
        pixels=hdr+colors*entry; stride=((w*bpp+31)/32)*4;
        length=pixels+stride*2; pp=8/bpp;
        put(0,hdr,4); put(4,w,hdr==12?2:4); put(hdr==12?6:8,2,hdr==12?2:4);
        put(hdr==12?8:12,1,2); put(hdr==12?10:14,bpp,2);
        for(x=0;x<colors;x++) { data[hdr+x*entry]=x; data[hdr+x*entry+2]=colors-x; }
        for(y=0;y<2;y++) for(x=0;x<w;x++)
            data[pixels+y*stride+x/pp]|=((x+y)%colors)<<(8-bpp-(x%pp)*bpp);
        verify(); assert(!dib_open(&d,reader,0,length-1));
        put(hdr==12?8:12,2,2); assert(!dib_open(&d,reader,0,length));
        put(hdr==12?8:12,1,2); put(hdr==12?10:14,8,2);
        assert(!dib_open(&d,reader,0,length));
    }
    for(i=1;i<argc;i++) {
        FILE *f=fopen(argv[i],"rb"); assert(f);
        length=fread(data,1,sizeof(data),f); assert(!ferror(f)&&feof(f)); fclose(f);
        verify();
    }
    puts("DIB synthetic and resource pixel checks passed"); return 0;
}
