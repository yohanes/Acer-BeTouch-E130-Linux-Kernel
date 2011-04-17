/*
 *  linux/arch/arm/plat-pnx/include/mach/extint.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *  Written by Loic Pallardy <loic.pallardy@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_PNX_EXTINT_H
#define __ARCH_ARM_PNX_EXTINT_H

struct pnx_extint_config { 
  void __iomem* reg_addr;
  u32 reg_value;
};

#endif
