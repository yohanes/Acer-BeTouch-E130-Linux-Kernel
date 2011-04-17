/*
 *  linux/arch/arm/plat-pnx/include/mach/memory.h
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

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

/*
 * Physical DRAM offset.
 */
#define PHYS_OFFSET	UL(0x20000000)
#define BUS_OFFSET	UL(0x20000000)

#ifdef CONFIG_NKERNEL

#ifndef CONFIG_MACH_PNX_REALLOC
/*
 * redefinition of TASK_SIZE mostly to avoid cache problem
 * 256 megs for programs and 238 Meg for shared library.
 */

#define TASK_SIZE UL(0x1f000000)
#define TASK_UNMAPPED_BASE UL(0x10000000)
#endif

/*
 * Superseede the value in asm/arm/memory.h
 */
#ifdef CONFIG_MACH_PNX_REALLOC
#define PAGE_OFFSET		UL(0xC0000000)
#define __PAGE_OFFSET		0xC0000000

#else

#define PAGE_OFFSET		UL(0x20000000)
#define __PAGE_OFFSET		0x20000000
#endif

#else

/* [User Space] ~2.8GB */
#define TASK_SIZE			UL(0xB0000000)
#define TASK_UNMAPPED_BASE	UL(0x40000000)	/* memory for shared libraries */
/* [Kernel Module Space] 32MB */
#define PAGE_OFFSET			UL(0xB4000000)
/* [Direct Mapped RAM] 64MB */

/* for VMALLOC_END see vmalloc.h */

#endif

/**
 * CONSISTENT_DMA_SIZE: Size of DMA-consistent memory region.
 * Must be multiple of 2M,between 2MB and 14MB inclusive
 */
#ifdef CONFIG_ANDROID_PMEM
#define CONSISTENT_DMA_SIZE	(SZ_4M + SZ_2M)
#else
#define CONSISTENT_DMA_SIZE (SZ_4M + SZ_4M + SZ_4M)
#endif

/*
 * Virtual view <-> DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *              address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *              to an address that the kernel can use.
 */
#define __virt_to_bus(x)	(x - PAGE_OFFSET + BUS_OFFSET)
#define __bus_to_virt(x)	(x - BUS_OFFSET + PAGE_OFFSET)


#endif
