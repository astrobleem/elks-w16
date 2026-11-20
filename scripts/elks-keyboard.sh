#!/bin/sh
# Called by the server builds. tty preserves the existing cooked backend.
set -eu
: "${ROOT:?}" "${MWDIR:?}" "${TOPDIR:?}"
case "${ELKS_KBD:-tty}" in tty|scancode) ;; *) echo 'ELKS_KBD must be tty or scancode' >&2;exit 1;; esac
if [ "${ELKS_KBD:-tty}" = scancode ]; then
 if ! grep -q '^CONFIG_KEYBOARD_SCANCODE=y' "$TOPDIR/.config" || grep -q '^CONFIG_ARCH_PC98=y' "$TOPDIR/.config"; then
  echo 'ELKS_KBD=scancode requires an IBM-PC CONFIG_KEYBOARD_SCANCODE kernel (US set-1)' >&2;exit 1
 fi
 if [ "${NANOX_STARTUP_TRACE:-0}" = 1 ]; then
  echo 'Use a native key probe with ELKS_KBD=scancode; NANOX_STARTUP_TRACE is for the tty initialization order' >&2;exit 1
 fi
fi
patchfile="$ROOT/patches/nanox-elks-rawkbd.patch"
if ! patch --fuzz=0 --force --reverse --dry-run -d "$MWDIR" -p1 < "$patchfile" >/dev/null 2>&1; then
 patch --fuzz=0 --batch --forward -d "$MWDIR" -p1 < "$patchfile"
fi
cp "$ROOT/drivers/kbd_elks_raw.c" "$ROOT/drivers/elks_set1.h" "$MWDIR/src/drivers/"
