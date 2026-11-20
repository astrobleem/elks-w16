#!/bin/sh
# Accept the pinned client or the previous autostart-options patch.
set -eu
: "${ROOT:?}" "${MWDIR:?}"
handoff="$ROOT/patches/nanox-autostart-handoff.patch"
if patch --fuzz=0 --force --reverse --dry-run -d "$MWDIR" -p1 < "$handoff" >/dev/null 2>&1; then
    exit 0
fi
options="$ROOT/patches/nanox-autostart-env.patch"
if ! patch --fuzz=0 --force --reverse --dry-run -d "$MWDIR" -p1 < "$options" >/dev/null 2>&1; then
    patch --fuzz=0 --batch --forward -d "$MWDIR" -p1 < "$options"
fi
patch --fuzz=0 --batch --forward -d "$MWDIR" -p1 < "$handoff"
