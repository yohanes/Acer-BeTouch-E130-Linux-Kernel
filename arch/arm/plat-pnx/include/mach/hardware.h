/*
 *  linux/arch/arm/plat-pnx/include/mach/hardware.h
 *
 *  This file contains the hardware definitions of PNX platforms
 *
 *  Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <asm/sizes.h>
#ifndef __ASSEMBLER__
#include <mach/cpu.h>
#endif
/*
 * Where in virtual memory the IO devices (timers, system controllers
 * and so on)
 */

#if defined(CONFIG_MACH_PNX_REALLOC)
#define IO_BASE_VIRT			0xE8000000        /* VA of IO */
#define IO_BASE_PHYS			0xC1000000		  /* phys base address of reg */
#define IO_SIZE     			0x03400000        /* How much */
#else
#define IO_BASE_VIRT			0xC0000000        /* VA of IO */
#define IO_BASE_PHYS			0xC0000000		  /* phys base address of reg */
#define IO_SIZE     			0x0B000000        /* How much */
#endif

/*
 * Similar to above, but for PCI addresses (memory, IO, Config and the
 * V3 chip itself).  WARNING: this has to mirror definitions in platform.h
 */
#define PCI_MEMORY_VADDR        0x0
#define PCI_CONFIG_VADDR        0x0
#define PCI_V3_VADDR            0x0
#define PCI_IO_VADDR            0x0

#define PCIO_BASE		PCI_IO_VADDR
#define PCIMEM_BASE		PCI_MEMORY_VADDR

/* macro to get at IO space when running virtually */
/* this version gives more IO address range to map*/
#define IO_ADDRESS(x)   ((x) - IO_BASE_PHYS + IO_BASE_VIRT)

#define pcibios_assign_all_busses()	1

#define PCIBIOS_MIN_IO		0x6000
#define PCIBIOS_MIN_MEM 	0x00100000

/*
 * ---------------------------------------------------------------------------
 * Processor specific registers defines
 * ---------------------------------------------------------------------------
 */
#include "regs-pnx67xx.h"

/*
 * ---------------------------------------------------------------------------
 * Board specific defines
 * ---------------------------------------------------------------------------
 */

#endif

