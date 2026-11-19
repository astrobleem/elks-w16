#ifndef W16_LAUNCH_H
#define W16_LAUNCH_H
#include "w16.h"
int launch_prepare(u16 sp);
int launch_attach(u16 ss);
void launch_reply(u16 result);
u16 launch_error(int ne_error);
void launch_poll(void);
u32 launch_wait(u32 wait);
void launch_finish(void);
u16 launch_show(void);
u16 launch_tasks(void);
void w16_run(const char *path,int argc,char **argv,const char *dllpath,const char *dllname,int checkonly);
#endif
