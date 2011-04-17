/*
 *  linux/arch/arm/plat-pnx/timer_rtke.c
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

#include <asm/io.h>
#include <mach/hardware.h>

#include <asm/mach/irq.h>
#include <linux/interrupt.h>

#include <linux/clk.h>

#include <linux/clocksource.h>
#include <linux/clockchips.h>

#include <nk/xos_area.h>
#include <nk/xos_ctrl.h>
#include "timer_rtke.h"

#include "power_sysfs.h"


/*** Module definition ***/
/*************************/

#define MODULE_NAME "PNX_TIMER"
#define PKMOD MODULE_NAME ": "

#define PNX_TIMER_DEBUG

#define PNX_RTKE_CLOCK_SOURCE

#include <mach/power_debug.h>
#ifdef CONFIG_PNX_POWER_TRACE
char * pnx_rtke_event_name = "RTKE Event handler";
char * pnx_rtke_stopped_name = "RTKE set stopped";
char * pnx_rtke_set_event_name = "RTKE set";
char * pnx_rtke_set_baggy_name = "RTKE baggy";
char * pnx_rtke_mode_name = "RTKE mode";
#endif


/*** RTKE clock devices ***/
/**************************/

/*** RTKE Clock event device ***/
#define RTKE_ROOT_FRQ 1083333
#define RTKE_TICK_PERIOD (12*10000000/13/2)
#define RTKE_DERIVATION 361

static int pnx_rtke_set_next_event(unsigned long cycles,
		struct clock_event_device *evt);
static int pnx_rtke_set_next_baggy(unsigned long baggy,
		struct clock_event_device *evt);
static void pnx_rtke_set_mode(enum clock_event_mode mode,
		struct clock_event_device *evt);

static struct clock_event_device clockevent_rtke = {
	.name		= "rtke_timer",
	.rating		= 300,
	.shift		= 30,
	.features   = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT  | CLOCK_EVT_FEAT_POWER,
	.set_next_event	= pnx_rtke_set_next_event,
	.set_next_baggy = pnx_rtke_set_next_baggy,
	.set_mode	= pnx_rtke_set_mode,
};

/*** RTKE Clock source device ***/
#ifdef PNX_RTKE_CLOCK_SOURCE
cycle_t pnx_rtke_read(void);
//static cycle_t pnx_rtke_read(void);

static struct clocksource clocksource_rtke = {
	.name		= "rtke_timer",
	.rating		= 370,
	.read		= pnx_rtke_read,
	.mask		= CLOCKSOURCE_MASK(64),
	.shift		= 12,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};
#endif


/*** RTKE driver ***/

struct rtke_clkevt_ctxt {
	xos_area_handle_t shared;
	struct idle_time_linux *base_adr;
	xos_ctrl_handle_t ctrl;
	unsigned long duration;
	unsigned long baggy;
	unsigned long baggy_enable;
};

static struct rtke_clkevt_ctxt rtke_evt_ctxt =
{
	.shared = NULL,
	.base_adr = NULL,
	.ctrl = NULL,
};

static void
pnx_rtke_timer_interrupt ( unsigned irq, void * cookie )
{
	struct rtke_clkevt_ctxt * ctxt = (struct rtke_clkevt_ctxt *)cookie;

	unsigned long enable, expired;

#ifdef PNX_TIMER_DEBUG
//	printk(PKMOD "rtke_timer_interrupt %d\n", irq );
#endif
	enable = ctxt->base_adr->enable;
	expired = ctxt->base_adr->expired;

	if (expired & enable)
	{
		struct clock_event_device *evt = &clockevent_rtke;

		if (ctxt->base_adr->periodic)
		{
			ctxt->base_adr->duration =
				ctxt->duration + ctxt->base_adr->baggy;
		}
		else
		{
			ctxt->base_adr->enable = 0;
		}

		ctxt->base_adr->expired = 0;

#ifdef CONFIG_PNX_POWER_TRACE
		pnx_dbg_put_element(pnx_rtke_event_name, 0, evt->event_handler);
#endif
		{
		unsigned long flags;
		raw_local_irq_save(flags);
		if (evt->event_handler)
			evt->event_handler(evt);
		raw_local_irq_restore(flags);
		}
	}

    return ;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_rtke_timer_interrupt);
#endif

/*** RTKE Clock event device ***/
static int pnx_rtke_set_next_baggy(unsigned long baggy,
					struct clock_event_device *evt)
{
#ifdef PNX_TIMER_DEBUG
//	printk(PKMOD "pnx_rtke_set_next_baggy %lu\n", baggy);
#endif

	rtke_evt_ctxt.baggy = baggy & rtke_evt_ctxt.baggy_enable;

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_rtke_set_baggy_name, rtke_evt_ctxt.baggy, NULL);
#endif
	 return 0;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_rtke_set_next_baggy);
#endif

static int pnx_rtke_set_next_event(unsigned long cycles,
				    struct clock_event_device *evt)
{
#ifdef PNX_TIMER_DEBUG
//	printk(PKMOD "rtke_set_next_event %lu\n", cycles);
#endif

	/* the cycle calculation precision is 0,28% which means that
	 * the derivation is 1 cycle each 360,21 requested cycles*/
	cycles += cycles / RTKE_DERIVATION;

	rtke_evt_ctxt.base_adr->enable = 0;
	rtke_evt_ctxt.base_adr->expired = 0;
	rtke_evt_ctxt.base_adr->duration = (cycles) + rtke_evt_ctxt.baggy;
	rtke_evt_ctxt.base_adr->baggy = rtke_evt_ctxt.baggy;
	rtke_evt_ctxt.duration = (cycles);
	rtke_evt_ctxt.baggy = 0;
	rtke_evt_ctxt.base_adr->enable = 1;

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_rtke_set_event_name, (cycles), NULL);
#endif

	return 0;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_rtke_set_next_event);
#endif

static void pnx_rtke_set_mode(enum clock_event_mode mode,
			      struct clock_event_device *evt)
{

#ifdef PNX_TIMER_DEBUG
//	printk(PKMOD "rtke_set_mode %d\n", mode);
#endif

	switch (mode) {
	case CLOCK_EVT_MODE_UNUSED:
		rtke_evt_ctxt.base_adr->enable = 0;
		rtke_evt_ctxt.base_adr->periodic = 0;
#ifdef CONFIG_PNX_POWER_TRACE
//	pnx_dbg_put_element(pnx_rtke_mode_name, mode, NULL);
#endif
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
		rtke_evt_ctxt.base_adr->enable = 0;
	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_RESUME:
		rtke_evt_ctxt.base_adr->periodic = 0;
		break;
	case CLOCK_EVT_MODE_PERIODIC:
		rtke_evt_ctxt.base_adr->enable = 1;
		rtke_evt_ctxt.base_adr->periodic = 1;
		break;
	}
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_rtke_set_mode);
#endif

static void pnx_clockevent_init_rtke(void)
{
	printk(PKMOD "clockevent_init_rtke\n");

	/* Initialise clock event */
// issue it is shorter than reality and generates spurious irq
	clockevent_rtke.mult = div_sc(1, RTKE_TICK_PERIOD,
			clockevent_rtke.shift);

//	clockevent_mtu.max_delta_ns = div_sc(RELOAD_COUNTER_RTKE, clockevent_rtke.mult, clockevent_rtke.shift);
//  In fact it is wider than the 32bits variable !!!
	clockevent_rtke.max_delta_ns = 0xFFFFFFFF;

	clockevent_rtke.min_delta_ns = div_sc(1, clockevent_rtke.mult,
			clockevent_rtke.shift);
	if (clockevent_rtke.min_delta_ns < 10000)
		clockevent_rtke.min_delta_ns = 10000; /* avoid to much timer interrupt with 10us min between 2 irq */
	else if (clockevent_rtke.max_delta_ns < 10000)
		clockevent_rtke.min_delta_ns = clockevent_rtke.max_delta_ns >> 1;

	clockevent_rtke.cpumask = get_cpu_mask(0);

	/* Initialiase shared object */
	rtke_evt_ctxt.duration = 0;
#ifdef CONFIG_POWER_TIMER_BAGGY
	rtke_evt_ctxt.baggy_enable = (unsigned long)(-1);
#else
	rtke_evt_ctxt.baggy_enable = 0;
#endif
	rtke_evt_ctxt.baggy = 0;

	rtke_evt_ctxt.shared = xos_area_connect(CLOCKEVENT_AREA_NAME,
			sizeof(struct idle_time_linux));
	if (rtke_evt_ctxt.shared)
	{
		rtke_evt_ctxt.base_adr = xos_area_ptr(rtke_evt_ctxt.shared);

		/* clear area */
		rtke_evt_ctxt.base_adr->enable = 0;
		rtke_evt_ctxt.base_adr->periodic = 0;
		rtke_evt_ctxt.base_adr->expired = 0;
		rtke_evt_ctxt.base_adr->duration = 0;

	}
	else
	{
		printk(PKMOD "failed to connect to xos area\n");
		return;
	}

	rtke_evt_ctxt.ctrl = xos_ctrl_connect(CLOCKEVENT_CTRL_NAME, CLOCKEVENT_CTRL_NB);
	if (rtke_evt_ctxt.ctrl)
	{
		xos_ctrl_register ( rtke_evt_ctxt.ctrl, CLOCKEVENT_CTRL_EXPIRED,
				pnx_rtke_timer_interrupt, &rtke_evt_ctxt, 1);

		/* register clock event */
		clockevents_register_device(&clockevent_rtke);

		pnx_rtke_set_next_event((NSEC_PER_SEC/RTKE_TICK_PERIOD)/HZ,
				&clockevent_rtke);
	}
	else
		printk(PKMOD "failed to connect to xos ctrl\n");

}

/*** RTKE Clock source device ***/
#ifdef PNX_RTKE_CLOCK_SOURCE

static struct clocksource_rtke_frame tmp_rtke_frame = {
	0,
	0,
};

struct rtke_clksrc_ctxt {
	xos_area_handle_t shared;
	struct clocksource_rtke_frame *base_adr;
};

static struct rtke_clksrc_ctxt rtke_src_ctxt =
{
	.shared = NULL,
	.base_adr = &tmp_rtke_frame,
};

//static cycle_t pnx_rtke_read(void)
cycle_t pnx_rtke_read(void)
{
	unsigned long long tmp_stamp;
	unsigned long tmp_atomic;
	unsigned long tmp_qbc;

	do
	{
		do
		{
			tmp_atomic = rtke_src_ctxt.base_adr->atomic;

			tmp_stamp = rtke_src_ctxt.base_adr->stamp;

			tmp_qbc = readw( TBU_QBC_REG );
		}
		while(tmp_atomic != rtke_src_ctxt.base_adr->atomic);

		if (!tmp_qbc)
			while(rtke_src_ctxt.shared && tmp_atomic == rtke_src_ctxt.base_adr->atomic);
	}
	while(tmp_atomic != rtke_src_ctxt.base_adr->atomic);

	tmp_qbc >>=2;
	tmp_stamp += tmp_qbc;

	return (cycle_t)(tmp_stamp);
}

static void pnx_clocksource_init_rtke(void)
{
	printk(PKMOD "clocksource_init_rtke\n");

	/* init clocksource struct */
	clocksource_rtke.mult =clocksource_hz2mult((RTKE_ROOT_FRQ),
			clocksource_rtke.shift)-1;

	/* get area */
	rtke_src_ctxt.shared = xos_area_connect(CLOCKSOURCE_AREA_NAME,
			sizeof(struct clocksource_rtke_frame));
	if (rtke_src_ctxt.shared)
	{
		rtke_src_ctxt.base_adr = xos_area_ptr(rtke_src_ctxt.shared);
		/* register clock source */
		clocksource_register(&clocksource_rtke);
	}
	else
		printk(PKMOD "failed to connect to xos area\n");

}

/*
 * Scheduler clock - returns current time in nanosec units.
 */
#if 1
#define pnx_rtke_time sched_clock
unsigned long long sched_clock(void)
#else
unsigned long long pnx_rtke_time(void)
#endif
{
	unsigned long long ticks64 = pnx_rtke_read() ;

	return (ticks64 * clocksource_rtke.mult) >> clocksource_rtke.shift;
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
	return sprintf(buf, "%s\n", clockevent_rtke.object);		\
}

#define showu_one_evt(file_name, object)					\
static ssize_t show_##file_name##_evt						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%u\n", clockevent_rtke.object);		\
}

#define showlu_one_evt(file_name, object)					\
static ssize_t show_##file_name##_evt						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%lu\n", clockevent_rtke.object);		\
}

#define storelu_one_evt(file_name, object)					\
static ssize_t store_##file_name##_evt						\
(struct kobject *kobj, const char *buf, size_t size)				\
{									\
	unsigned long object = simple_strtoul(buf, NULL, 10); \
	clockevent_rtke.object = object; \
	return size;		\
}

#define showx_one_evt(file_name, object)					\
static ssize_t show_##file_name##_evt						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "0x%x\n", clockevent_rtke.object);		\
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

static ssize_t show_periodic_evt(struct kobject *kobj, char *buf)
{
	return sprintf (buf, "%lu\n", rtke_evt_ctxt.base_adr->periodic);
}

static ssize_t show_next_event_evt(struct kobject *kobj, char *buf)
{
	if (rtke_evt_ctxt.base_adr->enable)
		return sprintf (buf, "%lu\n",
				(unsigned long)rtke_evt_ctxt.base_adr->duration);
	else
		return sprintf (buf, "No Event Set\n");
}

static ssize_t show_baggy_evt(struct kobject *kobj, char *buf)
{
	return sprintf (buf, "%lu\n", (unsigned long)rtke_evt_ctxt.baggy);
}

static ssize_t store_baggy_evt(struct kobject *kobj, const char *buf, size_t size)
{
#ifdef CONFIG_POWER_TIMER_BAGGY
	unsigned long object = simple_strtoul(buf, NULL, 10);
	if (object)
		rtke_evt_ctxt.baggy_enable = (unsigned long)(-1);
	else
#endif
		rtke_evt_ctxt.baggy_enable = 0;

	return size;
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
define_one_ro_ext(periodic, _evt);
define_one_ro_ext(next_event, _evt);
define_one_rw_ext(baggy, _evt);

static struct attribute * dbs_attributes_event[] = {
	&name_evt.attr,
	&rating_evt.attr,
	&mult_evt.attr,
	&shift_evt.attr,
	&features_evt.attr,
	&min_delta_ns_evt.attr,
	&max_delta_ns_evt.attr,
	&periodic_evt.attr,
	&next_event_evt.attr,
	&baggy_evt.attr,
	NULL
};

static struct attribute_group dbs_attr_event_group = {
	.attrs = dbs_attributes_event,
	.name = "rtke_event",
};


/*** Clock source sysfs interface ***/
#ifdef PNX_RTKE_CLOCK_SOURCE

#define shows_one_src(file_name, object)					\
static ssize_t show_##file_name##_src						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%s\n", clocksource_rtke.object);		\
}

#define showu_one_src(file_name, object)					\
static ssize_t show_##file_name##_src						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%u\n", clocksource_rtke.object);		\
}

#define showlu_one_src(file_name, object)					\
static ssize_t show_##file_name##_src						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "%lu\n", clocksource_rtke.object);		\
}

#define storelu_one_src(file_name, object)					\
static ssize_t store_##file_name##_src						\
(struct kobject *kobj, const char *buf, size_t size)				\
{									\
	unsigned long object = simple_strtoul(buf, NULL, 10); \
	clocksource_rtke.object = object; \
	return size;		\
}

#define showlx_one_src(file_name, object)					\
static ssize_t show_##file_name##_src						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "0x%lx\n", clocksource_rtke.object);		\
}

#define showllx_one_src(file_name, object)					\
static ssize_t show_##file_name##_src						\
(struct kobject *kobj, char *buf)				\
{									\
	return sprintf(buf, "0x%llx\n", clocksource_rtke.object);		\
}

shows_one_src(name, name);
showu_one_src(rating, rating);
showu_one_src(mult, mult);
#ifdef CONFIG_PNX_TIMER_TUNE
storelu_one_src(mult, mult);
#endif
showu_one_src(shift, shift);
showllx_one_src(mask, mask);
showlx_one_src(flags, flags);

static ssize_t show_base_adr_src(struct kobject *kobj, char *buf)
{
		return sprintf (buf, "%p\n", rtke_src_ctxt.base_adr);
}

static ssize_t show_atomic_src(struct kobject *kobj, char *buf)
{
	if(rtke_src_ctxt.base_adr)
		return sprintf (buf, "%lu\n", rtke_src_ctxt.base_adr->atomic);
	else
		return sprintf (buf, "Not Available\n");
}

static ssize_t show_stamp_src(struct kobject *kobj, char *buf)
{
	if(rtke_src_ctxt.base_adr)
		return sprintf (buf, "%llu\n", rtke_src_ctxt.base_adr->stamp);
	else
		return sprintf (buf, "Not Available\n");
}

static ssize_t show_time_src(struct kobject *kobj, char *buf)
{
	if(rtke_src_ctxt.base_adr)
		return sprintf (buf, "%llu\n", pnx_rtke_time());
	else
		return sprintf (buf, "Not Available\n");
}


define_one_ro_ext(name, _src);
define_one_ro_ext(rating, _src);
#ifdef CONFIG_PNX_TIMER_TUNE
define_one_rw_ext(mult, _src);
#else
define_one_ro_ext(mult, _src);
#endif
define_one_ro_ext(shift, _src);
define_one_ro_ext(mask, _src);
define_one_ro_ext(flags, _src);
define_one_ro_ext(base_adr, _src);
define_one_ro_ext(atomic, _src);
define_one_ro_ext(stamp, _src);
define_one_ro_ext(time, _src);

static struct attribute * dbs_attributes_srce[] = {
	&name_src.attr,
	&rating_src.attr,
	&mult_src.attr,
	&shift_src.attr,
	&mask_src.attr,
	&flags_src.attr,
	&base_adr_src.attr,
	&atomic_src.attr,
	&stamp_src.attr,
	&time_src.attr,
	NULL
};

static struct attribute_group dbs_attr_srce_group = {
	.attrs = dbs_attributes_srce,
	.name = "rtke_source",
};
#endif
#endif


/*** System timer init ***/
/*************************/

int pnx_rtke_timer_init(void)
{
	printk(PKMOD "rtke_timer_init\n");

#ifdef PNX_RTKE_CLOCK_SOURCE
	pnx_clocksource_init_rtke();
#endif

	pnx_clockevent_init_rtke();

	return 0;
}

#ifdef CONFIG_PNX_POWER_SYSFS
static int __init pnx_rtke_init_sysfs(void)
{
	printk(PKMOD "rtke_init_sysfs\n");

#ifdef PNX_RTKE_CLOCK_SOURCE
	if (sysfs_create_group(&pnx_power_kobj, &dbs_attr_srce_group))
		printk(PKMOD "Unable to register %s in sysfs\n",
				dbs_attr_srce_group.name);
#endif

	if (sysfs_create_group(&pnx_power_kobj, &dbs_attr_event_group))
		printk(PKMOD "Unable to register %s in sysfs\n",
				dbs_attr_event_group.name);

	return 0;
}
#endif

#ifdef CONFIG_PNX_POWER_SYSFS
module_init(pnx_rtke_init_sysfs);
#endif

