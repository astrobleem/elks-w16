#!/bin/sh
# Build DLGTEST.EXE with Open Watcom v2: 8086 code, medium model, Windows 3.0 format resources.
#   WATCOM=/path/to/open-watcom  sh build.sh
set -e
cd "$(dirname "$0")"
: "${WATCOM:?set WATCOM to the Open Watcom root}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h:$WATCOM/h/win"
python3 mkicon.py dlgtest.ico
wcc -bt=windows -0 -mm -zW -s -os -zq -wx -we -fo=DLGTEST.OBJ DLGTEST.C
wrc -q -r -30 -bt=windows -fo=DLGTEST.RES DLGTEST.RC
wlink system windows name DLGTEST.EXE file DLGTEST.OBJ \
      option stack=4096,heapsize=1024,packcode=0,map=DLGTEST.MAP,quiet \
      libpath "$WATCOM/lib286:$WATCOM/lib286/win"
wrc -q -30 -bt=windows DLGTEST.RES DLGTEST.EXE
ls -l DLGTEST.EXE
