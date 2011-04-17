/*
 *  mm.c
 *
 *  Copyright (C) 2004 Philips Semiconductors, Nuernberg
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

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>

#include <asm/mach/map.h>



static struct map_desc pnx67xx_io_desc[] __initdata = {
	{
		.virtual  = IO_BASE_VIRT,				/* only peripherals */
		.pfn      = __phys_to_pfn(IO_BASE_PHYS),
		.length   = IO_SIZE,
		.type     = MT_DEVICE,
	}
};

void __init pnx67xx_map_io(void)
{
    iotable_init(pnx67xx_io_desc, ARRAY_SIZE(pnx67xx_io_desc) );
}

