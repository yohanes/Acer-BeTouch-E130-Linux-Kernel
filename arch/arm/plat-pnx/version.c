/*
 * linux/arch/arm/plat-pnx/version.c
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *     Created:  03/02/2010 01:23:30 PM
 *      Author:  Alexandre Torgue (), alexandre.torgue@stericsson.com
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/tty.h>

#include <linux/sysfs.h>
#include <linux/kobject.h>

#include <asm/io.h>

 /* /sys/plat_pnx/ support */
#include "pnx_sysfs.h"

/*
 * sysfs definition
 */
extern struct kobject *pnx_kobj;
static const char *ste_version= "UNDEFINED";

static ssize_t version_show(struct kobject *kobj,
	 struct kobj_attribute *attr, char *buf)
{

	int len = sprintf(buf,ste_version); 
   return len;
}

PNX_ATTR_RO(version);

static struct attribute * attrs[] = {
		&version_attr.attr,
		NULL,
};

static struct attribute_group version_attr_group = {
	.attrs = attrs,
	.name = "ste_version",
};

static int __init version_sysfs_init(void)
{
	return sysfs_create_group(pnx_kobj,&version_attr_group);
}
arch_initcall(version_sysfs_init);



