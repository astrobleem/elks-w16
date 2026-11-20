/* Repository-owned OS/2 NE console probe, 8086 instructions only.
 * The fixture builder supplies import relocations at the named operands. */
.code16
.arch i8086, nojumps
.text
.global start, vio_operand, exit_operand, version_operand, mode_operand
start:
    pushw %ds
    movw $64,%ax
    pushw %ax
    .byte 0x9a
version_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jnz failed
    cmpw $0x0a1e,64
    jne failed
    pushw %ds
    movw $66,%ax
    pushw %ax
    .byte 0x9a
mode_operand:
    .word 0xffff,0xffff
    orw %ax,%ax
    jnz failed
    cmpb $0,66
    jne failed
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
    jnz failed
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
