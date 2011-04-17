/*
 * linux/arch/arm/plat-pnx/include/mach/vde.h
 *
 * Video Display Engine (VDE) driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __ARCH_PNX_VDE_H
#define __ARCH_PNX_VDE_H

/**
 * struct vde_platform_data - VDE platform initialization data
 * @clk_div: clock divider for the ARM clock
 * @burst: burst mode selection
 */
struct vde_platform_data {
	u32 clk_div;
	u32 burst;
};

#endif


