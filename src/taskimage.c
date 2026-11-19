/* Capture before task/session/USER/GDI initialization. Fork supplies a private
 * native data segment; the child restarts from this pristine image, never
 * cleaning up pointers/handles copied from the live parent. */
#include <unistd.h>
#include "taskimage.h"
#include "int21.h"
u16 w16_shared_safe=0xffff;
int task_image_save(int fd,u16 sp,u16 *bytes)
{
 void *brk;u16 n,seg,off=0,count;int ok=0;
 if(w16_shared_safe!=1||_sbrk(0,&brk))return -1;
 n=(u16)(unsigned)brk;
 if(!n||n>0xfff0u||sp<n||(u16)(sp-n)<512||w16_thunk_share())return -1;
 if(seg_alloc((n+15u)/16u,&seg))return -1;
 /* Native near offset zero is the valid start of DGROUP on ELKS. The copy
  * itself is assembly; no C null pointer is dereferenced. */
 fcopy_out(seg,0,(const void *)0,n);
 while(off<n) {
  count=n-off;if(count>W16_SCRATCH)count=W16_SCRATCH;
  fcopy_in(w16_scratch,seg,off,count);
  if(write(fd,w16_scratch,count)!=count)goto done;
  off+=count;
 }
 *bytes=n;ok=1;
 done:seg_free(seg);return ok?0:-1;
}
