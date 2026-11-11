#!/bin/sh
# Build a patched Nano-X checkout without mixing small- and medium-model objects.
# TOPDIR=/path/to/built-elks MWDIR=/path/to/patched-microwindows sh scripts/build-nanox.sh
set -eu
: "${TOPDIR:?set TOPDIR to a configured and built ELKS tree}"
: "${MWDIR:?set MWDIR to the Microwindows tree with all three patches applied}"
TOPDIR=$(cd "$TOPDIR" && pwd)
MWDIR=$(cd "$MWDIR" && pwd)
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
export TOPDIR
export PATH="${CROSSDIR:-$TOPDIR/cross}/bin:$TOPDIR/elks/tools/bin:$PATH"
CC=${CC:-ia16-elf-gcc}
AR=${AR:-ia16-elf-ar}
command -v "$CC" >/dev/null
test -f "$TOPDIR/.config"
test -f "$MWDIR/src/drivers/tandy4.c"
grep -q GR_MODE_EXCLUDEENDPOINT "$MWDIR/src/include/nano-X.h"

# Clean first, so a formerly medium-model client archive cannot be reused here.
make -C "$MWDIR/src" -f Makefile.elks clean
make -C "$MWDIR/src" -f Makefile.elks SCREEN_TANDY=y LOCALFLAGS=-DMEM_GETPCJR=15

# These utilities share the normal small-model screen-driver object.
for utility in nxcolor vidinfo vidreset; do
    if [ "$utility" = vidinfo ]; then
        "$CC" -mcmodel=small -melks-libc -march=i8086 -Os -DMEM_GETPCJR=15 \
            -I"$TOPDIR/include" -I"$TOPDIR/libc/include" -I"$TOPDIR/elks/include" \
            -o "$MWDIR/$utility" "$MWDIR/$utility.c"
    else
        "$CC" -mcmodel=small -melks-libc -march=i8086 -Os -DMEM_GETPCJR=15 \
            -I"$TOPDIR/include" -I"$TOPDIR/libc/include" -I"$TOPDIR/elks/include" \
            -I"$MWDIR/src/include" -DELKS=1 '-DMWCONFIG_FILE="mwconfig.elks"' \
            -o "$MWDIR/$utility" "$MWDIR/$utility.c" "$MWDIR/src/drivers/vgaplan4_asm.o"
    fi
done

mkdir -p "$ROOT/build/nanox-medium"
for source in nanox/client.c nanox/nxproto.c nanox/nxutil.c drivers/osdep.c; do
    object="$ROOT/build/nanox-medium/$(basename "$source" .c).o"
    "$CC" -mcmodel=medium -melks-libc -march=i8086 -Os \
        -DELKS=1 -DUNIX=1 -DNDEBUG=1 -DNONETWORK=0 \
        '-DMWCONFIG_FILE="mwconfig.elks"' -DMWPIXEL_FORMAT=MWPF_PALETTE \
        -DSCREEN_PIXTYPE=MWPF_PALETTE -DSCREEN_TANDY=1 -DMEM_GETPCJR=15 \
        -I"$TOPDIR/include" -I"$TOPDIR/libc/include" -I"$TOPDIR/elks/include" \
        -I"$MWDIR/src/include" -I"$MWDIR/src/elksdrivers" -c -o "$object" "$MWDIR/src/$source"
done
"$AR" rcs "$ROOT/build/nanox-medium/libnano-X-medium.a" \
    "$ROOT/build/nanox-medium/client.o" "$ROOT/build/nanox-medium/nxproto.o" \
    "$ROOT/build/nanox-medium/nxutil.o" "$ROOT/build/nanox-medium/osdep.o"
cp "$ROOT/build/nanox-medium/libnano-X-medium.a" "$MWDIR/src/lib/libnano-X-medium.a"
printf '\nBuilt Nano-X, utilities and separate small/medium client libraries.\n'
