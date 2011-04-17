/*
 * linux/arch/arm/mach-pnx67xx/cgu.h
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *     Created:  03/02/2010 01:23:30 PM
 *      Author:  Vincent Guittot (VGU), vincent.guittot@stericsson.com
 */

#ifndef  CGU_INC
#define  CGU_INC

#define CLOCKMNGT_AREA_NAME "CLKMNGT_FRAME"

struct clockmngt_os {
	unsigned long dma;
	unsigned long mtu;
};

struct clockmngt_shared {
	struct clockmngt_os kernel;
	struct clockmngt_os rtke;
};

#endif   /* ----- #ifndef CGU_INC  ----- */

