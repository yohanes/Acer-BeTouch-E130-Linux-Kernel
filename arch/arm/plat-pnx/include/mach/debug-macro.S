/* linux/arch/arm/plat-pnx/include/mach/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 1994-1999 Russell King
 *  Moved from linux/arch/arm/kernel/debug.S by Ben Dooks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

/* FIXME_PNX rewrite the complete assembler part */

		.macro	addruart,rx
		mov	\rx,	  #0xc2000000   @virtual base
		orr	\rx, \rx, #0x00004000	@ offset
		.endm

		.macro	senduart,rd,rx
		strb	\rd, [\rx, #0x0]        @ FIXME_PNX
		.endm

		.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #0x14]	@ UART1_LSR
		tst	\rd, #1 << 5		@ UART1_LSR_THRE - 0 when full
		beq	1001b
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, #0x14]	@ UART1_LSR
		tst	\rd, #1 << 6		@ UART1_LSR_TEMT - 0 when busy
		beq	1001b
		.endm
