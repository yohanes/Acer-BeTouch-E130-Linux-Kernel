/*
 *  linux/arch/arm/mach-pnx67xx/pdcupwr.c
 *
 *  PNX power Gating Unit basic driver/wrapper
 *
 *  Author:	Vincent Guittot 
 *  Copyright:	ST-Ericsson 2009
 *  
 *  Based on the code from Arnaud Troel
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/string.h>

#include <mach/hardware.h>

#include <mach/pwr.h>
#include <mach/power.h>
#include "power_pnx67xx.h"

#define MODULE_NAME "PNX_PDCU"
#define PKMOD MODULE_NAME ": "


/*-------------------------------------------------------------------------
 * PNX hw low level access clock functions
 *-------------------------------------------------------------------------*/

/**
 * HW enable clock function.
 **/

static int pnx67xx_pwr_enable_fake_power ( struct pwr * pwr )
{
//	printk(PKMOD "pnx67xx_pwr_enable_fake_power for %s\n", pwr->name);
	return 0;
}

static int pnx67xx_pwr_enable_hw_ivs ( struct pwr * pwr )
{
    unsigned long value;
#ifdef CONFIG_NKERNEL
    unsigned long flags;
#endif
//	printk(PKMOD "pnx67xx_pwr_enable_hw_ivs for %s\n", pwr->name);

	if (unlikely(pwr->enable_reg == 0)) {
		printk(KERN_ERR "%s: Enable for %s without enable code\n",
				__FILE__, pwr->name);
		return 0;
	}

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );
#endif /* CONFIG_NKERNEL */

    value = inl ( pwr->enable_reg );
    value &= ~(0x3 << pwr->enable_bit);
    outl ( value, pwr->enable_reg );

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */

	do 
	{
		value = inl ( pwr->enable_reg );
	}
	while (!(value & (0x4)));

	return 0;
}


/**
 * HW disable clock function.
 **/

static void pnx67xx_pwr_disable_fake_power ( struct pwr * pwr )
{
//	printk(PKMOD "pnx67xx_pwr_disable_fake_power for %s\n", pwr->name);

}

static void pnx67xx_pwr_disable_hw_ivs ( struct pwr * pwr )
{
#ifdef CONFIG_NKERNEL
    unsigned long flags;
#endif
    unsigned long value;

//	printk(PKMOD "pnx67xx_pwr_disable_hw_power for %s\n", pwr->name);

	if (unlikely(pwr->enable_reg == 0)) {
		printk(KERN_ERR "%s: Enable for %s without enable code\n",
				__FILE__, pwr->name);
		return;
	}

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );
#endif /* CONFIG_NKERNEL */

	value = inl ( pwr->enable_reg );
    value |= (0x3 << pwr->enable_bit);
    outl ( value, pwr->enable_reg );

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */

	return;
}


/**
 * HW init clock function.
 **/

static void pnx_pwr_dflt_init(struct pwr *pwr)
{
}

//static struct srcu_notifier_head pwr_ivs_transition_notifier_list;
static ATOMIC_NOTIFIER_HEAD(pwr_ivs_transition_notifier_list);

static void pnx_pwr_init_ivs(struct pwr *pwr)
{
	//set default tate to disable
	pnx67xx_pwr_disable_hw_ivs(pwr);
	pwr->notifier = &pwr_ivs_transition_notifier_list;
}


/*-------------------------------------------------------------------------
 * PNX platform access power functions
 *-------------------------------------------------------------------------*/
static int pwr_notify_transition(struct pwr *pwr,
		unsigned int state, void * data);

/**
 * @brief Disable a device power.
 * @param pwr power description structure
 */
static void pnx_pwr_disable ( struct pwr * pwr )
{
	int ret = 0;
//    BUG_ON ( !clk->ref_count );

	if((pwr->flags & ALWAYS_ENABLED) == ALWAYS_ENABLED)
		return;

    if ( pwr->usecount > 0 && !(--pwr->usecount) )
	{
		// notification pre change
		if (pwr->notifier)
			ret = pwr_notify_transition(pwr,
					PWR_DISABLE_PRECHANGE, NULL);

		if (!(ret & NOTIFY_STOP_MASK))
		{
			if (pwr->disable)
				pwr->disable(pwr);

			if (likely((u32)pwr->parent))
				pnx_pwr_disable(pwr->parent);
	
			// notification post change
			if (pwr->notifier)
				pwr_notify_transition(pwr,
						PWR_DISABLE_POSTCHANGE, NULL);
		}
		else
			pwr->usecount++;
	}
}

/**
 * @brief Enable a device power.
 * @param pwr power description structure
 */
static int pnx_pwr_enable ( struct pwr * pwr )
{
	int ret = 0;

 //   BUG_ON ( !clk->ref_count );

	if((pwr->flags & ALWAYS_ENABLED) == ALWAYS_ENABLED)
	{
		return 0;
	}

    if ( pwr->usecount++ == 0 )
	{

		// notification pre change
		if (pwr->notifier)
			ret = pwr_notify_transition(pwr,
					PWR_ENABLE_PRECHANGE, NULL);

		if (!(ret & NOTIFY_STOP_MASK))
		{
			ret = 0;

			if (likely((u32)pwr->parent))
				ret = pnx_pwr_enable(pwr->parent);

			if (unlikely(ret != 0)) {
				pwr->usecount--;
				return ret;
			}

			if (pwr->enable)
				ret = pwr->enable(pwr);

			if (unlikely(ret != 0) && pwr->parent) {
				pnx_pwr_disable(pwr->parent);
				pwr->usecount--;
			}

			// notification post change
			if (pwr->notifier)
				pwr_notify_transition(pwr,
						PWR_ENABLE_POSTCHANGE, NULL);
		}
		else
			pwr->usecount--;
	}

    return ret;
}

static int
pwr_notify_transition(struct pwr *pwr, unsigned int state, void *data)
{
//	BUG_ON(irqs_disabled());

	if (pwr == ERR_PTR(-ENOENT))
		return NOTIFY_BAD;

	/* printk(PKMOD "notification %u of power state transition of %s\n",	state, pwr->name); 
    */
	return atomic_notifier_call_chain(pwr->notifier, state, data);
}


/*-------------------------------------------------------------------------
 * PNX platform power constraint functions
 *-------------------------------------------------------------------------*/

unsigned long
pwr_get_hw_constraint(unsigned long reg_addr)
{
    unsigned long reg;

	reg = inl(reg_addr);
	return reg;
}

ssize_t
pnx_pwr_show_ivs_constraints(struct kobject *kobj, char *buf)
{
	int size = 0;
	unsigned long reg;

	size += sprintf(&buf[size], "--IVS-- : \n");
	reg = pwr_get_hw_constraint(PDCU_CPD_REG);

	if (reg & PDCU_IVSON)
		size += sprintf(&buf[size], "IVS is ON\n");
	else
		size += sprintf(&buf[size], "IVS is OFF\n");

	size += sprintf(&buf[size], "IVS control is ");
	switch (reg & 0x3)
	{
		case 0 :
			size += sprintf(&buf[size], "power on");
		break;
		case 1 :
			size += sprintf(&buf[size], "power off in IC sleep state");
		break;
		case 2 :
			size += sprintf(&buf[size], "reserved");
		break;
		case 3 :
			size += sprintf(&buf[size], "power off immediately");
		break;
	}
	size += sprintf(&buf[size], "\n");
	size += sprintf(&buf[size], "IVS wake-up delay is %lu\n",
			((reg >> 3) & 0x4));

	return size;
}


/*-------------------------------------------------------------------------
 * PNX power reset and init functions
 *-------------------------------------------------------------------------*/

static struct pwr_functions pnx_pwr_functions = {
	.pwr_enable			= pnx_pwr_enable,
	.pwr_disable		= pnx_pwr_disable,
};


/**
 *
 **/
static int __init pnx67xx_pwr_init ( void )
{
	struct pwr **pwrp;
	printk(PKMOD "pwr management init\n");

	/* init clock function pointer table */
	pwr_init(&pnx_pwr_functions);

	/* register and init clock elements */
	for (pwrp = onchip_pwrs; pwrp < onchip_pwrs + ARRAY_SIZE(onchip_pwrs);
			pwrp++)
	{
		pwr_register(*pwrp);
	}

	return 0;
   }

arch_initcall ( pnx67xx_pwr_init );
