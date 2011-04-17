/*
 *  linux/arch/arm/plat-pnx/timer_sctu.c
 *
 *  Copyright (C) 2007 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <linux/interrupt.h>
#include <asm/arch/platform.h>
#include <asm/arch/param.h>
#include <asm/mach/time.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/pm.h>


/*** Module definition ***/
#define MODULE_NAME "PNX_TIMER"
#define PKMOD MODULE_NAME ": "

#define PNX_TIMER_DEBUG

//#define PNX_SCTU_CLOCK_SOURCE

#include <asm/arch/power_debug.h>
#ifdef CONFIG_PNX_POWER_TRACE
char * pnx_sctu_stopped_name = "SCTU set stopped";
char * pnx_sctu_set_event_name = "SCTU set";
char * pnx_sctu_mode_name = "SCTU mode";
#endif

/*** SCTU clock event ***/
/************************/

/*** SCTU HW ip register index definition ***/
#define TIMCR_IDX 0
#define TIMRR_IDX 1
#define TIMWR_IDX 2
#define TIMC0_IDX 3
#define TIMC1_IDX 4
#define TIMC2_IDX 5
#define TIMC3_IDX 6
#define TIMSR_IDX 7
#define TIMPR_IDX 8
/* SCTU sys clock definition */
#define SCTU_SYS_FRQ 13000000


/*** SCTU1 Clock event device ***/
#define SCTU1_ROOT_FRQ 100000
static int pnx_sctu1_set_next_event(unsigned long cycles,
		struct clock_event_device *evt);
static void pnx_sctu1_set_mode(enum clock_event_mode mode,
		struct clock_event_device *evt);

static struct clock_event_device clockevent_sctu = {
	.name		= "sctu_timer1",
	.rating		= 350,
	.shift = 20,
	.features   = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event	= pnx_sctu1_set_next_event,
	.set_mode	= pnx_sctu1_set_mode,
};


/*** SCTU2 Clock source device ***/
#define SCTU2_ROOT_FRQ 52000
static cycle_t pnx_sctu2_read(void);

static struct clocksource clocksource_sctu = {
	.name		= "sctu_timer2",
	.rating		= 350,
	.read		= pnx_sctu2_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.shift		= 8,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};


/*** SCTU driver ***/
#define RELOAD_COUNTER_POWER_SCTU 16
#define RELOAD_COUNTER_SCTU (1 << RELOAD_COUNTER_POWER_SCTU)

struct sctu_ctxt {
	unsigned long * base_adr;
	int autoreload;
	int32_t compvalue;
	int32_t endvalue;
	unsigned long pnx_sctu_timer_overflows;
};

struct sctu_ctxt sctu_table[2] =
{
	{
		.base_adr = (unsigned long *)SCTU1_BASE,
		.autoreload = 0,
		.compvalue = 0,
		.endvalue = 0,
		.pnx_sctu_timer_overflows = 0,
	},
	{
		.base_adr = (unsigned long *)SCTU2_BASE,
		.autoreload = 0,
		.compvalue = 0,
		.endvalue = 0,
		.pnx_sctu_timer_overflows = 0,
	},
};

static inline struct sctu_ctxt *pnx_sctu_get_context(int id)
{
	return &(sctu_table[id]);
}

static irqreturn_t
pnx_sctu_timer_interrupt ( int irq, void * dev_id, struct pt_regs * regs )
{
	uint8_t status;
	struct sctu_ctxt *sctu;

#ifdef PNX_TIMER_DEBUG
//	printk(PKMOD "sctu_timer_interrupt %d\n", irq);
#endif

	if(irq == IRQ_SCTU1)
		sctu = pnx_sctu_get_context(0);
	else
		sctu = pnx_sctu_get_context(1);

	status = readb((sctu->base_adr + TIMSR_IDX) );

	if (status & 0x1)
	{
		writeb ( ~0x01, (sctu->base_adr + TIMSR_IDX) );
		sctu->pnx_sctu_timer_overflows++;
	}

	if (status & 0x2)
	{
		struct clock_event_device *evt = &clockevent_sctu;

		writeb ( ~0x02, (sctu->base_adr + TIMSR_IDX) );

		if (sctu->autoreload)
		{
			sctu->endvalue += sctu->compvalue;

			if (sctu->endvalue >= 0)
				sctu->endvalue -= (RELOAD_COUNTER_SCTU);

			writew ( sctu->endvalue, (sctu->base_adr + TIMC0_IDX) );
		}
		else
		{
			uint16_t control;
			control = readw ( (sctu->base_adr + TIMCR_IDX) );
			control &= ~(0xC);
			writew (control, (sctu->base_adr + TIMCR_IDX) );
		}

		if (evt->event_handler)
			evt->event_handler(evt);
	}

    return IRQ_HANDLED;
}

static struct irqaction pnx_sctu_timer1_irq = {
    .name    = "PNX SCTU Timer1 Tick",
    .flags   = IRQF_DISABLED,
    .handler = (irq_handler_t)pnx_sctu_timer_interrupt,
};

static struct irqaction pnx_sctu_timer2_irq = {
    .name    = "PNX SCTU Timer2 Tick",
    .flags   = IRQF_DISABLED,
    .handler = (irq_handler_t) pnx_sctu_timer_interrupt,
};

static int pnx_sctu_timer_start(unsigned long cycles, int id)
{
	uint16_t control;
	struct sctu_ctxt *sctu = pnx_sctu_get_context(id);

#ifdef PNX_TIMER_DEBUG
//	printk(PKMOD "sctu_timer_start\n");
#endif

	sctu->compvalue = cycles;

	writeb ( ~0x02, (sctu->base_adr + TIMSR_IDX) );

	sctu->endvalue = sctu->compvalue +
		(0xFFFF0000 | readw ( (sctu->base_adr + TIMWR_IDX) ));

	if (sctu->endvalue >= 0)
		sctu->endvalue -= (RELOAD_COUNTER_SCTU);

	writew ( sctu->endvalue, (sctu->base_adr + TIMC0_IDX) );

	control = readw ( (sctu->base_adr + TIMCR_IDX) );
	control |= 0xC;
	writew ( control, (sctu->base_adr + TIMCR_IDX) );

	return 0;
}

static int pnx_sctu_timer_init(int id, int reload, int prescale, int over_it)
{
	struct sctu_ctxt *sctu = pnx_sctu_get_context(id);
	uint16_t v;
	struct clk * clk;

#ifdef PNX_TIMER_DEBUG
	printk(PKMOD "sctu_timer_init %d\n", id);
#endif

	/* Clock System Controller Timer Unit.
	 */
	clk = clk_get ( 0, "SCTU" );
	if (!IS_ERR(clk)) {
		clk_enable ( clk );
		clk_put ( clk );
		}

	/* Reset timer */
	writew ( 0x0000, (sctu->base_adr + TIMCR_IDX) ); /* reset control register */
	writeb ( 0x00  , (sctu->base_adr + TIMSR_IDX) ); /* clear whole status register */
	writew ( 0x0000, (sctu->base_adr + TIMRR_IDX) ); /* reset reload register */
	writeb ( 0x00  , (sctu->base_adr + TIMPR_IDX) ); /* reset pre-scaler reload register */
	writew ( 0x0000, (sctu->base_adr + TIMC0_IDX) ); /* reset channel 0 register */
	writew ( 0x0000, (sctu->base_adr + TIMC1_IDX) ); /* reset channel 1 register */
	writew ( 0x0000, (sctu->base_adr + TIMC2_IDX) ); /* reset channel 2 register */
	writew ( 0x0000, (sctu->base_adr + TIMC3_IDX) ); /* reset channel 3 register */

	/* Prescaler overflow each microsecond (13 MHz).
     */
	writeb ( prescale, (sctu->base_adr + TIMPR_IDX) );
	writew ( reload, (sctu->base_adr + TIMRR_IDX) ); /* power of 2 system clock */

	if (id == 0)
		setup_irq ( IRQ_SCTU1, &pnx_sctu_timer1_irq );
	else
		setup_irq ( IRQ_SCTU2, &pnx_sctu_timer2_irq );

	/* Set timer overflow interrupt and timer enable flags.
	*/
	v = readw ( (sctu->base_adr + TIMCR_IDX) );
	v |= 0x1 | over_it << 1;
	writew (v, (sctu->base_adr + TIMCR_IDX) );

	return 0;
}


/*** SCTU1 Clock event device ***/

static int pnx_sctu1_set_next_event(unsigned long cycles,
				    struct clock_event_device *evt)
{
#ifdef PNX_TIMER_DEBUG
//	printk(PKMOD "sctu1_set_next_event %d\n", cycles);
#endif

	pnx_sctu_timer_start(cycles, 0);

#ifdef CONFIG_PNX_POWER_TRACE
	{
		struct tick_sched *ts;
		int cpu;

		cpu = smp_processor_id();
		ts = tick_get_tick_sched(cpu);

		if(ts->tick_stopped)
			pnx_dbg_put_element(pnx_sctu_stopped_name, cycles, NULL);
		else
			pnx_dbg_put_element(pnx_sctu_set_event_name, cycles, NULL);
	}
#endif

	return 0;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_sctu_set_next_event);
#endif

static void pnx_sctu1_set_mode(enum clock_event_mode mode,
			      struct clock_event_device *evt)
{
	uint16_t control;
	struct sctu_ctxt *sctu = pnx_sctu_get_context(0);

#ifdef PNX_TIMER_DEBUG
//	printk(PKMOD "sctu1_set_mode %d\n", mode);
#endif

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_sctu_mode_name, mode, NULL);
#endif

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		control = readw ( (sctu->base_adr + TIMCR_IDX) );
		control |= 0xC;
		writew ( control, (sctu->base_adr + TIMCR_IDX) );
		sctu->autoreload = 1;
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		sctu->autoreload = 0;
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		control = readw ( (sctu->base_adr + TIMCR_IDX) );
		control &= ~0xC;
		writew ( control, (sctu->base_adr + TIMCR_IDX) );
	case CLOCK_EVT_MODE_RESUME:
		sctu->autoreload = 0;
		break;
	}
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_sctu_set_mode);
#endif

static void pnx_clockevent_init_sctu(void)
{
#ifdef PNX_TIMER_DEBUG
	printk(PKMOD "clockevent_init_sctu\n");
#endif

	/* prescale 13Mhz -> 1Mhz */
	pnx_sctu_timer_init(0, -RELOAD_COUNTER_SCTU,
			-(SCTU_SYS_FRQ / SCTU1_ROOT_FRQ), 0);

// issue it is shorter than reality and generates spurious irq
	clockevent_sctu.mult = div_sc(SCTU1_ROOT_FRQ, NSEC_PER_SEC, 20)+1;

	clockevent_sctu.max_delta_ns = div_sc(RELOAD_COUNTER_SCTU,
			clockevent_sctu.mult, clockevent_sctu.shift);

	clockevent_sctu.min_delta_ns = div_sc(1, clockevent_sctu.mult,
			clockevent_sctu.shift);
	if (clockevent_sctu.min_delta_ns < 10000)
		clockevent_sctu.min_delta_ns = 10000; /* avoid to much timer interrupt with 10us min between 2 irq */
	else if (clockevent_sctu.max_delta_ns < 10000)
		clockevent_sctu.min_delta_ns = clockevent_sctu.max_delta_ns >> 1;

	clockevent_sctu.cpumask = cpumask_of_cpu(0);
	clockevents_register_device(&clockevent_sctu);

	pnx_sctu1_set_next_event(SCTU1_ROOT_FRQ/HZ, &clockevent_sctu);
}


/**** SCTU2 Clock source device ****/
#ifdef PNX_SCTU_CLOCK_SOURCE

static cycle_t pnx_sctu2_read(void)
{
	struct sctu_ctxt *sctu = pnx_sctu_get_context(1);
	unsigned long ticks;
	uint8_t status;

	ticks = RELOAD_COUNTER_SCTU + (0xFFFF0000 | readw ( (sctu->base_adr + TIMWR_IDX) ));

	/* this function can be called under irq context */
	status = readb( (sctu->base_adr + TIMSR_IDX) );

	if (status & 0x1)
	{
		writeb ( ~0x01, (sctu->base_adr + TIMSR_IDX) );
		sctu->pnx_sctu_timer_overflows++;
	}

	ticks |= sctu->pnx_sctu_timer_overflows << RELOAD_COUNTER_POWER_SCTU;
	return ticks;
}

static void pnx_clocksource_init_sctu(void)
{
#ifdef PNX_TIMER_DEBUG
	printk(PKMOD "clocksource_init_sctu\n");
#endif
	if(SCTU2_ROOT_FRQ >= 100000)
		clocksource_sctu.mult =clocksource_khz2mult((SCTU2_ROOT_FRQ/1000), clocksource_sctu.shift);
	else
		clocksource_sctu.mult =clocksource_hz2mult((SCTU2_ROOT_FRQ), clocksource_sctu.shift);

	pnx_sctu_timer_init(1, -RELOAD_COUNTER_SCTU, -(SCTU_SYS_FRQ / SCTU2_ROOT_FRQ), 1);

	clocksource_register(&clocksource_sctu);
}
#endif
#endif




/*** System timer init ***/
/*************************/

void __init pnx_timer_init(void)
{
	printk(PKMOD "pnx_timer_init\n");

#ifdef PNX_SCTU_CLOCK_SOURCE
	pnx_clocksource_init_sctu();
#endif

	pnx_clockevent_init_sctu();

}

struct sys_timer pnx_timer = {
    .init   = pnx_timer_init,
#ifndef CONFIG_GENERIC_TIME
    .offset = NULL,
#endif
};


