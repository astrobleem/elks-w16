.arch i8086, nojumps
.code16
.text
.global probe_guest, probe_old, probe_unknown, probe_init, probe_callback_a, probe_callback_b
probe_guest:
 mov $0x3000,%ax
 int $0x21
 mov %ax,0
 push %cs
 call w16_stubs
 mov %ax,2
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

probe_init:
 mov $1,%ax
 lret
probe_callback_a:
 push %bp
 mov %sp,%bp
 cmpw $0x2468,6(%bp)
 jne .Lbad_args
 cmpw $0x1357,8(%bp)
 jne .Lbad_args
 mov 0,%ax
 xor $0x100,%ax
 xor %dx,%dx
 pop %bp
 lret $4
probe_callback_b:
 push %bp
 mov %sp,%bp
 cmpw $0x2468,6(%bp)
 jne .Lbad_args
 cmpw $0x1357,8(%bp)
 jne .Lbad_args
 mov 0,%ax
 xor $0x200,%ax
 xor %dx,%dx
 pop %bp
 lret $4
.Lbad_args:
 xor %ax,%ax
 xor %dx,%dx
 pop %bp
 lret $4
