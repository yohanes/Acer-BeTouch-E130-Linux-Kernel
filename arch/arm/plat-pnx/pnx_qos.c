/*
 * linux/arch/arm/plat-pnx/pnx_qos.c
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *        Created:  30.04.2009 09:42:25
 *      Author:  Vincent Guittot (VGU), vincent.guittot@stericsson.com
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pm_qos_params.h>
#include <linux/clk.h>


#define MODULE_NAME "PNX_QOS"
#define PKMOD MODULE_NAME ": "

// ddr qos is given in MB/s
#define PNX_QOS_DDR_FACTOR 500000
// hclk2 qos is given is Mhz
#define PNX_QOS_HCLK2_FACTOR 1000000
// pclk2 qos is given is Mhz
#define PNX_QOS_PCLK2_FACTOR 1000000


struct s_qos_pnx {
	struct clk *hclk2;
	struct clk *ddr;
	struct clk *pclk2;
} qos_pnx;

static int pnx_ddr_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	clk_set_rate(qos_pnx.ddr,
		pm_qos_requirement(PM_QOS_DDR_THROUGHPUT) * PNX_QOS_DDR_FACTOR);
	//printk(PKMOD "pnx_ddr_notify : %d\n", pm_qos_requirement(PM_QOS_DDR_THROUGHPUT));

	return NOTIFY_OK;
}

static struct notifier_block pnx_ddr_notifier = {
	.notifier_call = pnx_ddr_notify,
};

static int pnx_hclk2_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	clk_set_rate(qos_pnx.hclk2,
		pm_qos_requirement(PM_QOS_HCLK2_THROUGHPUT) * PNX_QOS_HCLK2_FACTOR);
	//printk(PKMOD "pnx_hclk2_notify : %d\n", pm_qos_requirement(PM_QOS_HCLK2_THROUGHPUT));

	return NOTIFY_OK;
}

static struct notifier_block pnx_hclk2_notifier = {
	.notifier_call = pnx_hclk2_notify,
};

static int pnx_pclk2_notify(struct notifier_block *b,
		unsigned long l, void *v)
{
	clk_set_rate(qos_pnx.pclk2,
		pm_qos_requirement(PM_QOS_PCLK2_THROUGHPUT) * PNX_QOS_PCLK2_FACTOR);
	//printk(PKMOD "pnx_pclk2_notify : %d\n", pm_qos_requirement(PM_QOS_PCLK2_THROUGHPUT));

	return NOTIFY_OK;
}

static struct notifier_block pnx_pclk2_notifier = {
	.notifier_call = pnx_pclk2_notify,
};

static int __init pnx_constraint_notifier_init(void)
{
	printk(PKMOD "pnx_constraint_notifier_init\n");

	qos_pnx.ddr = clk_get(0, "sdm_ck");
	clk_set_rate(qos_pnx.ddr,
		pm_qos_requirement(PM_QOS_DDR_THROUGHPUT) * PNX_QOS_DDR_FACTOR );
	
	qos_pnx.hclk2 = clk_get(0, "hclk2_ck");
	clk_set_rate(qos_pnx.hclk2,
		pm_qos_requirement(PM_QOS_HCLK2_THROUGHPUT) * PNX_QOS_HCLK2_FACTOR);

	qos_pnx.pclk2 = clk_get(0, "pclk2_ck");
	clk_set_rate(qos_pnx.pclk2,
		pm_qos_requirement(PM_QOS_PCLK2_THROUGHPUT) * PNX_QOS_PCLK2_FACTOR);

	pm_qos_add_notifier(PM_QOS_DDR_THROUGHPUT, &pnx_ddr_notifier);

	pm_qos_add_notifier(PM_QOS_HCLK2_THROUGHPUT, &pnx_hclk2_notifier);

	pm_qos_add_notifier(PM_QOS_PCLK2_THROUGHPUT, &pnx_pclk2_notifier);

	return 0;
}

module_init(pnx_constraint_notifier_init);

