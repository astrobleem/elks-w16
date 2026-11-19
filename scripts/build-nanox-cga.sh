#!/bin/sh
# Isolated native-size CGA build. Default CGA_MODE=mono; CGA_MODE=color selects 320x200x4.
# CGA_PAN=1 adds a 300-row virtual screen; Alt+I/K pans vertically, Alt+H returns.
# TOPDIR=built-elks MWDIR=baseline-patched-microwindows sh scripts/build-nanox-cga.sh
# NANOX_HEAP=0xfff0 selects ELKS's maximum-heap sentinel; this isolated build defaults to 8 KiB.
set -eu
: "${TOPDIR:?set TOPDIR to a configured and built ELKS tree}"
: "${MWDIR:?set MWDIR to the checkout with the three baseline patches applied}"
TOPDIR=$(cd "$TOPDIR" && pwd)
MWDIR=$(cd "$MWDIR" && pwd)
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
case "${CGA_MODE:-mono}" in
    mono) CGA_COLOR=0; DEFAULT_OUT="$ROOT/build/nanox-cga"; MODE_LABEL="640x200 monochrome" ;;
    color) CGA_COLOR=1; DEFAULT_OUT="$ROOT/build/nanox-cga-color"; MODE_LABEL="320x200 four-color" ;;
    *) echo 'CGA_MODE must be mono or color' >&2; exit 1 ;;
esac
case "${CGA_PAN:-0}" in
    0) CGA_PAN=0 ;;
    1) CGA_PAN=1; DEFAULT_OUT="$DEFAULT_OUT-pan" ;;
    *) echo 'CGA_PAN must be 0 or 1' >&2; exit 1 ;;
esac
OUT=${CGA_OUT:-$DEFAULT_OUT}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
export TOPDIR
export PATH="${CROSSDIR:-$TOPDIR/cross}/bin:$TOPDIR/elks/tools/bin:$PATH"
CC=${CC:-ia16-elf-gcc}
AR=${AR:-ia16-elf-ar}
NANOX_HEAP=${NANOX_HEAP:-0x2000}
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
# Optional bounded startup diagnosis; normal builds keep their existing output.
if [ "${NANOX_STARTUP_TRACE:-0}" = 1 ]; then
    patch --batch --forward -d "$WORK" -p1 < "$ROOT/patches/nanox-startup-trace.patch"
fi
# Normalize already-applied extensions in reverse dependency order, then verify
# and apply all forward. Partial/conflicting patch sets fail rather than guessing.
for name in nanox-childclip.patch nanox-capturepixels.patch nanox-screenand.patch nanox-readpixel.patch; do
    patchfile="$ROOT/patches/$name"
    if patch --force --reverse --dry-run -d "$WORK" -p1 < "$patchfile" >/dev/null 2>&1; then
        patch --force --reverse -d "$WORK" -p1 < "$patchfile"
    fi
done
for name in nanox-readpixel.patch nanox-screenand.patch nanox-capturepixels.patch nanox-childclip.patch; do
    patchfile="$ROOT/patches/$name"
    if ! patch --batch --forward --dry-run -d "$WORK" -p1 < "$patchfile" >/dev/null 2>&1; then
        printf 'Cannot apply or verify %s; refusing an incompatible checkout\n' "$name" >&2
        exit 1
    fi
    patch --batch --forward -d "$WORK" -p1 < "$patchfile"
done
# Adapt only the disposable pinned baseline. Refuse missing integration points.
cp "$ROOT/drivers/scr_cga.c" "$WORK/src/drivers/scr_cga.c"
python3 - "$WORK/src" "$CGA_COLOR" "$CGA_PAN" <<'PYCODE'
from pathlib import Path
import sys
p=Path(sys.argv[1]); color=int(sys.argv[2]); pan=int(sys.argv[3])
def replace(name,old,new):
 f=p/name;s=f.read_text()
 if old not in s:raise SystemExit('Incompatible CGA baseline: '+name)
 f.write_text(s.replace(old,new))
replace('Makefile.elks',
 'CFLAGS += -DUSE_SMALL_CURSOR=1\nDRIVERS += drivers/scr_cga.o drivers/vgaplan4_cga.o drivers/vgaplan4_mem.o',
 'CFLAGS += -DUSE_SMALL_CURSOR=1 -DSCREEN_CGA=1 -DCGA_COLOR='+str(color)+' -DCGA_PAN='+str(pan)+'\nDRIVERS += drivers/scr_cga.o')
replace('drivers/fb.c','#if SCREEN_TANDY',
 '#if SCREEN_CGA\n\tif (psd->planes != 1 || psd->bpp != '+str(2 if color else 1)+' || psd->data_format != '+('MWIF_PAL2' if color else 'MWIF_MONOBYTEMSB')+') return NULL;\n#endif\n#if SCREEN_TANDY')
# ELKS normally omits the 2bpp palette case. Use the driver's fixed CGA palette.
if color:
 replace('engine/devopen.c','#define NOSTDPAL2','#if !CGA_COLOR\n#define NOSTDPAL2\n#endif')
if pan:
 replace('nanox/srvevent.c','\t/* handle special keys*/',  '\t/* Opt-in CGA viewport keys stay within the server. */\n'
  '#if CGA_PAN\n'
  '\tif(modifiers & MWKMOD_ALT) {\n'
  '\t\textern int cga_pan_key(MWKEY,int *,int *);\n'
  '\t\textern int cga_viewx,cga_viewy;\n'
  '\t\tint dx=0,dy=0;\n'
  '\t\tif(keystatus==KBD_KEYPRESS && cga_pan_key(mwkey,&dx,&dy)){\n'
  '\t\t\tGdRestrictMouse(cga_viewx,cga_viewy,cga_viewx+(CGA_COLOR?320:640)-1,cga_viewy+199);\n'
  '\t\t\tGrMoveCursor(cursorx+dx,cursory+dy);return TRUE;\n'
  '\t\t}\n\t}\n#endif\n\t/* handle special keys*/')
# Bounded readback and packed capture support native low-color indices.
for name in ['nanox/srvnet.c','nanox/srvfunc.c']:
 f=p/name;s=f.read_text();old=s
 for needle in ['wp->psd->ncolors!=16','wp->psd->ncolors != 16']:
  s=s.replace(needle,'(wp->psd->ncolors!=2 && wp->psd->ncolors!=4 && wp->psd->ncolors!=16)')
 if s==old:raise SystemExit('Missing low-color readback guard in '+name)
 f.write_text(s)
PYCODE
# Baseline hardcodes 8 KiB; change only our disposable copy to accept an override.
sed 's/-maout-heap=0x2000/-maout-heap=$(NANOX_HEAP)/g' "$WORK/src/Makefile.elks" > "$WORK/src/Makefile.cga"
# Empty PC98 is essential: upstream uses ifdef, so even CONFIG_ARCH_PC98=n selects it.
make -C "$WORK/src" -f Makefile.cga CONFIG_ARCH_PC98= SCREEN_TANDY=n SCREEN_CGA=y clean
make -C "$WORK/src" -f Makefile.cga CONFIG_ARCH_PC98= SCREEN_TANDY=n SCREEN_CGA=y \
    NANOX_HEAP="$NANOX_HEAP" bin/nano-x lib/libnano-X.a
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
cp "$WORK/src/lib/libnano-X.a" "$OUT/libnano-X.a"
cp "$WORK/medium/libnano-X-medium.a" "$OUT/libnano-X-medium.a"
# The caller can compile w16 against these negotiated API declarations.
mkdir -p "$OUT/src/include" "$OUT/src/lib"
cp -R "$WORK/src/include/." "$OUT/src/include/"
cp "$OUT/libnano-X-medium.a" "$OUT/src/lib/libnano-X-medium.a"
cp "$OUT/libnano-X.a" "$OUT/src/lib/libnano-X.a"
mkdir -p "$OUT/src/fonts/compiled"
cp "$WORK/src/fonts/compiled/winFreeSansSerif11x13.c" "$OUT/src/fonts/compiled/"
printf '\nCGA %s server: %s/nano-x\nMedium client: %s/libnano-X-medium.a\n' "$MODE_LABEL" "$OUT" "$OUT"
printf 'Build w16 with MWDIR=%s (matching headers and medium archive).\n' "$OUT"
