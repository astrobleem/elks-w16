# w16 - Win16 compatibility layer for ELKS
#
#   make TOPDIR=/path/to/elks MWDIR=/path/to/microwindows      native ELKS NE (ia16-elf-gcc, medium model, 8086)
#   make host                                                  host test build of the NE loader (gcc, ASan/UBSan)
#
# TOPDIR is a built ELKS 0.9.2 tree (cross/bin on PATH); MWDIR is the patched Microwindows tree whose
# src/lib/libnano-X-medium.a was built with the matching Nano-X helper.
# The target kernel needs CONFIG_EXEC_OS2=y and CONFIG_EXEC_MMODEL=y.

.DEFAULT_GOAL := all

CC      = ia16-elf-gcc
# Medium-model functions are far; the native NE build uses three code groups
# and a shared read-only bank. Keep this separate from Nano-X's small-model library.
NANOX_LIB ?= $(MWDIR)/src/lib/libnano-X-medium.a
CFLAGS  = -mcmodel=medium -melks-libc -march=i8086 -mtune=i8086 -Os -mregparmcall -Wall -Wno-unused-function \
          -I$(TOPDIR)/include -I$(TOPDIR)/libc/include -I$(TOPDIR)/elks/include \
          -I$(MWDIR)/src/include -DELKS=1 -DW16_COMPACT_LOG -DMWCONFIG_FILE='"mwconfig.elks"'
OBJS    = shell.o legacy.o main.o ne.o api.o mem.o kernel.o files.o filelist.o palette.o global.o session.o int21.o int21-route.o taskimage.o taskimage-asm.o launch.o icon.o drag.o clipboard.o caret.o user.o scrollbar.o menu.o cursor.o pointer.o gdi.o mapping.o polygon.o oval.o bitmap.o rasterfont.o dib.o rect.o registered.o textlayout.o linedda.o enumobj.o enumfont.o displayic.o ansistr.o dlgtmpl.o profile.o thunk.o


# Keep USER/menu helpers out of line within the native code-group limits.
user.o menu.o: CFLAGS += -fno-inline

all: w16

# Keep the installed name stable. The historical two-group a.out runtime no
# longer fits and is not a supported build target.
w16: check-native-kernel w16-ro.ne
	cp w16-ro.ne $@

check-native-kernel:
	@test -f "$(TOPDIR)/.config" || { echo 'Set TOPDIR to a configured, built ELKS tree' >&2; exit 1; }
	@grep -q '^CONFIG_EXEC_OS2=y$$' "$(TOPDIR)/.config" && \
	 grep -q '^CONFIG_EXEC_MMODEL=y$$' "$(TOPDIR)/.config" || \
	 { echo 'The native w16 NE executable requires CONFIG_EXEC_OS2=y and CONFIG_EXEC_MMODEL=y' >&2; exit 1; }

w16-medium.ld: ../scripts/w16-link-layout.sh
	sh $< "$(CC)" > $@.tmp
	mv $@.tmp $@

%.o: %.c w16.h wapi.h
	$(CC) $(CFLAGS) -c -o $@ $<

gdi.o bitmap.o polygon.o oval.o: polygon.h

api.o: api-slots.h

main.o int21.o: int21.h

main.o kernel.o user.o launch.o: launch.h
launch.o: taskimage.h int21.h

int21-route.o: int21-route.S int21.h
	$(CC) $(CFLAGS) -c -o $@ $<

taskimage.o: taskimage.h int21.h

taskimage-asm.o: taskimage.S taskimage.h
	$(CC) $(CFLAGS) -c -o $@ $<

thunk.o: thunk.S api-slots.h int21.h
	$(CC) $(CFLAGS) -c -o $@ $<

HOSTCC     = gcc
# Avoid ASan shadow-map collisions on Linux hosts with a large randomized address range.
HOSTCFLAGS = -DW16_HOST -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie

host: w16host

w16host: main.c ne.c api.c mem.c dlgtmpl.c profile.c w16.h wapi.h
	$(HOSTCC) $(HOSTCFLAGS) -o $@ main.c ne.c api.c mem.c dlgtmpl.c profile.c

clean:
	rm -f *.o w16 w16host o16 o16-host w16-medium.ld w16-medium.ld.tmp rasterfont-data.h rasterfont-data.h.tmp w16-ne.ld w16-ne.ld.tmp w16-ne.elf w16.ne w16-ro.elf w16-ro.ne
	rm -rf .ro

.PHONY: all w16 check-native-kernel host clean

# Preserve every original font pixel while avoiding wasteful near-data padding.
rasterfont.o: rasterfont-data.h
rasterfont-data.h: $(MWDIR)/src/fonts/compiled/winFreeSansSerif11x13.c ../scripts/pack-rasterfont.py
	python3 ../scripts/pack-rasterfont.py $< > $@.tmp
	mv $@.tmp $@

# Native NE layout, derived from the IA16 medium-model linker layout.
w16-ne.ld: w16-medium.ld ../scripts/w16-ne-layout.py
	python3 ../scripts/w16-ne-layout.py $< > $@.tmp
	mv $@.tmp $@

# Small native two-task routing check. This is not a Win16 app or WinExec.
.PHONY: int21route-probe
int21route-probe: int21.o int21-route.o thunk.o mem.o w16-ne.ld
	mkdir -p ../build/int21route
	$(CC) $(CFLAGS) -I. -c ../tests/elks/int21route.c -o ../build/int21route/probe.o
	$(CC) $(CFLAGS) -I. -c ../tests/elks/int21route.S -o ../build/int21route/probe-asm.o
	$(CC) $(CFLAGS) -I. -c ../tests/elks/int21route-third.c -o ../build/int21route/user.o
	@ne_crt=$$($(CC) -mcmodel=medium -mregparmcall -melks-libc -print-file-name=crt0.o); \
	ne_libc=$$($(CC) -mcmodel=medium -mregparmcall -melks-libc -print-file-name=libc.a); \
	ne_gcc=$$($(CC) -mcmodel=medium -mregparmcall -melks-libc -print-libgcc-file-name); \
	cd ../build/int21route && ia16-elf-ld.gold -m i386elks --gc-sections \
	--defsym=_heap=0x2800 --defsym=_stack=0x1800 -T $(CURDIR)/w16-ne.ld -o probe.elf \
	"$$ne_crt" probe.o probe-asm.o user.o $(addprefix $(CURDIR)/,int21.o int21-route.o thunk.o mem.o) "$$ne_gcc" "$$ne_libc" "$$ne_gcc"
	python3 ../scripts/elf2w16ne.py ../build/int21route/probe.elf ../build/int21route/route-ne

# Default runtime: private task data, lazy UI storage and a shared read-only bank.
# Keep these objects separate from the small native routing probe above.
RO_OBJS = $(addprefix .ro/,$(OBJS))
.ro/user.o .ro/menu.o: CFLAGS += -fno-inline
.ro/%.o: %.c w16.h wapi.h api-slots.h int21.h launch.h taskimage.h polygon.h rasterfont-data.h
	mkdir -p .ro
	$(CC) $(CFLAGS) -DW16_SHARED_RODATA -DW16_LAZY_UI -c -o $@ $<
.ro/%.o: %.S w16.h api-slots.h int21.h taskimage.h
	mkdir -p .ro
	$(CC) $(CFLAGS) -DW16_SHARED_RODATA -DW16_LAZY_UI -c -o $@ $<
.ro/taskimage-asm.o: taskimage.S taskimage.h
	mkdir -p .ro
	$(CC) $(CFLAGS) -DW16_SHARED_RODATA -DW16_LAZY_UI -c -o $@ $<
w16-ro.elf: $(RO_OBJS) $(NANOX_LIB) w16-ne.ld
	mkdir -p ../build
	@ne_crt=$$($(CC) -mcmodel=medium -mregparmcall -melks-libc -print-file-name=crt0.o); \
	ne_libc=$$($(CC) -mcmodel=medium -mregparmcall -melks-libc -print-file-name=libc.a); \
	ne_gcc=$$($(CC) -mcmodel=medium -mregparmcall -melks-libc -print-libgcc-file-name); \
	ia16-elf-ld.gold -m i386elks --gc-sections --defsym=_heap=0x2800 --defsym=_stack=0x1800 \
	-T w16-ne.ld -Map ../build/w16-ro.map -o $@ "$$ne_crt" $(RO_OBJS) $(NANOX_LIB) "$$ne_gcc" "$$ne_libc" "$$ne_gcc"
w16-ro.ne: w16-ro.elf ../scripts/elf2w16ne.py
	python3 ../scripts/elf2w16ne.py $< $@

.PHONY: check-shared-rodata
check-shared-rodata: w16-ro.ne
	cd .. && python3 tests/sharedrodatatest.py

# Opt-in OS/2 console ABI experiment. Independent of W16/Nano-X defaults.
O16FLAGS = -mcmodel=small -mregparmcall -melks-libc -march=i8086 -Os -Wall -DW16_OS2_PROBE \
           -I$(TOPDIR)/include -I$(TOPDIR)/libc/include -I$(TOPDIR)/elks/include
.PHONY: os2-probe os2-probe-host
os2-probe: o16
o16: os2probe.c os2probe.S ne.c w16.h
	$(CC) $(O16FLAGS) -o $@ os2probe.c os2probe.S ne.c
os2-probe-host: o16-host
o16-host: os2probe.c ne.c mem.c w16.h
	$(HOSTCC) $(HOSTCFLAGS) -DW16_OS2_PROBE -o $@ os2probe.c ne.c mem.c

# Owned native observation only; this does not advertise OS/2 console APIs.
.PHONY: os2-console-probe
os2-console-probe: ../build/os2-console-probe/ocon
../build/os2-console-probe/ocon: ../tests/elks/os2console.c
	mkdir -p ../build/os2-console-probe
	$(CC) $(O16FLAGS) -o $@ $<
