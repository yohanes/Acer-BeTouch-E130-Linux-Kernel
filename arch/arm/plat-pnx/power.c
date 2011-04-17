/*
 *  linux/arch/arm/plat-pnx/power.c
 *
 * Modified for pnx shared clock framework
 * by Vincent Guittot <vincent.guittot@stericsson.com>
 * Copyright (C) 2010 ST-Ericsson
 *
 *  Based on linux/arch/arm/plat-pnx/power.c file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <mach/pwr.h>
#include <linux/mutex.h>
#include <linux/notifier.h>

#include <asm/io.h>

#include <mach/power.h>

#include "power_sysfs.h"


LIST_HEAD(powers);
static DEFINE_MUTEX(power_mutex);
DEFINE_SPINLOCK(powerfw_lock);

static struct pwr_functions *arch_power;

/*--------------------------------------------------------
 * Standard power functions defined in mach/pwr.h
 *------------------------------------------------------*/

struct pwr * pwr_get(struct device *dev, const char *id)
{
	struct pwr *p, *pwr = ERR_PTR(-ENOENT);

	mutex_lock(&power_mutex);
	list_for_each_entry(p, &powers, node) {
		if (strcmp(id, p->name) == 0 && try_module_get(p->owner)) {
			pwr = p;
			break;
		}
	}
	mutex_unlock(&power_mutex);

	return pwr;
}
EXPORT_SYMBOL(pwr_get);

int pwr_enable(struct pwr *pwr)
{
	unsigned long flags;
	int ret = 0;

	if (pwr == ERR_PTR(-ENOENT))
		return -EINVAL;

	spin_lock_irqsave(&powerfw_lock, flags);
	if (arch_power->pwr_enable)
		ret = arch_power->pwr_enable(pwr);
	spin_unlock_irqrestore(&powerfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(pwr_enable);

void pwr_disable(struct pwr *pwr)
{
	unsigned long flags;

	if (pwr == ERR_PTR(-ENOENT))
		return;

	spin_lock_irqsave(&powerfw_lock, flags);
	if (arch_power->pwr_disable)
		arch_power->pwr_disable(pwr);
	spin_unlock_irqrestore(&powerfw_lock, flags);

}
EXPORT_SYMBOL(pwr_disable);

int pwr_get_usecount(struct pwr *pwr)
{
	unsigned long flags;
	int ret = 0;

	if (pwr == ERR_PTR(-ENOENT))
		return -EINVAL;

	spin_lock_irqsave(&powerfw_lock, flags);
	ret = pwr->usecount;
	spin_unlock_irqrestore(&powerfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(pwr_get_usecount);

void pwr_put(struct pwr *pwr)
{
	if (pwr && !IS_ERR(pwr))
		module_put(pwr->owner);
}
EXPORT_SYMBOL(pwr_put);


/*--------------------------------------------------------------------
 * Additionnal specific power functions define in include/mach/power.h
 *------------------------------------------------------------------*/

int pwr_register_notifier(struct notifier_block *nb, struct pwr *pwr )
{
	int ret;

	if (pwr == ERR_PTR(-ENOENT))
		return -EINVAL;

	mutex_lock(&power_mutex);
	if (pwr->notifier)
		ret = atomic_notifier_chain_register(pwr->notifier, nb);
	else
		ret = -EINVAL;
	mutex_unlock(&power_mutex);
	
	return ret;
}
EXPORT_SYMBOL(pwr_register_notifier);

int pwr_unregister_notifier(struct notifier_block *nb, struct pwr *pwr )
{
	int ret;

	if (pwr == ERR_PTR(-ENOENT))
		return -EINVAL;

	mutex_lock(&power_mutex);
	if (pwr->notifier)
		ret = atomic_notifier_chain_unregister(pwr->notifier, nb);
	else
		ret = -EINVAL;
	mutex_unlock(&power_mutex);

	return ret;
}
EXPORT_SYMBOL(pwr_unregister_notifier);


/*-------------------------------------------------------------------------
 * Internal power functions 
 *-------------------------------------------------------------------------*/

int pwr_register(struct pwr *pwr)
{
	mutex_lock(&power_mutex);
	list_add(&pwr->node, &powers);
	if (pwr->init)
		pwr->init(pwr);
	mutex_unlock(&power_mutex);

	return 0;
}
EXPORT_SYMBOL(pwr_register);

void pwr_unregister(struct pwr *pwr)
{
	mutex_lock(&power_mutex);
	list_del(&pwr->node);
	mutex_unlock(&power_mutex);
}
EXPORT_SYMBOL(pwr_unregister);

/*-------------------------------------------------------------------------
 * SysFs interface
 *-------------------------------------------------------------------------*/

/*** Power mngt sysfs interface **/
static char pwr_name[64] = "IVS";

static ssize_t show_select(struct kobject *kobj, char *buf)
{
	return sprintf(buf, "%s \n", pwr_name);
}

static ssize_t store_select(struct kobject *kobj, const char *buf, size_t size)
{
	strcpy(pwr_name, buf);

	pwr_name[strlen(pwr_name)-1] = 0;

	return size;
}

static ssize_t show_enable(struct kobject *kobj, char *buf)
{
	struct pwr *pwr;
	pwr=pwr_get(NULL, pwr_name);
	if (pwr == ERR_PTR(-ENOENT))
		return sprintf(buf, "unable to get pwr\n");
	pwr_put(pwr);
	return sprintf(buf, "%d \n", pwr_get_usecount(pwr));
}

static ssize_t store_enable(struct kobject *kobj, const char *buf, size_t size)
{
	struct pwr *pwr;
	pwr=pwr_get(NULL, pwr_name);
	if (pwr == ERR_PTR(-ENOENT))
		return size;
	pwr_enable(pwr);
	pwr_put(pwr);
	return size;
}

static ssize_t show_disable(struct kobject *kobj, char *buf)
{
	struct pwr *pwr;
	pwr=pwr_get(NULL, pwr_name);
	if (pwr == ERR_PTR(-ENOENT))
		return sprintf(buf, "unable to get pwr\n");
	pwr_put(pwr);
	return sprintf(buf, "%d \n", pwr_get_usecount(pwr));
}

static ssize_t store_disable(struct kobject *kobj, const char *buf, size_t size)
{
	struct pwr *pwr;
	pwr=pwr_get(NULL, pwr_name);
	if (pwr == ERR_PTR(-ENOENT))
		return size;
	pwr_disable(pwr);
	pwr_put(pwr);
	return size;
}

static ssize_t show_usecount(struct kobject *kobj, char *buf)
{
	struct pwr *pwr;
	pwr=pwr_get(NULL, pwr_name);
	if (pwr == ERR_PTR(-ENOENT))
		return sprintf(buf, "unable to get pwr\n");
	pwr_put(pwr);
	return sprintf(buf, "%d \n", pwr_get_usecount(pwr));
}

static ssize_t show_list(struct kobject *kobj, char *buf)
{
	int size = 0;

	struct pwr *p;

	list_for_each_entry(p, &powers, node) {
		size += sprintf(&buf[size], "%s\n", p->name);
	}
	return size;
}

/*** power gating hw debug ***/
static ssize_t show_hw_constraints(struct kobject *kobj, char *buf)
{
	int size = 0;

	size += pnx_pwr_show_ivs_constraints(kobj, &buf[size]);

	return size;
}

define_one_ro(hw_constraints);
define_one_ro(usecount);
define_one_rw(disable);
define_one_rw(enable);
define_one_rw(select);
define_one_ro(list);

static struct attribute * dbs_attributes_power[] = {

	&hw_constraints.attr,
	&usecount.attr,
	&disable.attr,
	&enable.attr,
	&select.attr,
	&list.attr,
	NULL
};

static struct attribute_group dbs_attr_power_group = {
	.attrs = dbs_attributes_power,
	.name = "pwr_mngt",
};


/*-------------------------------------------------------------------------
 * Init functions
 *-----------------------------------------------------------------------*/

int __init pwr_init(struct pwr_functions * custom_powers)
{
	if (!custom_powers) {
		printk(KERN_ERR "No custom power functions registered\n");
		BUG();
	}

	arch_power = custom_powers;

	return 0;
}

static int __init pwr_clock_init_sysfs(void)
{
	if (sysfs_create_group(&pnx_power_kobj, &dbs_attr_power_group))
		printk(KERN_ERR "Unable to register %s in sysfs\n",
				dbs_attr_power_group.name);

	return 0;
}

module_init(pwr_clock_init_sysfs);

