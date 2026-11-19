.arch i8086, nojumps
.code16
.text
.global probe_guest, probe_old, probe_unknown
probe_guest:
 mov $0x3000,%ax
 int $0x21
 mov %ax,0
 mov $0x4c00,%ax
 int $0x21
 // A normal terminate must unwind to the native caller, never reach here.
 jmp .
probe_old:
 mov $0xc333,%ax
 iret
probe_unknown:
 int $0x21
 lret
