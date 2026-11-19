#include "taskimage.h"
.arch i8086, nojumps
.code16
.text
.global task_image_sp,task_image_enter
task_image_sp:
 mov %sp,%ax
 add $4,%ax
 lret
// Packet is child-owned far memory. Its first 16 bytes are image_start;
// its metadata survives restoring DATA. Abandon copied parent frames on the
// private native stack above the saved image. ELKS checks SP against brk even
// for far stacks, so no syscall may use an unrelated low-offset stack.
task_image_enter:
 mov %sp,%bx
 mov %ss:4(%bx),%si
 mov %si,%es
 cli
 mov %es:8,%ax
 mov %ax,%ss
 mov %es:6,%sp
 sti
 xor %di,%di
.Lread:
 mov %es:4,%dx
 sub %di,%dx
 jz .Lread_done
 cmp $4096,%dx
 jbe 1f
 mov $4096,%dx
1: mov %di,%cx
 mov %es:0,%bx
 mov $3,%ax
 int $0x80
 or %ax,%ax
 jle .Lread_failed
 add %ax,%di
 jmp .Lread
.Lread_done:
 mov %es:0,%bx
 mov $6,%ax
 int $0x80
 // Reset brk only on the real native stack, preserving ELKS heap guards.
 mov %es:8,%ax
 mov %ax,%ds
 cli
 mov %ax,%ss
 mov %es:6,%sp
 sti
 mov %es:4,%bx
 mov $17,%ax
 int $0x80
 or %ax,%ax
 jnz .Lbrk_failed
 // A far C call with the packet segment in AX (the native regparm ABI).
 mov %es,%ax
 push %cs
 mov $.Lreturned,%bx
 push %bx
 pushw %es:12
 pushw %es:10
 mov %ds,%bx
 mov %bx,%es
 lret
.Lread_failed:
 mov $.Lread_error,%cx
 mov $.Lread_error_end-.Lread_error,%dx
 jmp .Lreport
.Lbrk_failed:
 mov $.Lbrk_error,%cx
 mov $.Lbrk_error_end-.Lbrk_error,%dx
.Lreport:
 push %cs
 pop %ds
 mov $2,%bx
 mov $4,%ax
 int $0x80
.Lfailed:
 // Failure before C entry: the child-owned packet is still present.
 mov %si,%es
 movw $11,%es:14
 mov %es,%ax
 mov %ax,%ds
 mov %es:2,%bx
 mov $14,%cx
 mov $2,%dx
 mov $4,%ax
 int $0x80
.Lreturned:
 mov $1,%bx
 mov $1,%ax
 int $0x80
1: hlt
 jmp 1b
.Lread_error:
 .ascii "w16: child image read failed\n"
.Lread_error_end:
.Lbrk_error:
 .ascii "w16: child heap restore failed\n"
.Lbrk_error_end:
