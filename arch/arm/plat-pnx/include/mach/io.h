/*
 *  linux/arch/arm/plat-pnx/include/mach/io.h
 *
 *  IO definitions for STE PNX processors and boards
 *
 *  Copyright (C) 1999 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#include <mach/hardware.h>

#define IO_SPACE_LIMIT 0xffff

/*
 * We don't actually have real ISA nor PCI buses, but there is so many
 * drivers out there that might just work if we fake them...
 */
#define __io(a)			((void __iomem *)(PCI_IO_VADDR + (a)))
#define __mem_pci(a)	(a)

void pnx67xx_map_io(void);

#endif /* __ASM_ARM_ARCH_IO_H */
