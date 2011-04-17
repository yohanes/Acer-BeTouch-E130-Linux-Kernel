/*
 *  linux/arch/arm/plat-pnx/cpu-idle_pnx.c
 *
 *  Copyright (C) 2001 Russell King
 *  Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Note: there are two erratas that apply to the SA1110 here:
 *  7 - SDRAM auto-power-up failure (rev A0)
 * 13 - Corruption of internal register reads/writes following
 *      SDRAM reads (rev A0, B0, B1)
 *
 * We ignore rev. A0 and B0 devices; I don't think they're worth supporting.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>	/* need_resched() */
#include <linux/clockchips.h>
#include <linux/tick.h>
#include <linux/cpuidle.h>
#include <linux/cpu.h>


#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/ktime.h>

#include <asm/mach/time.h>

#include <asm/proc-fns.h>

#include "cpu-idle_rtke.h"
#ifdef CONFIG_NKERNEL
#include <nk/xos_area.h>
#include <nk/xos_ctrl.h>
#else
typedef void* xos_area_handle_t;
#endif

#include <mach/clock.h>

/*** Module definition ***/
/*************************/

#define MODULE_NAME "PNX_CPUIDLE"
#define PKMOD MODULE_NAME ": "


#include <mach/power_debug.h>
#ifdef CONFIG_PNX_POWER_TRACE
char * pnx_cpu_enter_wait_name = "Enter wait";
char * pnx_cpu_exit_wait_name = "Exit wait";
char * pnx_cpu_enter_idle_name = "Enter idle";
char * pnx_cpu_exit_idle_name = "Exit idle";
char * pnx_cpu_rtke_state_name = "RTKE state";
char * pnx_cpu_rtke_duration_name = "RTKE duration";
char * pnx_cpu_rtke_durationLSB_name = "RTKE duration LSB";
char * pnx_cpu_rtke_durationMSB_name = "RTKE duration MSB";
char * pnx_cpu_enter_suspend_name = "Enter suspend";
char * pnx_cpu_exit_suspend_name = "Exit suspend";
#endif


/*** private data structure ***/
/******************************/

struct pnx_cpuidle_data {
	xos_area_handle_t shared;
	struct idle_state_linux * base_adr;
};

#ifdef CONFIG_NKERNEL
static struct pnx_cpuidle_data pnx_idle_data =
{
	.shared = NULL,
	.base_adr = NULL,
};
#else
static struct idle_state_linux fake_pnx_idle_state;

static struct pnx_cpuidle_data pnx_idle_data =
{
	.shared = NULL,
	.base_adr = &fake_pnx_idle_state,
};
#endif


/*** cpu idle device ***/
/***********************/
struct cpuidle_device pnx_cpuidle_device;

// Moved in process_pnx.h in order to share the function
#if 0
/* Idle function */
#ifdef CONFIG_NKERNEL
static inline void pnx_idle_cpu(void)
{
	local_irq_disable();
	if (!need_resched()) {
		// timer_dyn_reprogram(); // PLA FIXME no more exists
		hw_raw_local_irq_disable();
		if (!raw_local_irq_pending())
			(void)os_ctx->idle(os_ctx);
		hw_raw_local_irq_enable();
#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_rtke_state_name, pnx_idle_data.base_adr->RTKE_state.RTKE_power_state, (void*)pnx_idle_data.base_adr->RTKE_state.Spare0);
	pnx_dbg_put_element(pnx_cpu_rtke_duration_name, pnx_idle_data.base_adr->RTKE_state.RTKE_sleep_duration, NULL);
#endif

	}
	local_irq_enable();
}
#else
/*
 * This is our default idle handler.  We need to disable
 * interrupts here to ensure we don't miss a wakeup call.
 */
extern void default_idle(void);
static inline void pnx_idle(void)
{
	default_idle();
}
#endif
#else

#include "process_pnx.h"

static inline void pnx_idle_cpu(void)
{
	pnx_idle();

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_rtke_state_name,
			pnx_idle_data.base_adr->RTKE_state.RTKE_power_state,
			(void*)pnx_idle_data.base_adr->RTKE_state.Spare0);
	pnx_dbg_put_element(pnx_cpu_rtke_durationLSB_name,
			pnx_idle_data.base_adr->RTKE_state.RTKE_sleep_durationLSB,
			NULL);
	pnx_dbg_put_element(pnx_cpu_rtke_durationMSB_name,
			pnx_idle_data.base_adr->RTKE_state.RTKE_sleep_durationMSB,
			NULL);
#endif
}

inline void pnx_suspend_cpu(void)
{

	pnx_suspend_core();

}


#endif

/* Idle states process */
int pnx_idle_wait(struct cpuidle_device *dev, struct cpuidle_state *state)
{
//	printk(PKMOD "*** pnx_idle_wait : %s *** \n", state->name);
#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_enter_wait_name, PNX_CPU_IDLE, NULL);
#endif

	udelay(20);

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_exit_wait_name, PNX_CPU_IDLE, NULL);
#endif
	
	return (20);
}
#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_idle_wait);
#endif


/* Idle states process */
int pnx_idle_halt(struct cpuidle_device *dev, struct cpuidle_state *state)
{
	struct clocksource *clock;
	cycle_t enter_time, exit_time;

//	printk(PKMOD "*** pnx_idle_halt : %s *** \n", state->name);
#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_enter_idle_name, PNX_CPU_IDLE, NULL);
#endif

	/* set clocks & state constraints */
	pnx_idle_data.base_adr->CPU_state = PNX_CPU_IDLE;
#ifdef CONFIG_NKERNEL
	// to be changed when get clock constraints will be available
	pnx_idle_data.base_adr->clock_state.global = clk_get_root_constraint();
#endif

	/* for statistic */
	clock = clocksource_get_next();
	enter_time = clocksource_read(clock);

	/* really enter idle */
	pnx_idle_cpu();

	/* for statistic */
	exit_time = clocksource_read(clock);

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_exit_idle_name, PNX_CPU_IDLE, NULL);
#endif
	return (int)(cyc2ns(clock, exit_time - enter_time)) / 1000;
}
#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_idle_halt);
#endif


int pnx_idle_stop(struct cpuidle_device *dev, struct cpuidle_state *state)
{
	struct clocksource *clock;
	cycle_t enter_time, exit_time;

//	printk(PKMOD "*** pnx_idle_halt : %s *** \n", state->name);
#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_enter_idle_name, PNX_CPU_STOP, NULL);
#endif

	/* switch to a power capable clock event */
	tick_check_power_device();

	/* set clocks & state constraints */
	pnx_idle_data.base_adr->CPU_state = PNX_CPU_STOP;
#ifdef CONFIG_NKERNEL
	// to be changed when get clock constraints will be available
	pnx_idle_data.base_adr->clock_state.global = clk_get_root_constraint();
#endif

	/* for statistic */
	clock = clocksource_get_next();
	enter_time = clocksource_read(clock);

	/* really enter idle */
	pnx_idle_cpu();

	/* for statistic */
	exit_time = clocksource_read(clock);

	/* restore previous clock event */
	clockevents_force_notify_released();

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_exit_idle_name, PNX_CPU_STOP, NULL);
#endif
	return (int)(cyc2ns(clock, exit_time - enter_time)) / 1000;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_idle_stop);
#endif

int pnx_idle_sleep(struct cpuidle_device *dev, struct cpuidle_state *state)
{
	struct clocksource *clock;
	cycle_t enter_time, exit_time;

//	printk(PKMOD "*** pnx_idle_sleep : %s *** \n", state->name);
#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_enter_idle_name, PNX_CPU_SLEEP, NULL);
#endif

	/* switch to a power capable clock event */
	tick_check_power_device();

	/* set clocks & state constraints */
	pnx_idle_data.base_adr->CPU_state = PNX_CPU_SLEEP;
#ifdef CONFIG_NKERNEL
	// to be changed when get clock constraints will be available
	pnx_idle_data.base_adr->clock_state.global = clk_get_root_constraint();
#endif

	/* for statistic */
	clock = clocksource_get_next();
	enter_time = clocksource_read(clock);

	/* really enter idle */
	pnx_idle_cpu();

	/* for statistic */
	exit_time = clocksource_read(clock);

	/* restore previous clock event */
	clockevents_force_notify_released();

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_exit_idle_name, PNX_CPU_SLEEP, NULL);
#endif
	return (int)(cyc2ns(clock, exit_time - enter_time)) / 1000;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_idle_sleep);
#endif


int pnx_idle_disable(struct cpuidle_device *dev, struct cpuidle_state *state)
{
	struct clocksource *clock;
	cycle_t enter_time, exit_time;
	ktime_t sleep_duration;

//	printk(PKMOD "*** pnx_idle_sleep : %s *** \n", state->name);
#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_enter_idle_name, PNX_CPU_DISABLE, NULL);
#endif

	/* switch to a power capable clock event */
	tick_check_power_device();

	/* force 10 sec sleep duration */
	sleep_duration.tv.sec = 10;
	sleep_duration.tv.nsec=0;
	tick_program_power_force_sleep_duration(sleep_duration);
	
	/* set clocks & state constraints */
	pnx_idle_data.base_adr->CPU_state = PNX_CPU_DISABLE;
	pnx_idle_data.base_adr->clock_state.global = ALL_CLOCKS_OFF;

	/* for statistic */
	clock = clocksource_get_next();
	enter_time = clocksource_read(clock);

	/* really enter idle */
	pnx_idle_cpu();

	/* for statistic */
	exit_time = clocksource_read(clock);

	/* restore previous clock event */
	clockevents_force_notify_released();

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_exit_idle_name, PNX_CPU_DISABLE, NULL);
#endif
	return (int)(cyc2ns(clock, exit_time - enter_time)) / 1000;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_idle_disable);
#endif

int pnx_idle_suspend(void)
{
	struct clocksource *clock;
	cycle_t enter_time, exit_time;

//	printk(PKMOD "*** pnx_idle_sleep : %s *** \n", state->name);
#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_enter_suspend_name, PNX_CPU_SLEEP, NULL);
#endif

	/* set clocks & state constraints */
	pnx_idle_data.base_adr->CPU_state = PNX_CPU_SLEEP;
#ifdef CONFIG_NKERNEL
	// to be changed when get clock constraints will be available
	pnx_idle_data.base_adr->clock_state.global = clk_get_root_constraint();
#endif

	/* for statistic */
	clock = clocksource_get_next();
	enter_time = clocksource_read(clock);

	/* really enter idle */
	pnx_suspend_cpu();

	/* for statistic */
	exit_time = clocksource_read(clock);

#ifdef CONFIG_PNX_POWER_TRACE
	pnx_dbg_put_element(pnx_cpu_exit_suspend_name, PNX_CPU_SLEEP, NULL);
#endif
	return (int)(cyc2ns(clock, exit_time - enter_time)) / 1000;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(pnx_idle_suspend);
#endif

#define PNX_MAX_POWER_STATE 5

static struct cpuidle_state pnx_idle_state[PNX_MAX_POWER_STATE] =
{
#if (PNX_MAX_POWER_STATE == 5)
	{
		.name = "pnx_wait",
		.flags = CPUIDLE_FLAG_SHALLOW | CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_CHECK_BM,
		.exit_latency = 1,
		.power_usage = 0,
		.target_residency = 0, /* */
		.enter = pnx_idle_wait,
	},
#endif
	{
		.name = "pnx_idle",
		.flags = CPUIDLE_FLAG_SHALLOW | CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_CHECK_BM,
		.exit_latency = 5,
		.power_usage = 10,
		.target_residency = 125, /* */
		.enter = pnx_idle_halt,
	},
	{
		.name = "pnx_stop",
		.flags = CPUIDLE_FLAG_SHALLOW | CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_CHECK_BM,
		.exit_latency = 4615,
		.power_usage = 10,
		.target_residency = 9231, /* */
		.enter = pnx_idle_stop,
	},
	{
		.name = "pnx_sleep",
		.flags = CPUIDLE_FLAG_SHALLOW | CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_CHECK_BM,
		.exit_latency = 4615,
		.power_usage = 10,
		.target_residency = 9231, /* */
		.enter = pnx_idle_sleep,
	},
	{
		.name = "pnx_disable",
		.flags = CPUIDLE_FLAG_SHALLOW | CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_CHECK_BM,
		.exit_latency = (unsigned int)(-1),
		.power_usage = 0,
		.target_residency = (unsigned int)(-1), /* */
		.enter = pnx_idle_disable,
	},
};

/* Cpu idle init */
static int pnx_init_cpuidle_device_state(struct cpuidle_device *dev)
{
	int i, count = 0;
	struct cpuidle_state *state, *init_state;

	for (i = 0; i < PNX_MAX_POWER_STATE; i++) {
		init_state = &pnx_idle_state[i];
		state = &dev->states[count + CPUIDLE_DRIVER_STATE_START];

		cpuidle_set_statedata(state, &pnx_idle_data);

		strcpy(state->name, init_state->name);

		state->flags = init_state->flags;

		state->exit_latency = init_state->exit_latency;
		state->target_residency = init_state->target_residency;
		state->power_usage = init_state->power_usage;

		state->enter = init_state->enter;

		count++;
	}

	dev->state_count = count + CPUIDLE_DRIVER_STATE_START;

	if (!count)
		return -EINVAL;

	return 0;
}

#ifdef CONFIG_NKERNEL
/* Cpu idle init */
static int pnx_init_cpuidle_wrapper(struct cpuidle_device *dev)
{
	/* get area */
	pnx_idle_data.shared = xos_area_connect(IDLESTATE_AREA_NAME,
			sizeof(struct idle_state_linux));
	if (pnx_idle_data.shared)
		pnx_idle_data.base_adr = xos_area_ptr(pnx_idle_data.shared);
	else
		printk(PKMOD "failed to connect to xos area\n");

	return 0;
}
#endif


/*** cpu idle driver ***/
/***********************/
struct cpuidle_driver pnx_cpuidle_driver = {
	.name =		"pnx_idle",
	.owner =	THIS_MODULE,
};

/*** init idle of the platform ***/
static int __init pnx_cpu_idle_init(void)
{
	printk(PKMOD "cpu-idle_init\n");

	if (cpuidle_register_driver(&pnx_cpuidle_driver))
		return -EIO;

#ifdef CONFIG_NKERNEL
	pnx_init_cpuidle_wrapper(&pnx_cpuidle_device);
#endif

	pnx_init_cpuidle_device_state(&pnx_cpuidle_device);
	pnx_cpuidle_device.cpu = 0;

	if (cpuidle_register_device(&pnx_cpuidle_device))
		return -EIO;

	return 0;
}

module_init(pnx_cpu_idle_init);


