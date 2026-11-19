#include <assert.h>
#include <stdio.h>
#include "w16.h"
#include "launch.h"
/* Standalone loader check: inject session counts without a native launcher. */
static u16 session_task_count=1;
u16 launch_tasks(void){return session_task_count;}
extern int host_live,host_fail_after;
u32 k_GetModuleHandle(u16,u16),k_GetNumTasks(u16,u16),k_GetDriveType(u16,u16);
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
    assert(!ne_reserve_library_heap(&dll,8192));
    assert(dll.heap==32 && dll.dgroup_size==8208);
    assert(ne_reserve_library_heap(&dll,31)==W16_EUNSUPP);
    assert(ne_reserve_library_heap(&dll,65535)==W16_EUNSUPP);
    assert(!ne_check_imports(&dll));
    assert(!ne_open(&app,argv[2]));
    assert(ne_bind_library(&app,0,&dll)==W16_EIMPORT);
    assert(!ne_load(&dll));
    assert(ne_reserve_library_heap(&dll,8192)==W16_EUNSUPP);
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
    {u16 args;assert(!seg_alloc(16,&args));w16_mod=&app;
     wr32(args,0,0);assert(k_GetModuleHandle(args,0)==app.dgroup&&k_GetNumTasks(0,0)==1);
     session_task_count=3;assert(k_GetNumTasks(0,0)==3);
     app.loaded=0;assert(!k_GetModuleHandle(args,0)&&!k_GetNumTasks(0,0));app.loaded=1;session_task_count=1;
     fcopy_out(args,32,"dllname.exe",12);wr16(args,0,32);wr16(args,2,args);
     assert(k_GetModuleHandle(args,0)==app.dgroup);
     fcopy_out(args,32,"test.DLL",9);assert(k_GetModuleHandle(args,0)==dll.dgroup);
     dll.loaded=0;assert(!k_GetModuleHandle(args,0));dll.loaded=1;
     fcopy_out(args,32,"USER",5);assert(!k_GetModuleHandle(args,0)); /* no fabricated provider heap */
     fcopy_out(args,32,"missing",8);assert(!k_GetModuleHandle(args,0));
     fcopy_out(args,32,"C:\\test.dll",12);assert(!k_GetModuleHandle(args,0));
     fcopy_out(args,32,"test.bad",9);assert(!k_GetModuleHandle(args,0));
     assert(!k_GetModuleHandle(args,0xffff));
     wr16(args,0,2);assert(k_GetDriveType(args,0)==3);
     wr16(args,0,0);assert(!k_GetDriveType(args,0));wr16(args,0,3);assert(!k_GetDriveType(args,0));
     wr16(args,0,0xffff);assert(!k_GetDriveType(args,0)&&!k_GetDriveType(args,0xffff));
     w16_mod=0;assert(!k_GetModuleHandle(args,0)&&!k_GetNumTasks(0,0));seg_free(args);
    }
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
