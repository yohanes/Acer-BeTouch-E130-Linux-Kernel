/*
 * linux/arch/arm/plat-pnx/include/mach/entry-macro.S
 *
 * Low-level IRQ helper macros for PNX-based platforms
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <mach/hardware.h>

		.macro  arch_ret_to_user, tmp1, tmp2
		.endm

		.macro	disable_fiq
		.endm

		.macro  get_irqnr_preamble, base, tmp
		.endm

/******************************************************************************
* Procedure  : get_irqnr_and_base
*------------------------------------------------------------------------------
* Description: This macro determines the interrupt request number of the
*              highest prio irq.
*------------------------------------------------------------------------------
* Input Parameter(s)   : processor registers to work on,
*                        no register contents to be saved.
*
*------------------------------------------------------------------------------
* Output Parameter(s)  : if IRQ pending: Z-flag=0, r0=irqnrirqnr
*                        if no IRQ pending: Z-flag=1
*------------------------------------------------------------------------------
* Procedures called    : __irq_svc
*                        __irq_usr
*
*------------------------------------------------------------------------------
* Notes    :
*    - register handed over in tmp:   not used
*    - the two realisations below distinguish as follows:
*      1. realisation: execution fast (8 instr), no loops, to be prooven
*         status: to be debugged
*******************************************************************************/
		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp

       	ldr	\base, =IO_ADDRESS(INTC_BASE_ADDR)
		ldr	\irqnr, [\base, #INTC_VECTOR_IRQ_OFFSET]        @ load intc vector reg
		mov \irqnr, \irqnr, lsr #3                          @ Bits[0..2] are reserved
		ands \irqnr, \irqnr, #0x00FF                        @ mask Bits[11..31]
		.endm

       	.macro	irq_prio_table
		.endm

