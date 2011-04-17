/*
 * linux/arch/arm/mach-pnx67xx/devices.h
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Vincent Guittot <vincent.guittot@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */

#ifndef __DEVICES_H
/* Functions used to register generic devices */
extern int __init pnx67xx_devices_init_pre(void);
extern int __init pnx67xx_devices_init_post(void);
#endif
