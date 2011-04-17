/*
 * linux/arch/arm/plat-pnx/test_kernel_panic.c
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *     Created:  29.07.2009
 *      Author:  Loic Pallardy (LPA), loic.pallardy@stericsson.com
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/sysfs.h>
#include <linux/kobject.h>

#include <asm/io.h>

 /* /sys/plat_pnx/ support */
#include "pnx_sysfs.h"


/********************/
/* sysfs definition */
/********************/
extern struct kobject *pnx_kobj;

static ssize_t test_kernel_panic_show(struct kobject *kobj,
	 struct kobj_attribute *attr, char *buf)
{
	int len = sprintf(buf,"write anything to generate a kernel panic\n");
   return len;
}

static ssize_t
test_kernel_panic_store(struct kobject *kobj,
		struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	panic("Generate Kernel Panic for test purpose");
	return 0;
}


PNX_ATTR_RW(test_kernel_panic);

static struct attribute * attrs[] = {
		&test_kernel_panic_attr.attr,
		NULL,
};

static struct attribute_group test_kernel_panic_attr_group = {
	.attrs = attrs,
	.name = "test_kernel_panic",
};

static int __init test_kernel_panic_sysfs_init(void)
{
	return sysfs_create_group(pnx_kobj,&test_kernel_panic_attr_group);
}
arch_initcall(test_kernel_panic_sysfs_init);







