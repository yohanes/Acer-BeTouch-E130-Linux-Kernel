/*
 *  linux/arch/arm/plat-pnx/include/mach/uncompress.h
 *
 *  Copyright (C) 1999 ARM Limited
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

#include <linux/io.h>
#include "platform.h"

/*
 * This does not append a newline
 */
static void putc(int c)
{
	while ( !( readb ( UART1_LSR_REG ) & ( 1 << 5 ) ) )
	    barrier ( );

	writeb ( c, UART1_THR_REG );

	if (c == '\n') {
	    while ( !( readb ( UART1_LSR_REG ) & ( 1 << 5 ) ) )
		barrier ( );

	    writeb ( '\r', UART1_THR_REG );
	}

    while ( !( readb ( UART1_LSR_REG ) & ( 1 << 5 ) ) )
	barrier ( );
}

static inline void flush(void)
{
}

/*
 * nothing to do
 */
#define arch_decomp_setup()

#define arch_decomp_wdog()
