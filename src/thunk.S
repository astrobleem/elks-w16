// thunk.S - real-mode glue between Win16 code and the ELKS loader (8086 instructions only).
//
// Two stacks are in use: the program's stack (SS = its automatic data segment, as on
// Windows) and the loader's own stack in its ELKS data segment. Every transition saves
// the other side's SS:SP so calls can nest:
//   app -> API stub -> api_common -> C handler -> w16_call16 -> app window procedure -> API ...
// The loader's DS is kept in the code segment (w16_ldr_ds) because an API stub has no
// free register on entry.
//
// w16 is built in the medium memory model: C functions are far (called with lcall, return with
// lret) and may live in a second code segment, while this file stays in the near text segment.
// So every entry point below returns with lret, its arguments start at 6(%bp), and calls into
// C are far calls.

	.arch	i8086, nojumps
	.code16

	.data
	.global w16_app_ss, w16_app_sp, w16_ldr_sp
	.global w16_ret_bx, w16_ret_cx, w16_ret_es
w16_app_ss:	.word 0		// program stack while it is suspended in an API or interrupt
w16_app_sp:	.word 0
w16_ldr_sp:	.word 0		// where the loader stack continues when the program calls in
w16_ret_bx:	.word 0		// extra return registers (InitTask)
w16_ret_cx:	.word 0
w16_ret_es:	.word 0
w16_exit_sp:	.word 0		// loader SP to unwind to when the program terminates

	.text
	.global w16_ldr_ds
w16_ldr_ds:	.word 0
cb_target:	.word 0, 0

// int w16_thunk_init(void): record the loader DS in the code segment.
// Returns -1 if another w16 process sharing this text segment already did.
	.global w16_thunk_init
w16_thunk_init:
	mov	%cs:w16_ldr_ds, %ax
	or	%ax, %ax
	jz	1f
	mov	%ds, %dx
	cmp	%ax, %dx
	je	2f
	mov	$-1, %ax
	lret
1:	mov	%ds, %cs:w16_ldr_ds
2:	xor	%ax, %ax
	lret

	.global w16_thunk_done
w16_thunk_done:
	movw	$0, %cs:w16_ldr_ds
	lret

	.global w16_get_cs
w16_get_cs:
	mov	%cs, %ax
	lret

	.global w16_get_ds
w16_get_ds:
	mov	%ds, %ax
	lret

// void w16_farcpy(u16 dseg, u16 doff, u16 sseg, u16 soff, u16 n)
	.global w16_farcpy
w16_farcpy:
	push	%bp
	mov	%sp, %bp
	push	%si
	push	%di
	push	%es
	push	%ds
	mov	6(%bp), %es
	mov	8(%bp), %di
	mov	12(%bp), %si
	mov	14(%bp), %cx
	mov	10(%bp), %ds
	cld
	rep movsb
	pop	%ds
	pop	%es
	pop	%di
	pop	%si
	pop	%bp
	lret

// void w16_farset(u16 seg, u16 off, u16 val, u16 n)
	.global w16_farset
w16_farset:
	push	%bp
	mov	%sp, %bp
	push	%di
	push	%es
	mov	6(%bp), %es
	mov	8(%bp), %di
	mov	10(%bp), %ax
	mov	12(%bp), %cx
	cld
	rep stosb
	pop	%es
	pop	%di
	pop	%bp
	lret

// void w16_setvect(u16 vec, u16 off, u16 seg)
	.global w16_setvect
w16_setvect:
	push	%bp
	mov	%sp, %bp
	push	%es
	push	%bx
	xor	%ax, %ax
	mov	%ax, %es
	mov	6(%bp), %bx
	shl	%bx
	shl	%bx
	mov	8(%bp), %ax
	mov	10(%bp), %dx
	pushf
	cli
	mov	%ax, %es:(%bx)
	mov	%dx, %es:2(%bx)
	popf
	pop	%bx
	pop	%es
	pop	%bp
	lret

// u32 w16_getvect(u16 vec)
	.global w16_getvect
w16_getvect:
	push	%bp
	mov	%sp, %bp
	push	%es
	push	%bx
	xor	%ax, %ax
	mov	%ax, %es
	mov	6(%bp), %bx
	shl	%bx
	shl	%bx
	mov	%es:(%bx), %ax
	mov	%es:2(%bx), %dx
	pop	%bx
	pop	%es
	pop	%bp
	lret

// int w16_cpu_is_nec(void): AAD with base 16 gives 0x11 on Intel 8086/8088; the NEC V20/V30 always
// uses base 10 and gives 0x0B. Safe on both (no invalid opcodes).
	.global w16_cpu_is_nec
w16_cpu_is_nec:
	mov	$0x0101, %ax
	.byte	0xD5, 0x10		// aad 16
	cmp	$0x0B, %al
	mov	$0, %ax
	jne	1f
	inc	%ax
1:	lret

// void w16_outb(u16 port, u16 val)
	.global w16_outb
w16_outb:
	push	%bp
	mov	%sp, %bp
	mov	6(%bp), %dx
	mov	8(%bp), %ax
	outb	%al, %dx
	pop	%bp
	lret

// ---------------------------------------------------------------- API entry
// One 8-byte stub per api_table entry: AX = index, then the common path.
	.balign	8, 0x90
	.global w16_stubs
w16_stubs:
	.set	w16_i, 0
	.rept	160
	.balign	8, 0x90
	mov	$w16_i, %ax
	jmp	api_common
	.set	w16_i, w16_i + 1
	.endr

// Program stack on entry: IP CS args...   (far call from the program)
api_common:
	push	%bp
	push	%ds
	push	%si
	push	%di
	mov	%sp, %bp		// program frame: DI SI DS BP IP CS args
	mov	%ss, %bx
	mov	%cs:w16_ldr_ds, %si
	mov	%si, %ds
	mov	w16_app_ss, %cx		// enclosing program frame (nesting)
	mov	w16_app_sp, %dx
	mov	%bx, w16_app_ss
	mov	%bp, w16_app_sp
	cli
	mov	%si, %ss
	mov	w16_ldr_sp, %sp
	sti
	push	%cx
	push	%dx
	mov	%si, %es
	cld
	push	%ax
	lcall	$"w16_api_dispatch!"@SEG, $w16_api_dispatch	// DX:AX result; may move the frame up to pop arguments
	add	$2, %sp
	mov	w16_app_ss, %bx
	mov	w16_app_sp, %bp
	pop	%cx
	mov	%cx, w16_app_sp
	pop	%cx
	mov	%cx, w16_app_ss
	mov	w16_ret_cx, %cx
	mov	w16_ret_es, %di
	mov	w16_ret_bx, %si
	cli
	mov	%bx, %ss
	mov	%bp, %sp
	sti
	mov	%di, %es
	mov	%si, %bx
	pop	%di
	pop	%si
	pop	%ds
	pop	%bp
	lret

// ---------------------------------------------------------------- DOS services
// INT 21h while the program runs. Program stack: IP CS FLAGS.
	.global w16_int21
w16_int21:
	push	%bp
	push	%ds
	push	%es
	push	%di
	push	%si
	push	%dx
	push	%cx
	push	%bx
	push	%ax			// frame: AX BX CX DX SI DI ES DS BP IP CS FLAGS
	mov	%sp, %bp
	mov	%ss, %bx
	mov	%cs:w16_ldr_ds, %si
	mov	%si, %ds
	mov	w16_app_ss, %cx
	mov	w16_app_sp, %dx
	mov	%bx, w16_app_ss
	mov	%bp, w16_app_sp
	mov	%si, %ss
	mov	w16_ldr_sp, %sp
	sti
	push	%cx
	push	%dx
	mov	%si, %es
	cld
	lcall	$"w16_int21_dispatch!"@SEG, $w16_int21_dispatch	// edits the register frame in place
	pop	%dx
	pop	%cx
	mov	w16_app_ss, %bx
	mov	w16_app_sp, %bp
	mov	%dx, w16_app_sp
	mov	%cx, w16_app_ss
	cli
	mov	%bx, %ss
	mov	%bp, %sp
	pop	%ax
	pop	%bx
	pop	%cx
	pop	%dx
	pop	%si
	pop	%di
	pop	%es
	pop	%ds
	pop	%bp
	iret

// KERNEL.DOS3Call: a far call with DOS register arguments; returns flags like INT 21h.
	.global w16_dos3call
w16_dos3call:
	pushf
	push	%cs
	call	w16_int21		// emulate INT: FLAGS CS IP
	lret

// ---------------------------------------------------------------- calls into the program
// u32 w16_call16(u16 off, u16 seg, u16 ds, u16 nwords, const u16 *words)
// Pushes words[0..n-1] on the program stack (Pascal order), sets AX = DS = ds, far-calls.
	.global w16_call16
w16_call16:
	push	%bp
	mov	%sp, %bp
	push	%si
	push	%di
	push	%es
	push	w16_ldr_sp
	mov	%sp, w16_ldr_sp
	mov	6(%bp), %ax
	mov	%ax, %cs:cb_target
	mov	8(%bp), %ax
	mov	%ax, %cs:cb_target+2
	mov	12(%bp), %cx
	mov	14(%bp), %si
	mov	w16_app_ss, %es
	mov	w16_app_sp, %di
	jcxz	2f
1:	sub	$2, %di
	lodsw
	mov	%ax, %es:(%di)
	loop	1b
2:	mov	10(%bp), %ax
	mov	%es, %bx
	cli
	mov	%bx, %ss
	mov	%di, %sp
	sti
	mov	%ax, %ds
	mov	%ax, %es
	lcallw	*%cs:cb_target
	mov	%cs:w16_ldr_ds, %bx
	mov	%bx, %ds
	cli
	mov	%bx, %ss
	mov	w16_ldr_sp, %sp
	sti
	pop	w16_ldr_sp
	pop	%es
	pop	%di
	pop	%si
	pop	%bp
	lret

// u16 w16_start16(const u16 *regs)  regs: IP CS SS SP DS ES AX BX CX DX SI DI BP
// Starts the program like a Windows task; returns its exit code when w16_exit16 is called.
	.global w16_start16
w16_start16:
	push	%bp
	mov	%sp, %bp
	push	%si
	push	%di
	push	%es
	push	w16_ldr_sp
	mov	%sp, w16_ldr_sp
	mov	%sp, w16_exit_sp
	mov	6(%bp), %si
	mov	(%si), %ax
	mov	%ax, %cs:cb_target
	mov	2(%si), %ax
	mov	%ax, %cs:cb_target+2
	mov	4(%si), %es		// program SS
	mov	6(%si), %di		// program SP
	mov	%es, w16_app_ss
	mov	%di, w16_app_sp
	// build the register image below the program SP: BP DI SI DX CX BX AX ES DS
	mov	8(%si), %ax		// DS
	sub	$2, %di
	mov	%ax, %es:(%di)
	mov	10(%si), %ax		// ES
	sub	$2, %di
	mov	%ax, %es:(%di)
	mov	12(%si), %ax		// AX
	sub	$2, %di
	mov	%ax, %es:(%di)
	mov	14(%si), %ax		// BX
	sub	$2, %di
	mov	%ax, %es:(%di)
	mov	16(%si), %ax		// CX
	sub	$2, %di
	mov	%ax, %es:(%di)
	mov	18(%si), %ax		// DX
	sub	$2, %di
	mov	%ax, %es:(%di)
	mov	20(%si), %ax		// SI
	sub	$2, %di
	mov	%ax, %es:(%di)
	mov	22(%si), %ax		// DI
	sub	$2, %di
	mov	%ax, %es:(%di)
	mov	24(%si), %ax		// BP
	sub	$2, %di
	mov	%ax, %es:(%di)
	mov	%es, %ax
	cli
	mov	%ax, %ss
	mov	%di, %sp
	sti
	pop	%bp
	pop	%di
	pop	%si
	pop	%dx
	pop	%cx
	pop	%bx
	pop	%ax
	pop	%es
	pop	%ds
	ljmpw	*%cs:cb_target

// void w16_exit16(u16 code): called on the loader stack; abandons every nested frame and
// returns from w16_start16 with the code.
	.global w16_exit16
w16_exit16:
	mov	%sp, %bx
	mov	4(%bx), %ax		// past the far return address
	cli
	mov	w16_exit_sp, %sp
	sti
	pop	w16_ldr_sp
	pop	%es
	pop	%di
	pop	%si
	pop	%bp
	lret
