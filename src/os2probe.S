/* Small-model cdecl native entry, Pascal far OS/2 imports, 8086 only.
 * The shared text guard prevents another native task borrowing our DS/stack. */
.code16
.arch i8086, nojumps
.text
.global o16_cs, o16_run, o16_vio, o16_exit, o16_version, o16_mode
.global o16_alloc, o16_free, o16_close, o16_type, o16_read, o16_write
o16_cs:
    movw %cs,%ax
    ret
native_ds: .word 0
o16_run:
    cld
    pushw %bp
    movw %sp,%bp
    pushw %ds
    pushw %es
    pushw %bx
    pushw %si
    pushw %di
    cmpw $0,%cs:native_ds
    jne busy
    movw %ds,%ax
    movw %ax,%cs:native_ds
    movw %sp,native_sp
    movw 8(%bp),%ax
    movw %ax,guest_ss
    movw 10(%bp),%ax
    movw %ax,guest_sp
    movw $0,o16_done
    movw $0,o16_code
    movw 4(%bp),%cx
    movw 6(%bp),%dx
    movw 12(%bp),%bx
    cli
    movw guest_ss,%ss
    movw guest_sp,%sp
    sti
    pushw %dx
    pushw %cx
    movw %bx,%ds
    movw %bx,%es
    xorw %ax,%ax
    xorw %bx,%bx
    xorw %cx,%cx
    xorw %dx,%dx
    xorw %si,%si
    xorw %di,%di
    xorw %bp,%bp
    lret
busy:
    movw $12,%ax
    jmp restore
/* Saved frame: ES DI SI DX CX BX DS BP, then far return, then args. */
o16_vio:
    pushw %bp
    movw $0,%bp
    jmp dispatch
o16_exit:
    pushw %bp
    movw $1,%bp
    jmp dispatch
o16_version:
    pushw %bp
    movw $2,%bp
    jmp dispatch
o16_mode:
    pushw %bp
    movw $3,%bp
    jmp dispatch
o16_alloc:
    pushw %bp
    movw $4,%bp
    jmp dispatch
o16_free:
    pushw %bp
    movw $5,%bp
    jmp dispatch
o16_close:
    pushw %bp
    movw $6,%bp
    jmp dispatch
o16_type:
    pushw %bp
    movw $7,%bp
    jmp dispatch
o16_read:
    pushw %bp
    movw $8,%bp
    jmp dispatch
o16_write:
    pushw %bp
    movw $9,%bp
    jmp dispatch
dispatch:
    cld
    pushw %ds
    pushw %bx
    pushw %cx
    pushw %dx
    pushw %si
    pushw %di
    pushw %es
    movw %ss,%dx
    movw %sp,%cx
    movw %cs:native_ds,%ax
    movw %ax,%ds
    movw %sp,guest_sp
    movw %dx,guest_ss
    cli
    movw %ax,%ss
    movw native_sp,%sp
    sti
    addw $20,%cx
    pushw %cx
    pushw %dx
    pushw %bp
    call o16_dispatch
    addw $6,%sp
    cmpw $0,o16_done
    jne finished
    movw o16_pop,%bp
    cli
    movw guest_ss,%ss
    movw guest_sp,%sp
    sti
    cmpw $2,%bp
    je return2
    cmpw $4,%bp
    je return4
    cmpw $8,%bp
    je return8
    cmpw $10,%bp
    je return10
    jmp return12
.macro return_frame bytes
return\bytes:
    popw %es
    popw %di
    popw %si
    popw %dx
    popw %cx
    popw %bx
    popw %ds
    popw %bp
    lret $\bytes
.endm
return_frame 2
return_frame 4
return_frame 8
return_frame 10
return_frame 12
finished:
    movw o16_code,%ax
    movw $0,%cs:native_ds
restore:
    popw %di
    popw %si
    popw %bx
    popw %es
    popw %ds
    popw %bp
    ret
.data
native_sp: .word 0
guest_ss: .word 0
guest_sp: .word 0
