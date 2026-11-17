#include <assert.h>
#include <stdio.h>
#include "w16.h"
extern int host_live,host_fail_after;
int main(int argc,char **argv)
{
    struct ne_module app,dll;
    u16 s,o;
    int e;
    assert(argc==4);
    e=ne_open(&dll,argv[1]); assert(e==W16_EUNSUPP); ne_free(&dll);
    assert(!ne_open_library(&dll,argv[1]));
    assert(dll.library && !dll.loaded && dll.init_sp==0);
    assert(!ne_export(&dll,"draw",0,&s,&o) && s==1 && o==0);
    assert(!ne_export(&dll,"X",0,&s,&o)); /* resident export */
    assert(!ne_export(&dll,"DRAWALIAS",0,&s,&o));
    assert(ne_export(&dll,0,0,&s,&o));
    assert(ne_export(&dll,"DLLNAME",0,&s,&o));
    assert(ne_export(&dll,"missing",0,&s,&o));
    assert(ne_export(&dll,0,2,&s,&o)); /* out of code bounds */
    assert(ne_export(&dll,0,3,&s,&o)); /* not exported */
    assert(ne_export(&dll,0,4,&s,&o)); /* missing */
    assert(!ne_check_imports(&dll));
    assert(!ne_open(&app,argv[2]));
    assert(ne_bind_library(&app,0,&dll)==W16_EIMPORT);
    assert(!ne_load(&dll));
    assert(rd8(dll.seg[0].sel,0)==0xb8);
    assert(rd16(dll.seg[0].sel,1)==dll.dgroup);
    assert(!ne_bind_library(&app,0,&dll));
    assert(ne_bind_library(&app,1,&dll)==W16_EIMPORT);
    assert(!ne_check_imports(&app));
    assert(!ne_load(&app));
    assert(rd16(app.seg[0].sel,4)==0);
    assert(rd16(app.seg[0].sel,6)==dll.seg[0].sel);
    assert(rd16(app.seg[0].sel,10)==0);
    assert(rd16(app.seg[0].sel,12)==dll.seg[0].sel);
    assert(ne_bind_library(&app,0,&dll)==W16_EIMPORT);
    ne_free(&app); ne_free(&dll); assert(host_live==0);

    assert(!ne_open_library(&dll,argv[1]));
    assert(!ne_check_imports(&dll));
    host_fail_after=1;
    assert(ne_load(&dll)==W16_ENOMEM);
    ne_free(&dll); assert(host_live==0); host_fail_after=-1;
    assert(!ne_open_library(&dll,argv[3]));
    assert(ne_export(&dll,"draw",0,&s,&o)); /* truncated name entry */
    ne_free(&dll);
    puts("DLL parsing, exports, prologue, named/ordinal relocations and cleanup passed");
    return 0;
}
