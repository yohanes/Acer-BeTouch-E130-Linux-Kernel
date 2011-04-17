/*
 *  linux/arch/arm/plat-pnx/power_debug.c
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

#include <linux/kallsyms.h>

#include <asm/io.h>
#include <mach/hardware.h>

#include "power_sysfs.h"
#include <mach/power_debug.h>

#include <asm/cacheflush.h>


/*** Module definition ***/
/*************************/

#define MODULE_NAME "PNX_POWER"
#define PKMOD MODULE_NAME ": "

#define PNX_TIMER_DEBUG

/*** Power saving debug ***/
/**************************/

/*** Debug buffer variable ***/
static DEFINE_SPINLOCK(pnx_dbg_lock);

#define DBG_SYSFS_ARRAY_SIZE 1024
static unsigned long long pnx_dbg_time_array[DBG_SYSFS_ARRAY_SIZE];
static char *		 pnx_dbg_id_array[DBG_SYSFS_ARRAY_SIZE];
static unsigned long pnx_dbg_event_array[DBG_SYSFS_ARRAY_SIZE];
static void * pnx_dbg_function_array[DBG_SYSFS_ARRAY_SIZE];

static unsigned long pnx_dbg_event_start_idx = 0;
static unsigned long pnx_dbg_event_stop_idx = 0;

static unsigned long pnx_dbg_stat_enable = 0;
static unsigned long pnx_dbg_stat_activate = 1;

char * pnx_dbg_null_name = " ";
char * pnx_dbg_jiffies_name = "jiffies";

/*** statistic info ***/
inline void pnx_dbg_init_element(void)
{
	int i;

	/* init stat device */
	for (i=0; i < DBG_SYSFS_ARRAY_SIZE; i++)
	{
		pnx_dbg_time_array[i] = 0;
		pnx_dbg_id_array[i]= pnx_dbg_null_name;
		pnx_dbg_event_array[i]= 0;
		pnx_dbg_function_array[i]= 0;
	}

	pnx_dbg_event_start_idx = 0;
	pnx_dbg_event_stop_idx = 0;

}

#if !defined(CONFIG_NKERNEL) && defined(CONFIG_PNX_MTU_TIMER)
static inline cycle_t pnx_rtke_read(void)
{
	return (cycle_t)(readl( MTU_TCVAL_REG ));
}

static unsigned long pnx_dbg_read(unsigned long long ticks64)
{
	return (unsigned long)(ticks64);
}

#else
#include <linux/clocksource.h>

extern cycle_t pnx_rtke_read(void);

static unsigned long pnx_dbg_read(unsigned long long ticks64)
{
	ticks64 = (ticks64 * 967916) >> 20;

	return (unsigned long)(ticks64);
}
#endif

void pnx_dbg_put_element(char *id, unsigned long element, void * function)
{
	unsigned long flags;

	spin_lock_irqsave(pnx_dbg_lock, flags);
	if (pnx_dbg_stat_enable)
	{
		pnx_dbg_time_array[pnx_dbg_event_stop_idx]= pnx_rtke_read();
		pnx_dbg_id_array[pnx_dbg_event_stop_idx] = id;
		pnx_dbg_event_array[pnx_dbg_event_stop_idx]= element;
		pnx_dbg_function_array[pnx_dbg_event_stop_idx++]= function;
		pnx_dbg_event_stop_idx %= DBG_SYSFS_ARRAY_SIZE;
		if (pnx_dbg_event_stop_idx == pnx_dbg_event_start_idx)
		{
			pnx_dbg_event_start_idx++;
			pnx_dbg_event_start_idx %= DBG_SYSFS_ARRAY_SIZE;
		}
	}
	spin_unlock_irqrestore(pnx_dbg_lock, flags);

	return;
}

/*** activate debug ***/
#define DBG_KERNEL_NOP_CODE		 0xE1A06006
extern pnx_dbg_register_t __pnx_dbg_reg_start[], __pnx_dbg_reg_end[];
extern unsigned long _stext[], _end[];

inline void pnx_dbg_activate_element(void)
{
	unsigned long fct_ptr = (unsigned long)pnx_dbg_put_element;
	unsigned long * start_address;
    unsigned long *	end_address;
	unsigned long code;

	start_address = (unsigned long *)pnx_dbg_put_element - (32*1024*1024);
	if (start_address < _stext)
		start_address = _stext;

	end_address = (unsigned long *)pnx_dbg_put_element + (32*1024*1024);
	if (end_address > _end)
		end_address = _end;

	while (start_address < end_address)
	{
		if (*start_address == DBG_KERNEL_NOP_CODE)
		{
		  	pnx_dbg_register_t * funk;
			char symname[KSYM_NAME_LEN];
			unsigned long fct_address;

			if (lookup_symbol_name((unsigned long)(start_address), symname) < 0)
			{
				printk("Call pnx_dbg_... found at %p\n", start_address);
				fct_address = (unsigned long)start_address;
			}
			else
			{
				printk("Call pnx_dbg_... found at %s\n", symname);
				fct_address = kallsyms_lookup_name(symname);
			}

			for ( funk = __pnx_dbg_reg_start; funk < __pnx_dbg_reg_end; funk += 1 )
			{
				if (fct_address == *((unsigned long *)funk))
				{
					printk("activate call to pnx_dbg_... at %p\n", start_address);
					code = fct_ptr - 8 - (unsigned long)start_address;
					code >>= 2;
					code &= 0xFFFFFF;
					code |= 0xEB000000;

					*start_address = code;
				}
			}
		}

		start_address++;
	}

	__cpuc_flush_kern_all();
}

inline void pnx_dbg_deactivate_element(void)
{
	unsigned long fct_ptr = (unsigned long)pnx_dbg_put_element;
	unsigned long * start_address;
    unsigned long *	end_address;
	unsigned long code;


	start_address = (unsigned long *)pnx_dbg_put_element - (32*1024*1024);
	if (start_address < _stext)
		start_address = _stext;

	end_address = (unsigned long *)pnx_dbg_put_element + (32*1024*1024);
	if (end_address > _end)
		end_address = _end;

	while (start_address < end_address)
	{

		code = fct_ptr - 8 - (unsigned long)start_address;
		code >>= 2;
		code &= 0xFFFFFF;
		code |= 0xEB000000;

		if (*start_address == code)
		{
		  	pnx_dbg_register_t * funk;
			char symname[KSYM_NAME_LEN];
			unsigned long fct_address;

			if (lookup_symbol_name((unsigned long)(start_address), symname) < 0)
			{
				printk("Call pnx_dbg_... found at %p\n", start_address);
				fct_address = (unsigned long)start_address;
			}
			else
			{
				printk("Call pnx_dbg_... found at %s\n", symname);
				fct_address = kallsyms_lookup_name(symname);
			}

			for ( funk = __pnx_dbg_reg_start; funk < __pnx_dbg_reg_end; funk += 1 )
			{
				if (fct_address == *((unsigned long *)funk))
				{
					printk("deactivate call to pnx_dbg_... at %p\n", start_address);
					*start_address = DBG_KERNEL_NOP_CODE;
				}
			}
		}

		start_address++;
	}

	__cpuc_flush_kern_all();
}


/*** SysFs interface ***/
/***********************/

static ssize_t show_statistic(struct kobject *kobj, char *buffer)
{
	int i;
	int size = 0;
	unsigned long enable;
	unsigned long flags;

#ifdef PNX_TIMER_DEBUG
	printk(PKMOD "debug_show_stat start %lu stop %lu\n", pnx_dbg_event_start_idx, pnx_dbg_event_stop_idx);
#endif

	spin_lock_irqsave(pnx_dbg_lock, flags);
	enable = pnx_dbg_stat_enable;
	pnx_dbg_stat_enable = 0;
	spin_unlock_irqrestore(pnx_dbg_lock, flags);


	size += sprintf(buffer + size, "event list:\n");
	while ((pnx_dbg_event_start_idx != pnx_dbg_event_stop_idx) && (size < 4000))
	{
	char symname[KSYM_NAME_LEN];

	if (lookup_symbol_name((unsigned long)(pnx_dbg_function_array[pnx_dbg_event_start_idx]), symname) < 0)
		size += sprintf(buffer + size, "%lu : %s %lu / %p\n", pnx_dbg_read(pnx_dbg_time_array[pnx_dbg_event_start_idx]),
			pnx_dbg_id_array[pnx_dbg_event_start_idx],
		   	pnx_dbg_event_array[pnx_dbg_event_start_idx],
			pnx_dbg_function_array[pnx_dbg_event_start_idx]);
	else
		size += sprintf(buffer + size, "%lu : %s %lu / %s\n", pnx_dbg_read(pnx_dbg_time_array[pnx_dbg_event_start_idx]),
			pnx_dbg_id_array[pnx_dbg_event_start_idx],
		   	pnx_dbg_event_array[pnx_dbg_event_start_idx],
			symname);

		pnx_dbg_event_start_idx++;
		pnx_dbg_event_start_idx %= DBG_SYSFS_ARRAY_SIZE;
	}

	i =  pnx_dbg_event_stop_idx >= pnx_dbg_event_start_idx ? pnx_dbg_event_stop_idx - pnx_dbg_event_start_idx : DBG_SYSFS_ARRAY_SIZE - (pnx_dbg_event_start_idx - pnx_dbg_event_stop_idx);

	size += sprintf(buffer + size, "still %u event in the list \n", i);

	spin_lock_irqsave(pnx_dbg_lock, flags);
	pnx_dbg_stat_enable = enable;
	spin_unlock_irqrestore(pnx_dbg_lock, flags);

	return size;
}

static ssize_t show_enable(struct kobject *kobj, char *buffer)
{
	return sprintf(buffer, "%lu\n", pnx_dbg_stat_enable);
}

static ssize_t store_enable(struct kobject *kobj, const char *buffer, size_t size)
{
	unsigned int enable = simple_strtoul(buffer, NULL, 2);
	unsigned long flags;

	if (enable)
	{

		spin_lock_irqsave(pnx_dbg_lock, flags);
		pnx_dbg_stat_enable = 0;
		spin_unlock_irqrestore(pnx_dbg_lock, flags);

		pnx_dbg_init_element();

		spin_lock_irqsave(pnx_dbg_lock, flags);
		pnx_dbg_stat_enable = 1;
		spin_unlock_irqrestore(pnx_dbg_lock, flags);

		pnx_dbg_put_element(pnx_dbg_jiffies_name, jiffies, NULL);
	}
	else
	{
		spin_lock_irqsave(pnx_dbg_lock, flags);
		pnx_dbg_stat_enable = 0;
		spin_unlock_irqrestore(pnx_dbg_lock, flags);
	}

	return size;
}
static ssize_t show_activate(struct kobject *kobj, char *buffer)
{
	return sprintf(buffer, "%lu\n", pnx_dbg_stat_activate);
}

static ssize_t store_activate(struct kobject *kobj, const char *buffer, size_t size)
{
	unsigned int activate = simple_strtoul(buffer, NULL, 2);
	unsigned long flags;
	unsigned long tmp_enable;

	if (activate)
	{

		spin_lock_irqsave(pnx_dbg_lock, flags);
		tmp_enable = pnx_dbg_stat_enable;
		pnx_dbg_stat_enable = 0;
		spin_unlock_irqrestore(pnx_dbg_lock, flags);

		pnx_dbg_stat_activate = 1;
		pnx_dbg_activate_element();

		spin_lock_irqsave(pnx_dbg_lock, flags);
		pnx_dbg_stat_enable = tmp_enable;
		spin_unlock_irqrestore(pnx_dbg_lock, flags);

	}
	else
	{

		spin_lock_irqsave(pnx_dbg_lock, flags);
		tmp_enable = pnx_dbg_stat_enable;
		pnx_dbg_stat_enable = 0;
		spin_unlock_irqrestore(pnx_dbg_lock, flags);

		pnx_dbg_stat_activate = 0;
		pnx_dbg_deactivate_element();

		spin_lock_irqsave(pnx_dbg_lock, flags);
		pnx_dbg_stat_enable = tmp_enable;
		spin_unlock_irqrestore(pnx_dbg_lock, flags);

	}

	return size;
}

static ssize_t show_stamp(struct kobject *kobj, char *buffer)
{
	return sprintf(buffer, "%lu us\n", pnx_dbg_read(pnx_rtke_read()));
}


/*** create sysfs dbg file tree ***/

define_one_ro(statistic);
define_one_rw(enable);
define_one_rw(activate);
define_one_ro(stamp);

static struct attribute * dbs_attributes_dbg[] = {
	&statistic.attr,
	&enable.attr,
	&activate.attr,
	&stamp.attr,
	NULL
};

static struct attribute_group dbs_attr_debug_group = {
	.attrs = dbs_attributes_dbg,
	.name = "debug_power",
};


/*** System timer init ***/
/*************************/

static int __init pnx_debug_init_sysfs(void)
{
#ifdef PNX_TIMER_DEBUG
	printk(PKMOD "debug_init_sysfs\n");
#endif

	pnx_dbg_stat_enable = 0;
	pnx_dbg_stat_activate = 1;

	pnx_dbg_init_element();

	if (sysfs_create_group(&pnx_power_kobj, &dbs_attr_debug_group))
		printk(PKMOD "Unable to register %s in sysfs\n", dbs_attr_debug_group.name);

	return 0;
}

module_init(pnx_debug_init_sysfs);



