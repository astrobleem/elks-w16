#include "w16.h"
#include <assert.h>
#include <string.h>
u32 sh_ShellAbout(u16,u16);
static char title[64],text[256];static u16 result=1;
u16 message_box(const char*t,const char*c,u16 type){
    assert(type==0);strcpy(title,c);strcpy(text,t);return result;
}
int main(void){
    u16 s; assert(!seg_alloc(4096,&s));
    wr16(s,0,123);wr16(s,2,200);wr16(s,4,s);wr16(s,6,100);wr16(s,8,s);wr16(s,10,0);
    fcopy_out(s,100,"Mines",6);fcopy_out(s,200,"Author",7);
    assert(sh_ShellAbout(s,0)==1&&!strcmp(title,"About Mines")&&!strcmp(text,"Mines - Author"));
    fcopy_out(s,100,"Caption#Application",20);
    assert(sh_ShellAbout(s,0)==1&&!strcmp(title,"Caption")&&!strcmp(text,"Application - Author"));
    wr16(s,4,0);assert(sh_ShellAbout(s,0)==1&&!strcmp(text,"Application"));
    wr16(s,6,65535);wr8(s,65535,'X');assert(sh_ShellAbout(s,0)==1&&!strcmp(text,"X"));
    result=0;assert(sh_ShellAbout(s,0)==0);
    seg_free(s);return 0;
}
