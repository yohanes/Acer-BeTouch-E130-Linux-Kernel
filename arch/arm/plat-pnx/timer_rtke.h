/*
 * linux/arch/arm/plat-pnx/timer_rtke.h
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *     Created:  08/22/2008
 *      Author:  Vincent Guittot (VGU), vincent.guittot@stericsson.com
 */

#ifndef  TIMER_RTKE_INC
#define  TIMER_RTKE_INC

#define CLOCKSOURCE_AREA_NAME "CLKSRC_FRAME"

struct clocksource_rtke_frame {
	volatile unsigned long long stamp;
	volatile unsigned long atomic;
};

#define CLOCKEVENT_AREA_NAME "CLKEVT_FRAME"

struct idle_time_linux {
	volatile unsigned long duration;
	volatile unsigned long baggy;
	volatile unsigned long enable;
	volatile unsigned long periodic;
	volatile unsigned long expired;
};

#define CLOCKEVENT_CTRL_NAME "CLKEVT_CTRL"

#define CLOCKEVENT_CTRL_EXPIRED 0
#define CLOCKEVENT_CTRL_NB		(CLOCKEVENT_CTRL_EXPIRED+1)

#endif   /* ----- #ifndef TIMER_RTKE_INC  ----- */


