/*
 *  linux/arch/arm/plat-pnx/include/mach/vmalloc.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *  Copyright (C) 2000 Russell King.
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

/*
 * Just any arbitrary offset to the start of the vmalloc VM area: the
 * current 8MB value just means that there will be a 8MB "hole" after the
 * physical memory until the kernel virtual memory starts.  That means that
 * any out-of-bounds memory accesses will hopefully be caught.
 * The vmalloc() routines leaves a hole of 4kB between each vmalloced
 * area for the same reason. ;)
 * 
 * PNX platform config (allows SDRAM size support up to 512MByte)
 *   vmalloc starts 8Mbyte above end of logical address space
 *   vmalloc ends at E7E0.00000, leaving 128MB if 512MB SDRAM used.
 *   Note: keep 2MB protection gap between VMALLOC_END and IO_BASE.
 */
#define VMALLOC_OFFSET	  (8*1024*1024) 
#define VMALLOC_START	  (((unsigned long)high_memory + VMALLOC_OFFSET) \
				& ~(VMALLOC_OFFSET-1))
#define VMALLOC_END       (PAGE_OFFSET + 0x27E00000) 
