# w16 - Win16 compatibility layer for ELKS
#
#   make TOPDIR=/path/to/elks MWDIR=/path/to/microwindows      ELKS a.out (ia16-elf-gcc, small model, 8086)
#   make host                                                  host test build of the NE loader (gcc, ASan/UBSan)
#
# TOPDIR is a built ELKS 0.9.2 tree (cross/bin on PATH); MWDIR is the patched Microwindows tree whose
# src/lib/libnano-X.a was built with Makefile.elks.

CC      = ia16-elf-gcc
CFLAGS  = -mcmodel=small -melks-libc -march=i8086 -mtune=i8086 -Os -Wall -Wno-unused-function \
          -I$(TOPDIR)/include -I$(TOPDIR)/libc/include -I$(TOPDIR)/elks/include \
          -I$(MWDIR)/src/include -DELKS=1 -DMWCONFIG_FILE='"mwconfig.elks"'
LDFLAGS = -mcmodel=small -melks-libc -maout-heap=0x1000 -maout-stack=0x2000
OBJS    = main.o ne.o api.o mem.o kernel.o user.o gdi.o thunk.o

all: w16

w16: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(MWDIR)/src/lib/libnano-X.a
	ia16-elf-size $@ 2>/dev/null || true

%.o: %.c w16.h wapi.h
	$(CC) $(CFLAGS) -c -o $@ $<

thunk.o: thunk.S
	$(CC) $(CFLAGS) -c -o $@ $<

HOSTCC     = gcc
HOSTCFLAGS = -DW16_HOST -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -fsanitize=address,undefined -fno-omit-frame-pointer

host: w16host

w16host: main.c ne.c api.c mem.c w16.h
	$(HOSTCC) $(HOSTCFLAGS) -o $@ main.c ne.c api.c mem.c

clean:
	rm -f *.o w16 w16host
