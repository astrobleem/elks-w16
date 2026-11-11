# w16 - Win16 compatibility layer for ELKS
#
#   make TOPDIR=/path/to/elks MWDIR=/path/to/microwindows      ELKS a.out (ia16-elf-gcc, medium model, 8086)
#   make host                                                  host test build of the NE loader (gcc, ASan/UBSan)
#
# TOPDIR is a built ELKS 0.9.2 tree (cross/bin on PATH); MWDIR is the patched Microwindows tree whose
# src/lib/libnano-X-medium.a was built with scripts/build-nanox.sh.

CC      = ia16-elf-gcc
# Medium memory model: C functions are far and may fill two code segments (the loader passed 61 KB
# of the 64 KB small-model limit). Keep this separate from Nano-X's small-model library.
NANOX_LIB ?= $(MWDIR)/src/lib/libnano-X-medium.a
CFLAGS  = -mcmodel=medium -melks-libc -march=i8086 -mtune=i8086 -Os -Wall -Wno-unused-function \
          -I$(TOPDIR)/include -I$(TOPDIR)/libc/include -I$(TOPDIR)/elks/include \
          -I$(MWDIR)/src/include -DELKS=1 -DMWCONFIG_FILE='"mwconfig.elks"'
LDFLAGS = -mcmodel=medium -melks-libc -maout-heap=0x1400 -maout-stack=0x1800
OBJS    = main.o ne.o api.o mem.o kernel.o user.o menu.o cursor.o gdi.o dlgtmpl.o profile.o thunk.o

all: w16

w16: $(OBJS) $(NANOX_LIB)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(NANOX_LIB)
	ia16-elf-size $@ 2>/dev/null || true

%.o: %.c w16.h wapi.h
	$(CC) $(CFLAGS) -c -o $@ $<

thunk.o: thunk.S
	$(CC) $(CFLAGS) -c -o $@ $<

HOSTCC     = gcc
# Avoid ASan shadow-map collisions on Linux hosts with a large randomized address range.
HOSTCFLAGS = -DW16_HOST -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie

host: w16host

w16host: main.c ne.c api.c mem.c dlgtmpl.c profile.c w16.h wapi.h
	$(HOSTCC) $(HOSTCFLAGS) -o $@ main.c ne.c api.c mem.c dlgtmpl.c profile.c

clean:
	rm -f *.o w16 w16host

.PHONY: all host clean
