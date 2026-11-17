#!/bin/sh
set -eu
: "${TOPDIR:?set TOPDIR to the built ELKS tree}"
: "${MWDIR:?set MWDIR to patched Microwindows}"
: "${NANOX_LIB:?set NANOX_LIB to the patched small-model client archive}"
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
${CC:-ia16-elf-gcc} -mcmodel=small -melks-libc -march=i8086 -Os \
 -DELKS=1 -DMWPIXEL_FORMAT=MWPF_PALETTE -I"$MWDIR/src/include" \
 -o "$HERE/readpixel-test" "$HERE/readpixel.c" "$NANOX_LIB"
