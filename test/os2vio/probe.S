/* Repository-owned OS/2 NE console probe, 8086 instructions only.
 * The fixture builder supplies import relocations at the named operands. */
.code16
.arch i8086, nojumps
.text
.global start, vio_operand, exit_operand, version_operand, mode_operand
.global alloc_operand, free_operand, write_operand, close_operand
start:
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
    movw $48,%ax
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
