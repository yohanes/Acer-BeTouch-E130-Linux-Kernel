/*
 * linux/arch/arm/plat-pnx/include/mach/cpu.h
 *
 * PNX cpu type detection
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * Written by Philippe Langlais
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __ASM_ARCH_PNX_CPU_H
#define __ASM_ARCH_PNX_CPU_H

/*
 * Macros to group PNXxxxx into cpu classes.
 * These can be used in most places.
 * cpu_is_pnx67xx():	True for PNX6708, PNX6711, PNX6712 V1 & V2...
 */

#define cpu_is_pnx67xx()	0
#define cpu_is_pnx6708()	0
#define cpu_is_pnx6711()	0
#define cpu_is_pnx6712()	0
#define cpu_is_pnx67xx_v1()	0
#define cpu_is_pnx67xx_v2()	0

#if defined(CONFIG_ARCH_PNX67XX)
#  undef cpu_is_pnx67xx
#  define cpu_is_pnx67xx()	1
#endif

#if defined(CONFIG_ARCH_PNX67XX_V2)
#  undef cpu_is_pnx67xx_v2
#  define cpu_is_pnx67xx_v2()	1
#endif

#if defined(CONFIG_ARCH_PNX6708)
#  undef cpu_is_pnx6708
#  define cpu_is_pnx6708()	1
#endif

#if defined(CONFIG_ARCH_PNX6711)
#  undef cpu_is_pnx6711
#  define cpu_is_pnx6711()	1
#endif

#if defined(CONFIG_ARCH_PNX6712)
#  undef cpu_is_pnx6712
#  define cpu_is_pnx6712()	1
#endif

/* FIXME to be completed if needed */

#endif /* __ASM_ARCH_PNX_CPU_H */

