#!/bin/bash
# Build test: an ELKS kernel with the headless console (CONFIG_CONSOLE_HEADLESS), which builds the BIOS
# keyboard driver (kbd-poll.c) without console.c, so elks-graphics-keys.patch must not need console.c
# there. Uses ELKS_TOPDIR's .config with the console changed, and its keyboard and console drivers as
# they are now (patched), in a temporary git worktree: the tree itself is not touched.
# Usage: ELKS_TOPDIR=/path/to/elks tests/headless_build.sh   (a git checkout, built once, cross tools in it)
set -u
M=${ELKS_TOPDIR:-}
if [ -z "$M" ] || [ ! -f "$M/.config" ] || ! git -C "$M" rev-parse --git-dir > /dev/null 2>&1; then
    echo "usage: ELKS_TOPDIR=/path/to/elks tests/headless_build.sh   (a configured ELKS git checkout)" >&2
    exit 2
fi
M=$(cd "$M" && pwd)
D=$(mktemp -d); W=$D/elks
git -C "$M" worktree add --detach "$W" HEAD > /dev/null 2>&1 || { echo "FAIL cannot make a worktree of $M"; rm -rf "$D"; exit 1; }
trap 'git -C "$M" worktree remove --force "$W" > /dev/null 2>&1; rm -rf "$D"' EXIT
C=elks/arch/i86/drivers/char
cp "$M"/$C/*.c "$M"/$C/*.h "$W"/$C/
cp "$M"/$C/KeyMaps/*.h "$W"/$C/KeyMaps/ 2> /dev/null
cd "$W"
export TOPDIR="$W" CROSSDIR="${CROSSDIR:-$M/cross}"
export PATH="$CROSSDIR/bin:$W/elks/tools/bin:$M/elks/tools/bin:$PATH"
sed -e 's/^CONFIG_CONSOLE_DIRECT=y/# CONFIG_CONSOLE_DIRECT is not set/' \
    -e 's/^CONFIG_CONSOLE_BIOS=y/# CONFIG_CONSOLE_BIOS is not set/' \
    -e 's/^CONFIG_KEYBOARD_SCANCODE=y/# CONFIG_KEYBOARD_SCANCODE is not set/' \
    -e 's/^# CONFIG_CONSOLE_HEADLESS is not set/CONFIG_CONSOLE_HEADLESS=y/' \
    -e '/^CONFIG_CONSOLE_MAX=/d' -e '/^CONFIG_KEYMAP_/d' "$M/.config" > .config
if ! { make kconfig < /dev/null && make include/autoconf.h; } > "$D/config.log" 2>&1; then
    echo "FAIL headless configuration"; tail -5 "$D/config.log"; exit 1
fi
if ! grep -q '^CONFIG_CONSOLE_HEADLESS=y' .config || grep -q '^CONFIG_CONSOLE_DIRECT=y\|^CONFIG_CONSOLE_BIOS=y' .config; then
    echo "FAIL the headless console was not selected"; exit 1
fi
if make kernel > "$D/build.log" 2>&1; then
    echo "ok   headless-console kernel with the keyboard patch links ($(stat -c %s elks/arch/i86/boot/Image) bytes)"
else
    echo "FAIL headless-console kernel does not build:"
    grep -v '^elftoolchain' "$D/build.log" | grep -m5 'undefined reference\|error:'
    exit 1
fi
