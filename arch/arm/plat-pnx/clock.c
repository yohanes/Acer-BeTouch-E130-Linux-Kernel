/*
 *  linux/arch/arm/plat-pnx/clock.c
 *
 *  Copyright (C) 2004 - 2005 Nokia corporation
 *  Written by Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>
 *  Copyright (C) 2010 ST-Ericsson
 *
 *  Modified for omap shared clock framework by Tony Lindgren <tony@atomide.com>
 *
 *  Modified for pnx shared clock framework by Loic Pallardy @ stericsson.com
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
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/notifier.h>

#include <linux/io.h>

#include <mach/clock.h>

#include "power_sysfs.h"

LIST_HEAD(clocks);

static DEFINE_MUTEX(clocks_mutex);
DEFINE_SPINLOCK(clockfw_lock);

static struct clk_functions *arch_clock;

/*--------------------------------------------------------------------
 * Additionnal specific clock functions define in include/mach/clock.h
 *------------------------------------------------------------------*/

/**
 *	clk_register_notifier - register a driver with clock mngt
 *	@nb: notifier function to register
 *  @clk: clock element rate change to be notified
 *
 *	Add a driver to one of two lists: either a list of drivers that
 *      are notified about clock rate changes (once before and once after
 *      the transition),
 *	This function may sleep, and has the same return conditions as
 *	sru_blocking_notifier_chain_register.
 */
int clk_register_notifier(struct notifier_block *nb, struct clk *clk)
{
	int ret;

	if (clk == ERR_PTR(-ENOENT))
		return -EINVAL;

	mutex_lock(&clocks_mutex);
	if (clk->notifier)
		ret = srcu_notifier_chain_register(clk->notifier, nb);
	else
		ret = -EINVAL;
	mutex_unlock(&clocks_mutex);

	return ret;
}
EXPORT_SYMBOL(clk_register_notifier);

/**
 *	clk_unregister_notifier - unregister a driver with clock mngt
 *	@nb: notifier function to be unregistered
 *  @clk: clock element rate change to be notified
 *
 *	Remove a driver from the clock mngt notifier list.
 *
 *	This function may sleep, and has the same return conditions as
 *	sru_blocking_notifier_chain_unregister.
 */
int clk_unregister_notifier(struct notifier_block *nb, struct clk *clk)
{
	int ret;

	if (clk == ERR_PTR(-ENOENT))
		return -EINVAL;

	mutex_lock(&clocks_mutex);
	if (clk->notifier)
		ret = srcu_notifier_chain_unregister(clk->notifier, nb);
	else
		ret = -EINVAL;
	mutex_unlock(&clocks_mutex);

	return ret;
}
EXPORT_SYMBOL(clk_unregister_notifier);

/**
 * clk_notify_transition - call notifier chain
 *
 * This function calls the transition notifiers and the and set rate
 * It is called twice on all frequency changes that have
 * external effects.
 */
int clk_notify_transition(struct clk *clk, unsigned int state, void *data)
{
	BUG_ON(irqs_disabled());

	if (clk == ERR_PTR(-ENOENT))
		return NOTIFY_BAD;

	switch (state) {

	case CLK_RATE_PRECHANGE:
	case CLK_RATE_POSTCHANGE:
/*		printk("notif %u of frequency transition of %s to %lu Hz\n",
			state, clk->name, (unsigned long)data);
*/
		break;

	case CLK_SRCE_PRECHANGE:
	case CLK_SRCE_POSTCHANGE:
/*              printk("notif %u of srce transition of %s to %s\n",
			state, clk->name, ((struct clk *)data)->name);
*/
		break;
	default:
		printk(KERN_ERR "Unknown notification srce transition\n");
		return NOTIFY_BAD;
		break;
	}

	return srcu_notifier_call_chain(clk->notifier, state, data);
}

/*--------------------------------------------------------
 * Standard clock functions defined in include/linux/clk.h
 *------------------------------------------------------*/

struct clk * clk_get(struct device *dev, const char *id)
{
	struct clk *p, *clk = ERR_PTR(-ENOENT);

	mutex_lock(&clocks_mutex);
	list_for_each_entry(p, &clocks, node) {
		if (strcmp(id, p->name) == 0 && try_module_get(p->owner)) {
			clk = p;
			break;
		}
	}
	mutex_unlock(&clocks_mutex);

	return clk;
}
EXPORT_SYMBOL(clk_get);

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	int ret = 0;

	if (clk == ERR_PTR(-ENOENT))
		return -EINVAL;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (arch_clock->clk_enable)
		ret = arch_clock->clk_enable(clk);
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;

	if (clk == ERR_PTR(-ENOENT))
		return;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (arch_clock->clk_disable)
		arch_clock->clk_disable(clk);
	spin_unlock_irqrestore(&clockfw_lock, flags);
}
EXPORT_SYMBOL(clk_disable);

int clk_get_usecount(struct clk *clk)
{
	unsigned long flags;
	int ret = 0;

	if (clk == ERR_PTR(-ENOENT))
		return -EINVAL;

	spin_lock_irqsave(&clockfw_lock, flags);
	ret = clk->usecount;
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_get_usecount);

unsigned long clk_get_rate(struct clk *clk)
{
	unsigned long flags;
	unsigned long ret = 0;

	if (clk == ERR_PTR(-ENOENT))
		return -EINVAL;

	spin_lock_irqsave(&clockfw_lock, flags);
	ret = clk->rate;
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_get_rate);

void clk_put(struct clk *clk)
{
	if (clk && !IS_ERR(clk))
		module_put(clk->owner);
}
EXPORT_SYMBOL(clk_put);

/*--------------------------------------------------------
 * Optional clock functions defined in include/linux/clk.h
 *------------------------------------------------------*/

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	long ret = rate;

	if (clk == ERR_PTR(-ENOENT))
		return -EINVAL;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (arch_clock->clk_round_rate)
		ret = arch_clock->clk_round_rate(clk, rate);
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	int ret = 0;

	if (clk == ERR_PTR(-ENOENT))
		return -EINVAL;

	/* notification pre change */
	if (clk->notifier)
		ret = clk_notify_transition(clk,
					    CLK_RATE_PRECHANGE, (void *)rate);

	if (!(ret & NOTIFY_STOP_MASK)) {
		spin_lock_irqsave(&clockfw_lock, flags);
		if (arch_clock->clk_set_rate)
			ret = arch_clock->clk_set_rate(clk, rate);
		spin_unlock_irqrestore(&clockfw_lock, flags);

		/* notification post change */
		if (clk->notifier)
			clk_notify_transition(clk,
					      CLK_RATE_POSTCHANGE,
					      (void *)clk->rate);
	}

	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	unsigned long flags;
	int ret = 0;

	if (clk == ERR_PTR(-ENOENT))
		return -EINVAL;

	/* notification pre change */
	if (clk->notifier)
		ret = clk_notify_transition(clk,
					    CLK_SRCE_PRECHANGE, (void *)parent);

	if (!(ret & NOTIFY_STOP_MASK)) {
		spin_lock_irqsave(&clockfw_lock, flags);
		if (arch_clock->clk_set_parent)
			ret =  arch_clock->clk_set_parent(clk, parent);
		spin_unlock_irqrestore(&clockfw_lock, flags);

		/* notification post change */
		if (clk->notifier)
			clk_notify_transition(clk,
					      CLK_SRCE_POSTCHANGE,
					      (void *)clk->parent);
	}

	return ret;
}
EXPORT_SYMBOL(clk_set_parent);

struct clk *clk_get_parent(struct clk *clk)
{
	unsigned long flags;
	struct clk * ret = ERR_PTR(-ENOENT);

	if (clk == ERR_PTR(-ENOENT))
		return ret;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (arch_clock->clk_get_parent)
		ret = arch_clock->clk_get_parent(clk);
	spin_unlock_irqrestore(&clockfw_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_get_parent);

/*-------------------------------------------------------------------------
 * Internal clock functions 
 *-------------------------------------------------------------------------*/

unsigned int __initdata mpurate;
/*
 * By default we use the rate set by the bootloader.
 * You can override this with mpurate= cmdline option.
 */
static int __init pnx_clk_setup(char *str)
{
	unsigned int tmp_rate;

	get_option(&str, &tmp_rate);

	if (!tmp_rate)
		return 1;

	mpurate = tmp_rate;

	return 1;
}

__setup("mpurate=", pnx_clk_setup);

/* Used for clocks that always have same value as the parent clock */
void followparent_recalc(struct clk *clk)
{
	clk->rate = clk->parent->rate;
}

/* Propagate rate to children */
void propagate_rate(struct clk * tclk)
{
	struct clk *clkp;

	list_for_each_entry(clkp, &clocks, node) {
		if (likely(clkp->parent != tclk))
			continue;
		if (likely((u32)clkp->recalc))
			clkp->recalc(clkp);
	}
}

int clk_register(struct clk *clk)
{
	mutex_lock(&clocks_mutex);
	list_add(&clk->node, &clocks);
	if (clk->init)
		clk->init(clk);
	mutex_unlock(&clocks_mutex);

	return 0;
}
EXPORT_SYMBOL(clk_register);

void clk_unregister(struct clk *clk)
{
	mutex_lock(&clocks_mutex);
	list_del(&clk->node);
	mutex_unlock(&clocks_mutex);
}
EXPORT_SYMBOL(clk_unregister);

void clk_deny_idle(struct clk *clk)
{
	unsigned long flags;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (arch_clock->clk_deny_idle)
		arch_clock->clk_deny_idle(clk);
	spin_unlock_irqrestore(&clockfw_lock, flags);
}
EXPORT_SYMBOL(clk_deny_idle);

void clk_allow_idle(struct clk *clk)
{
	unsigned long flags;

	spin_lock_irqsave(&clockfw_lock, flags);
	if (arch_clock->clk_allow_idle)
		arch_clock->clk_allow_idle(clk);
	spin_unlock_irqrestore(&clockfw_lock, flags);
}
EXPORT_SYMBOL(clk_allow_idle);

/*-------------------------------------------------------------------------
 * SysFs interface
 *-------------------------------------------------------------------------*/

/*** Clock mngt sysfs interface **/
static char clk_name[64] = "MTU";

static ssize_t show_select(struct kobject *kobj, char *buf)
{
	return sprintf(buf, "%s \n", clk_name);
}

static ssize_t store_select(struct kobject *kobj, const char *buf, size_t size)
{
	strcpy(clk_name, buf);

	clk_name[strlen(clk_name)-1] = 0;

	return size;
}

static ssize_t show_enable(struct kobject *kobj, char *buf)
{
	struct clk *clk;
	clk=clk_get(NULL, clk_name);
	if (clk == ERR_PTR(-ENOENT))
		return sprintf(buf, "unable to get clk\n");
	clk_put(clk);
	return sprintf(buf, "%d \n", clk_get_usecount(clk));
}

static ssize_t store_enable(struct kobject *kobj, const char *buf, size_t size)
{
	struct clk *clk;
	clk=clk_get(NULL, clk_name);
	if (clk == ERR_PTR(-ENOENT))
		return size;
	clk_enable(clk);
	clk_put(clk);
	return size;
}

static ssize_t show_disable(struct kobject *kobj, char *buf)
{
	struct clk *clk;
	clk=clk_get(NULL, clk_name);
	if (clk == ERR_PTR(-ENOENT))
		return sprintf(buf, "unable to get clk\n");
	clk_put(clk);
	return sprintf(buf, "%d \n", clk_get_usecount(clk));
}

static ssize_t store_disable(struct kobject *kobj, const char *buf, size_t size)
{
	struct clk *clk;
	clk=clk_get(NULL, clk_name);
	if (clk == ERR_PTR(-ENOENT))
		return size;
	clk_disable(clk);
	clk_put(clk);
	return size;
}

static ssize_t show_usecount(struct kobject *kobj, char *buf)
{
	struct clk *clk;
	clk=clk_get(NULL, clk_name);
	if (clk == ERR_PTR(-ENOENT))
		return sprintf(buf, "unable to get clk\n");
	clk_put(clk);
	return sprintf(buf, "%d \n", clk_get_usecount(clk));
}

static ssize_t show_rate(struct kobject *kobj, char *buf)
{
	struct clk *clk;
	clk=clk_get(NULL, clk_name);
	if (clk == ERR_PTR(-ENOENT))
		return sprintf(buf, "unable to get clk\n");
	clk_put(clk);
	return sprintf(buf, "%lu \n", clk_get_rate(clk));
}

static ssize_t store_rate(struct kobject *kobj, const char *buf, size_t size)
{
	struct clk *clk;
	unsigned long rate;
	if (strict_strtoul(buf, 10, &rate) == 0) {
	clk=clk_get(NULL, clk_name);
	if (clk == ERR_PTR(-ENOENT))
		return size;
	clk_set_rate(clk,rate);
	clk_put(clk);
	}
	return size;
}

static ssize_t show_parent(struct kobject *kobj, char *buf)
{
	struct clk *clk, *pclk;
	clk=clk_get(NULL, clk_name);
	if (clk == ERR_PTR(-ENOENT))
		return sprintf(buf, "unable to get clk\n");
	pclk = clk_get_parent(clk);
	clk_put(clk);
	if ((pclk) && (pclk->name))
		return sprintf(buf, "%s\n", pclk->name);
	else
		return sprintf(buf, "root\n");
}

static ssize_t store_parent(struct kobject *kobj, const char *buf, size_t size)
{
	struct clk *clk, *pclk;
	char parent[32];
	strcpy(parent, buf);
	parent[strlen(parent)-1] = 0;

	clk=clk_get(NULL, clk_name);
	if (clk == ERR_PTR(-ENOENT))
		return size;

	pclk=clk_get(NULL, parent);
	if (pclk == ERR_PTR(-ENOENT)) {
		clk_put(clk);
		return size;
	}
	clk_put(pclk);
	clk_set_parent(clk,pclk);
	clk_put(clk);
	return size;
}

static ssize_t show_list(struct kobject *kobj, char *buf)
{
	int size = 0;

	struct clk *p;

	list_for_each_entry(p, &clocks, node) {
		size += sprintf(&buf[size], "%s\n", p->name);
	}
	return size;
}

/*** Clock mngt hw debug ***/
static ssize_t show_hw_constraints(struct kobject *kobj, char *buf)
{
	int size = 0;

#if defined(CONFIG_NKERNEL)
	size += pnx_clk_show_root_constraints(kobj, &buf[size]);

	size += pnx_clk_show_shared_constraints(kobj, &buf[size]);
#endif

	size += pnx_clk_show_hw_rates(kobj, &buf[size]);

	size += pnx_clk_show_sc_constraints(kobj, &buf[size]);

	size += pnx_clk_show_ivs_constraints(kobj, &buf[size]);

	size += pnx_clk_show_tvo_constraints(kobj, &buf[size]);

	size += pnx_clk_show_cam_constraints(kobj, &buf[size]);

	return size;
}

static ssize_t show_hw_rates(struct kobject *kobj, char *buf)
{
	int size = 0;

	size += pnx_clk_show_hw_rates(kobj, buf);

	return size;
}

define_one_ro(hw_rates);
define_one_ro(hw_constraints);
define_one_rw(parent);
define_one_rw(rate);
define_one_ro(usecount);
define_one_rw(disable);
define_one_rw(enable);
define_one_rw(select);
define_one_ro(list);

static struct attribute * dbs_attributes_clock[] = {

	&hw_rates.attr,
	&hw_constraints.attr,
	&parent.attr,
	&rate.attr,
	&usecount.attr,
	&disable.attr,
	&enable.attr,
	&select.attr,
	&list.attr,
	NULL
};

static struct attribute_group dbs_attr_clock_group = {
	.attrs = dbs_attributes_clock,
	.name = "clk_mngt",
};

/*-------------------------------------------------------------------------
 * Init functions
 *-----------------------------------------------------------------------*/

int __init clk_init(struct clk_functions * custom_clocks)
{
	if (!custom_clocks) {
		printk(KERN_ERR "No custom clock functions registered\n");
		BUG();
	}

	arch_clock = custom_clocks;

	return 0;
}

static int __init pnx_clock_init_sysfs(void)
{
	if (sysfs_create_group(&pnx_power_kobj, &dbs_attr_clock_group))
		printk(KERN_ERR "Unable to register %s in sysfs\n",
		       dbs_attr_clock_group.name);

	return 0;
}

module_init(pnx_clock_init_sysfs);

