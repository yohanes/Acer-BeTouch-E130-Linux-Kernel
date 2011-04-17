/*
 *  linux/arch/arm/plat-pnx/timer_mtu.c
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
#include <linux/io.h>

#include <asm/mach/time.h>

#include <mach/hardware.h>

#include <asm/mach/irq.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>

#include <linux/clk.h>

#include <linux/clocksource.h>
#include <linux/clockchips.h>

#include "power_sysfs.h"

/*** System timer variable ***/
struct sys_timer pnx_timer;

/*** Module definition ***/
/*************************/

#define MODULE_NAME "PNX_TIMER"
#define PKMOD MODULE_NAME ": "

#undef PNX_TIMER_DEBUG
#if defined(PNX_TIMER_DEBUG)
#define debug(fmt, args...)                                          \
	printk(PKMOD fmt, ## args)
#else
#define debug(fmt, args...)
#endif

#if ! defined(CONFIG_NKERNEL) /* Linux standalone ? */
#define PNX_MTU_CLOCK_SOURCE
#endif

#include <mach/power_debug.h>
#ifdef CONFIG_PNX_POWER_TRACE
char * pnx_mtu_event_name = "MTU Event handler";
char * pnx_mtu_stopped_name = "MTU set stopped";
char * pnx_mtu_set_event_name = "MTU set";
char * pnx_mtu_mode_name = "MTU mode";
#endif

/*** MTU clock devices ***/
/*************************/

/*** MTU HW ip register index definition ***/
#define MTU_CON_IDX  0
#define MTU_TCVAL_IDX 1
#define MTU_PRESCALER_IDX 2
#define MTU_MATCH_CON_IDX 3
#define MTU_MATCH0_IDX 5
#define MTU_MATCH1_IDX 6
#define MTU_INT_OFFSET 0x3F4
#define MTU_MOD_CONF_IDX 1
#define MTU_INT_CLR_ENA_IDX 2
#define MTU_INT_SET_ENA_IDX 3
#define MTU_INT_STATUS_IDX 4
#define MTU_INT_ENABLE_IDX 5
#define MTU_INT_CLR_STAT_IDX 6
#define MTU_INT_SET_STAT_IDX 7

#define MTU_IRQ_MASK		0x1
#define MTU_USED_MATCH_IDX	MTU_MATCH0_IDX

/* MTU sys clock definition */
#define MTU_SYS_FRQ 13000000

/*** MTU Clock event device ***/
#define MTU_ROOT_FRQ 8000
static int pnx_mtu_set_next_event(unsigned long cycles,
				struct clock_event_device *evt);
static void pnx_mtu_set_mode(enum clock_event_mode mode,
				struct clock_event_device *evt);

static struct clock_event_device clockevent_mtu = {
	.name		= "mtu_timer",
	.rating		= 360,
	.shift		= 30,
	.features   = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event	= pnx_mtu_set_next_event,
	.set_mode	= pnx_mtu_set_mode,
};

/*** MTU Clock source device ***/
#ifdef PNX_MTU_CLOCK_SOURCE
static cycle_t pnx_mtu_read(struct clocksource *);

static struct clocksource clocksource_mtu = {
	.name		= "mtu_timer",
	.rating		= 360,
	.read		= pnx_mtu_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.shift		= 8,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};
#endif

/*** MTU driver ***/
#define RELOAD_COUNTER_POWER_MTU 32
#define RELOAD_COUNTER_MTU (1 << RELOAD_COUNTER_POWER_MTU)

struct mtu_ctxt {
	unsigned long * base_adr;
	int autoreload;
	uint32_t compvalue;
	uint32_t endvalue;
	struct clk * clk;
	int mode;
};

struct mtu_ctxt mtu_table[1] = {
	{
		.base_adr = (unsigned long *)MTU_BASE,
		.autoreload = 0,
		.compvalue = 0,
		.endvalue = 0,
		.clk = NULL,
		.mode = 0,
	},
};

static inline struct mtu_ctxt *pnx_mtu_get_context(int id)
{
	return &(mtu_table[id]);
}

static inline int pnx_mtu_timer_start(unsigned long cycles, int id);
static inline void pnx_mtu_clk_enable(int id);
static inline void pnx_mtu_clk_disable(int id);

static irqreturn_t
pnx_mtu_timer_interrupt ( int irq, void * dev_id, struct pt_regs * regs )
{
	uint8_t status, enable;
	struct mtu_ctxt *mtu;

	mtu = pnx_mtu_get_context(0);

	status = readl( (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_STATUS_IDX) );
	enable = readl( (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_ENABLE_IDX) );

	debug("mtu_timer_interrupt %d\n", status);

	if (status & enable & MTU_IRQ_MASK) {
		struct clock_event_device *evt = &clockevent_mtu;

		writel(MTU_IRQ_MASK,
		       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_CLR_STAT_IDX));

		if (mtu->autoreload)
			pnx_mtu_timer_start(mtu->compvalue, 0);
		else
			writel(MTU_IRQ_MASK,
			       (mtu->base_adr + MTU_INT_OFFSET +
				MTU_INT_CLR_ENA_IDX));

#ifdef CONFIG_PNX_POWER_TRACE
		pnx_dbg_put_element(pnx_mtu_event_name, 0, evt->event_handler);
#endif

		if (evt->event_handler)
			evt->event_handler(evt);
	}
#if 0
/* should be disable when hwlmm will hanlde second match irq */
	if (status & enable & 0x2) {
		writel(0x2,
		       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_CLR_STAT_IDX));
		writel(0x2,
		       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_CLR_ENA_IDX));
	}
#endif

    return IRQ_HANDLED;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_mtu_timer_interrupt);
#endif

static struct irqaction pnx_mtu_timer_irq = {
    .name    = "PNX MTU timer Tick",
    .flags   = IRQF_DISABLED,
    .handler = (irq_handler_t)pnx_mtu_timer_interrupt,
};

static inline void pnx_mtu_clk_enable(int id)
{
	struct mtu_ctxt *mtu = pnx_mtu_get_context(id);

	/* Clock Multimedia Timer Unit.
	 */
	if ((mtu->clk != NULL) && (mtu->mode==0)) {
		debug("mtu_clk_enable\n");
		mtu->mode = 1;
		clk_enable ( mtu->clk );
	}
}

static inline void pnx_mtu_clk_disable(int id)
{
	struct mtu_ctxt *mtu = pnx_mtu_get_context(id);

	/* Clock Multimedia Timer Unit.
	 */
	if ((mtu->clk != NULL) && (mtu->mode == 1)) {
		debug("mtu_clk_disable\n");
		clk_disable ( mtu->clk );
		mtu->mode = 0;
	}
}

static inline int pnx_mtu_timer_start(unsigned long cycles, int id)
{
	struct mtu_ctxt *mtu = pnx_mtu_get_context(id);
#ifdef CONFIG_NKERNEL
    unsigned long flags;
#endif

	debug("mtu_timer_start %d\n", cycles);
	pnx_mtu_clk_enable(id);
	
	/* MTU limitation : can't set a value smaller or equal to tcval + 1 */
	cycles = cycles < 2 ? 2 : cycles;
	
	mtu->compvalue = cycles;

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );
#endif /* CONFIG_NKERNEL */
	mtu->endvalue = mtu->compvalue + readl( (mtu->base_adr + MTU_TCVAL_IDX) );

	writel(MTU_IRQ_MASK,
	       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_CLR_STAT_IDX));
	
	writel ( mtu->endvalue, (mtu->base_adr + MTU_USED_MATCH_IDX) );
#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */

	writel(MTU_IRQ_MASK,
	       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_SET_ENA_IDX));

	/* the value has already expired */
	if ( (mtu->endvalue <= readl( (mtu->base_adr + MTU_TCVAL_IDX) ))
	  && (mtu->endvalue > mtu->compvalue)
	    && !(readl((mtu->base_adr + MTU_INT_OFFSET + MTU_INT_STATUS_IDX)) &
		 MTU_IRQ_MASK))
		writel(MTU_IRQ_MASK,
		       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_SET_STAT_IDX));

	return 0;
}

static int pnx_mtu_timer_init(int id, unsigned long reload,
			     unsigned long prescale, int over_it)
{

	struct mtu_ctxt *mtu = pnx_mtu_get_context(id);

	debug("mtu_timer_init %d\n", id);

	/* Enable clock */
/*
	pnx_mtu_clk_enable(id);
	clk mngt not available yet
	directly enable it
*/
	{
		unsigned long flags;
		unsigned long reg;
		hw_raw_local_irq_save ( flags );
		reg = readl ( CGU_GATESC2_REG );
		reg |= 0x1 << 2;
		writel ( reg , CGU_GATESC2_REG );
		hw_raw_local_irq_restore ( flags );
	}

	/* Reset timer */
	/* reset control register */
	writel(0x0000, (mtu->base_adr + MTU_CON_IDX));
	writel(0x0002, (mtu->base_adr + MTU_CON_IDX));
	/* reset control register */
	writel(0x0000, (mtu->base_adr + MTU_CON_IDX));

	/* clear whole enable irq register */
	writel(0xFF, (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_CLR_ENA_IDX));
	/* clear whole status register */
	writel(0xFF, (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_CLR_STAT_IDX));

	/* reset pre-scaler reload register */
	writel(0x00000000, (mtu->base_adr + MTU_PRESCALER_IDX));

	/* reset match control register */
	writel(0x0000, (mtu->base_adr + MTU_MATCH_CON_IDX));
	/* reset match 0 register */
	writel(0x00000000, (mtu->base_adr + MTU_MATCH0_IDX));
	/* reset match 1 register */
	writel(0x00000000, (mtu->base_adr + MTU_MATCH1_IDX));

	/* Initialize timer */
	writel ( prescale-1, (mtu->base_adr + MTU_PRESCALER_IDX) );
	/* power of 2 system clock */
	writel(reload, (mtu->base_adr + MTU_MATCH0_IDX));

	/* enable counter register */
	writel(0x0001, (mtu->base_adr + MTU_CON_IDX));

	/* clear whole status register */
	writel(0xFF, (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_CLR_STAT_IDX));

	if (id == 0)
		setup_irq ( IRQ_MTU, &pnx_mtu_timer_irq );

	/* Disable clock */
#ifndef PNX_MTU_CLOCK_SOURCE
	pnx_mtu_clk_disable(id);
#endif
	return 0;
}

/*** MTU Clock event device ***/

static int pnx_mtu_set_next_event(unsigned long cycles,
				    struct clock_event_device *evt)
{
	debug("mtu_set_next_event %d\n", cycles);
	pnx_mtu_timer_start(cycles, 0);

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_mtu_set_event_name, cycles, NULL);
#endif

	return 0;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_mtu_set_next_event);
#endif

static void pnx_mtu_set_mode(enum clock_event_mode mode,
			      struct clock_event_device *evt)
{
	struct mtu_ctxt *mtu = pnx_mtu_get_context(0);
	unsigned long reg;

	debug("mtu_set_mode %d\n", mode);
#ifdef CONFIG_PNX_POWER_TRACE
/*	pnx_dbg_put_element(pnx_mtu_mode_name, mode, NULL);*/
#endif

	switch (mode) {
	case CLOCK_EVT_MODE_UNUSED:
		mtu->autoreload = 0;

		pnx_mtu_clk_enable(0);
		writel(MTU_IRQ_MASK,
		       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_CLR_ENA_IDX));
		writel(MTU_IRQ_MASK,
		       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_CLR_STAT_IDX));

		reg = readl( (mtu->base_adr + MTU_TCVAL_IDX) );
		writel ( reg-1, (mtu->base_adr + MTU_USED_MATCH_IDX) );

#ifndef PNX_MTU_CLOCK_SOURCE
		pnx_mtu_clk_disable(0);

		if (mtu->clk != NULL)
			clk_put ( mtu->clk );
#endif
#ifdef CONFIG_PNX_POWER_TRACE
/*		pnx_dbg_put_element(pnx_mtu_mode_name, mode, NULL); */
#endif
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
		mtu->autoreload = 0;

		if (mtu->clk == NULL) {
			mtu->clk = clk_get ( 0, "MTU" );
			if (IS_ERR(mtu->clk))
				mtu->clk = NULL;
			}

		pnx_mtu_clk_enable(0);
		writel(MTU_IRQ_MASK,
		       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_SET_ENA_IDX));
		writel(MTU_IRQ_MASK,
		       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_CLR_STAT_IDX));
#ifndef PNX_MTU_CLOCK_SOURCE
		pnx_mtu_clk_disable(0);
#endif
		break;
	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_RESUME:
		mtu->autoreload = 0;

		pnx_mtu_clk_enable(0);
		writel(MTU_IRQ_MASK,
			(mtu->base_adr + MTU_INT_OFFSET + MTU_INT_SET_ENA_IDX));
		break;
	case CLOCK_EVT_MODE_PERIODIC:
		mtu->autoreload = 1;

		pnx_mtu_clk_enable(0);
		writel(MTU_IRQ_MASK,
		       (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_SET_ENA_IDX));
		break;
	}
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_mtu_set_mode);
#endif

static void pnx_clockevent_init_mtu(void)
{
	printk(PKMOD "clockevent_init_mtu\n");

	/* prescale 13Mhz -> 1Mhz */
#ifndef PNX_MTU_CLOCK_SOURCE
	pnx_mtu_timer_init(0, 0, (MTU_SYS_FRQ / MTU_ROOT_FRQ), 0);
#endif

/* issue it is shorter than reality and generates spurious irq */
/*      clockevent_mtu.mult = div_sc(MTU_ROOT_FRQ, NSEC_PER_SEC,
 *      clockevent_mtu.shift) + 1;*/
	clockevent_mtu.mult =
	    div_sc(MTU_ROOT_FRQ, NSEC_PER_SEC, clockevent_mtu.shift);

/*      clockevent_mtu.max_delta_ns = div_sc(RELOAD_COUNTER_MTU,
 *      clockevent_mtu.mult, clockevent_mtu.shift);*/
/*  In fact it is wider than the 32bits variable !!! */
	clockevent_mtu.max_delta_ns = 0xFFFFFFFF;

/*  MTU HW limitation: match register can't be set w/ tcval+1 */
/*      clockevent_mtu.min_delta_ns = div_sc(1, clockevent_mtu.mult,
 *      clockevent_mtu.shift)+1;*/
	clockevent_mtu.min_delta_ns =
	    div_sc(2, clockevent_mtu.mult, clockevent_mtu.shift) + 1;
	/* avoid to much timer interrupt with 10us min between 2 irq */
	if (clockevent_mtu.min_delta_ns < 10000)
		clockevent_mtu.min_delta_ns = 10000;
	else if (clockevent_mtu.max_delta_ns < 10000)
		clockevent_mtu.min_delta_ns = clockevent_mtu.max_delta_ns >> 1;

	clockevent_mtu.cpumask = get_cpu_mask(0);
	clockevents_register_device(&clockevent_mtu);

	pnx_mtu_set_next_event(MTU_ROOT_FRQ/HZ, &clockevent_mtu);
}

/*** MTU Clock source device ***/
#ifdef PNX_MTU_CLOCK_SOURCE

static cycle_t pnx_mtu_read(struct clocksource *source)
{
	struct mtu_ctxt *mtu = pnx_mtu_get_context(0);

	return readl((mtu->base_adr + MTU_TCVAL_IDX)) & source->mask;
}

static void pnx_clocksource_init_mtu(void)
{
	printk(PKMOD "clocksource_init_mtu\n");

	if(MTU_ROOT_FRQ >= 1000000)
		clocksource_mtu.mult =
		    clocksource_khz2mult((MTU_ROOT_FRQ / 1000),
					 clocksource_mtu.shift);
	else
		clocksource_mtu.mult =
		    clocksource_hz2mult((MTU_ROOT_FRQ), clocksource_mtu.shift);

	pnx_mtu_timer_init(0, 0, (MTU_SYS_FRQ / MTU_ROOT_FRQ), 0);

	clocksource_register(&clocksource_mtu);
}

#if 0
/*
 * Scheduler clock - returns current time in nanosec units.
 */
#define pnx_mtu_time sched_clock
unsigned long long sched_clock(void)
#else
unsigned long long pnx_mtu_time(void)
#endif
{
	unsigned long long ticks64 = pnx_mtu_read(&clocksource_mtu);

	return (ticks64 * clocksource_mtu.mult) >> clocksource_mtu.shift;
}
#endif

/*** SysFs interface ***/
/***********************/
#ifdef CONFIG_PNX_POWER_SYSFS

/*** Clock event sysfs interface **/

#define shows_one_evt(file_name, object)					\
static ssize_t show_##file_name##_evt						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%s\n", clockevent_mtu.object);		\
}

#define showu_one_evt(file_name, object)					\
static ssize_t show_##file_name##_evt						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%u\n", clockevent_mtu.object);		\
}

#define showlu_one_evt(file_name, object)					\
static ssize_t show_##file_name##_evt						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%lu\n", clockevent_mtu.object);		\
}

#define storelu_one_evt(file_name, object)					\
static ssize_t store_##file_name##_evt						\
(struct kobject *kobj, const char *buf, size_t size)				\
{									\
	unsigned long object; \
	strict_strtoul(buf, 10, &object); \
	clockevent_mtu.object = object; \
	return size;		\
}

#define showx_one_evt(file_name, object)					\
static ssize_t show_##file_name##_evt						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "0x%x\n", clockevent_mtu.object);		\
}

shows_one_evt(name, name);
showu_one_evt(rating, rating);
showlu_one_evt(mult, mult);
#ifdef CONFIG_PNX_TIMER_TUNE
storelu_one_evt(mult, mult);
#endif
showu_one_evt(shift, shift);
showx_one_evt(features, features);
showlu_one_evt(min_delta_ns, min_delta_ns);
showlu_one_evt(max_delta_ns, max_delta_ns);

static ssize_t show_clock_mode_evt(struct kobject *kobj, char *buf)
{
	struct mtu_ctxt *mtu = pnx_mtu_get_context(0);

	return sprintf (buf, "%u\n", mtu->mode);
}

static ssize_t show_periodic_evt(struct kobject *kobj, char *buf)
{
	struct mtu_ctxt *mtu = pnx_mtu_get_context(0);

	return sprintf (buf, "%u\n", mtu->autoreload);
}

static ssize_t show_next_event_evt(struct kobject *kobj, char *buf)
{
	struct mtu_ctxt *mtu = pnx_mtu_get_context(0);
	uint8_t enable;

	enable = readb( (mtu->base_adr + MTU_INT_OFFSET + MTU_INT_ENABLE_IDX) );

	if (enable & 0x1)
		return sprintf (buf, "%lu\n", (unsigned long)mtu->endvalue);
	else
		return sprintf (buf, "No Event Set\n");
}

define_one_ro_ext(name, _evt);
define_one_ro_ext(rating, _evt);
#ifdef CONFIG_PNX_TIMER_TUNE
define_one_rw_ext(mult, _evt);
#else
define_one_ro_ext(mult, _evt);
#endif
define_one_ro_ext(shift, _evt);
define_one_ro_ext(features, _evt);
define_one_ro_ext(min_delta_ns, _evt);
define_one_ro_ext(max_delta_ns, _evt);
define_one_ro_ext(clock_mode, _evt);
define_one_ro_ext(periodic, _evt);
define_one_ro_ext(next_event, _evt);

static struct attribute * dbs_attributes_event[] = {
	&name_evt.attr,
	&rating_evt.attr,
	&mult_evt.attr,
	&shift_evt.attr,
	&features_evt.attr,
	&min_delta_ns_evt.attr,
	&max_delta_ns_evt.attr,
	&clock_mode_evt.attr,
	&periodic_evt.attr,
	&next_event_evt.attr,
	NULL
};

static struct attribute_group dbs_attr_event_group = {
	.attrs = dbs_attributes_event,
	.name = "mtu_event",
};

/*** Clock source sysfs interface ***/
#ifdef PNX_MTU_CLOCK_SOURCE

#define shows_one_src(file_name, object)					\
static ssize_t show_##file_name##_src						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%s\n", clocksource_mtu.object);		\
}

#define showu_one_src(file_name, object)					\
static ssize_t show_##file_name##_src						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%u\n", clocksource_mtu.object);		\
}

#define showlu_one_src(file_name, object)					\
static ssize_t show_##file_name##_src						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%lu\n", clocksource_mtu.object);		\
}

#define showlx_one_src(file_name, object)					\
static ssize_t show_##file_name##_src						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "0x%lx\n", clocksource_mtu.object);		\
}

#define showllx_one_src(file_name, object)					\
static ssize_t show_##file_name##_src						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "0x%llx\n", clocksource_mtu.object);		\
}

shows_one_src(name, name);
showu_one_src(rating, rating);
showu_one_src(mult, mult);
showu_one_src(shift, shift);
showllx_one_src(mask, mask);
showlx_one_src(flags, flags);

static ssize_t show_stamp_src(struct kobject *kobj, char *buf)
{
	return sprintf(buf, "%lu\n",
		       (unsigned long)(pnx_mtu_read(&clocksource_mtu)));
}

static ssize_t show_time_src(struct kobject *kobj, char *buf)
{
		return sprintf (buf, "%llu\n", pnx_mtu_time());
}

define_one_ro_ext(name, _src);
define_one_ro_ext(rating, _src);
define_one_ro_ext(mult, _src);
define_one_ro_ext(shift, _src);
define_one_ro_ext(mask, _src);
define_one_ro_ext(flags, _src);
define_one_ro_ext(stamp, _src);
define_one_ro_ext(time, _src);

static struct attribute * dbs_attributes_srce[] = {
	&name_src.attr,
	&rating_src.attr,
	&mult_src.attr,
	&shift_src.attr,
	&mask_src.attr,
	&flags_src.attr,
	&stamp_src.attr,
	&time_src.attr,
	NULL
};

static struct attribute_group dbs_attr_srce_group = {
	.attrs = dbs_attributes_srce,
	.name = "mtu_source",
};
#endif
#endif

/*** System timer init ***/
/*************************/

void __init pnx_timer_init(void)
{
	printk(PKMOD "mtu_timer_init\n");

#ifdef PNX_MTU_CLOCK_SOURCE
	pnx_clocksource_init_mtu();
#endif

	pnx_clockevent_init_mtu();

}

struct sys_timer pnx_timer = {
    .init   = pnx_timer_init,
#ifndef CONFIG_GENERIC_TIME
    .offset = NULL,
#endif
};

#ifdef CONFIG_PNX_POWER_SYSFS
static int __init pnx_mtu_init_sysfs(void)
{
	printk(PKMOD "mtu_init_sysfs\n");

#ifdef PNX_MTU_CLOCK_SOURCE
	if (sysfs_create_group(&pnx_power_kobj, &dbs_attr_srce_group))
		printk(PKMOD "Unable to register %s in sysfs\n",
		       dbs_attr_srce_group.name);
#endif

	if (sysfs_create_group(&pnx_power_kobj, &dbs_attr_event_group))
		printk(PKMOD "Unable to register %s in sysfs\n",
		       dbs_attr_event_group.name);

	return 0;
}

module_init(pnx_mtu_init_sysfs);
#endif
