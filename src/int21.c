/* ELKS switches user registers, not real-mode interrupt vectors. Install a
 * single resident router, never a second process-local INT 21h vector. Each
 * private native-NE task registers its guest SS and own DOS handler; the
 * existing thunk then selects that task's loader DS/stack as before.
 *
 * This is routing/lifetime infrastructure, not a WinExec implementation.
 * It cannot make two a.out loaders sharing CS:w16_ldr_ds safe. Attach only
 * after w16_thunk_init has accepted a private native code context. */
#include <stdlib.h>
#include "int21.h"
void w16_int21_router(void);
u16 W16_CDECL w16_route_begin(u16,u16,u16);
u16 W16_CDECL w16_route_add(u16,u16,u16,u16,u16,u16);
i16 W16_CDECL w16_route_finish(void);
struct route_state {u32 vector;u16 table,key,token;u8 owner;};
static struct route_state *route;

static int signature(u32 v)
{
 u16 s=(u16)(v>>16),o=(u16)v;
 return o<=65518u&&((u32)s<<4)+o+18<=0x100000UL&&
  rd16(s,o+2)==W16_INT21_MAGIC0&&rd16(s,o+4)==W16_INT21_MAGIC1&&rd16(s,o+6)==1;
}
static int add(u16 key)
{
 route->key=key;
 route->token=w16_route_add(route->table,key,FOFF(w16_int21),FSEG(w16_int21),
                            (u16)route->vector,(u16)(route->vector>>16));
 return route->token?0:-1;
}
int int21_router_open(u16 key)
{
 u32 old;u16 table;
 if(route||!key)return -1;
 old=w16_getvect(0x21);
 if(signature(old))return -1; /* explicit same-session attachment required */
 route=malloc(sizeof(*route));if(!route)return -1;
 if(seg_alloc((W16_INT21_BYTES+15)/16,&table)){free(route);route=0;return -1;}
 route->owner=1;route->table=table;route->token=0;
 route->vector=((u32)FSEG(w16_int21_router)<<16)|FOFF(w16_int21_router);
 fzero(table,0,W16_INT21_BYTES);wr16(table,0,1);
 /* Compare-and-install atomically: another root may have started while we
  * allocated. Never overwrite a vector that changed under us. */
 if(!w16_route_begin(table,(u16)old,(u16)(old>>16))) {
  seg_free(table);free(route);route=0;return -1;
 }
 if(add(key)){int21_router_close();return -1;}
 return 0;
}
int int21_router_attach(u16 key,u16 table,u32 vector)
{
 if(route||!key||!table||w16_getvect(0x21)!=vector||!signature(vector)||
    rd16((u16)(vector>>16),(u16)vector+8)!=table)return -1;
 route=malloc(sizeof(*route));if(!route)return -1;
 route->owner=0;route->table=table;route->vector=vector;route->token=0;
 /* The assembly check repeats the IVT comparison with interrupts disabled
  * BEFORE touching the table. A root closing during attachment cannot free
  * a table that the new task is about to access. */
 if(add(key)){free(route);route=0;return -1;}
 return 0;
}
int int21_router_info(u16 *table,u32 *vector)
{
 if(!route||!route->token)return -1;
 *table=route->table;*vector=route->vector;return 0;
}
void int21_router_leave(void)
{
 if(route&&route->token) {
  u16 at=route->token-1;
  /* Aligned word stores cannot be interrupted halfway on an 8088. Only
   * this task removes its entry; no other task reuses it until key is zero. */
  if(rd16(route->table,at)==route->key)wr16(route->table,at,0);
  route->token=0;
 }
}
int int21_router_close(void)
{
 i16 done;
 if(!route)return 0;
 int21_router_leave();
 if(route->owner) {
  done=w16_route_finish();
  if(done!=1)return done<0?-1:1;
  seg_free(route->table);
 }
 free(route);route=0;return 0;
}
