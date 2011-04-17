/*
 *  linux/arch/arm/plat-pnx/ramdump_pnx.c
 *
 *  Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *      Author:  Loic Pallardy (LPA), loic.pallardy@stericsson.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>	/* need_resched() */

#include <linux/kernel.h>

#include <linux/types.h>
#include <linux/init.h>

#include <asm/mach/time.h>

#include <asm/proc-fns.h>

#ifdef CONFIG_NKERNEL
#include <nk/xos_area.h>
#include <nk/xos_ctrl.h>
#else
typedef void* xos_area_handle_t;
#endif

#include "ramdump_pnx.h"


/*** Module definition ***/
/*************************/
#define MODULE_NAME "PNX_RAMDUMP"
#define PKMOD MODULE_NAME ": "

/*** RAMP DUMP driver ***/
/***********************/

#define RAMDUMP_NAME_LEN	16

struct ramdump_driver {
	char			name[RAMDUMP_NAME_LEN];
	struct module 		*owner;
};


static struct ramdump_ctxt ramdump_data =
{
	.shared = NULL,
	.base_adr = NULL,
};


static int pnx_ramdump_kernel_panic_event(struct notifier_block *this,
		unsigned long event,
	 void *ptr)
{
	//strcpy(ramdump_data.base_adr->kernel.panic_buf,/*buf*/ptr);
	/* send information to Modem side to generate Ramdump operation */
	printk("Ramdump preparation ...\n");
	xos_ctrl_raise(ramdump_data.new_request,
			RAMDUMP_SEND_REQUEST_EVENT_ID);
	return NOTIFY_DONE;
}


static struct notifier_block panic_block = {
	.notifier_call	= pnx_ramdump_kernel_panic_event,
};


/* Ramdump init */
static int pnx_init_ramdump(void)
{
	/* get area */
	ramdump_data.shared = xos_area_connect(RAMDUMP_AREA_NAME,
			sizeof(struct ramdump_mngt_shared));
	if (ramdump_data.shared) {
		ramdump_data.base_adr = xos_area_ptr(ramdump_data.shared);

		printk("rtke.var = %ld\n",ramdump_data.base_adr->rtke.var);
		printk("kernel.var = %lx\n",ramdump_data.base_adr->kernel.var);
		
		ramdump_data.new_request=xos_ctrl_connect(RAMDUMP_AREA_NAME, 1);
		if (ramdump_data.new_request==NULL)
			printk(PKMOD "pnx_init_ramdump connection to wrapper function failed\n");
	}
	else
		printk(PKMOD "failed to connect to xos area\n");
	/* Setup panic notifier */
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);

	return 0;
}


struct ramdump_driver pnx_ramdump_driver = {
	.name =		"pnx_ramdump",
	.owner =	THIS_MODULE,
};

/*** init idle of the platform ***/
static int __init pnx_ramdump_init(void)
{
	printk(PKMOD "ramdump_init\n");

	pnx_init_ramdump();

	return 0;
}

module_init(pnx_ramdump_init);


