/*
 * linux/arch/arm/plat-pnx/pnx_sysfs.c
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

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/init.h>

#include "pnx_sysfs.h"

struct kobject *pnx_kobj;
EXPORT_SYMBOL_GPL(pnx_kobj);

static struct attribute * pnx_attrs[] = {
	NULL
};

static struct attribute_group pnx_attr_group = {
	.attrs = pnx_attrs,
};

static int __init pnxsysfs_init(void)
{
	int error;

	pnx_kobj=kobject_create_and_add("plat_pnx", NULL);
	if (!pnx_kobj) {
		error = -ENOMEM;
		goto exit;
	}
	error = sysfs_create_group(pnx_kobj, &pnx_attr_group);
	if (error)
		goto kset_exit;

	return 0;

kset_exit:
	kobject_put(pnx_kobj);
exit:
	return error;
}

core_initcall(pnxsysfs_init);
