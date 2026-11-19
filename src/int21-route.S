#include "int21.h"
// 8086 only. No C calls or scheduling while the registry/IVT lock (IF=0)
// is held. The original INT frame and all registers reach the task handler.
.arch i8086, nojumps
.code16
.text
.global w16_int21_router
w16_int21_router:
 .byte 0xeb, .Ldispatch-w16_int21_router-2
 .word W16_INT21_MAGIC0, W16_INT21_MAGIC1, 1
.Ltable: .word 0
.Lold: .word 0,0
.Ltarget: .word 0,0
.Ldispatch:
 push %bp
 push %ds
 push %si
 push %cx
 push %ax
 push %bx
 mov %ss,%bx
 mov %cs:.Ltable,%ax
 mov %ax,%ds
 mov $2,%si
 mov $W16_INT21_TASKS,%cx
.Lscan:
 cmp %bx,(%si)
 je .Lfound
 add $6,%si
 loop .Lscan
 mov %cs:.Lold,%ax
 or %cs:.Lold+2,%ax
 jz .Lunknown
 pop %bx
 pop %ax
 pop %cx
 pop %si
 pop %ds
 pop %bp
 ljmpw *%cs:.Lold
.Lfound:
 mov 2(%si),%ax
 mov %ax,%cs:.Ltarget
 mov 4(%si),%ax
 mov %ax,%cs:.Ltarget+2
 pop %bx
 pop %ax
 pop %cx
 pop %si
 pop %ds
 pop %bp
 ljmpw *%cs:.Ltarget
.Lunknown:
 pop %bx
 pop %ax
 pop %cx
 pop %si
 pop %ds
 pop %bp
 // No old vector: unsupported DOS function, rather than jumping to 0:0.
 push %bp
 mov %sp,%bp
 orw $1,6(%bp)
 pop %bp
 mov $1,%ax
 iret

// begin(table, old offset, old CS) -> 1 installed, 0 ownership changed.
.global w16_route_begin
w16_route_begin:
 push %bp
 mov %sp,%bp
 push %es
 push %bx
 pushf
 cli
 xor %ax,%ax
 mov %ax,%es
 mov 8(%bp),%bx
 cmp %bx,%es:0x84
 jne .Lbegin_fail
 mov 10(%bp),%bx
 cmp %bx,%es:0x86
 jne .Lbegin_fail
 mov %bx,%cs:.Lold+2
 mov 8(%bp),%bx
 mov %bx,%cs:.Lold
 mov 6(%bp),%bx
 mov %bx,%cs:.Ltable
 mov $w16_int21_router,%bx
 mov %bx,%es:0x84
 mov %cs,%bx
 mov %bx,%es:0x86
 mov $1,%ax
 jmp .Lbegin_done
.Lbegin_fail:
 xor %ax,%ax
.Lbegin_done:
 popf
 pop %bx
 pop %es
 pop %bp
 lret

// add(table,key,handler offset,handler CS,router offset,router CS)
// -> table byte offset + 1, or 0. Check IVT before dereferencing the table.
.global w16_route_add
w16_route_add:
 push %bp
 mov %sp,%bp
 push %ds
 push %es
 push %si
 push %bx
 push %cx
 push %dx
 pushf
 cli
 xor %ax,%ax
 mov %ax,%es
 mov 14(%bp),%ax
 cmp %ax,%es:0x84
 jne .Ladd_fail
 mov 16(%bp),%ax
 cmp %ax,%es:0x86
 jne .Ladd_fail
 mov 6(%bp),%ds
 cmpw $1,0
 jne .Ladd_fail
 mov 8(%bp),%dx
 mov $2,%si
 mov $W16_INT21_TASKS,%cx
 mov $0xffff,%bx
.Ladd_scan:
 mov (%si),%ax
 cmp %dx,%ax
 je .Ladd_fail
 or %ax,%ax
 jnz .Ladd_next
 cmp $0xffff,%bx
 jne .Ladd_next
 mov %si,%bx
.Ladd_next:
 add $6,%si
 loop .Ladd_scan
 cmp $0xffff,%bx
 je .Ladd_fail
 mov 10(%bp),%ax
 mov %ax,2(%bx)
 mov 12(%bp),%ax
 mov %ax,4(%bx)
 mov %dx,(%bx)
 mov %bx,%ax
 inc %ax
 jmp .Ladd_done
.Ladd_fail:
 xor %ax,%ax
.Ladd_done:
 popf
 pop %dx
 pop %cx
 pop %bx
 pop %si
 pop %es
 pop %ds
 pop %bp
 lret

// finish() -> 1 restored, 0 peers active, -1 IVT ownership changed.
// Close registration and restore the original vector in ONE critical section.
.global w16_route_finish
w16_route_finish:
 push %ds
 push %es
 push %si
 push %cx
 pushf
 cli
 xor %ax,%ax
 mov %ax,%es
 mov $w16_int21_router,%ax
 cmp %ax,%es:0x84
 jne .Lfinish_foreign
 mov %cs,%ax
 cmp %ax,%es:0x86
 jne .Lfinish_foreign
 mov %cs:.Ltable,%ax
 mov %ax,%ds
 mov $2,%si
 mov $W16_INT21_TASKS,%cx
.Lfinish_scan:
 cmpw $0,(%si)
 jne .Lfinish_busy
 add $6,%si
 loop .Lfinish_scan
 movw $0,0
 mov %cs:.Lold,%ax
 mov %ax,%es:0x84
 mov %cs:.Lold+2,%ax
 mov %ax,%es:0x86
 movw $0,%cs:.Ltable
 mov $1,%ax
 jmp .Lfinish_done
.Lfinish_foreign:
 mov $-1,%ax
 jmp .Lfinish_done
.Lfinish_busy:
 xor %ax,%ax
.Lfinish_done:
 popf
 pop %cx
 pop %si
 pop %es
 pop %ds
 lret
