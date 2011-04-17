/*
 *  linux/arch/arm/plat-pnx/include/mach/modem_gpio.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *  Written by Loic Pallardy <loic.pallardy@stericsson.com>
 *  Based on clocks.h by Tony Lindgren, Gordon McNutt and RidgeRun, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_PNX_MODEM_GPIO_H
#define __ARCH_ARM_PNX_MODEM_GPIO_H

struct pnx_modem_gpio { 
	__u32 gpio_start;
	__u32 dvm1;
	__u32 dvm2;
	__u32 rf_pdn_needed;
	__u32 rf_pdn;
	__u32 rf_on_needed;
	__u32 rf_on;
	__u32 rf_clk320_needed;
	__u32 rf_clk320;
	__u32 rf_reset_needed;
	__u32 rf_reset;
	__u32 rf_antdet_needed;
	__u32 rf_antdet;
	__u32 agps_pwm_needed;
	__u32 agps_pwm;
	__u8  gpio_reserved[PNX_GPIO_COUNT];
	__u32 gpio_end;
};

struct pnx_modem_extint {
	__u32 extint_start;
	__u32 sim_under_volt_needed;
	__u32 sim_under_volt;
	__u32 extint_end;
};


#endif
