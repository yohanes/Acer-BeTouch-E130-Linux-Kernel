/*
 *  linux/arch/arm/plat-pnx/include/mach/scon.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *  Written by Loic Pallardy <loic.pallardy@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_PNX_SCON_H
#define __ARCH_ARM_PNX_SCON_H

/**
 * this structure allows to store SCON register values to set during initialization step
 */

struct pnx_scon_config { 
  void __iomem* scon_reg_addr;
  u32 scon_reg_value;
};

#define SCON_REGISTER_NB 15

/**
 * PAD configuration defines 
 */
#define SCON_PAD_PULL_UP        0
#define SCON_PAD_REPEATER       1
#define SCON_PAD_PLAIN_INPUT    2
#define SCON_PAD_PULL_DOWN      3

#endif
