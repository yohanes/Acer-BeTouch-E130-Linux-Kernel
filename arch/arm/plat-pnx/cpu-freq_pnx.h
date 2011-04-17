/*
 * linux/arch/arm/plat-pnx/cpu-freq_pnx.h
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

#ifndef _LINUX_CPUFREQ_PNX_H
#define _LINUX_CPUFREQ_PNX_H

enum eClk_type {
	SC_CLK,
	HCLK1,
	HCLK2,
	SDM_CLK,
	PCLK2,
	FC_CLK,
	CAM_CLK	
};

int pnx67xx_set_freq (enum eClk_type type, u32 min, u32 max);
unsigned long pnx67xx_get_freq (enum eClk_type type );
int pnx67xx_register_freq_notifier(struct notifier_block *nb);
void pnx67xx_unregister_freq_notifier(struct notifier_block *nb);
#ifdef CONFIG_SENSORS_PCF50616
int pnx67xx_reduce_wp (int val);
#endif

#endif
