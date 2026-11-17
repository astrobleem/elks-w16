/* Process-local registered-message names; no cross-process broker is present. */
#include "w16.h"
#include <stdlib.h>
#include <string.h>
#define REGISTERED_MAX 32
#define REGISTERED_NAME_MAX 255
static char *names[REGISTERED_MAX];
void registered_reset(void)
{
    u16 i;
    for(i=0;i<REGISTERED_MAX;i++) { free(names[i]); names[i]=0; }
}
u32 u_RegisterWindowMessage(u16 ss,u16 ap)
{
    char name[REGISTERED_NAME_MAX+1], *copy;
    u16 off,seg,n,i,slot=REGISTERED_MAX;
    u32 linear;
    if(ap>0xfffcu) return 0;
    off=AW(0); seg=AW(2);
    if(!seg && !off) return 0;
    linear=((u32)seg<<4)+off;
    for(n=0;n<=REGISTERED_NAME_MAX;n++) {
        if((u32)off+n>0xffffUL || linear+n>0xfffffUL) return 0;
        name[n]=(char)rd8(seg,off+n);
        if(!name[n]) break;
    }
    if(!n || n>REGISTERED_NAME_MAX) return 0;
    for(i=0;i<REGISTERED_MAX;i++) {
        if(names[i]) { if(!strcmp(names[i],name)) return 0xc000u+i; }
        else if(slot==REGISTERED_MAX) slot=i;
    }
    if(slot==REGISTERED_MAX) return 0;
    copy=malloc(n+1);
    if(!copy) return 0;
    memcpy(copy,name,n+1); names[slot]=copy;
    return 0xc000u+slot;
}
