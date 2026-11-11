#!/bin/bash
# Build a 20 MB ELKS test disk (615 cylinders x 4 heads x 17 sectors, MBR + MINIX partition) with the
# Nano-X colour server, w16, HELLO16.EXE, malformed test files and the boot-time autotest.
#
#   ELKS_ROOT=/path/to/elks MWDIR=/path/to/microwindows [W16_EXTRA="/path/WINVER.EXE /path/CLOCK.EXE"] \
#       tests/mkimage.sh OUT_NAME [FLAG...]
#
# ELKS_ROOT: built tandy-video-v0.9.2 tree (its cross/bin and elks/tools/bin provide ia16-elf-gcc, mfs,
# setboot). MWDIR: Microwindows built by scripts/build-nanox.sh (both memory models retained).
# Build w16 and all three samples first. Flags are listed below ("kbd": keyboard only).
# Use a disposable, fully built ELKS checkout: upstream image generation may rebuild its kernel
# and bootblocks. The existing image/Config and target staging directory are not touched.
set -eu
NAME=${1:?output name}
W=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(cd "$(dirname "$NAME")" && pwd)/$(basename "$NAME")
if [ -e "$OUT.img" ] || [ -e "$OUT-mbr.img" ]; then
    echo "refusing to overwrite $OUT.img or $OUT-mbr.img; choose a new output name" >&2
    exit 2
fi
: "${ELKS_ROOT:?}" "${MWDIR:?}"
# ELKS's upstream makefiles do not support whitespace in build paths.
case "$W:$OUT:$ELKS_ROOT:$MWDIR" in
    *[[:space:]]*) echo "build and output paths must not contain whitespace" >&2; exit 2;;
esac
ELKS_ROOT=$(cd "$ELKS_ROOT" && pwd)
MWDIR=$(cd "$MWDIR" && pwd)
for f in "${@:2}"; do
    case $f in
    kbd|quiet|nocache|servertext|beat|ptykbd|repeat|skipneg|skiphello16|skipdlgtest|skipmbtest|skipwinver|skipclock|skipreversi) ;;
    *) echo "unknown flag $f" >&2; exit 2;;
    esac
done
for f in "$W/src/w16" "$W/test/hello16/HELLO16.EXE" "$W/test/dlgtest/DLGTEST.EXE" \
         "$W/test/mbtest/MBTEST.EXE" "$MWDIR/src/bin/nano-x" "$MWDIR/vidreset" \
         "$MWDIR/src/lib/libnano-X.a"; do
    test -f "$f" || { echo "missing build input: $f" >&2; exit 2; }
done
cd "$ELKS_ROOT" && . ./env.sh > /dev/null
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
ia16-elf-gcc -mcmodel=small -melks-libc -march=i8086 -Os -maout-heap=0x400 -maout-stack=0x600 -o $T/e9 $W/tests/elks/e9.c
ia16-elf-gcc -mcmodel=small -melks-libc -march=i8086 -Os -maout-heap=0x400 -maout-stack=0x600 -o $T/w16mouse $W/tests/elks/w16mouse.c
ia16-elf-gcc -mcmodel=small -melks-libc -march=i8086 -Os -maout-heap=0x400 -maout-stack=0x600 -I$ELKS_ROOT/elks/include -o $T/tvprobe $W/tests/elks/tvprobe.c
ia16-elf-gcc -mcmodel=small -melks-libc -march=i8086 -Os -maout-heap=0x400 -maout-stack=0x600 -DELKS=1 -DMWCONFIG_FILE='"mwconfig.elks"' -I$MWDIR/src/include -I$MWDIR/src/nanox     -o $T/nxstress $W/tests/elks/nxstress.c $MWDIR/src/lib/libnano-X.a
python3 $W/tests/mkbad.py $W/test/hello16/HELLO16.EXE $T/badall > /dev/null
python3 $W/tests/mkbadres.py $W/test/dlgtest/DLGTEST.EXE $T/badres > /dev/null
cd "$ELKS_ROOT/image"
cat > "$T/Config" <<CFG
CONFIG_APPS_HD=y
CONFIG_IMG_HD=y
CONFIG_IMG_BLOCKS=20000
CONFIG_IMG_SECT=17
CONFIG_IMG_HEAD=4
CONFIG_IMG_CYL=615
CONFIG_IMG_MINIX=y
CONFIG_IMG_DEV=y
CONFIG_IMG_BOOT=y
CFG
sed -n -e '/CONFIG_TIME_/p' < "$ELKS_ROOT/.config" >> "$T/Config"
export DESTDIR="$T/target"
make -f Make.image "CONFIG=$T/Config" "DESTDIR=$T/target" "TARGET_FILE=$T/base.img" > $T/make.log 2>&1 || { tail -30 $T/make.log; exit 1; }
IMG=$OUT.img
cp "$T/base.img" "$IMG"
mfs $IMG cp $MWDIR/src/bin/nano-x /bin/nano-color
mfs $IMG cp $MWDIR/vidreset /bin/vidreset
mfs $IMG cp $W/src/w16 /bin/w16
for f in e9 w16mouse tvprobe nxstress; do mfs $IMG cp $T/$f /bin/$f; done
mfs $IMG cp $W/test/hello16/HELLO16.EXE /root/hello16.exe
mfs $IMG cp $W/test/dlgtest/DLGTEST.EXE /root/dlgtest.exe
mfs $IMG cp $W/test/mbtest/MBTEST.EXE /root/mbtest.exe
# W16_EXTRA: your own copies of other programs to test (for example Windows 3.0 WINVER.EXE as winver.exe)
for f in ${W16_EXTRA:-}; do n=$(basename $f | tr A-Z a-z); mfs $IMG cp $f /root/$n; done
mfs $IMG mkdir /root/bad
for f in notmz.exe pe.exe dll.exe i286.exe tickcount.exe badmod.exe chainloop.exe truncseg.exe bigdata.exe; do
    mfs $IMG cp $T/badall/$f /root/bad/$f
done
for f in $T/badres/*.exe; do mfs $IMG cp $f /root/bad/$(basename $f); done
# flags for the autotest: kbd (keyboard only), quiet (measure CLOCK without log and frame dumps),
# nocache (W16_NOCACHE=1), ptykbd (Nano-X reads its keyboard from a pty the test driver writes), repeat
# (every program a second time),
# skipneg, skiphello16, skipdlgtest, skipmbtest, skipwinver, skipclock, skipreversi
for f in "${@:2}"; do
    case $f in
    kbd) fl=kbdmode;;
    quiet|nocache|servertext|beat|ptykbd|repeat|skipneg|skiphello16|skipdlgtest|skipmbtest|skipwinver|skipclock|skipreversi) fl=$f;;
    *) echo "unknown flag $f"; exit 2;;
    esac
    echo $f > $T/flag; mfs $IMG cp $T/flag /root/$fl
done
mfs $IMG cp $W/tests/elks/autotest.sh /root/autotest.sh
mfs $IMG extract /etc/rc.sys $T/rc.sys
printf '\nif test -f /root/autotest.sh; then exec sh /root/autotest.sh; fi\n' >> $T/rc.sys
mfs $IMG rm /etc/rc.sys && mfs $IMG cp $T/rc.sys /etc/rc.sys
mfs $IMG extract /bootopts $T/bootopts
grep -q '^tandyvideo=1' $T/bootopts || printf 'tandyvideo=1\n' >> $T/bootopts
mfs $IMG rm /bootopts && mfs $IMG cp $T/bootopts /bootopts
mfsck -f $IMG > /dev/null
dd if=/dev/zero bs=512 count=17 2>/dev/null | cat - "$IMG" > "$OUT-mbr.img"
setboot "$OUT-mbr.img" -P17,4,615 -Sm "$ELKS_ROOT/bootblocks/mbr.bin"
truncate -s $((615 * 4 * 17 * 512)) "$OUT-mbr.img"
ls -l "$OUT-mbr.img"
