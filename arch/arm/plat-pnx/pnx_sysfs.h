/*
 * linux/arch/arm/plat-pnx/pnx_sysfs.h
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *     Created:  22.01.2009
 *      Author:  Ludovic Barre (LBA), ludovic.barre@stn-wireless.com
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#define PNX_ATTR_RO(_name) \
static struct kobj_attribute _name##_attr = __ATTR_RO(_name)

#define PNX_ATTR_RW(_name) \
static struct kobj_attribute _name##_attr = \
	__ATTR(_name, 0644, _name##_show, _name##_store)


