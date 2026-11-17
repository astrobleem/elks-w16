#!/usr/bin/env python3
import pathlib,subprocess,tempfile,os,re
root=pathlib.Path(__file__).resolve().parents[1]
patch=(root/'patches/nanox-capturepixels.patch').read_text()
a='\n'.join(l[1:] for l in patch.splitlines() if (l.startswith('+') or l.startswith(' ')) and not l.startswith('+++'))
def function(marker):
 matches=[m.start() for m in re.finditer(re.escape(marker),a)];s=a[matches[-1]:];start=s.index('{');p=start+1;level=1
 while level:level+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[:p]
client=function('int GrCapturePixels(')
backend=function('static void GrCapturePixelsWrapper(')
common='''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define ELKS 1
#define MW_FEATURE_AREAS 0
#define GR_MODE_HAVE_CAPTUREPIXELS 0x4000
#define GrNumCapturePixels 126
typedef unsigned GR_DRAW_ID,GR_GC_ID;typedef int GR_COORD,GR_SIZE;
typedef void (*GR_CAPTURE_CALLBACK)(void*,unsigned,const unsigned char*,unsigned);
typedef struct {unsigned char reqType,hilength;unsigned short length;unsigned drawid,gcid;int x,y,width,height;} nxCapturePixelsReq;
'''
cp=common+'''
typedef struct {unsigned gcid;int region,mode;} GR_GC_INFO;
static nxCapturePixelsReq request;
static unsigned cap=0x4000,region,allocs,reads,callbacks,failread,returned=3456;
#define LOCK(x) ((void)0)
#define UNLOCK(x) ((void)0)
#define AllocReq(n) (allocs++, &request)
void GrGetGCInfo(unsigned gc,GR_GC_INFO*g){g->gcid=gc;g->region=region;g->mode=cap;}
int TypedReadBlock(void*p,int n,int type){assert(n==2&&type==126);*(unsigned short*)p=returned;return 0;}
int ReadBlock(void*p,int n){reads++;if(reads==failread)return -1;memset(p,0x12,n);return 0;}
void callback(void*ctx,unsigned row,const unsigned char*p,unsigned len){assert(ctx==&callbacks&&row==callbacks&&len==36&&p[0]==0x12);callbacks++;}
'''+client+'''
int main(void){
assert(GrCapturePixels(3,4,281,116,71,96,callback,&callbacks));assert(callbacks==96&&allocs==1);
callbacks=reads=0;cap=0;assert(!GrCapturePixels(3,4,281,116,71,96,callback,&callbacks));assert(!callbacks&&allocs==1);cap=0x4000;
region=1;assert(!GrCapturePixels(3,4,281,116,71,96,callback,&callbacks));assert(!callbacks&&allocs==1);region=0;
returned=0;assert(!GrCapturePixels(3,4,281,116,71,96,callback,&callbacks));assert(!callbacks);
returned=3456;failread=2;assert(!GrCapturePixels(3,4,281,116,71,96,callback,&callbacks));assert(callbacks==1);
callbacks=reads=failread=0;returned=7;assert(!GrCapturePixels(3,4,281,116,71,96,callback,&callbacks));assert(!callbacks&&reads==1);
assert(!GrCapturePixels(3,4,0,0,640,480,callback,&callbacks));assert(!GrCapturePixels(3,4,-1,0,71,96,callback,&callbacks));
puts("Capture client negotiation, row framing and partial-transport failure checks passed");return 0;}
'''
bp=common+'''
#define GetReqLen(r) ((r)->length)
typedef struct screen SCREEN;typedef SCREEN* PSD;
struct screen {int ncolors,xvirtres,yvirtres;unsigned (*ReadPixel)(SCREEN*,int,int);};
typedef struct {int realized,output,owner,width,height,x,y;SCREEN*psd;} GR_WINDOW;
typedef struct {int owner,regionid;} GR_GC;
static unsigned pixelreads,writes,bytesout,hidden,fixed,covered,failwrite;static unsigned short status;
static unsigned readpixel(SCREEN*s,int x,int y){(void)s;pixelreads++;return (x+y)&15;}
static SCREEN screen={16,640,480,readpixel};static GR_WINDOW root={1,1,0,640,480,0,0,&screen},win={1,1,7,400,300,13,17,&screen};
static GR_WINDOW *rootwp=&root;static GR_GC gc={7,0};static int curclient=7,current_fd=1;
GR_WINDOW*GsFindWindow(unsigned id){return id==3?&win:0;}
GR_GC*GsFindGC(unsigned id){return id==4?&gc:0;}
GR_WINDOW*GsFindVisibleWindow(int x,int y){return covered&&x==350&&y==210?&root:&win;}
void GdCheckCursor(SCREEN*s,int x,int y,int r,int b){(void)s;(void)x;(void)y;(void)r;(void)b;hidden++;}
void GdFixCursor(SCREEN*s){(void)s;fixed++;}
int GsWriteType(int fd,int type){assert(fd==1&&type==126);writes=0;return 0;}
int GsWrite(int fd,void*p,unsigned n){assert(fd==1);if(++writes==failwrite){win.psd=0;return -1;}if(writes==1)status=*(unsigned short*)p;else{assert(n==36);assert(!(((unsigned char*)p)[35]&15));bytesout+=n;}return 0;}
'''+backend+'''
int main(void){nxCapturePixelsReq r={126,0,sizeof(r),3,4,281,116,71,96};
GrCapturePixelsWrapper(&r);assert(status==3456&&pixelreads==6816&&bytesout==3456&&hidden==1&&fixed==1);
pixelreads=bytesout=hidden=fixed=0;covered=1;GrCapturePixelsWrapper(&r);assert(status==0&&!pixelreads&&!bytesout&&!hidden&&!fixed);covered=0;
r.length=4;GrCapturePixelsWrapper(&r);assert(status==0&&!pixelreads);r.length=sizeof(r);
r.hilength=1;GrCapturePixelsWrapper(&r);assert(status==0&&!pixelreads);r.hilength=0;
gc.regionid=1;GrCapturePixelsWrapper(&r);assert(status==0&&!pixelreads);gc.regionid=0;
gc.owner=8;GrCapturePixelsWrapper(&r);assert(status==0&&!pixelreads);gc.owner=7;
r.x=350;GrCapturePixelsWrapper(&r);assert(status==0&&!pixelreads);r.x=281;
win.realized=0;GrCapturePixelsWrapper(&r);assert(status==0&&!pixelreads);
win.realized=1;failwrite=2;GrCapturePixelsWrapper(&r);assert(hidden==1&&fixed==1&&pixelreads==71);
puts("Capture server complete preflight, packed pixels and refusal atomicity checks passed");return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 for name,source in [('client',cp),('server',bp)]:
  p=pathlib.Path(td);(p/(name+'.c')).write_text(source)
  subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-no-pie',str(p/(name+'.c')),'-o',str(p/name)],check=True)
  subprocess.run([str(p/name)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
