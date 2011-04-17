/*
 * Filename:     spi.h
 *
 * Description:  
 *
 * Created:      04.06.2009 
 * Author:       Patrice Chotard (PCH), Patrice PT Chotard AT stericsson PT com
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * changelog:
 *
 */
#ifndef _ARCH_SPI_H
#define _ARCH_SPI_H

struct pnx_spi_pdata {
	int gpio_sclk:16;
	int gpio_mux_sclk:16;
	int gpio_sdatin:16;
	int gpio_mux_sdatin:16;
	int gpio_sdatio:16;
	int gpio_mux_sdatio:16;
};
#endif /* _ARCH_SPI_H */

