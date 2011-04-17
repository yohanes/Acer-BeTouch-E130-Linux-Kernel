/*
 ****************************************************************
 *
 * Component = Nano-Kernel vtick back-end  Driver
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  ""
 *
 * Contributor(s):
 *
 ****************************************************************
 */

#include <linux/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/leds.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>

#include <asm/nkern.h>
#include <nk/nkern.h>

#define NK_VTICK_MODULO

/*
 * Functions which are board dependent which must be provided
 * to support this vtick front-end driver
 */
extern unsigned long nk_vtick_get_ticks_per_hz(void);
extern unsigned long nk_vtick_get_modulo(void);
extern unsigned long nk_vtick_read_stamp(void);
extern unsigned long nk_vtick_ticks_to_usecs(unsigned long);

NkDevTick* nktick;
static NkOsId     nktick_owner;
static NkOsId     nkosid;
int    nk_use_htimer = 1;

struct sys_timer nk_vtick_timer;

unsigned long nk_vtick_last_tick = 0;
unsigned long nk_vtick_ticks_per_hz;
unsigned long nk_vtick_modulo;

NkDevTick* nk_tick_lookup (void)
{
	int waiting = 0;
	NkDevTick* nktick;

	for (;;) {
		NkPhAddr pdev = 0;
		while ((pdev = nkops.nk_dev_lookup_by_type(NK_DEV_ID_TICK,
							   pdev))) {
			unsigned long flags;

			NkDevDesc* vdev = (NkDevDesc*)nkops.nk_ptov(pdev);
			nktick = (NkDevTick*)nkops.nk_ptov(vdev->dev_header);
			nktick_owner = (vdev->dev_owner ? vdev->dev_owner :
					NK_OS_PRIM);
			hw_raw_local_irq_save(flags);
			if (!nktick->freq) {
				nktick->freq = HZ;
			}
			if (nktick->freq != HZ) {
				nktick = 0;
			}
			hw_raw_local_irq_restore(flags);
			if (nktick) {
				if (waiting) {
					printnk("Virtual NK Timer device "
						"detected.\n");
				}
				return nktick;
			}
		}
		if (!waiting) {
			waiting = 1;
			printnk("Looking for a virtual NK Timer device...\n");
		}
	}
	return nktick;
}

void nk_tick_connect (NkDevTick * nktick, NkXIrqHandler tick_hdl)
{
	NkOsMask mask;
	NkXIrq   xirq;

	nkosid        = nkops.nk_id_get();
	mask = nkops.nk_bit2mask(nkosid);
	xirq = nkops.nk_xirq_alloc(1);

	if (!xirq) {
		panic("cannot allocate a xirq");
	}

	/*
	 * This is a hack... In fact two first parameters (cookie and xirq)
	 * are inverted in the xirq handler, but these arguments are not
	 * really used in the timer handler... only the regs are really used.
	 */
	if (!nkops.nk_xirq_attach_masked(xirq, tick_hdl, 0)){
	        panic("cannot attach xirq %d", xirq);
	}
	nktick->xirq[nkosid] = xirq;
	nkops.nk_atomic_set(&(nktick->enabled), mask);
	nkops.nk_xirq_trigger(nktick->xirq[nktick_owner], nktick_owner);
}

static int __init nk_vtimer_opt (char *s)
{
	if (!strcmp(s, "virtual")) {
		nk_use_htimer = 0;
		system_timer = &nk_vtick_timer;
	} else {
		nk_use_htimer = 1;
	}
	return 1;
}

__setup("linux-timer=", nk_vtimer_opt);


/*
 * The following is relying on some board dependent services
 */
static unsigned nk_modulo_count = 0; /* Counts 1/HZ units */

/*
 * Returns elapsed usecs since last timer interrupt
 */
static unsigned long nk_vtick_timer_gettimeoffset(void)
{
	unsigned long now = nk_vtick_read_stamp();
	return nk_vtick_ticks_to_usecs(now
				       - (unsigned long)  nk_vtick_last_tick);
}

static irqreturn_t nk_vtick_timer_interrupt(int irq, void *dev_id,
					    struct pt_regs *regs)
{
	unsigned long flags;
	unsigned long now;

	write_seqlock_irqsave(&xtime_lock, flags);
#if 1
	{
		char atomic;
		do{
			atomic=nktick->atomic;
	now = nktick->last_stamp;

		}while (nktick->atomic!=atomic);
	}
#else
	now = nk_vtick_read_stamp();
#endif
	while (now - nk_vtick_last_tick >= nk_vtick_ticks_per_hz) {
#ifdef NK_VTICK_MODULO
		/* Modulo addition may put nk_vtick_last_tick ahead of now
		 * and cause unwanted repetition of the while loop.
		 */
		if (unlikely(now - nk_vtick_last_tick == ~0))
			break;

		nk_modulo_count += nk_vtick_modulo;
		if (nk_modulo_count > HZ) {
			++nk_vtick_last_tick;
			nk_modulo_count -= HZ;
		}
#endif
		nk_vtick_last_tick += nk_vtick_ticks_per_hz;
#ifndef CONFIG_GENERIC_CLOCKEVENTS
		timer_tick(/*regs checl LPA*/);
#endif
	}

	write_sequnlock_irqrestore(&xtime_lock, flags);
	return IRQ_HANDLED;
}

#ifdef CONFIG_NO_IDLE_HZ


void nk_vtick_timer_reprogram(unsigned long /* * */next_tick)
{

#if 1
		nktick->silent[nkosid] = next_tick; //next_tick[0];
		nktick->delta[nkosid] = 0; // LPA next_tick[1];
#else
		nktick->silent[nkosid] = (next_tick[0]*1000000)/HZ;
		nktick->delta[nkosid] = (next_tick[1]*1000000)/HZ;
#endif

}

static int nk_vtick_timer_enable_dyn_tick(void)
{
	nktick->silent[nkosid] = 0;
	nktick->delta[nkosid] = 0;
	/* No need to reprogram timer, just use the next interrupt */
	return 0;
}

static int nk_vtick_timer_disable_dyn_tick(void)
{
	nktick->silent[nkosid] = 0;
	nktick->delta[nkosid] = 0;
	return 0;
}


static struct dyn_tick_timer nk_vtick_dyn_tick_timer = {
	.state     = DYN_TICK_ENABLED,
	.enable		= nk_vtick_timer_enable_dyn_tick,
	.disable	= nk_vtick_timer_disable_dyn_tick,
	.reprogram	= nk_vtick_timer_reprogram,
	.handler	= nk_vtick_timer_interrupt,
};
#endif

static __init void nk_init_vtick_timer(void)
{
	nk_vtick_ticks_per_hz = nk_vtick_get_ticks_per_hz();
	nk_vtick_modulo = nk_vtick_get_modulo();

#ifdef CONFIG_NO_IDLE_HZ
	nk_vtick_timer.dyn_tick = &nk_vtick_dyn_tick_timer;
#endif
#ifndef CONFIG_GENERIC_TIME
	nk_vtick_timer.offset  = nk_vtick_timer_gettimeoffset;
#endif
	nk_vtick_last_tick = nk_vtick_read_stamp();

	nktick = nk_tick_lookup();

	nktick->silent[nkosid] = 0;
	nktick->delta[nkosid] = 0;

	nk_tick_connect(nktick, (NkXIrqHandler)nk_vtick_timer_interrupt);
}

struct sys_timer nk_vtick_timer = {
	.init		= nk_init_vtick_timer,
	.offset		= NULL,		/* Initialized later */
};

