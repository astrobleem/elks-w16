#!/bin/sh
# Own Win16 parent/child fixture for the native-NE -T task path.
set -e
cd "$(dirname "$0")"
: "${WATCOM:?set WATCOM to the Open Watcom root}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h:$WATCOM/h/win"
for name in WXPARENT WXCHILD WXBAD; do
 wcc -bt=windows -0 -mm -zW -s -os -zq -fo="$name.OBJ" "$name.C"
 wlink system windows name "$name.EXE" file "$name.OBJ" \
  option stack=4096,heapsize=1024,packcode=0,quiet \
  libpath "$WATCOM/lib286:$WATCOM/lib286/win"
done
