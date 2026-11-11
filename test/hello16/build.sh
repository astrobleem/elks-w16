#!/bin/sh
# Build HELLO16.EXE with Open Watcom v2 (open-source; no Windows SDK or media needed).
#   WATCOM=/path/to/open-watcom  sh build.sh
# -0: 8086/8088 instructions only; -mm: medium model (one code segment per module);
# -zW: Windows prolog/epilog for exported callbacks; -bt=windows: Win16 target.
set -e
cd "$(dirname "$0")"
: "${WATCOM:?set WATCOM to the Open Watcom root}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h:$WATCOM/h/win"
CFLAGS="-bt=windows -0 -mm -zW -s -os -zq -wx -we"
wcc $CFLAGS -fo=HELLO.OBJ HELLO.C
wcc $CFLAGS -fo=PAINT.OBJ PAINT.C
wlink system windows name HELLO16.EXE file HELLO.OBJ,PAINT.OBJ \
      option stack=4096,heapsize=1024,packcode=0,map=HELLO16.MAP,quiet \
      libpath "$WATCOM/lib286:$WATCOM/lib286/win"
ls -l HELLO16.EXE
