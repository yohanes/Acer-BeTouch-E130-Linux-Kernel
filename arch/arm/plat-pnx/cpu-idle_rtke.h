/*
 * linux/arch/arm/plat-pnx/cpu-idle_rtke.h
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *     Created:  03/02/2010 01:23:30 PM
 *      Author:  Vincent Guittot (VGU), vincent.guittot@stericsson.com
 */

#ifndef  CPUIDLE_RTKE_INC
#define  CPUIDLE_RTKE_INC

typedef union uclock_constraint {
	unsigned long global;
	struct sdetails {
		unsigned long fix_clk :1;
		unsigned long hclk :1;
		unsigned long hclk2 :1;
		unsigned long sdmclk :1;
		unsigned long pclk2 :1;
		unsigned long clk24m :1;
		unsigned long clk26m :1;
		unsigned long clk13m :1;
		unsigned long dsp2_pll_clk :1;
	} details;
} clock_constraint;

#define ALL_CLOCKS_ON ((unsigned long)(-1))
#define ALL_CLOCKS_OFF ((unsigned long)(0))

#define IDLESTATE_AREA_NAME "CPUIDLE_CONSTRAINT"

enum eCPU_state {
	PNX_CPU_IDLE,
	PNX_CPU_STOP,
	PNX_CPU_SLEEP,
	PNX_CPU_DISABLE
};

struct RTKE_debug {
	unsigned long RTKE_power_state;
	unsigned long RTKE_sleep_durationLSB;
	unsigned long Spare0;
	unsigned long RTKE_sleep_durationMSB;
};

struct idle_state_linux {
	volatile unsigned long CPU_state;
	volatile clock_constraint clock_state;
	volatile struct RTKE_debug RTKE_state;
};

#endif   /* ----- #ifndef CPUIDLE_RTKE_INC  ----- */


