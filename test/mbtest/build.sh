#!/bin/sh
# Build MBTEST.EXE with Open Watcom v2: 8086 code, medium model.
#   WATCOM=/path/to/open-watcom  sh build.sh
set -e
cd "$(dirname "$0")"
: "${WATCOM:?set WATCOM to the Open Watcom root}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h:$WATCOM/h/win"
wcc -bt=windows -0 -mm -zW -s -os -zq -wx -we -fo=MBTEST.OBJ MBTEST.C
wlink system windows name MBTEST.EXE file MBTEST.OBJ \
      option stack=4096,heapsize=1024,packcode=0,map=MBTEST.MAP,quiet \
      libpath "$WATCOM/lib286:$WATCOM/lib286/win"
ls -l MBTEST.EXE
