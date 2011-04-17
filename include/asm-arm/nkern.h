/*
 ****************************************************************
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  "@(#)nkern.h 1.32     05/09/21 Jaluna"
 *
 * Contributor(s):
 *	Guennadi Maslov <guennadi.maslov@jaluna.com>
 *
 ****************************************************************
 */

#ifndef __NK_NKERN_H
#define __NK_NKERN_H

#include <asm/nk/f_nk.h>

#define	__NK_HARD_LOCK_IRQ_SAVE(x, flag)       hw_raw_local_irq_save(flag)
#define	__NK_HARD_UNLOCK_IRQ_RESTORE(x, flag)  hw_raw_local_irq_restore(flag)

#ifdef __ARMEB__
#define	__VEX_IRQ_BYTE		(3 - (NK_VEX_IRQ_BIT >> 3))
#else
#define	__VEX_IRQ_BYTE		(NK_VEX_IRQ_BIT >> 3)
#endif
#define	__VEX_IRQ_BIT		(NK_VEX_IRQ_BIT & 0x7)
#define	__VEX_IRQ_FLAG		(1 << __VEX_IRQ_BIT)

#ifdef  __ASSEMBLY__

#define	__VEX_IRQ_CTX_PEN	(ctx_pending_off + __VEX_IRQ_BYTE)
#define	__VEX_IRQ_CTX_ENA	(ctx_enabled_off + __VEX_IRQ_BYTE)
#define	__VEX_IRQ_CTX_E2P	(__VEX_IRQ_CTX_PEN - __VEX_IRQ_CTX_ENA)
#define	__VEX_IRQ_CTX_P2E	(__VEX_IRQ_CTX_ENA - __VEX_IRQ_CTX_PEN)
#define	__VEX_IRQ_CTX_V2E	(__VEX_IRQ_CTX_ENA - ctx_vil_off)
#define	__VEX_IRQ_CTX_V2P	(__VEX_IRQ_CTX_PEN - ctx_vil_off)

#endif

	/*
	 * In the thread context, the pending irq bitmask is saved in
	 * unused bits of CPRS image (bits 16-23).
	 */
#define	NK_VPSR_SHIFT	16

#ifndef __ASSEMBLY__

#ifdef CONFIG_NKERNEL_CONSOLE
extern void printnk (const char* fmt, ...);
#else
#define printnk printk
#endif
extern void nksetprio(int);


extern NkOsCtx* os_ctx;	 	/* pointer to OS context */
extern nku8_f*  __irq_enabled;  /* points to the enabled IRQ flag */

#endif

#endif
