/*
 *  linux/arch/arm/kernel/entry-header.S
 *
 *  Adaptation to Jaluna OSware
 *  Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/autoconf.h> /* for CONFIG_ARCH_xxxx */
#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/errno.h>
#include <asm/thread_info.h>

#ifdef CONFIG_NKERNEL

#include <asm/nkern.h>

	.macro	get_os_ctx, reg
#ifdef CONFIG_XIP_KERNEL
	ldr	\reg, _os_ctx
	ldr	\reg, [\reg]
#else
	ldr	\reg, os_ctx
#endif
	.endm

	/*
	 * __irq_enabled__ must be defined locally in the includer file
	 */
	.macro	get_irq_enabled, reg
#ifdef CONFIG_XIP_KERNEL
	ldr	\reg, ___irq_enabled
	ldr	\reg, [\reg]
#else
	ldr	\reg, __irq_enabled__
#endif
	.endm

#define	DASH #

#define	NK_SIRQ_SAVE_SVC(rcpsr, rwork) 				 \
	get_irq_enabled	rwork					;\
	ldrb	rwork, [rwork]					;\
	orr	rcpsr, rcpsr, rwork, lsl DASH NK_VPSR_SHIFT

#define	NK_SIRQ_SAVE_USR(rcpsr) 				 \
	orr	rcpsr, rcpsr, DASH (__VEX_IRQ_FLAG << NK_VPSR_SHIFT)

#define	NK_SIRQ_RESTORE(rcpsr, rwork1, rwork2)		 	 \
	mov	rwork1, rcpsr, lsr DASH NK_VPSR_SHIFT		;\
	get_irq_enabled	rwork2					;\
	strb	rwork1, [rwork2]				;\
	ldrb	rwork2, [rwork2, DASH __VEX_IRQ_CTX_E2P]	;\
	tst	rwork1, rwork2

#define	NK_SIRQ_RESTORE_SVC(rcpsr, rwork1, rwork2) 		 \
	NK_SIRQ_RESTORE(rcpsr, rwork1, rwork2)			;\
	bne	__irq_svc_handler

#define	NK_SIRQ_RESTORE_USR_SLOW(rcpsr, rwork1, rwork2)		 \
	NK_SIRQ_RESTORE(rcpsr, rwork1, rwork2)			;\
	bne	__irq_usr_handler

#define	NK_SIRQ_RESTORE_USR_FAST(rcpsr, rwork1, rwork2)		 \
	NK_SIRQ_RESTORE(rcpsr, rwork1, rwork2)			;\
	strne	r0, [sp, #S_R0+S_OFF]!				;\
	bne	__irq_usr_handler

#endif /* CONFIG_NKERNEL */


@ Bad Abort numbers
@ -----------------
@
#define BAD_PREFETCH	0
#define BAD_DATA	1
#define BAD_ADDREXCPTN	2
#define BAD_IRQ		3
#define BAD_UNDEFINSTR	4

@
@ Most of the stack format comes from struct pt_regs, but with
@ the addition of 8 bytes for storing syscall args 5 and 6.
@ This _must_ remain a multiple of 8 for EABI.
@
#define S_OFF		8

/* 
 * The SWI code relies on the fact that R0 is at the bottom of the stack
 * (due to slow/fast restore user regs).
 */
#if S_R0 != 0
#error "Please fix"
#endif

	.macro	zero_fp
#ifdef CONFIG_FRAME_POINTER
	mov	fp, #0
#endif
	.endm

	.macro	get_thread_info, rd
	mov	\rd, sp, lsr #13
	mov	\rd, \rd, lsl #13
	.endm

	.macro	alignment_trap, rtemp
#ifdef CONFIG_ALIGNMENT_TRAP
	ldr	\rtemp, .LCcralign
	ldr	\rtemp, [\rtemp]
	mcr	p15, 0, \rtemp, c1, c0
#endif
	.endm


/*
 * These are the registers used in the syscall handler, and allow us to
 * have in theory up to 7 arguments to a function - r0 to r6.
 *
 * r7 is reserved for the system call number for thumb mode.
 *
 * Note that tbl == why is intentional.
 *
 * We must set at least "tsk" and "why" when calling ret_with_reschedule.
 */
scno	.req	r7		@ syscall number
tbl	.req	r8		@ syscall table pointer
why	.req	r8		@ Linux syscall (!= 0)
tsk	.req	r9		@ current thread_info
