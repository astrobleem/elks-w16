#include "w16.h"
#include <assert.h>
#include <setjmp.h>
#include <string.h>

u32 s_OpenSound(u16,u16), s_CloseSound(u16,u16), s_Unavailable(u16,u16);
u32 k_FatalExit(u16,u16), k_FatalAppExit(u16,u16);
static jmp_buf jump;
static u16 exit_code;
void w16_exit16(u16 code) { exit_code=code; longjmp(jump,1); }

int main(void)
{
    int i, j;
    u16 seg;
    const u16 sound_ord[]={1,2,4,5,9,10};
    const u16 sound_args[]={0,0,8,10,0,0};
    for(i=0;i<6;i++){
        int idx=api_find_ord(MOD_SOUND,sound_ord[i]);
        assert(idx>=0 && api_table[idx].argbytes==sound_args[i] && !api_table[idx].flags);
    }
    j=api_find_ord(MOD_KERNEL,115);
    assert(j>=0 && api_table[j].flags==API_RAW && api_table[j].argbytes==4);
    for(i=0;i<3;i++){
        assert(s_OpenSound(0,0)==0);
        assert(s_Unavailable(0,0)==0xffff);
        assert(s_CloseSound(0,0)==0);
    }
    assert(!seg_alloc(4096,&seg));
    wr16(seg,0,42);
    if(!setjmp(jump)){k_FatalExit(seg,0);assert(0);}
    assert(exit_code==42);
    wr16(seg,0,0);
    if(!setjmp(jump)){k_FatalExit(seg,0);assert(0);}
    assert(exit_code!=0);
    wr16(seg,0,0xffff);wr16(seg,2,seg);wr8(seg,0xffff,'X');
    if(!setjmp(jump)){k_FatalAppExit(seg,0);assert(0);}
    assert(exit_code!=0);
    wr16(seg,2,0);
    if(!setjmp(jump)){k_FatalAppExit(seg,0);assert(0);}
    assert(exit_code!=0);
    seg_free(seg);
    return 0;
}
