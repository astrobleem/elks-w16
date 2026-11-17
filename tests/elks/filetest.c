/* Native ELKS execution of the same file/bitmap roundtrip checks. */
#include <malloc.h>
#include "../../src/w16.h"
int host_live;
int seg_alloc(u16 n,u16 *seg)
{
    if(_fmemalloc(n,seg)) return -1;
    host_live++;return 0;
}
void seg_free(u16 seg) { _fmemfree(seg);host_live--; }
void fcopy_in(void *dst,u16 seg,u16 off,u16 n)
{ u16 i;for(i=0;i<n;i++) ((u8*)dst)[i]=rd8(seg,off+i); }
void fcopy_out(u16 seg,u16 off,const void *src,u16 n)
{ u16 i;for(i=0;i<n;i++) wr8(seg,off+i,((const u8*)src)[i]); }
void fzero(u16 seg,u16 off,u16 n)
{ u16 i;for(i=0;i<n;i++) wr8(seg,off+i,0); }
#include "../files_test.c"
