/*
 * linux/arch/arm/mach-pnx67xx/power_pnx67xx.h
 *
 * Copyright (C) 2009 ST-Ericsson.
 * Vincent Guittot <vincent.guittot@stericsson.com>
 * Created for PNX.
 * Based on clocks.h by Tony Lindgren, Gordon McNutt and RidgeRun, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_PNX67XX_CLOCK_H
#define __ARCH_ARM_MACH_PNX67XX_CLOCK_H

static int pnx67xx_pwr_enable_fake_power ( struct pwr * pwr );
static int pnx67xx_pwr_enable_hw_ivs ( struct pwr * pwr );

static void pnx67xx_pwr_disable_fake_power ( struct pwr * pwr );
static void pnx67xx_pwr_disable_hw_ivs ( struct pwr * pwr );

static void pnx_pwr_dflt_init(struct pwr *pwr);
static void pnx_pwr_init_ivs(struct pwr *pwr);


/*-------------------------------------------------------------------------
 * PNX power tree.
 *-------------------------------------------------------------------------*/

/*** Basic power tree ***/

static struct pwr ivs_pw = {
	.name		= "ivs_pw",
	.enable_reg = (void __iomem*) PDCU_CPD_REG,
	.enable_bit = 0,
	.init		= pnx_pwr_init_ivs,
	.enable		= pnx67xx_pwr_enable_hw_ivs,
	.disable	= pnx67xx_pwr_disable_hw_ivs,
};

/* PDCU IVS */
static struct pwr cam_pw = {
	.name = "CAM",
	.init = pnx_pwr_dflt_init,
	.enable = pnx67xx_pwr_enable_fake_power,
	.disable = pnx67xx_pwr_disable_fake_power,
	.parent = &ivs_pw,
};

static struct pwr ipp_pw = {
	.name = "IPP",
	.init = pnx_pwr_dflt_init,
	.enable = pnx67xx_pwr_enable_fake_power,
	.disable = pnx67xx_pwr_disable_fake_power,
	.parent = &ivs_pw,
};

static struct pwr vec_pw = {
	.name = "VEC",
	.init = pnx_pwr_dflt_init,
	.enable = pnx67xx_pwr_enable_fake_power,
	.disable = pnx67xx_pwr_disable_fake_power,
	.parent = &ivs_pw,
};

static struct pwr vdc_pw = {
	.name = "VDC",
	.init = pnx_pwr_dflt_init,
	.enable = pnx67xx_pwr_enable_fake_power,
	.disable = pnx67xx_pwr_disable_fake_power,
	.parent = &ivs_pw,
};

static struct pwr jdu_pw = {
	.name = "JDU",
	.init = pnx_pwr_dflt_init,
	.enable = pnx67xx_pwr_enable_fake_power,
	.disable = pnx67xx_pwr_disable_fake_power,
	.parent = &ivs_pw,
};

static struct pwr tvoclk_pw = {
	.name = "TVO",
	.init = pnx_pwr_dflt_init,
	.enable = pnx67xx_pwr_enable_fake_power,
	.disable = pnx67xx_pwr_disable_fake_power,
	.parent = &ivs_pw,
};

static struct pwr camjpe_pw = {
	.name = "CAMJPE",
	.init = pnx_pwr_dflt_init,
	.enable = pnx67xx_pwr_enable_fake_power,
	.disable = pnx67xx_pwr_disable_fake_power,
	.parent = &ivs_pw,
};


static struct pwr *onchip_pwrs[] = {
	&ivs_pw,
	&cam_pw,
	&vdc_pw,
	&vec_pw,
	&ipp_pw,
	&jdu_pw,
	&tvoclk_pw,
	&camjpe_pw
};

#endif
