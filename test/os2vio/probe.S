/* Repository-owned OS/2 NE console probe, 8086 instructions only.
 * The fixture builder supplies import relocations at the named operands. */
.code16
.arch i8086, nojumps
.text
.global start, vio_operand, exit_operand, version_operand, mode_operand
.global alloc_operand, free_operand, write_operand, close_operand
start:
.ifdef DENY_SETVEC_PROBE
.global setvec_operand
    movw $0x1357,100
    movw $0x2468,102
    xorw %ax,%ax
    pushw %ax
    pushw %cs
    movw $deny_handler-start,%ax
    pushw %ax
    pushw %ds
    movw $100,%ax
    pushw %ax
    .byte 0x9a
setvec_operand:
    .word 0xffff,0xffff
    cmpw $1,%ax                /* Explicit failure, never fake registration */
    je 1f
    jmp failed
1:
    cmpw $0x1357,100
    je 1f
    jmp failed
1:
    cmpw $0x2468,102
    je 1f
    jmp failed
1:
.endif
    pushw %ds
    movw $64,%ax
    pushw %ax
    .byte 0x9a
version_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    cmpw $0x0a1e,64
    je 1f
    jmp failed
1:
    pushw %ds
    movw $66,%ax
    pushw %ax
    .byte 0x9a
mode_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    cmpb $0,66
    je 1f
    jmp failed
1:
.ifdef ARENA_PROBE
.global dbcs_operand, dsgrow_operand, avail_operand, realloc_operand, regrow_operand
    movw $0,80
    movw $0,82
    movw $0xffff,84
    movw $10,%ax
    pushw %ax
    pushw %ds
    movw $80,%ax
    pushw %ax
    pushw %ds
    movw $84,%ax
    pushw %ax
    .byte 0x9a
dbcs_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    cmpw $0,84
    je 1f
    jmp failed
1:
    cmpw $0,92
    je 1f
    jmp failed
1:
    movw $8192,%ax
    pushw %ax
    pushw %ds
    .byte 0x9a
dsgrow_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    cmpb $0,8191
    je 1f
    jmp failed
1:
    pushw %ds
    movw $96,%ax
    pushw %ax
    .byte 0x9a
avail_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    movw 96,%ax
    cmpw $0,98
    je 1f
    movw $0xffff,%ax
1:
.else
    movw $48,%ax
.endif
    pushw %ax
    pushw %ds
    movw $68,%ax
    pushw %ax
    xorw %ax,%ax
    pushw %ax
    .byte 0x9a
alloc_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
.ifdef ARENA_PROBE
    movw 68,%ax
    movw %ax,%es
    movw $0x1234,%es:0
    movw $1024,%ax
    pushw %ax
    pushw 68
    .byte 0x9a
realloc_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    movw $4096,%ax
    pushw %ax
    pushw 68
    .byte 0x9a
regrow_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    cmpw $0x1234,%es:0
    je 1f
    jmp failed
1:
.endif
    pushw 68
    .byte 0x9a
free_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    movw $1,%ax
    pushw %ax                  /* redirected guest stdout */
    pushw %ds
    xorw %ax,%ax
    pushw %ax
    movw $26,%ax
    pushw %ax
    pushw %ds
    movw $70,%ax
    pushw %ax
    .byte 0x9a
write_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    cmpw $26,70
    je 1f
    jmp failed
1:
    movw $2,%ax
    pushw %ax                  /* closing guest stderr preserves loader log */
    .byte 0x9a
close_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    pushw %ds
    xorw %ax,%ax
    pushw %ax                  /* message offset zero in data segment */
    movw $26,%ax
    pushw %ax
    xorw %ax,%ax
    pushw %ax                  /* HVIO = 0 */
    .byte 0x9a                 /* FAR CALL operand is fixed by NE loader */
vio_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jz 1f
    jmp failed
1:
    movw $37,%ax
    jmp exit
failed:
    movw $98,%ax
exit:
    movw $1,%dx
    pushw %dx                  /* EXIT_PROCESS */
    pushw %ax
    .byte 0x9a
exit_operand:
    .word 0xffff,0xffff
    jmp .                      /* DosExit must not return */

.ifdef DENY_SETVEC_PROBE
deny_handler:
    iret                      /* supplied as code, never invoked or faulted */
.endif
