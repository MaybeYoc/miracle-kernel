/*
 * Low-level exception handling code
 *
 * Copyright (C) 2012 ARM Ltd.
 * Authors:	Catalin Marinas <catalin.marinas@arm.com>
 *		Will Deacon <will.deacon@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/uaccess.h>
#include <asm/asm-uaccess.h>
#include <asm/asm-offsets.h>
#include <asm/ptrace.h>
#include <asm/esr.h>

/*
 * Context tracking subsystem.  Used to instrument transitions
 * between user and kernel mode.
 */
	.macro ct_user_exit
	.endm

	.macro ct_user_enter
	.endm

	.macro clear_gp_regs
	.irp	n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29
	mov x\n, xzr
	.endr
	.endm

/*
 * Bad Abort numbers
 *-----------------
 */
#define BAD_SYNC	0
#define BAD_IRQ		1
#define BAD_FIQ		2
#define BAD_ERROR	3

	.macro kernel_ventry, el, label, regsize = 64
	.align 7
	sub sp, sp, #S_FRAME_SIZE
	b	el\()\el\()_\label
	.endm

	// This macro corrupts x0-x3. It is the caller's duty
	// to save/restore them if required.
	.macro apply_ssbd, state, tmp1, tmp2
	.endm

	.macro	kernel_entry, el, regsize = 64
	.if	\regsize == 32
	mov	w0, w0				// zero upper 32 bits of x0
	.endif
	stp	x0, x1, [sp, #16 * 0]
	stp	x2, x3, [sp, #16 * 1]
	stp	x4, x5, [sp, #16 * 2]
	stp	x6, x7, [sp, #16 * 3]
	stp	x8, x9, [sp, #16 * 4]
	stp	x10, x11, [sp, #16 * 5]
	stp	x12, x13, [sp, #16 * 6]
	stp	x14, x15, [sp, #16 * 7]
	stp	x16, x17, [sp, #16 * 8]
	stp	x18, x19, [sp, #16 * 9]
	stp	x20, x21, [sp, #16 * 10]
	stp	x22, x23, [sp, #16 * 11]
	stp	x24, x25, [sp, #16 * 12]
	stp	x26, x27, [sp, #16 * 13]
	stp	x28, x29, [sp, #16 * 14]

	.if	\el == 0
	clear_gp_regs
	mrs	x21, sp_el0
	ldr_this_cpu	tsk, __entry_task, x20	// Ensure MDSCR_EL1.SS is clear,
	ldr	x19, [tsk, #TSK_TI_FLAGS]	// since we can unmask debug
	disable_step_tsk x19, x20		// exceptions when scheduling.

	apply_ssbd 1, x22, x23

	.else
	add	x21, sp, #S_FRAME_SIZE
	get_thread_info tsk
	/* Save the task's original addr_limit and set USER_DS */
	ldr	x20, [tsk, #TSK_TI_ADDR_LIMIT]
	str	x20, [sp, #S_ORIG_ADDR_LIMIT]
	mov	x20, #USER_DS
	str	x20, [tsk, #TSK_TI_ADDR_LIMIT]
	/* No need to reset PSTATE.UAO, hardware's already set it to 0 for us */
	.endif /* \el == 0 */
	mrs	x22, elr_el1
	mrs	x23, spsr_el1
	stp	lr, x21, [sp, #S_LR]

	/*
	 * In order to be able to dump the contents of struct pt_regs at the
	 * time the exception was taken (in case we attempt to walk the call
	 * stack later), chain it together with the stack frames.
	 */
	.if \el == 0
	stp	xzr, xzr, [sp, #S_STACKFRAME]
	.else
	stp	x29, x22, [sp, #S_STACKFRAME]
	.endif
	add	x29, sp, #S_STACKFRAME

	stp	x22, x23, [sp, #S_PC]

	/* Not in a syscall by default (el0_svc overwrites for real syscall) */
	.if	\el == 0
	mov	w21, #NO_SYSCALL
	str	w21, [sp, #S_SYSCALLNO]
	.endif

	/*
	 * Set sp_el0 to current thread_info.
	 */
	.if	\el == 0
	msr	sp_el0, tsk
	.endif

	/*
	 * Registers that may be useful after this macro is invoked:
	 *
	 * x21 - aborted SP
	 * x22 - aborted PC
	 * x23 - aborted PSTATE
	*/
	.endm

	.macro	kernel_exit, el
	.if	\el != 0
	disable_daif

	/* Restore the task's original addr_limit. */
	ldr	x20, [sp, #S_ORIG_ADDR_LIMIT]
	str	x20, [tsk, #TSK_TI_ADDR_LIMIT]

	/* No need to restore UAO, it will be restored from SPSR_EL1 */
	.endif

	ldp	x21, x22, [sp, #S_PC]		// load ELR, SPSR
	.if	\el == 0
	ct_user_enter
	.endif

	.if	\el == 0
	ldr	x23, [sp, #S_SP]		// load return stack pointer
	msr	sp_el0, x23
	tst	x22, #PSR_MODE32_BIT		// native task?
	b.eq	3f

#ifdef CONFIG_ARM64_ERRATUM_845719
	mrs	x29, contextidr_el1
	msr	contextidr_el1, x29
#endif
3:
	apply_ssbd 0, x0, x1
	.endif

	msr	elr_el1, x21			// set up the return data
	msr	spsr_el1, x22
	ldp	x0, x1, [sp, #16 * 0]
	ldp	x2, x3, [sp, #16 * 1]
	ldp	x4, x5, [sp, #16 * 2]
	ldp	x6, x7, [sp, #16 * 3]
	ldp	x8, x9, [sp, #16 * 4]
	ldp	x10, x11, [sp, #16 * 5]
	ldp	x12, x13, [sp, #16 * 6]
	ldp	x14, x15, [sp, #16 * 7]
	ldp	x16, x17, [sp, #16 * 8]
	ldp	x18, x19, [sp, #16 * 9]
	ldp	x20, x21, [sp, #16 * 10]
	ldp	x22, x23, [sp, #16 * 11]
	ldp	x24, x25, [sp, #16 * 12]
	ldp	x26, x27, [sp, #16 * 13]
	ldp	x28, x29, [sp, #16 * 14]
	ldr	lr, [sp, #S_LR]
	add	sp, sp, #S_FRAME_SIZE		// restore sp

	eret
	sb
	.endm

	.macro	irq_stack_entry
	mov	x19, sp			// preserve the original sp

	/*
	 * Compare sp with the base of the task stack.
	 * If the top ~(THREAD_SIZE - 1) bits match, we are on a task stack,
	 * and should switch to the irq stack.
	 */
	ldr	x25, [tsk, TSK_STACK]
	eor	x25, x25, x19
	and	x25, x25, #~(THREAD_SIZE - 1)
	cbnz	x25, 9998f

	ldr_this_cpu x25, irq_stack_ptr, x26
	mov	x26, #IRQ_STACK_SIZE
	add	x26, x25, x26

	/* switch to the irq stack */
	mov	sp, x26
9998:
	.endm

	/*
	 * x19 should be preserved between irq_stack_entry and
	 * irq_stack_exit.
	 */
	.macro	irq_stack_exit
	mov	sp, x19
	.endm

/* GPRs used by entry code */
tsk	.req	x28		// current thread_info

/*
 * Interrupt handling.
 */
	.macro	irq_handler
	ldr_l	x1, handle_arch_irq
	mov	x0, sp
	//irq_stack_entry
	blr	x1
	//irq_stack_exit
	.endm

	.text

/*
 * Exception vectors.
 */
	.pushsection ".entry.text", "ax"

	.align	11
ENTRY(vectors)
	kernel_ventry	1, sync_invalid			// Synchronous EL1t
	kernel_ventry	1, irq_invalid			// IRQ EL1t
	kernel_ventry	1, fiq_invalid			// FIQ EL1t
	kernel_ventry	1, error_invalid		// Error EL1t

	kernel_ventry	1, sync				// Synchronous EL1h
	kernel_ventry	1, irq				// IRQ EL1h
	kernel_ventry	1, fiq_invalid			// FIQ EL1h
	kernel_ventry	1, error			// Error EL1h

	kernel_ventry	0, sync				// Synchronous 64-bit EL0
	kernel_ventry	0, irq				// IRQ 64-bit EL0
	kernel_ventry	0, fiq_invalid			// FIQ 64-bit EL0
	kernel_ventry	0, error			// Error 64-bit EL0

#ifdef CONFIG_COMPAT
	kernel_ventry	0, sync_compat, 32		// Synchronous 32-bit EL0
	kernel_ventry	0, irq_compat, 32		// IRQ 32-bit EL0
	kernel_ventry	0, fiq_invalid_compat, 32	// FIQ 32-bit EL0
	kernel_ventry	0, error_compat, 32		// Error 32-bit EL0
#else
	kernel_ventry	0, sync_invalid, 32		// Synchronous 32-bit EL0
	kernel_ventry	0, irq_invalid, 32		// IRQ 32-bit EL0
	kernel_ventry	0, fiq_invalid, 32		// FIQ 32-bit EL0
	kernel_ventry	0, error_invalid, 32		// Error 32-bit EL0
#endif
END(vectors)

/*
 * Invalid mode handlers
 */
	.macro	inv_entry, el, reason, regsize = 64
	kernel_entry \el, \regsize
	mov	x0, sp
	mov	x1, #\reason
	mrs	x2, esr_el1
	bl	bad_mode
	ASM_BUG()
	.endm

el0_sync_invalid:
	inv_entry 0, BAD_SYNC
ENDPROC(el0_sync_invalid)

el0_irq_invalid:
	inv_entry 0, BAD_IRQ
ENDPROC(el0_irq_invalid)

el0_fiq_invalid:
	inv_entry 0, BAD_FIQ
ENDPROC(el0_fiq_invalid)

el0_error_invalid:
	inv_entry 0, BAD_ERROR
ENDPROC(el0_error_invalid)

#ifdef CONFIG_COMPAT
el0_fiq_invalid_compat:
	inv_entry 0, BAD_FIQ, 32
ENDPROC(el0_fiq_invalid_compat)
#endif

el1_sync_invalid:
	inv_entry 1, BAD_SYNC
ENDPROC(el1_sync_invalid)

el1_irq_invalid:
	inv_entry 1, BAD_IRQ
ENDPROC(el1_irq_invalid)

el1_fiq_invalid:
	inv_entry 1, BAD_FIQ
ENDPROC(el1_fiq_invalid)

el1_error_invalid:
	inv_entry 1, BAD_ERROR
ENDPROC(el1_error_invalid)

/*
 * EL1 mode handlers.
 */
	.align	6
el1_sync:
	kernel_entry 1
	mrs	x1, esr_el1			// read the syndrome register
	lsr	x24, x1, #ESR_ELx_EC_SHIFT	// exception class
	cmp	x24, #ESR_ELx_EC_DABT_CUR	// data abort in EL1
	b.eq	el1_da
	cmp	x24, #ESR_ELx_EC_IABT_CUR	// instruction abort in EL1
	b.eq	el1_ia
	cmp	x24, #ESR_ELx_EC_SYS64		// configurable trap
	b.eq	el1_undef
	cmp	x24, #ESR_ELx_EC_SP_ALIGN	// stack alignment exception
	b.eq	el1_sp_pc
	cmp	x24, #ESR_ELx_EC_PC_ALIGN	// pc alignment exception
	b.eq	el1_sp_pc
	cmp	x24, #ESR_ELx_EC_UNKNOWN	// unknown exception in EL1
	b.eq	el1_undef
	cmp	x24, #ESR_ELx_EC_BREAKPT_CUR	// debug exception in EL1
	b.ge	el1_dbg
	b	el1_inv

el1_ia:
	/*
	 * Fall through to the Data abort case
	 */
el1_da:
	/*
	 * Data abort handling
	 */
	mrs	x3, far_el1
	inherit_daif	pstate=x23, tmp=x2
	clear_address_tag x0, x3
	mov	x2, sp				// struct pt_regs
	bl	do_mem_abort
	kernel_exit 1
el1_sp_pc:
	/*
	 * Stack or PC alignment exception handling
	 */
	mrs	x0, far_el1
	inherit_daif	pstate=x23, tmp=x2
	mov	x2, sp
	bl	do_sp_pc_abort
	ASM_BUG()
el1_undef:
	/*
	 * Undefined instruction
	 */
	inherit_daif	pstate=x23, tmp=x2
	mov	x0, sp
	bl	do_undefinstr
	kernel_exit 1
el1_dbg:
	/*
	 * Debug exception handling
	 */
	cmp	x24, #ESR_ELx_EC_BRK64		// if BRK64
	cinc	x24, x24, eq			// set bit '0'
	tbz	x24, #0, el1_inv		// EL1 only
	mrs	x0, far_el1
	mov	x2, sp				// struct pt_regs
	bl	do_debug_exception
	kernel_exit 1
el1_inv:
	// TODO: add support for undefined instructions in kernel mode
	inherit_daif	pstate=x23, tmp=x2
	mov	x0, sp
	mov	x2, x1
	mov	x1, #BAD_SYNC
	bl	bad_mode
	ASM_BUG()
ENDPROC(el1_sync)

	.align	6
el1_irq:
	kernel_entry 1
	enable_da_f

	irq_handler

#ifdef CONFIG_PREEMPT /* TODO */
	ldr	x24, [tsk, #TSK_TI_PREEMPT]	// get preempt count
	cbnz	x24, 1f				// preempt count != 0
	bl	el1_preempt
1:
#endif
	kernel_exit 1
ENDPROC(el1_irq)

/*
 * EL0 mode handlers.
 */
	.align	6
el0_sync:
	kernel_entry 0
	mrs	x25, esr_el1			// read the syndrome register
	lsr	x24, x25, #ESR_ELx_EC_SHIFT	// exception class
	cmp	x24, #ESR_ELx_EC_SVC64		// SVC in 64-bit state
	b.eq	el0_svc
	cmp	x24, #ESR_ELx_EC_DABT_LOW	// data abort in EL0
	b.eq	el0_da
	cmp	x24, #ESR_ELx_EC_IABT_LOW	// instruction abort in EL0
	b.eq	el0_ia
	cmp	x24, #ESR_ELx_EC_FP_ASIMD	// FP/ASIMD access
	b.eq	el0_fpsimd_acc
	cmp	x24, #ESR_ELx_EC_SVE		// SVE access
	b.eq	el0_sve_acc
	cmp	x24, #ESR_ELx_EC_FP_EXC64	// FP/ASIMD exception
	b.eq	el0_fpsimd_exc
	cmp	x24, #ESR_ELx_EC_SYS64		// configurable trap
	ccmp	x24, #ESR_ELx_EC_WFx, #4, ne
	b.eq	el0_sys
	cmp	x24, #ESR_ELx_EC_SP_ALIGN	// stack alignment exception
	b.eq	el0_sp_pc
	cmp	x24, #ESR_ELx_EC_PC_ALIGN	// pc alignment exception
	b.eq	el0_sp_pc
	cmp	x24, #ESR_ELx_EC_UNKNOWN	// unknown exception in EL0
	b.eq	el0_undef
	cmp	x24, #ESR_ELx_EC_BREAKPT_LOW	// debug exception in EL0
	b.ge	el0_dbg
	b	el0_inv

#ifdef CONFIG_COMPAT
	.align	6
el0_sync_compat:
	kernel_entry 0, 32
	mrs	x25, esr_el1			// read the syndrome register
	lsr	x24, x25, #ESR_ELx_EC_SHIFT	// exception class
	cmp	x24, #ESR_ELx_EC_SVC32		// SVC in 32-bit state
	b.eq	el0_svc_compat
	cmp	x24, #ESR_ELx_EC_DABT_LOW	// data abort in EL0
	b.eq	el0_da
	cmp	x24, #ESR_ELx_EC_IABT_LOW	// instruction abort in EL0
	b.eq	el0_ia
	cmp	x24, #ESR_ELx_EC_FP_ASIMD	// FP/ASIMD access
	b.eq	el0_fpsimd_acc
	cmp	x24, #ESR_ELx_EC_FP_EXC32	// FP/ASIMD exception
	b.eq	el0_fpsimd_exc
	cmp	x24, #ESR_ELx_EC_PC_ALIGN	// pc alignment exception
	b.eq	el0_sp_pc
	cmp	x24, #ESR_ELx_EC_UNKNOWN	// unknown exception in EL0
	b.eq	el0_undef
	cmp	x24, #ESR_ELx_EC_CP15_32	// CP15 MRC/MCR trap
	b.eq	el0_cp15
	cmp	x24, #ESR_ELx_EC_CP15_64	// CP15 MRRC/MCRR trap
	b.eq	el0_cp15
	cmp	x24, #ESR_ELx_EC_CP14_MR	// CP14 MRC/MCR trap
	b.eq	el0_undef
	cmp	x24, #ESR_ELx_EC_CP14_LS	// CP14 LDC/STC trap
	b.eq	el0_undef
	cmp	x24, #ESR_ELx_EC_CP14_64	// CP14 MRRC/MCRR trap
	b.eq	el0_undef
	cmp	x24, #ESR_ELx_EC_BREAKPT_LOW	// debug exception in EL0
	b.ge	el0_dbg
	b	el0_inv
el0_svc_compat:
	mov	x0, sp
	bl	el0_svc_compat_handler
	b	ret_to_user

	.align	6
el0_irq_compat:
	kernel_entry 0, 32
	b	el0_irq_naked

el0_error_compat:
	kernel_entry 0, 32
	b	el0_error_naked

el0_cp15:
	/*
	 * Trapped CP15 (MRC, MCR, MRRC, MCRR) instructions
	 */
	enable_daif
	ct_user_exit
	mov	x0, x25
	mov	x1, sp
	bl	do_cp15instr
	b	ret_to_user
#endif

el0_da:
	/*
	 * Data abort handling
	 */
	mrs	x26, far_el1
	enable_daif
	ct_user_exit
	clear_address_tag x0, x26
	mov	x1, x25
	mov	x2, sp
	bl	do_mem_abort
	b	ret_to_user
el0_ia:
	/*
	 * Instruction abort handling
	 */
	mrs	x26, far_el1
	enable_da_f
	ct_user_exit
	mov	x0, x26
	mov	x1, x25
	mov	x2, sp
	bl	do_el0_ia_bp_hardening
	b	ret_to_user
el0_fpsimd_acc:
	/*
	 * Floating Point or Advanced SIMD access
	 */
	enable_daif
	ct_user_exit
	mov	x0, x25
	mov	x1, sp
	bl	do_fpsimd_acc
	b	ret_to_user
el0_sve_acc:
	/*
	 * Scalable Vector Extension access
	 */
	enable_daif
	ct_user_exit
	mov	x0, x25
	mov	x1, sp
	bl	do_sve_acc
	b	ret_to_user
el0_fpsimd_exc:
	/*
	 * Floating Point, Advanced SIMD or SVE exception
	 */
	enable_daif
	ct_user_exit
	mov	x0, x25
	mov	x1, sp
	bl	do_fpsimd_exc
	b	ret_to_user
el0_sp_pc:
	/*
	 * Stack or PC alignment exception handling
	 */
	mrs	x26, far_el1
	enable_da_f
	ct_user_exit
	mov	x0, x26
	mov	x1, x25
	mov	x2, sp
	bl	do_sp_pc_abort
	b	ret_to_user
el0_undef:
	/*
	 * Undefined instruction
	 */
	enable_daif
	ct_user_exit
	mov	x0, sp
	bl	do_undefinstr
	b	ret_to_user
el0_sys:
	/*
	 * System instructions, for trapped cache maintenance instructions
	 */
	enable_daif
	ct_user_exit
	mov	x0, x25
	mov	x1, sp
	bl	do_sysinstr
	b	ret_to_user
el0_dbg:
	/*
	 * Debug exception handling
	 */
	tbnz	x24, #0, el0_inv		// EL0 only
	mrs	x0, far_el1
	mov	x1, x25
	mov	x2, sp
	bl	do_debug_exception
	enable_daif
	ct_user_exit
	b	ret_to_user
el0_inv:
	enable_daif
	ct_user_exit
	mov	x0, sp
	mov	x1, #BAD_SYNC
	mov	x2, x25
	bl	bad_el0_sync
	b	ret_to_user
ENDPROC(el0_sync)

	.align	6
el0_irq:
	kernel_entry 0
el0_irq_naked:
	enable_da_f
	ct_user_exit

	irq_handler

	b	ret_to_user
ENDPROC(el0_irq)

el1_error:
	kernel_entry 1
	mrs	x1, esr_el1
	enable_dbg
	mov	x0, sp
	bl	do_serror
	kernel_exit 1
ENDPROC(el1_error)

el0_error:
	kernel_entry 0
el0_error_naked:
	mrs	x1, esr_el1
	enable_dbg
	mov	x0, sp
	bl	do_serror
	enable_daif
	ct_user_exit
	b	ret_to_user
ENDPROC(el0_error)

/*
 * Ok, we need to do extra processing, enter the slow path.
 */
work_pending:
	mov	x0, sp				// 'regs'
	bl	do_notify_resume
	ldr	x1, [tsk, #TSK_TI_FLAGS]	// re-check for single-step
	b	finish_ret_to_user
/*
 * "slow" syscall return path.
 */
ret_to_user:
	disable_daif
	ldr	x1, [tsk, #TSK_TI_FLAGS]
	and	x2, x1, #_TIF_WORK_MASK
	cbnz	x2, work_pending
finish_ret_to_user:
	enable_step_tsk x1, x2
#ifdef CONFIG_GCC_PLUGIN_STACKLEAK
	bl	stackleak_erase
#endif
	kernel_exit 0
ENDPROC(ret_to_user)

/*
 * SVC handler.
 */
	.align	6
el0_svc:
	mov	x0, sp
	bl	el0_svc_handler
	b	ret_to_user
ENDPROC(el0_svc)

	.popsection				// .entry.text

/*
 * Register switch for AArch64. The callee-saved registers need to be saved
 * and restored. On entry:
 *   x0 = previous task_struct (must be preserved across the switch)
 *   x1 = next task_struct
 * Previous and next are guaranteed not to be the same.
 *
 */
ENTRY(cpu_switch_to)
	mov	x10, #THREAD_CPU_CONTEXT
	add	x8, x0, x10
	mov	x9, sp
	stp	x19, x20, [x8], #16		// store callee-saved registers
	stp	x21, x22, [x8], #16
	stp	x23, x24, [x8], #16
	stp	x25, x26, [x8], #16
	stp	x27, x28, [x8], #16
	stp	x29, x9, [x8], #16
	str	lr, [x8]
	add	x8, x1, x10
	ldp	x19, x20, [x8], #16		// restore callee-saved registers
	ldp	x21, x22, [x8], #16
	ldp	x23, x24, [x8], #16
	ldp	x25, x26, [x8], #16
	ldp	x27, x28, [x8], #16
	ldp	x29, x9, [x8], #16
	ldr	lr, [x8]
	mov	sp, x9
	msr	sp_el0, x1
	ret
ENDPROC(cpu_switch_to)

/*
 * This is how we return from a fork.
 */
ENTRY(ret_from_fork)
	bl	schedule_tail
	cbz	x19, 1f				// not a kernel thread
	mov	x0, x20
	blr	x19
1:	get_thread_info tsk
	b	ret_to_user
ENDPROC(ret_from_fork)
