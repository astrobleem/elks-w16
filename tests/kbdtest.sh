#!/bin/bash
# Host tests of the ELKS keyboard drivers with elks-graphics-keys.patch applied to ELKS_TOPDIR:
# kbd-scancode.c (direct console) and kbd-poll.c (BIOS keyboard), each compiled on the host against
# stub headers and fed key sequences. Usage: ELKS_TOPDIR=/path/to/elks tests/kbdtest.sh
: "${ELKS_TOPDIR:?}"
C=$ELKS_TOPDIR/elks/arch/i86/drivers/char
T=$(cd "$(dirname "$0")" && pwd)
S=$(mktemp -d)
mkdir -p $S/linuxmt $S/arch
for h in config kernel sched types errno fs fcntl timer chqueue signal ntty memory; do : > $S/linuxmt/$h.h; done
for h in io irq ports system; do : > $S/arch/$h.h; done
cat > $S/poll.h <<'H'
struct timer_list { unsigned long tl_expires; void (*tl_function)(int); int tl_data; };
static unsigned long jiffies;
#define HZ 100
static void add_timer(struct timer_list *t) { (void)t; }
typedef unsigned short dev_t;
typedef unsigned char byte_t;
typedef unsigned short word_t, seg_t;
H
r=0
gcc -w -I$S -include $T/kbdscan_stubs.h -DKBD_C="\"$C/kbd-scancode.c\"" -o $S/kbdscan $T/kbdscan_test.c && $S/kbdscan || r=1
gcc -w -I$S -include $S/poll.h -DCONFIG_EMUL_ANSI -DKBD_C="\"$C/kbd-poll.c\"" -o $S/kbdpoll $T/kbdpoll_test.c && $S/kbdpoll || r=1
rm -rf $S
exit $r
