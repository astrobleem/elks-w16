/* Optional US XT/AT set-1 backend using stock ELKS console ioctls.
 * Select only for CONFIG_KEYBOARD_SCANCODE kernels; polling/serial use tty.
 * Copyright (c) 2026 elks-w16 contributors; MPL 1.1 / GPLv2 alternative. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linuxmt/ntty.h>
#include "device.h"
#include "elks_set1.h"
static int fd=-1,raw;
static struct termios saved;
static struct elks_keys keys;
/* Stock TCSETSF changes attributes but does not discard queued input. */
static void discard_input(void)
{
 unsigned char pending[128];int n,count;
 for(n=0;n<4;n++){
  count=read(fd,pending,sizeof(pending));
  if(count<0&&errno==EINTR)continue;
  if(count<=0)break;
 }
}
static int raw_open(KBDDEVICE *device)
{
 struct termios mode;int have_saved=0;const char *name=getenv("KEYBOARD");(void)device;
 if(!name)name=getenv("CONSOLE");
 if(!name)name="/dev/tty1";
 if(fd>=0)return DRIVER_FAIL;
 fd=open(name,O_RDONLY|O_NONBLOCK);if(fd<0)return DRIVER_FAIL;
 if(tcgetattr(fd,&saved)<0)goto fail;
 have_saved=1;
 mode=saved;mode.c_lflag&=~(ECHO|ICANON|IEXTEN|ISIG);
 mode.c_iflag&=~(ICRNL|INLCR|IGNCR|INPCK|ISTRIP|IXON|IXOFF|BRKINT|PARMRK);
 mode.c_cflag=(mode.c_cflag&~(CSIZE|PARENB))|CS8;
 mode.c_cc[VMIN]=0;mode.c_cc[VTIME]=0;
 if(tcsetattr(fd,TCSAFLUSH,&mode)<0)goto fail;
 discard_input(); /* do not decode a cooked-mode tail as scan bytes */
 /* GdOpenScreen must already hold DCGET_GRAPH on this same console. */
 if(ioctl(fd,DCSET_KRAW,0)<0)goto fail;
 raw=1;memset(&keys,0,sizeof(keys));return DRIVER_OKFILEDESC(fd);
fail:
 if(have_saved)tcsetattr(fd,TCSAFLUSH,&saved);
 close(fd);fd=-1;return DRIVER_FAIL;
}
static void raw_close(void)
{
 if(fd<0)return;
 /* GdCloseScreen must not release the graphics console until after this. */
 if(raw){
  if(ioctl(fd,DCREL_KRAW,0)<0)EPRINTF("ELKS: cannot release raw keyboard\n");
  raw=0;
  /* Stock ELKS TCSETSF changes attributes but does not flush the input
   * queue. Disable scan-byte production, then discard its queued tail while
   * reads are still noncanonical/nonblocking. Bound work under key repeat. */
  discard_input();
 }
 tcsetattr(fd,TCSAFLUSH,&saved);close(fd);fd=-1;memset(&keys,0,sizeof(keys));
}
static void raw_modifiers(MWKEYMOD *possible,MWKEYMOD *current)
{
 if(possible)*possible=MWKMOD_SHIFT|MWKMOD_CTRL|MWKMOD_ALT|MWKMOD_CAPS|MWKMOD_NUM|MWKMOD_SCR;
 if(current)*current=keys.mods;
}
static int raw_read(MWKEY *key,MWKEYMOD *mods,MWSCANCODE *scan)
{
 unsigned char byte;int n,result;
 for(;;){
  n=read(fd,&byte,1);
  if(n<0){if(errno==EINTR)continue;return errno==EAGAIN?0:KBD_FAIL;}
  if(!n)return 0;
  result=elks_decode(&keys,byte,key,mods,scan);if(result)return result;
 }
}
KBDDEVICE kbddev={raw_open,raw_close,raw_modifiers,raw_read,NULL};
