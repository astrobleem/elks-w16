/* One resident session vector; caller SS selects a private task handler. */
#ifndef W16_INT21_H
#define W16_INT21_H
#define W16_INT21_TASKS 16
#define W16_INT21_BYTES (2+6*W16_INT21_TASKS)
#define W16_INT21_MAGIC0 0x3157
#define W16_INT21_MAGIC1 0x5236
#ifndef __ASSEMBLER__
#include "w16.h"
int int21_router_open(u16 guest_ss);
int int21_router_attach(u16 guest_ss,u16 table,u32 vector);
int int21_router_info(u16 *table,u32 *vector);
void int21_router_leave(void);
/* 0 closed; 1 registered peers remain; -1 vector ownership changed.
 * A root must stay resident until closed, even after its own guest exits. */
int int21_router_close(void);
#endif
#endif
