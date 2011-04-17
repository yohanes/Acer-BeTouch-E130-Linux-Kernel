/*
 *  linux/arch/arm/plat-pnx/include/mach/modem_adc.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *  Written by Loic Pallardy <loic.pallardy@stericsson.com>
 *  Based on clocks.h by Tony Lindgren, Gordon McNutt and RidgeRun, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_PNX_MODEM_ADC_H
#define __ARCH_ARM_PNX_MODEM_ADC_H

/**
 * this structure allows to store ADC configuration to transmit to modem
 */

struct pnx_modem_adc  { 
	__u32 adc_start;
	__u32 bat_voltage;
	__u32 bat_current;
	__u32 bat_fast_temp;
	__u32 ref_clock_temperature;
	__u32 product_temperature;
	__u32 audio_accessories;
	__u32 bat_type;
	__u32 adc_end;
};


#endif
