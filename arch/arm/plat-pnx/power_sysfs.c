/*
 *  linux/arch/arm/plat-pnx/power_sysfs.c
 *
 *  Copyright (C) 2010 ST-Ericsson
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/device.h>

#include "power_sysfs.h"


/*** Module definition ***/
#define MODULE_NAME "PNX_POWER"
#define PKMOD MODULE_NAME ": "

#define PNX_TIMER_DEBUG


/*** General SysFs entry point ***/
/*********************************/

/*** Power kobject ***/
struct kobject pnx_power_kobj;

static struct attribute * default_pnx_power_attrs[] = {
	NULL
};

/*** sysfs fops ***/
#define to_private_data(k) container_of(k,struct private_date,kobj)
#define to_attr(a) container_of(a,struct pnx_power_attr,attr)

static ssize_t
pnx_power_sysfs_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
	struct pnx_power_attr * power_attr = to_attr(attr);
	ssize_t ret;

	if (power_attr->show)
		ret = power_attr->show(kobj, buffer);
	else
		ret = -EIO;

	return ret;
}

static ssize_t
pnx_power_sysfs_store(struct kobject *kobj, struct attribute *attr,
		const char *buffer, size_t size)
{
	struct pnx_power_attr * power_attr = to_attr(attr);
	ssize_t ret;

	if (power_attr->store)
		ret = power_attr->store(kobj, buffer, size);
	else
		ret = -EIO;

	return ret;
}

static struct sysfs_ops sysfs_pnx_power_ops = {
	.show	= pnx_power_sysfs_show,
	.store	= pnx_power_sysfs_store,
};

/*** Release ***/
static void pnx_power_sysfs_release(struct kobject *kobj)
{
#ifdef PNX_TIMER_DEBUG
	printk(PKMOD "pnx_power_sysfs_release\n");
#endif
}

/***  Kobject type ***/

static struct kobj_type ktype_pnx_power = {
	.sysfs_ops	= &sysfs_pnx_power_ops,
	.default_attrs	= default_pnx_power_attrs,
	.release	= pnx_power_sysfs_release,
};

char *pnx_power_sysfs_name = "power_pnx";

/*** initialize kobject and debug ***/

static int __init pnx_power_init_sysfs(void)
{
	struct pnx_power_attr **drv_attr =
		(struct pnx_power_attr **) default_pnx_power_attrs;
	int ret = 0;

#ifdef PNX_TIMER_DEBUG
	printk(PKMOD "power_sysfs_init\n");
#endif

	/* init and register kobject */
	ret = kobject_init_and_add(&pnx_power_kobj, &ktype_pnx_power, NULL,
				     "%s", pnx_power_sysfs_name);

	/* set up sys files for this cpu device */
	while (!ret && (drv_attr) && (*drv_attr)) {
		ret = sysfs_create_file(&pnx_power_kobj, &((*drv_attr)->attr));
		if (ret)
			goto err_out_kobj_exit;
		drv_attr++;
	}

err_out_kobj_exit:
	return 0;

}

module_init(pnx_power_init_sysfs);

