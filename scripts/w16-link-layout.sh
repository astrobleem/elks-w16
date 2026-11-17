#!/bin/sh
# Retain the installed IA-16 runtime layout and its segment markers. Its default
# hash-based split puts too much W16 code in the second segment; place the whole
# files.c module in the first, including both marker sections used by far calls.
set -eu
CC=${1:-ia16-elf-gcc}
layout=$("$CC" -mcmodel=medium -melks-libc -print-file-name=elks-medium.ld)
if [ ! -f "$layout" ] ||
   ! grep -Fq '*(.text .text$*' "$layout" ||
   ! grep -Fq '".text!*"' "$layout" ||
   ! grep -Fq '".text&*"' "$layout"; then
    echo 'Unsupported IA-16 medium linker layout; cannot balance code segments' >&2
    exit 1
fi
sed -e 's/".text!\*"/".fartext.f.files.c.*!" ".text!*"/' \
    -e 's/\*(.text .text\$\*/\*(.fartext.f.files.c.*$ .text .text$*/' \
    -e 's/".text\&\*"/".fartext.f.files.c.*\&" ".text\&*"/' "$layout"
# Do not silently produce wrapped 16-bit code offsets if future growth exhausts
# either segment. Unused Nano-X entry points are discarded by --gc-sections.
printf '\nASSERT (SIZEOF (.text) <= 0x10000, "W16 first code segment exceeds 64 KiB");\n'
printf 'ASSERT (SIZEOF (.fartext) <= 0x10000, "W16 second code segment exceeds 64 KiB");\n'
