/*
 *  linux/arch/arm/plat-pnx/include/mach/clock.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *  Written by Loic Pallardy <loic.pallardy@stericsson.com>
 *  Based on clocks.h by Tony Lindgren, Gordon McNutt and RidgeRun, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_PNX_CLOCK_H
#define __ARCH_ARM_PNX_CLOCK_H

struct module;

struct clk {
	struct list_head	node;
	struct module		*owner;
	const char		*name;
	struct clk		*parent;
	unsigned long		rate;
	__u32			flags;
	void __iomem	*enable_reg;
	__u8			enable_bit;
	__u8			rate_offset;
	__u8			src_offset;
	__s8			usecount;
	void			(*recalc)(struct clk *);
	int				(*set_rate)(struct clk *, unsigned long);
	long			(*round_rate)(struct clk *, unsigned long);
	void			(*init)(struct clk *);
	int				(*enable)(struct clk *);
	void			(*disable)(struct clk *);
	int				(*set_parent)(struct clk *, struct clk *);
	struct srcu_notifier_head *notifier;
};

struct clk_functions {
	int			(*clk_enable)(struct clk *clk);
	void		(*clk_disable)(struct clk *clk);
	long		(*clk_round_rate)(struct clk *clk, unsigned long rate);
	int			(*clk_set_rate)(struct clk *clk, unsigned long rate);
	int			(*clk_set_parent)(struct clk *clk, struct clk *parent);
	struct clk *	(*clk_get_parent)(struct clk *clk);
	void		(*clk_allow_idle)(struct clk *clk);
	void		(*clk_deny_idle)(struct clk *clk);
};

extern unsigned int mpurate;
extern struct list_head clocks;
extern spinlock_t clockfw_lock;

extern int clk_init(struct clk_functions * custom_clocks);
extern int clk_register(struct clk *clk);
extern void clk_unregister(struct clk *clk);

extern void propagate_rate(struct clk *clk);
extern void followparent_recalc(struct clk * clk);
extern void clk_allow_idle(struct clk *clk);
extern void clk_deny_idle(struct clk *clk);
extern int clk_get_usecount(struct clk *clk);

#ifdef CONFIG_NKERNEL
extern unsigned long clk_get_root_constraint(void);
extern ssize_t pnx_clk_show_root_constraints(struct kobject *kobj, char *buf);
extern ssize_t pnx_clk_show_shared_constraints(struct kobject *kobj, char *buf);
#endif
extern ssize_t pnx_clk_show_hw_rates(struct kobject *kobj, char *buf);
extern ssize_t pnx_clk_show_sc_constraints(struct kobject *kobj, char *buf);
extern ssize_t pnx_clk_show_usb_constraints(struct kobject *kobj, char *buf);
extern ssize_t pnx_clk_show_ivs_constraints(struct kobject *kobj, char *buf);
extern ssize_t pnx_clk_show_tvo_constraints(struct kobject *kobj, char *buf);
extern ssize_t pnx_clk_show_cam_constraints(struct kobject *kobj, char *buf);

extern unsigned long clk_get_hw_constraint(unsigned long reg_addr);


/* Clock flags */
#define RATE_CKCTL		(1 << 0)	/* Main fixed ratio clocks */
#define RATE_FIXED		(1 << 1)	/* Fixed clock rate */
#define RATE_PROPAGATES		(1 << 2)	/* Program children too */
#define VIRTUAL_CLOCK		(1 << 3)	/* Composite clock from table */
#define ALWAYS_ENABLED		(1 << 4)	/* Clock cannot be disabled */
#define ENABLE_REG_32BIT	(1 << 5)	/* Use 32-bit access */
#define VIRTUAL_IO_ADDRESS	(1 << 6)	/* Clock in virtual address */
#define CLOCK_IDLE_CONTROL	(1 << 7)
#define CLOCK_NO_IDLE_PARENT	(1 << 8)
#define DELAYED_APP		(1 << 9)	/* Delay application of clock */
#define CONFIG_PARTICIPANT	(1 << 10)	/* Fundamental clock */


#define CLK_HCLK2_MAX_FREQ		104000000
#define CLK_FC_CLK_MAX_FREQ		104000000
#define CLK_CAM_CLK_MAX_FREQ	 78000000

/**
 * clk_register_notifier - register a driver with clock mngt
 *	@nb: notifier function to register
 *  @clk: clock element rate change to be notified
 */
extern int clk_register_notifier(struct notifier_block *nb, struct clk *clk );

/**
 * clk_unregister_notifier - unregister a driver with clock mngt
 *	@nb: notifier function to register
 *  @clk: clock element rate change to be notified
 */
extern int clk_unregister_notifier(struct notifier_block *nb, struct clk *clk );

/**
 * clk_notify_transition - call notifier chain 
 *
 * This function calls the transition notifiers and the and set rate
 * It is called twice on all frequency changes that have
 * external effects.
 */
extern int clk_notify_transition(struct clk *clk,
		unsigned int state, void *data);

#define CLK_RATE_PRECHANGE 0
#define CLK_RATE_POSTCHANGE 1
#define CLK_SRCE_PRECHANGE 2
#define CLK_SRCE_POSTCHANGE 3

#endif
