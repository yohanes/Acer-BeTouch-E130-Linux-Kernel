/*
 * linux/arch/arm/plat-pnx/cpu-freq_pnx.c
 *  Copyright (C) 2001 Russell King
 *  Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Note: there are two erratas that apply to the SA1110 here:
 *  7 - SDRAM auto-power-up failure (rev A0)
 * 13 - Corruption of internal register reads/writes following
 *      SDRAM reads (rev A0, B0, B1)
 *
 * We ignore rev. A0 and B0 devices; I don't think they're worth supporting. 
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/clockchips.h> /**/
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/types.h>


#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/system.h>

#include "cpu-freq_rtke.h"
#include "cpu-freq_pnx.h"

#include <nk/xos_area.h>
#include <nk/xos_ctrl.h>

#define MODULE_NAME "PNX_CPUFREQ"
#define PKMOD MODULE_NAME ": "

#undef DEBUG

#define INITIAL_WP_NR 1 /* number of WP before calling librema */
#define MAX_WP_NR 10 /* maximum allowed number of WP : TBD */

DECLARE_WAIT_QUEUE_HEAD(updating_freq_event);
DECLARE_WAIT_QUEUE_HEAD(updating_wp_event);

static struct cpufreq_driver pnx67xx_frq_driver;
struct cpufreq_policy *policy;

struct cpufreq_governor *pnx_cpufreq_governor;
struct cpufreq_device *dev;

static ATOMIC_NOTIFIER_HEAD(pnx_freq_notifier_list);

static void pnx_rtke_async_update(struct work_struct *work);

DECLARE_WORK(pnx_async_freq_work, pnx_rtke_async_update);

#ifdef CONFIG_NKERNEL
/*******/
struct pnx_cpufreq_wp_list {
	xos_area_handle_t shared;
	struct  working_points_list* base_adr;
	xos_ctrl_handle_t wp_list_available;

};

static struct pnx_cpufreq_wp_list pnx_cpufreq_wp_list;

/*******/
struct pnx_cpufreq_send_data {
	xos_area_handle_t shared;
	struct  linux_frequencies* base_adr;
	xos_ctrl_handle_t new_request;
};

static struct pnx_cpufreq_send_data pnx_cpufreq_send_data;

/*******/
struct pnx_cpufreq_current_data {
	xos_area_handle_t shared;
	struct current_frequencies *base_adr;
	xos_ctrl_handle_t change_occurs;
};

static struct pnx_cpufreq_current_data pnx_cpufreq_current_data;

/*******/
struct pnx_cpufreq_librema_data {
	xos_area_handle_t shared;
	struct  librema_intf_var* base_adr;
#ifndef CONFIG_SENSORS_PCF50616
	xos_ctrl_handle_t enable_pid;
#else
	xos_ctrl_handle_t rema_info;
#endif
};
static struct pnx_cpufreq_librema_data pnx_cpufreq_librema_data;
#endif

/*******/
/* number of working point managed on linux side */
unsigned int nr_freqs = INITIAL_WP_NR;

/* current working point index */
static int pnx67xx_frq_idx;
static int volatile pnx67xx_frq_req_pending;

/* Arm clock frequency table*/
struct cpufreq_frequency_table * arm_clk_frequency_table;

/*** internal function to get frequency ***/
unsigned int pnx67xx_freq_to_idx(unsigned int khz)
{
	int i = 0;

	while ((arm_clk_frequency_table[i].frequency != CPUFREQ_TABLE_END )
		&& (arm_clk_frequency_table[i].frequency != khz)
		&& (arm_clk_frequency_table[i+1].frequency != CPUFREQ_TABLE_END))
	{
		i++;
	}

	return i;
}

inline unsigned int pnx67xx_idx_to_freq(unsigned int idx)
{
	if (idx < nr_freqs)
		return arm_clk_frequency_table[idx].frequency;
	else
		return 0;
}

inline unsigned int pnx67xx_getspeed(unsigned int cpu)
{
	/* FIXME : It must be better to get the real (hardware) clock value */
	return arm_clk_frequency_table[pnx67xx_frq_idx].frequency;
}

/* *
 * Send a reduce working point change (0 disable / 1 enable )
 * */
#ifdef CONFIG_SENSORS_PCF50616
int pnx67xx_reduce_wp (int val)
{
	if (pnx_cpufreq_librema_data.base_adr)
	{
		pnx_cpufreq_librema_data.base_adr->librema_reduce_WP=val;
		xos_ctrl_raise(pnx_cpufreq_librema_data.rema_info, CPUFREQ_LIBREMA_REDUCE_WP_EVENT_ID);
		printk(KERN_INFO PKMOD "Reduce working point is %s\n", val ? "disabled":"enabled");
		return 0;
	}
	else
	{
		printk(KERN_INFO PKMOD "Can't set working point\n");
		return -1;
	}
}
#endif		


/* *
 * Send a frequency change
 * */
void pnx_update_current_frequency_handler(unsigned event, void * cookies);

int pnx67xx_set_freq (enum eClk_type type, u32 min, u32 max)
{
	unsigned long change = 0;

	//	printk(PKMOD "pnx67xx_set_freq  %d : min %lu KHz : max %lu Khz\n", type, min, max);
	if (pnx_cpufreq_send_data.base_adr)
	{
		switch (type)
		{
			case SC_CLK :
				if ((pnx_cpufreq_send_data.base_adr->SC_clk_min != min) ||
						(pnx_cpufreq_send_data.base_adr->SC_clk_max != max))
					change = 1;
				pnx_cpufreq_send_data.base_adr->SC_clk_min = min; 
				pnx_cpufreq_send_data.base_adr->SC_clk_max = max; 
				break ;
			case HCLK1 :
				if (pnx_cpufreq_send_data.base_adr->HCLK1 != min)
					change = 1;
				pnx_cpufreq_send_data.base_adr->HCLK1 = min;
			   	break ;
			case SDM_CLK:	
				if (pnx_cpufreq_send_data.base_adr->SDM_clk != min)
					change = 1;
				pnx_cpufreq_send_data.base_adr->SDM_clk = min;
				break ;
			case HCLK2 :
				if (pnx_cpufreq_send_data.base_adr->HCLK2 != min)
					change = 1;
				pnx_cpufreq_send_data.base_adr->HCLK2 = min;
			   	break ;
			case PCLK2 :
				if (pnx_cpufreq_send_data.base_adr->PCLK2 != min)
					change = 1;
				pnx_cpufreq_send_data.base_adr->PCLK2 = min;
				break ;
			case FC_CLK :
				if (pnx_cpufreq_send_data.base_adr->FC_clk != min)
					change = 1;
				pnx_cpufreq_send_data.base_adr->FC_clk = min;
			   	break ;
			case CAM_CLK :
				if (pnx_cpufreq_send_data.base_adr->CAM_clk != min)
					change = 1;
				pnx_cpufreq_send_data.base_adr->CAM_clk = min;
			   	break ;
			default :
				return -1;
			break;
		}

#ifdef CONFIG_NKERNEL
		if (change)
			/* send linux frequency request to RTK/Librema */
			xos_ctrl_raise(pnx_cpufreq_send_data.new_request,CPUFREQ_SEND_REQUEST_EVENT_ID );
		else
#endif
		/* simulate freq update event */
		pnx_update_current_frequency_handler(0, NULL);
	}

 	return 0;
}

/* *
 * get a frequency value
 * */
unsigned long pnx67xx_get_freq (enum eClk_type type)
{
	unsigned long freq = -1;

	if (pnx_cpufreq_current_data.base_adr)
	{
		switch (type)
		{
			case SC_CLK :
				freq = pnx_cpufreq_current_data.base_adr->SC_clk; 
				break ;
			case HCLK1 :
				freq = pnx_cpufreq_current_data.base_adr->HCLK1;
			   	break ;
			case SDM_CLK:	
				freq = pnx_cpufreq_current_data.base_adr->SDM_clk;
				break ;
			case HCLK2 :
				freq = pnx_cpufreq_current_data.base_adr->HCLK2;
			   	break ;
			case PCLK2 :
				freq = pnx_cpufreq_current_data.base_adr->PCLK2;
				break ;
			case FC_CLK :
				freq = pnx_cpufreq_current_data.base_adr->FC_clk;
			   	break ;
			case CAM_CLK :
				freq = pnx_cpufreq_current_data.base_adr->CAM_clk;
			   	break ;
			default :
				freq = -1;
			break;
		}

	}
//	printk(PKMOD "pnx67xx_get_freq  %d : %lu KHz\n", type, freq);
	
 	return freq;
}

/* *
 * register notification handler 
 * */
int pnx67xx_register_freq_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&pnx_freq_notifier_list, nb);
}

/* *
 * unregister notification handler 
 * */
void pnx67xx_unregister_freq_notifier(struct notifier_block *nb)
{
	atomic_notifier_chain_unregister(&pnx_freq_notifier_list, nb);
}

/* *
 * Notification of a frequency change
 * */
void pnx_update_current_frequency_handler(unsigned event, void * cookies) 
{ 
//	printk(PKMOD "pnx_update_current_frequency_handler\n");
//	printk(PKMOD " SC %lu KHz\n", pnx_cpufreq_current_data.base_adr->SC_clk);
//	printk(PKMOD " HCLK1 %lu KHz\n", pnx_cpufreq_current_data.base_adr->HCLK1);
//	printk(PKMOD " SDM %lu KHz\n", pnx_cpufreq_current_data.base_adr->SDM_clk);
//	printk(PKMOD " HCLK2 %lu KHz\n", pnx_cpufreq_current_data.base_adr->HCLK2);
//	printk(PKMOD " PCLK2 %lu KHz\n", pnx_cpufreq_current_data.base_adr->PCLK2);
//	printk(PKMOD " FC %lu KHz\n", pnx_cpufreq_current_data.base_adr->FC_clk);
//	printk(PKMOD " CAM %lu KHz\n", pnx_cpufreq_current_data.base_adr->CAM_clk);

	atomic_notifier_call_chain(&pnx_freq_notifier_list, 0, NULL);

}


/*****************/
int pnx67xx_verify_speed(struct cpufreq_policy *policy)
{
	if (policy->cpu)
		return -EINVAL;

	cpufreq_frequency_table_verify(policy, arm_clk_frequency_table);
	
	return 0;
}

/*****************/
static int pnx67xx_target(struct cpufreq_policy *policy,
			 unsigned int target_freq,
			 unsigned int relation)
{
	struct cpufreq_freqs freqs;
	unsigned int idx;

	/* set current frequency value */
	freqs.cpu = 0; /* cpu nb 0 is used */
	freqs.old = pnx67xx_getspeed(0);

	/* find targeted freq */
	cpufreq_frequency_table_target(policy, arm_clk_frequency_table, target_freq, relation, &idx);

	/* set targeted frequency value */
	freqs.new = pnx67xx_idx_to_freq(idx);

	/* notify user from pre change */
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

//	printk(PKMOD "pnx67xx_target new freq :%d\n",(int) freqs.new );

	pnx67xx_frq_req_pending = 1;

	/* Send new request */
	pnx67xx_set_freq(SC_CLK, freqs.new, policy->max);

	/* wait until librema replies with frequency update via
	 * pnx_update_current_frequency_handler */
	/* Following command is useless because rtke has got
	 * higher priority and pnx67xx_frq_idx is updated under
	 * irq context */
	/* wait_event(updating_freq_event, 1); */

	pnx67xx_frq_req_pending = 0;

	/* update new real frequency */
	freqs.cpu = 0; 
	freqs.new = pnx67xx_getspeed(0);

//	printk(PKMOD "pnx67xx_target updated freq :%d\n",(int) freqs.new );
	
	/* notify user from post change */
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	return 0;
}

static void pnx_rtke_async_update(struct work_struct *work)
{
	cpufreq_update_policy(0);
}

/* to be notified of rtke freq update */
static int cpu_freq_rtke_notifier_handler(struct notifier_block *nb, unsigned long type, void *data)
{
	int old_frq_idx;

	/* normalize cpu freq value */
	old_frq_idx = pnx67xx_frq_idx;
	pnx67xx_frq_idx = pnx67xx_freq_to_idx(pnx_cpufreq_current_data.base_adr->SC_clk);

	/* printk(PKMOD "cpu_freq_rtke_notifier_handler new freq :%d %d\n",
	 *		(int) pnx67xx_frq_idx,
	 *		pnx_cpufreq_current_data.base_adr->SC_clk ); */

	/* wakeup pnx67xx_target function*/
	/* wake_up(&updating_freq_event); */

	if ((!pnx67xx_frq_req_pending) && (pnx67xx_frq_idx != old_frq_idx))
		schedule_work(&pnx_async_freq_work);

	return NOTIFY_DONE;
}

struct notifier_block cpu_freq_rtke_notifier_block = {
	.notifier_call = cpu_freq_rtke_notifier_handler,
};

/*********************/
static int pnx67xx_suspend(struct cpufreq_policy *policy, pm_message_t pmsg)
{
//	printk(PKMOD "pnx67xx_cpufreq_suspend :%d\n",(int) pmsg.event );

	if (pmsg.event == PM_EVENT_SUSPEND)
	{
		pnx_cpufreq_librema_data.base_adr->suspend_state=1;

		pnx67xx_set_freq(SC_CLK, pnx67xx_idx_to_freq((pnx_cpufreq_wp_list.base_adr->wrapper_nr_wp) - 1), policy->max);
	}

	return 0;
}

static int pnx67xx_resume(struct cpufreq_policy *policy)
{
//	printk(PKMOD "pnx67xx_cpufreq_resume\n");

//	set to max value to speed up the resume
	pnx67xx_frq_idx = 0;

	pnx67xx_set_freq(SC_CLK, pnx67xx_idx_to_freq(pnx67xx_frq_idx), policy->max);

	pnx_cpufreq_librema_data.base_adr->suspend_state=0;

	return 0;
}

/*********************/
static int __init pnx67xx_cpu_init(struct cpufreq_policy *policy)
{
	if (policy->cpu != 0)
		return -EINVAL;

	/*  init policy */
	cpufreq_frequency_table_cpuinfo(policy, arm_clk_frequency_table);
	
	//policy->cpuinfo.transition_latency = CPUFREQ_ETERNAL;
	policy->cpuinfo.transition_latency = 100;
	
	policy->cur = pnx67xx_getspeed(0);
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;

	pnx67xx_register_freq_notifier(&cpu_freq_rtke_notifier_block);

	return 0;
}

static struct freq_attr *pnx_freq_attrs[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL
};

static struct cpufreq_driver pnx67xx_frq_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= pnx67xx_verify_speed,
	.target		= pnx67xx_target,
	.get		= pnx67xx_getspeed,
	.init		= pnx67xx_cpu_init,
	.suspend	= pnx67xx_suspend,
	.resume		= pnx67xx_resume,
	.name		= "pnx cpufreq",
	.owner		= THIS_MODULE,
	.attr		= pnx_freq_attrs,
};

/************/
void pnx_fill_frequency_table (unsigned event, void * cookies)
{ /* initialize frequency table with RTKE/Librema Working Points*/

	int i;
#ifdef CONFIG_NKERNEL
	struct cpufreq_frequency_table * p_frequency_table;
	/* get number of working points */
	if ((!pnx_cpufreq_wp_list.base_adr->wrapper_nr_wp)||(pnx_cpufreq_wp_list.base_adr->wrapper_nr_wp>MAX_WP_NR))
	{
		/* exit execution instead*/
		printk(KERN_INFO PKMOD "pnx67xx_cpufreq WP list load failure : original WP kept\n ");
	}
	else
	{
		/* get number of working points */
		nr_freqs=pnx_cpufreq_wp_list.base_adr->wrapper_nr_wp;

		/* enlarge frequency table if necessary */ 
	 	p_frequency_table=kmalloc((nr_freqs+1)* sizeof(struct cpufreq_frequency_table), GFP_KERNEL);
		if (p_frequency_table)
		{
			/* free previous arm_clk_frequency_table */
			kfree(arm_clk_frequency_table);
			
			/* replace arm_clk_frequency_table */
			arm_clk_frequency_table = p_frequency_table;

			/* fill in arm_clk_frequency_table with RTKE/Librema working points */
			for (i=0 ; i<nr_freqs ; i++)
			{
				/* startup frequency must be fill in here */
				arm_clk_frequency_table[i].frequency = pnx_cpufreq_wp_list.base_adr->P_SC[i];
				arm_clk_frequency_table[i].index = i;
			}
	
			/* last element needs specific input */
			arm_clk_frequency_table[nr_freqs].frequency = CPUFREQ_TABLE_END;
			arm_clk_frequency_table[nr_freqs].index = 0;
		}
	}
#endif

	/* record freq table */
	cpufreq_frequency_table_get_attr(arm_clk_frequency_table, 0);

	/* wake up init sequence */
	wake_up(&updating_wp_event);
}


/*****************/
static void pnx_init_wplist_object (void)
{
#ifdef CONFIG_NKERNEL
/* event comes from RTK, Linux receives working point lists */	
	pnx_cpufreq_wp_list.shared = xos_area_connect(WPLIST_AREA_NAME, sizeof(struct pnx_cpufreq_wp_list));
	if (pnx_cpufreq_wp_list.shared )
	{
		pnx_cpufreq_wp_list.base_adr = xos_area_ptr(pnx_cpufreq_wp_list.shared );
		pnx_cpufreq_wp_list.wp_list_available =xos_ctrl_connect( CPUFREQ_INIT_WP_REQUEST_EVENT_NAME, TWO_EVENTS);
	
		if (pnx_cpufreq_wp_list.wp_list_available==NULL)
		{
			printk(PKMOD "connection of wp_list_request to wrapper function failed\n");
		}
		else
		{
			xos_ctrl_register(  pnx_cpufreq_wp_list.wp_list_available , 
								WP_LIST_AVAILABLE_EVENT_ID ,
								pnx_fill_frequency_table,
								NULL, 
								CLEAR_EVENT_BEFORE_REGISTRATION );
		}
	}	
	else printk(PKMOD "failed to connect to xos WP_AVAILABLE area\n");
#endif
}

/************/
static void pnx_init_send_data_object (void)
{
#ifdef CONFIG_NKERNEL
/* event sends from Linux to RTK */
	pnx_cpufreq_send_data.shared = xos_area_connect(SENDFREQ_AREA_NAME, sizeof( struct linux_frequencies));
	if (pnx_cpufreq_send_data.shared)
	{
		pnx_cpufreq_send_data.base_adr = xos_area_ptr(pnx_cpufreq_send_data.shared);
#ifdef CONFIG_ARMCLOCK_468_MHZ_MAX
		pnx_cpufreq_send_data.base_adr->SC_clk_max =	468000;
		pnx_cpufreq_send_data.base_adr->SC_clk_min =		468000;
#endif
#ifdef CONFIG_ARMCLOCK_416_MHZ_MAX
		pnx_cpufreq_send_data.base_adr->SC_clk_max =	416000;
		pnx_cpufreq_send_data.base_adr->SC_clk_min =		416000;
#endif

		pnx_cpufreq_send_data.base_adr->HCLK1 =				 0;
		pnx_cpufreq_send_data.base_adr->SDM_clk =			 0;
		pnx_cpufreq_send_data.base_adr->HCLK2 =				 0;
		pnx_cpufreq_send_data.base_adr->PCLK2 =				 0;
		pnx_cpufreq_send_data.base_adr->FC_clk =		 52000;
		pnx_cpufreq_send_data.base_adr->CAM_clk =		 12000;

		pnx_cpufreq_send_data.new_request=xos_ctrl_connect(CPUFREQ_SEND_REQUEST_EVENT_NAME, ONE_EVENT);
		if (pnx_cpufreq_send_data.new_request==NULL)
		{
			printk(PKMOD "connection of send_data to wrapper function failed : use xoscore 1.2\n");
		}
 	}
	else
		printk(PKMOD "failed to connect to xos NEW_REQUEST area\n");
#endif
}

/********************/
static void pnx_init_current_data_object (void)
{
#ifdef CONFIG_NKERNEL
/* event comes from RTK, Linux receives current frequencies */	
	pnx_cpufreq_current_data.shared = xos_area_connect(CURRENTFREQ_AREA_NAME, sizeof (struct current_frequencies ));
	if (pnx_cpufreq_current_data.shared )
	{
		pnx_cpufreq_current_data.base_adr = xos_area_ptr(pnx_cpufreq_current_data.shared);

		pnx67xx_frq_idx = pnx67xx_freq_to_idx(pnx_cpufreq_current_data.base_adr->SC_clk);
	
		pnx_cpufreq_current_data.change_occurs=xos_ctrl_connect(CPUFREQ_RECEIVED_CURRENT_FREQ_EVENT_NAME, ONE_EVENT);
		if (pnx_cpufreq_current_data.change_occurs==NULL)
		{
			printk(PKMOD "connection of current_freq to wrapper function failed\n");
		}
		else
		{
			xos_ctrl_register(  pnx_cpufreq_current_data.change_occurs , 
								CPUFREQ_RECEIVE_FREQUENCY_CHANGE_EVENT_ID ,
								pnx_update_current_frequency_handler ,
								(void*)&(pnx_cpufreq_current_data.change_occurs), 
								CLEAR_EVENT_BEFORE_REGISTRATION );
		}
	}	
	else printk(PKMOD "failed to connect to xos RECEIVED_CURRENT area\n");
#endif
}


/************/
static void pnx_init_librema_data_object (void)
{
#ifdef CONFIG_NKERNEL
/* miscellanous variables shared between linux and librema */
	pnx_cpufreq_librema_data.shared = xos_area_connect(LIBREMA_VAR_AREA_NAME, sizeof( struct librema_intf_var));
	if (pnx_cpufreq_librema_data.shared)
	{
		pnx_cpufreq_librema_data.base_adr = xos_area_ptr(pnx_cpufreq_librema_data.shared);
	
		pnx_cpufreq_librema_data.base_adr->suspend_state=0;

#ifndef CONFIG_SENSORS_PCF50616
		pnx_cpufreq_librema_data.enable_pid=xos_ctrl_connect(CPUFREQ_ENABLE_LIBREMA_PID_EVENT_NAME, ONE_EVENT);
		if (pnx_cpufreq_librema_data.enable_pid==NULL)
		{
			printk(PKMOD "connection of librema_data_enable_pid to wrapper function failed\n");
		}
#else		
		pnx_cpufreq_librema_data.rema_info=xos_ctrl_connect(CPUFREQ_LIBREMA_CHANGE_EVENT_NAME, TWO_EVENTS);
		if (pnx_cpufreq_librema_data.rema_info==NULL)
		{
			printk(PKMOD "connection of librema_data_change to wrapper function failed\n");
		}
#endif
		
	}
	else
	{
		printk(PKMOD "failed to connect to xos LIBREMA_VAR area\n");
		pnx_cpufreq_librema_data.base_adr = NULL;
	}
#endif
}

/*****************/
static int pnx_init_cpufreq_wrapper(void /*struct cpufreq_governor *dev*/)
{
	pnx_init_wplist_object();

	pnx_init_send_data_object();
	
	pnx_init_current_data_object();

	pnx_init_librema_data_object();
	return 0;
}



static int __init pnx67xx_cpufreq_init(void)
{
    int retval = 0;

	printk(KERN_INFO PKMOD "pnx_cpu-freq_init\n");

	/*** set a default frequency table ***/
	/* allocate Arm frequency table*/
 	arm_clk_frequency_table=kmalloc((nr_freqs+1)* sizeof(struct cpufreq_frequency_table), GFP_KERNEL);
	if (!arm_clk_frequency_table)
	{
		return -1; /* FIX ME : find which error to return*/
	}

	/* set internal frequency table index */
	pnx67xx_frq_idx = 0;
	pnx67xx_frq_req_pending = 0;

	/* startup frequency must be fill in here */
#ifdef CONFIG_ARMCLOCK_468_MHZ_MAX
	arm_clk_frequency_table[pnx67xx_frq_idx].frequency = 468000;
#endif
#ifdef CONFIG_ARMCLOCK_416_MHZ_MAX
	arm_clk_frequency_table[pnx67xx_frq_idx].frequency=416000;
#endif
	arm_clk_frequency_table[pnx67xx_frq_idx].index=0;
	
	/* last element needs specific input */
	arm_clk_frequency_table[nr_freqs].frequency=CPUFREQ_TABLE_END;
	arm_clk_frequency_table[nr_freqs].index=0;
	/* record freq table */
	cpufreq_frequency_table_get_attr(arm_clk_frequency_table, 0);

	/*** get policy ***/	
	policy = cpufreq_cpu_get(0);

	/*** initialize cpufreq wrapper ***/
	pnx_init_cpufreq_wrapper();
	
#ifdef CONFIG_NKERNEL

	/* get RTKE working points */
	xos_ctrl_raise(pnx_cpufreq_wp_list.wp_list_available, WP_LIST_REQUEST_EVENT_ID); 

	/* wait until RTKE/librema answers */
	wait_event(updating_wp_event, 1);

#endif

	/*** init cpufreq driver ***/
	if (cpufreq_register_driver(&pnx67xx_frq_driver))
			return -EIO;

#ifdef CONFIG_NKERNEL
	/* unlock librema PID  */
	if (pnx_cpufreq_librema_data.base_adr)
	{
		pnx_cpufreq_librema_data.base_adr->librema_PID_state=1;
#ifndef CONFIG_SENSORS_PCF50616
		xos_ctrl_raise(pnx_cpufreq_librema_data.enable_pid, CPUFREQ_ENABLE_LIBREMA_PID_EVENT_ID);
#else		
		xos_ctrl_raise(pnx_cpufreq_librema_data.rema_info, CPUFREQ_LIBREMA_ENABLE_PID_EVENT_ID);
#endif
		printk(KERN_INFO PKMOD "LIBREMA PID : enabled\n");
	}
	else
	{
		printk(KERN_INFO PKMOD "LIBREMA PID : Disabled\n");
	}
	
#endif


    return retval;
}

module_init(pnx67xx_cpufreq_init);

