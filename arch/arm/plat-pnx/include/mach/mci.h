/*
 * Filename:     mci.h
 *
 * Created:      10.04.2009
 * Author:       Ludovic Barre (LBA), Ludovic PT Barre AT stericsson PT com
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef _ARCH_MCI_H
#define _ARCH_MCI_H
#include <linux/delay.h>
#include <linux/mmc/host.h>

#include <linux/pcf506XX.h>

/* FCI modes*/
enum FCI_power_mode {
	FCI_OFF=0,
	FCI_ECO,
	FCI_ON,
};

/*
 * function that abtract set power of mmc driver
 * because mmc driver do not depend of pmu pcf50623
 */
static inline int
mci_set_power(enum FCI_power_mode mode, unsigned short voltage)
{
	int err=0;

	if (voltage){
		err=pcf506XX_voltage_set(pcf506XX_global,
				PCF506XX_REGULATOR_HCREG,voltage);
		if ( err < 0 )
			return err;
	}

	switch ( mode ) {
	case FCI_OFF:
		err=pcf506XX_onoff_set(pcf506XX_global,
				PCF506XX_REGULATOR_HCREG,PCF506XX_REGU_OFF);
		break;
	case FCI_ECO:
		err=pcf506XX_onoff_set(pcf506XX_global,
				PCF506XX_REGULATOR_HCREG,PCF506XX_REGU_ECO);
		break;
	case FCI_ON:
		err=pcf506XX_onoff_set(pcf506XX_global,
				PCF506XX_REGULATOR_HCREG,PCF506XX_REGU_ON);
		udelay(20);
		break;
	default:
		err=-EINVAL;
		break;
	}

	return err;
}

struct pnx_mci_pdata {
	int		gpio_fcicmd:16;
	int		gpio_mux_fcicmd:16;
	int		gpio_fciclk:16;
	int		gpio_mux_fciclk:16;
	int		gpio_data0:16;
	int		gpio_mux_data0:16;
	int		gpio_data1:16;
	int		gpio_mux_data1:16;
	int		gpio_data2:16;
	int		gpio_mux_data2:16;
	int		gpio_data3:16;
	int		gpio_mux_data3:16;

	unsigned int detect_invert:1;

	u32		ocr_avail; /* available voltages */
	int    (*set_power)(enum FCI_power_mode mode, unsigned short vdd);
};
#endif /* _ARCH_MCI_H */

