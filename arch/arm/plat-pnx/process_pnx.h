/*
 * process_pnx.c
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Author:  E. Vigier <emeric.vigier@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>	/* need_resched() */
#include <linux/types.h>
#include <linux/module.h>

#ifndef INLINE
#define INLINE extern inline
#else
#define INLINE inline
#endif

#ifdef CONFIG_VMON_MODULE
/* Linux 2 modem idle switches */
extern unsigned int vmon_l2m_idle_switch;
#endif

extern unsigned long long pnx_rtke_read(void);

/* Idle function */
#ifdef CONFIG_NKERNEL
INLINE void pnx_idle(void)
{
	local_irq_disable();
	if (!need_resched()) {
		hw_raw_local_irq_disable();
		if (!raw_local_irq_pending()) {
#ifdef CONFIG_VMON_MODULE
			/* Linux 2 modem idle switches */
			vmon_l2m_idle_switch++;
#endif
			(void)os_ctx->idle(os_ctx);
		}
		hw_raw_local_irq_enable();
	}
	local_irq_enable();
}

INLINE void pnx_suspend_core(void)
{
	hw_raw_local_irq_disable();
	if (!raw_local_irq_pending()) {
		local_irq_disable();
		(void)os_ctx->idle(os_ctx);
	}
	hw_raw_local_irq_enable();
}

#else
/*
 * This is our default idle handler.  We need to disable
 * interrupts here to ensure we don't miss a wakeup call.
 */
extern void default_idle(void);
INLINE void pnx_idle(void)
{
	default_idle();
}

inline void pnx_suspend_cpu(void)
{
#warn "suspend mode not supported in stand alone"
}

#endif

