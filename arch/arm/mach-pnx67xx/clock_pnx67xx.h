/*
 *  linux/arch/arm/mach-pnx67xx/clock_pnx67xx.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *  Loic Pallardy <loic.pallardy@stericsson.com
 *  Created for STE PNX.
 *  Based on clocks.h by Tony Lindgren, Gordon McNutt and RidgeRun, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_PNX67XX_CLOCK_H
#define __ARCH_ARM_MACH_PNX67XX_CLOCK_H

static int pnx67xx_cgu_enable_fake_clock ( struct clk * clk );
static int pnx67xx_cgu_enable_hw_clock ( struct clk * clk );
static int pnx67xx_cgu_enable_rtke_clock ( struct clk * clk );
static int pnx67xx_cgu_enable_shared_clock ( struct clk * clk );
static int pnx67xx_cgu_enable_tvo_pll_clock ( struct clk * clk );
static int pnx67xx_cgu_enable_camout ( struct clk * clk );

static void pnx67xx_cgu_disable_fake_clock ( struct clk * clk );
static void pnx67xx_cgu_disable_hw_clock ( struct clk * clk );
static void pnx67xx_cgu_disable_rtke_clock ( struct clk * clk );
static void pnx67xx_cgu_disable_shared_clock ( struct clk * clk );

static void pnx_clk_dflt_init(struct clk *clk);
static void pnx_clk_init_hclk2(struct clk *clk);
static void pnx_clk_init_fci(struct clk *clk);
static void pnx_clk_init_camout(struct clk *clk);
static void pnx_clk_init_rtke(struct clk *clk);

static int pnx_clk_set_rate_fci(struct clk *clk, unsigned long rate);
static int pnx_clk_set_rate_camout(struct clk *clk, unsigned long rate);
static int pnx_clk_set_rate_rtke(struct clk *clk, unsigned long rate);

static int pnx_clk_set_parent_camout(struct clk *clk, struct clk *parent);
static int pnx_clk_set_parent_uart(struct clk *clk, struct clk *parent);

static void followparent_ivs_recalc(struct clk *clk);
static void pnx_clk_rtke_recalc(struct clk *clk);

/* used to remove useless clock cells */
#define PNX_OPTIMIZED_TREE

/*-------------------------------------------------------------------------
 * PNX clock tree.
 *
 * NOTE:In many cases here we are assigning a 'default' parent.	In many
 *	cases the parent is selectable.	The get/set parent calls will also
 *	switch sources.
 *
 *	Many some clocks say always_enabled, but they can be auto idled for
 *	power savings. They will always be available upon clock request.
 *
 *	Several sources are given initial rates which may be wrong, this will
 *	be fixed up in the init func.
 *
 *	Things are broadly separated below by clock domains. It is
 *	noteworthy that most periferals have dependencies on multiple clock
 *	domains. Many get their interface clocks from the L4 domain, but get
 *	functional clocks from fixed sources or other core domain derived
 *	clock.
 *-------------------------------------------------------------------------*/

/*** Basic clocks ***/

/* Base external input clocks */
static struct clk func_32k_ck = {
	.name		= "func_32k_ck",
	.rate		= 32000,
	.flags		= RATE_FIXED | ALWAYS_ENABLED,
	.usecount	= 1,
};

/* Typical 26MHz in standalone mode */
static struct clk osc_ck = {		/* (*12, *13, 19.2, *26, 38.4)MHz */
	.name		= "osc_ck",
	.rate		= 26000000,		/* fixed up in clock init */
	.flags		= RATE_FIXED | ALWAYS_ENABLED | RATE_PROPAGATES,
	.usecount	= 1,
};

/* System clock MCLK */
/* With out modem likely 12MHz, with modem likely 13MHz */
static struct clk sys_ck = {		/* (*12, *13, 19.2, 26, 38.4)MHz */
	.name		= "sys_ck",		/* ~ ref_clk also */
	.parent		= &osc_ck,
	.rate		= 13000000,
	.flags		= RATE_FIXED | ALWAYS_ENABLED | RATE_PROPAGATES,
	.usecount	= 1,
};

/*** System PLLs ***/

static struct clk sc_ck = {			/* 275-550MHz */
	.name		= "sc_ck",
	.parent		= &sys_ck,
#ifdef CONFIG_ARMCLOCK_468_MHZ_MAX
	.rate = 468000000,
#endif
#ifdef CONFIG_ARMCLOCK_416_MHZ_MAX
	.rate		= 416000000,
#endif
	.flags		= RATE_PROPAGATES,
	.enable = pnx67xx_cgu_enable_fake_clock,
	.disable = pnx67xx_cgu_disable_fake_clock,
};

static struct clk fix_ck = {		/* 312MHz */
	.name		= "fix_ck",
	.parent		= &sys_ck,
	.rate		= 312000000,
	.flags		= RATE_FIXED,
	.enable = pnx67xx_cgu_enable_rtke_clock,
	.disable = pnx67xx_cgu_disable_rtke_clock,
	.enable_bit = 0,
};

static struct clk tv_ck = {			/* 216MHz */
	.name		= "tv_ck",
	.parent		= &fix_ck,
	.rate		= 216000000,
	.flags		= RATE_FIXED,
	.enable = pnx67xx_cgu_enable_fake_clock,
	.disable = pnx67xx_cgu_disable_fake_clock,
};

#ifndef PNX_OPTIMIZED_TREE
static struct clk dsp2_ck = {		/* 156-320MHz */
	.name		= "dsp2_ck",
	.parent		= &sys_ck,
	.rate		= 208000000,
	.flags		= RATE_PROPAGATES,
	.enable = pnx67xx_cgu_enable_rtke_clock,
	.disable = pnx67xx_cgu_disable_rtke_clock,
	.enable_bit = 8,
};
#endif

static struct clk sdm_ck = {		/* 275-550MHz */
	.name		= "sdm_ck",
	.parent		= &sys_ck,
	.rate		= 200000000,
/*	.flags		= RATE_PROPAGATES, */
	.enable = pnx67xx_cgu_enable_rtke_clock,
	.disable = pnx67xx_cgu_disable_rtke_clock,
	.init = pnx_clk_init_rtke,
	.set_rate = pnx_clk_set_rate_rtke,
	.enable_bit = 3,
	.rate_offset = SDM_CLK,
};

/*** Master clock sources ***/

static struct clk arm_ck = {
	.name		= "arm_ck",
#ifdef CONFIG_ARMCLOCK_468_MHZ_MAX
	.rate = 468000000,
#endif
#ifdef CONFIG_ARMCLOCK_416_MHZ_MAX
	.rate		= 416000000,
#endif
	.parent		= &sc_ck,
/*	.flags		= RATE_PROPAGATES, */
	.enable = pnx67xx_cgu_enable_fake_clock,
	.disable = pnx67xx_cgu_disable_fake_clock,
	.rate_offset = SC_CLK,
};

static struct clk hclk_ck = {
	.name		= "hclk_ck",
#ifdef CONFIG_ARMCLOCK_468_MHZ_MAX
	.rate = 234000000,
#endif
#ifdef CONFIG_ARMCLOCK_416_MHZ_MAX
	.rate		= 208000000,
#endif
	.parent		= &arm_ck,
	.flags		= RATE_PROPAGATES,
	.enable = pnx67xx_cgu_enable_rtke_clock,
	.disable = pnx67xx_cgu_disable_rtke_clock,
	.init = pnx_clk_init_rtke,
	.set_rate = pnx_clk_set_rate_rtke,
	.enable_bit = 1,
	.rate_offset = HCLK1,
};

static struct clk hclk2_ck = {
	.name		= "hclk2_ck",
/*	.rate		= 104000000, */
	.rate		= 52000000,
	.parent		= &fix_ck,
	.flags		= RATE_PROPAGATES,
	.init = pnx_clk_init_hclk2,
	.recalc	= pnx_clk_rtke_recalc,
	.enable = pnx67xx_cgu_enable_rtke_clock,
	.disable = pnx67xx_cgu_disable_rtke_clock,
	.set_rate = pnx_clk_set_rate_rtke,
	.enable_bit = 2,
	.rate_offset = HCLK2,
};

static struct clk sdm_ivs_ck = {		
	.name		= "sdm_ivs_ck",
	.parent		= &hclk2_ck,
	.rate		= 104000000,
	.flags		= RATE_PROPAGATES,
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_ivs_recalc,
	.enable_reg = (void __iomem*) CGU_SDMCON_REG,
	.enable_bit = 23,
};

static struct clk pclk1_ck = {
	.name		= "pclk1_ck",
	.rate		= 104000000,
	.parent		= &hclk2_ck,
	.flags		= RATE_PROPAGATES,
	.enable = pnx67xx_cgu_enable_fake_clock,
	.disable = pnx67xx_cgu_disable_fake_clock,
};

static struct clk tvclk_ck = {
	.name		= "tvclk_ck",
	.rate		= 27000000,
	.parent		= &tv_ck,
	.flags		= RATE_FIXED,
	.enable = pnx67xx_cgu_enable_tvo_pll_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.enable_reg = (void __iomem*) CGU_TVCON_REG,
	.enable_bit = 15,
};

static struct clk pclk2_ck = {
	.name		= "pclk2_ck",
	.rate		= 52000000,
	.parent		= &fix_ck,
	.flags		= RATE_PROPAGATES,
	.enable = pnx67xx_cgu_enable_rtke_clock,
	.disable = pnx67xx_cgu_disable_rtke_clock,
	.init = pnx_clk_init_rtke,
	.set_rate = pnx_clk_set_rate_rtke,
	.enable_bit = 4,
	.rate_offset = PCLK2,
};

static struct clk clk24m_ck = {
	.name		= "clk24m_ck",
	.rate		= 24000000,
	.parent		= &fix_ck,
	.flags		= RATE_FIXED,
	.enable = pnx67xx_cgu_enable_rtke_clock,
	.disable = pnx67xx_cgu_disable_rtke_clock,
	.enable_bit = 5,
};

static struct clk clk26m_ck = {
	.name		= "clk26m_ck",
	.rate		= 26000000,
	.parent		= &fix_ck,
	.flags		= RATE_FIXED,
	.enable = pnx67xx_cgu_enable_rtke_clock,
	.disable = pnx67xx_cgu_disable_rtke_clock,
	.enable_bit = 6,
};

static struct clk clk13m_ck = {
	.name		= "clk13m_ck",
	.rate		= 13000000,
	.parent		= &fix_ck,
	.flags		= RATE_FIXED,
	.enable = pnx67xx_cgu_enable_rtke_clock,
	.disable = pnx67xx_cgu_disable_rtke_clock,
	.enable_bit = 7,
};

static struct clk clk4m_ck = {
	.name		= "clk4m_ck",
	.rate		= 4000000,
	.parent		= &fix_ck,
	.flags		=  RATE_FIXED | ALWAYS_ENABLED | RATE_PROPAGATES,
	.enable = pnx67xx_cgu_enable_fake_clock,
	.disable = pnx67xx_cgu_disable_fake_clock,
};

#ifndef PNX_OPTIMIZED_TREE
static struct clk sioy1clk_ck = {
	.name		= "sioy1clk_ck",
	.rate		= 208000000,
	.flags		= RATE_FIXED | ALWAYS_ENABLED | RATE_PROPAGATES,
};

static struct clk sioy2clk_ck = {
	.name		= "sioy2clk_ck",
	.rate		= 208000000,
	.flags		= RATE_FIXED | ALWAYS_ENABLED | RATE_PROPAGATES,
};

static struct clk rfclk_ck = {
	.name		= "rfclk_ck",
	.rate		= 208000000,
	.flags		= RATE_FIXED | ALWAYS_ENABLED | RATE_PROPAGATES,
};
#endif

static struct clk fc_ck = {
	.name = "fc_ck",
/*	.rate = 104000000, */
	.rate = 52000000,
	.init = pnx_clk_init_fci,
	.enable = pnx67xx_cgu_enable_fake_clock,
	.disable = pnx67xx_cgu_disable_fake_clock,
	.set_rate = pnx_clk_set_rate_fci,
	.parent = &fix_ck,
};

static struct clk usb_d_ck = {
	.name		= "usb_d_ck",
	.rate		= 24000000,
	.parent		= &clk24m_ck,
	.enable = pnx67xx_cgu_enable_fake_clock,
	.disable = pnx67xx_cgu_disable_fake_clock,
};

static struct clk usb_h_ck = {
	.name		= "usb_h_ck",
	.rate		= 24000000,
	.parent		= &clk24m_ck,
	.flags		= RATE_FIXED,
	.enable = pnx67xx_cgu_enable_fake_clock,
	.disable = pnx67xx_cgu_disable_fake_clock,
};

#ifndef PNX_OPTIMIZED_TREE
static struct clk dcclk1_ck = {
	.name		= "dcclk1_ck",
	.rate		= 130000000,
	.flags		= RATE_FIXED | ALWAYS_ENABLED | RATE_PROPAGATES,
};

static struct clk diclk1_ck = {
	.name		= "diclk1_ck",
	.rate		= 130000000,
	.flags		= RATE_FIXED | ALWAYS_ENABLED | RATE_PROPAGATES,
};

static struct clk dcclk2_ck = {
	.name		= "dcclk2_ck",
	.rate		= 130000000,
	.flags		= RATE_FIXED | ALWAYS_ENABLED | RATE_PROPAGATES,
};

static struct clk diclk2_ck = {
	.name		= "dicl2_ck",
	.rate		= 130000000,
	.flags		= RATE_FIXED | ALWAYS_ENABLED | RATE_PROPAGATES,
};
#endif

static struct clk camo_ck = {
	.name		= "camo_ck",
	.rate		= 12000000,
	.parent		= &fix_ck,
	.flags		= RATE_PROPAGATES,
	.init = pnx_clk_init_camout,
	.enable = pnx67xx_cgu_enable_camout,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.set_rate = pnx_clk_set_rate_camout,
	.set_parent = pnx_clk_set_parent_camout,
	.enable_reg = (void __iomem*) CGU_CAMCON_REG,
	.enable_bit = 7,
};

/*** Peripheral clocks ***/

/* CGUTVCON */
static struct clk plltv_ck = {
	.name = "TVOPLL",
	.enable_reg = (void __iomem*) CGU_TVCON_REG,
	.enable_bit = 12,
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.parent = &tvclk_ck,
};

/* CGUGATESC1 */
#ifndef PNX_OPTIMIZED_TREE
static struct clk intc_ck = {
	.name = "INTC",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 0,
	.parent = &hclk2_ck,
};

static struct clk extint_ck = {
	.name = "EXTINT",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 1,
	.parent = &pclk2_ck,
};

static struct clk sctu_ck = {
	.name = "SCTU",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 2,
	.parent = &clk13m_ck,
};
#endif

static struct clk spi1_ck = {
	.name = "SPI1",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 3,
	.parent = &pclk1_ck,
};

static struct clk spi2_ck = {
	.name = "SPI2",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 4,
	.parent = &pclk1_ck,
};

static struct clk iic1_ck = {
	.name = "IIC1",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 5,
	.parent = &pclk2_ck,
};

static struct clk iic2_ck = {
	.name = "IIC2",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 6,
	.parent = &pclk2_ck,
};

static struct clk uart1_ck = {
	.name = "UART1",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.set_parent = pnx_clk_set_parent_uart,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 7,
	.parent = &clk13m_ck,
};

static struct clk uart2_ck = {
	.name = "UART2",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.set_parent = pnx_clk_set_parent_uart,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 8,
	.parent = &clk13m_ck,
};

#ifndef PNX_OPTIMIZED_TREE
static struct clk gpio_ck = {
	.name = "GPIO",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 9,
	.parent = &pclk2_ck,
};
#endif

static struct clk kbs_ck = {
	.name = "KBS",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 10,
	.parent = &func_32k_ck,
};

static struct clk pwm1_ck = {
	.name = "PWM1",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 11,
	.parent = &clk13m_ck,
};

static struct clk pwm2_ck = {
	.name = "PWM2",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 12,
	.parent = &clk13m_ck,
};

static struct clk pwm3_ck = {
	.name = "PWM3",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 13,
	.parent = &clk13m_ck,
};

static struct clk geac_ck = {
	.name = "GEAC",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 14,
	.parent = &pclk1_ck,
};

#ifndef PNX_OPTIMIZED_TREE
static struct clk usim_ck = {
	.name = "USIM",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 15,
	.parent = &pclk2_ck,
};
#endif

static struct clk fci_ck = {
	.name = "FCI",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 16,
	.parent = &fc_ck,
};

static struct clk usbd_ck = {
	.name = "USBD",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 17,
	.parent = &hclk2_ck,
};

#ifndef PNX_OPTIMIZED_TREE
static struct clk iis_ck = {
	.name = "IIS",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 18,
	.parent = &clk24m_ck,
};

static struct clk rfsm1_ck = {
	.name = "RFSM1",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 19,
	.parent = &pclk1_ck,
};
#endif

static struct clk dmau_ck = {
	.name = "DMAU",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_shared_clock,
	.disable = pnx67xx_cgu_disable_shared_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 20,
	.parent = &hclk2_ck,
};

static struct clk jdi_ck = {
	.name = "JDI",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 24,
	.parent = &pclk2_ck,
};

#ifndef PNX_OPTIMIZED_TREE
static struct clk ucc_ck = {
	.name = "UCC",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 25,
	.parent = &pclk1_ck,
};
#endif

static struct clk msi_ck = {
	.name = "MSI",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 26,
	.parent = &fc_ck,
};

static struct clk sdi_ck = {
	.name = "SDI",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 28,
	.parent = &hclk2_ck,
};

static struct clk nfi_ck = {
	.name = "NFI",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 29,
	.parent = &hclk2_ck,

};

static struct clk ebi_ck = {
	.name = "EBI",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATESC1_REG,
	.enable_bit = 31,
	.parent = &hclk2_ck,

};

/* CGUGATESC2 */
static struct clk cae_ck = {
	.name = "CAE",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.enable_reg = (void __iomem*) CGU_GATESC2_REG,
	.enable_bit = 0,
	.parent = &pclk1_ck,
};

static struct clk mtu_ck = {
	.name = "MTU",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_shared_clock,
	.disable = pnx67xx_cgu_disable_shared_clock,
	.enable_reg = (void __iomem*) CGU_GATESC2_REG,
	.enable_bit = 2,
	.parent = &clk13m_ck,
};

/* OCL-2008-07-02: how to manage etbclk ? */
/* static struct clk ETB_ck = */
/* { */
/* 	.name = "ETB", */
/* 	.enable_reg = (void __iomem*) CGU_GATESC2_REG, */
/* 	.enable_bit = 4, */
/* 	.parent = &etbclk_ck, */
/* }; */

/* CGUGATEIVS */
static struct clk cam_ck = {
	.name = "CAM",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATEIVS_REG,
	.enable_bit = 0,
	.parent = &sdm_ivs_ck,
};

static struct clk vde_ck = {
	.name = "VDE",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATEIVS_REG,
	.enable_bit = 1,
	.parent = &hclk2_ck,
};

static struct clk ipp_ck = {
	.name = "IPP",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATEIVS_REG,
	.enable_bit = 2,
	.parent = &sdm_ivs_ck,
};

static struct clk vec_ck = {
	.name = "VEC",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATEIVS_REG,
	.enable_bit = 3,
	.parent = &sdm_ivs_ck,
};

static struct clk vdc_ck = {
	.name = "VDC",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATEIVS_REG,
	.enable_bit = 4,
	.parent = &sdm_ivs_ck,
};

static struct clk jdu_ck = {
	.name = "JDU",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATEIVS_REG,
	.enable_bit = 5,
	.parent = &sdm_ivs_ck,
};

static struct clk tvoclk_ck = {
	.name = "TVO",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATEIVS_REG,
	.enable_bit = 6,
	.parent = &sdm_ivs_ck,
};

static struct clk camjpe_ck = {
	.name = "CAMJPE",
	.init = pnx_clk_dflt_init,
	.enable = pnx67xx_cgu_enable_hw_clock,
	.disable = pnx67xx_cgu_disable_hw_clock,
	.recalc = followparent_recalc,
	.enable_reg = (void __iomem*) CGU_GATEIVS_REG,
	.enable_bit = 7,
	.parent = &hclk_ck,
};

static struct clk *onchip_clks[] = {
	/* external root sources */
	&func_32k_ck,
	&osc_ck,
	&sys_ck,
	&sc_ck,
	&fix_ck,
	&tv_ck,
#ifndef PNX_OPTIMIZED_TREE
	&dsp2_ck,
#endif
	&sdm_ck,
	&arm_ck,
	&hclk_ck,
	&hclk2_ck,
	&pclk1_ck,
	&pclk2_ck,
	&tvclk_ck,
	&clk24m_ck,
	&clk26m_ck,
	&clk13m_ck,
	&clk4m_ck,
	&fc_ck,
#ifndef PNX_OPTIMIZED_TREE
	&sioy1clk_ck,
	&sioy2clk_ck,
	&rfclk_ck,
#endif
	&usb_h_ck,
	&usb_d_ck,
#ifndef PNX_OPTIMIZED_TREE
	&dcclk1_ck,
	&diclk1_ck,
	&dcclk2_ck,
	&diclk2_ck,
#endif
	&camo_ck,
	&tvoclk_ck,
	&plltv_ck,
#ifndef PNX_OPTIMIZED_TREE
	&intc_ck,
	&extint_ck,
	&sctu_ck,
#endif
	&spi1_ck,
	&spi2_ck,
	&iic1_ck,
	&iic2_ck,
	&uart1_ck,
	&uart2_ck,
#ifndef PNX_OPTIMIZED_TREE
	&gpio_ck,
#endif
	&kbs_ck,
	&pwm1_ck,
	&pwm2_ck,
	&pwm3_ck,
	&geac_ck,
#ifndef PNX_OPTIMIZED_TREE
	&usim_ck,
#endif
	&fci_ck,
	&usbd_ck,
#ifndef PNX_OPTIMIZED_TREE
	&iis_ck,
	&rfsm1_ck,
#endif
	&dmau_ck,
	&jdi_ck,
#ifndef PNX_OPTIMIZED_TREE
	&ucc_ck,
#endif
	&msi_ck,
	&sdi_ck,
	&nfi_ck,
	&ebi_ck,
	&cae_ck,
	&mtu_ck,
	&sdm_ivs_ck,
	&cam_ck,
	&vde_ck,
	&vdc_ck,
	&vec_ck,
	&ipp_ck,
	&jdu_ck,
	&camjpe_ck
};

#endif
