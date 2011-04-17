/*
 *  linux/arch/arm/plat-pnx/include/mach/power.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *  Written by Vincent Guittot <vincent.guittot@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_PNX_POWER_H
#define __ARCH_ARM_PNX_POWER_H

#include <linux/list.h>
#include <linux/module.h>

struct pwr {
	struct list_head	node;
	struct module		*owner;
	const char		*name;
	struct pwr		*parent;
	unsigned long		rate;
	__u32			flags;
	void __iomem	*enable_reg;
	__u8			enable_bit;
	__s8			usecount;
	void			(*init)(struct pwr *);
	int				(*enable)(struct pwr *);
	void			(*disable)(struct pwr *);
	struct atomic_notifier_head *notifier;
};

struct pwr_functions {
	int			(*pwr_enable)(struct pwr *pwr);
	void		(*pwr_disable)(struct pwr *pwr);
};

extern struct list_head powers;
extern spinlock_t powerfw_lock;

#define ALWAYS_ENABLED		(1 << 4)	/* Clock cannot be disabled */

extern int pwr_init(struct pwr_functions * custom_powers);

extern int pwr_register(struct pwr *pwr);

extern void pwr_unregister(struct pwr *pwr);

extern int pwr_get_usecount(struct pwr *pwr);

extern ssize_t pnx_pwr_show_ivs_constraints(struct kobject *kobj, char *buf);

extern unsigned long pwr_get_hw_constraint(unsigned long reg_addr);

#define PWR_ENABLE_PRECHANGE 0
#define PWR_ENABLE_POSTCHANGE 1
#define PWR_DISABLE_PRECHANGE 2
#define PWR_DISABLE_POSTCHANGE 3

extern int pwr_register_notifier(struct notifier_block *nb, struct pwr *pwr );

extern int pwr_unregister_notifier(struct notifier_block *nb, struct pwr *pwr );

#endif
