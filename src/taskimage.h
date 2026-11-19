/* Native pristine-data task restart, not a guest executable format. */
#ifndef W16_TASKIMAGE_H
#define W16_TASKIMAGE_H
#define IMAGE_PACKET_BYTES 1024
#define IMAGE_HEADER_BYTES 16
#ifndef __ASSEMBLER__
#include "w16.h"
/* Patched only by the checked native NE converter. Other targets stay off. */
extern u16 w16_shared_safe;
struct image_start {u16 fd,ack,bytes,sp,ds,entry_off,entry_seg,error;};
int task_image_save(int fd,u16 sp,u16 *bytes);
u16 W16_CDECL task_image_sp(void);
void W16_CDECL task_image_enter(u16 packet);
#endif
#endif
