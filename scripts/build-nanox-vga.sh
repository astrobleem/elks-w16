#!/bin/sh
# Optional isolated VGA build. Existing Tandy checkout and defaults are unchanged.
# TOPDIR=built-elks MWDIR=baseline-patched-microwindows sh scripts/build-nanox-vga.sh
# NANOX_HEAP=0xfff0 selects ELKS's maximum-heap sentinel; default is 32 KiB.
set -eu
: "${TOPDIR:?set TOPDIR to a configured and built ELKS tree}"
: "${MWDIR:?set MWDIR to the checkout with the three baseline patches applied}"
TOPDIR=$(cd "$TOPDIR" && pwd)
MWDIR=$(cd "$MWDIR" && pwd)
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=${VGA_OUT:-$ROOT/build/nanox-vga}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
export TOPDIR
export PATH="${CROSSDIR:-$TOPDIR/cross}/bin:$TOPDIR/elks/tools/bin:$PATH"
CC=${CC:-ia16-elf-gcc}
AR=${AR:-ia16-elf-ar}
NANOX_HEAP=${NANOX_HEAP:-0x8000}
command -v "$CC" >/dev/null
command -v "$AR" >/dev/null
test -f "$TOPDIR/.config"
test -f "$MWDIR/src/drivers/scr_tandy.c"
grep -q GR_MODE_EXCLUDEENDPOINT "$MWDIR/src/include/nano-X.h"
grep -q rowoff_init "$MWDIR/src/drivers/tandy4.c"
# Build in a new copy: never patch or reuse objects from the caller's live tree.
WORK=$(mktemp -d "$OUT/work.XXXXXX")
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
cp -R "$MWDIR/src" "$WORK/src"
# Normalize already-applied extensions in reverse dependency order, then verify
# and apply all forward. Partial/conflicting patch sets fail rather than guessing.
for name in nanox-capturepixels.patch nanox-screenand.patch nanox-readpixel.patch; do
    patchfile="$ROOT/patches/$name"
    if patch --force --reverse --dry-run -d "$WORK" -p1 < "$patchfile" >/dev/null 2>&1; then
        patch --force --reverse -d "$WORK" -p1 < "$patchfile"
    fi
done
for name in nanox-readpixel.patch nanox-screenand.patch nanox-capturepixels.patch; do
    patchfile="$ROOT/patches/$name"
    if ! patch --batch --forward --dry-run -d "$WORK" -p1 < "$patchfile" >/dev/null 2>&1; then
        printf 'Cannot apply or verify %s; refusing an incompatible checkout\n' "$name" >&2
        exit 1
    fi
    patch --batch --forward -d "$WORK" -p1 < "$patchfile"
done
# Baseline hardcodes 8 KiB; change only our disposable copy to accept an override.
sed 's/-maout-heap=0x2000/-maout-heap=$(NANOX_HEAP)/g' "$WORK/src/Makefile.elks" > "$WORK/src/Makefile.vga"
# Empty PC98 is essential: upstream uses ifdef, so even CONFIG_ARCH_PC98=n selects it.
make -C "$WORK/src" -f Makefile.vga CONFIG_ARCH_PC98= SCREEN_TANDY=n SCREEN_CGA=n clean
make -C "$WORK/src" -f Makefile.vga CONFIG_ARCH_PC98= SCREEN_TANDY=n SCREEN_CGA=n \
    NANOX_HEAP="$NANOX_HEAP" bin/nano-x
mkdir -p "$WORK/medium"
for source in nanox/client.c nanox/nxproto.c nanox/nxutil.c drivers/osdep.c; do
    object="$WORK/medium/$(basename "$source" .c).o"
    "$CC" -mcmodel=medium -melks-libc -mregparmcall -march=i8086 -Os \
        -ffunction-sections -fdata-sections \
        -DELKS=1 -DUNIX=1 -DNDEBUG=1 -DNONETWORK=0 \
        '-DMWCONFIG_FILE="mwconfig.elks"' -DMWPIXEL_FORMAT=MWPF_PALETTE \
        -DSCREEN_PIXTYPE=MWPF_PALETTE \
        -I"$TOPDIR/include" -I"$TOPDIR/libc/include" -I"$TOPDIR/elks/include" \
        -I"$WORK/src/include" -I"$WORK/src/elksdrivers" -c -o "$object" "$WORK/src/$source"
done
"$CC" -mcmodel=medium -melks-libc -mregparmcall -march=i8086 -Os \
    -c -o "$WORK/medium/nanoxabi.o" "$ROOT/src/nanoxabi.c"
"$AR" rcs "$WORK/medium/libnano-X-medium.a" "$WORK/medium/client.o" \
    "$WORK/medium/nxproto.o" "$WORK/medium/nxutil.o" "$WORK/medium/osdep.o" "$WORK/medium/nanoxabi.o"
cp "$WORK/src/bin/nano-x" "$OUT/nano-x"
cp "$WORK/medium/libnano-X-medium.a" "$OUT/libnano-X-medium.a"
# The caller can compile w16 against these negotiated API declarations.
mkdir -p "$OUT/src/include" "$OUT/src/lib"
cp -R "$WORK/src/include/." "$OUT/src/include/"
cp "$OUT/libnano-X-medium.a" "$OUT/src/lib/libnano-X-medium.a"
printf '\nVGA 640x480 server: %s/nano-x\nMedium client: %s/libnano-X-medium.a\n' "$OUT" "$OUT"
printf 'Build w16 with MWDIR=%s (matching headers and medium archive).\n' "$OUT"
