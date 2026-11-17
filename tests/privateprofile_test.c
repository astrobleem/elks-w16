/* Exercise the actual Win16 argument frames and filesystem-backed handlers. */
#include "w16.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
u32 k_GetPrivateProfileInt(u16,u16), k_GetPrivateProfileString(u16,u16);
static void ptr(u16 s,u16 slot,u16 off) {
    wr16(s,slot,off);
    wr16(s,slot+2,s);
}
int main(void) {
    u16 s;
    FILE *f=fopen("winmine.ini","w");
    assert(f && fputs("[Mine]\nScore=42\nPlayer=Alice\n",f)>=0 && !fclose(f));
    assert(!seg_alloc(64,&s));
    fcopy_out(s,100,"WinMine.INI",12);
    fcopy_out(s,130,"Mine",5);
    fcopy_out(s,150,"Score",6);
    ptr(s,0,100);wr16(s,4,99);ptr(s,6,150);ptr(s,10,130);
    assert(k_GetPrivateProfileInt(s,0)==42);
    fcopy_out(s,100,"Missing.ini",12);
    assert(k_GetPrivateProfileInt(s,0)==99);
    fcopy_out(s,100,"./winmine.ini",14);
    assert(k_GetPrivateProfileInt(s,0)==99);
    fcopy_out(s,100,"WINMINE.INI",12);
    fcopy_out(s,150,"Player",7);fcopy_out(s,180,"Bob",4);
    wr16(s,4,4);ptr(s,6,220);ptr(s,10,180);ptr(s,14,150);ptr(s,18,130);
    assert(k_GetPrivateProfileString(s,0)==3 && !strcmp((char*)host_ptr(s,220,4),"Ali"));
    fcopy_out(s,150,"Other",6);
    assert(k_GetPrivateProfileString(s,0)==3 && !strcmp((char*)host_ptr(s,220,4),"Bob"));
    wr16(s,4,0);wr8(s,220,0xab);
    assert(k_GetPrivateProfileString(s,0)==0 && rd8(s,220)==0xab);
    seg_free(s);
    return 0;
}
