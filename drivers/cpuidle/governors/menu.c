/*
 * menu.c - the menu idle governor
 *
 * Copyright (C) 2006-2007 Adam Belay <abelay@novell.com>
 *
 * This code is licenced under the GPL.
 */

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/pm_qos_params.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/sysfs.h>
#include <linux/cpu.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define BREAK_FUZZ	4	/* 4 us */
#define PRED_HISTORY_PCT	50

struct menu_device {
	int		last_state_idx;

	unsigned int	expected_us;
	unsigned int	predicted_us;
	unsigned int    current_predicted_us;
	unsigned int	last_measured_us;
	unsigned int	elapsed_us;
	unsigned int	enable;
	unsigned int	baggy_enable;
	unsigned int	baggy_mode;
	unsigned int	baggy_lenght;
	unsigned int	baggy_max;
	unsigned int	deepest_idle_state_user;
	unsigned int	deepest_idle_state_kernel;
};

static DEFINE_PER_CPU(struct menu_device, menu_devices);

/**
 * menu_select - selects the next idle state to enter
 * @dev: the CPU
 */
#include <mach/power_debug.h>
#ifdef CONFIG_PNX_POWER_TRACE
char * pnx_cpu_menu_expected_name = "Enter menu for";
//char * pnx_cpu_menu_predicted_name = "Menu prediction";
//char * pnx_cpu_menu_latency_name = "Menu latency";
#endif

static int menu_select(struct cpuidle_device *dev)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);
	int latency_req = pm_qos_requirement(PM_QOS_CPU_DMA_LATENCY);
	int i;
	s64 expected_ns;

	/* Special case when user has set very strict latency requirement */
	if (unlikely(latency_req == 0)) {
		data->last_state_idx = 0;
		return 0;
	}


	if(data->enable)
	{
		ktime_t lenght_kt;

		if((data->baggy_enable) && (data->baggy_mode) && (data->baggy_lenght != (unsigned int) (-1)))
		{
			struct timespec lenght_ts;

			jiffies_to_timespec(data->baggy_lenght, &lenght_ts);
			lenght_kt = timespec_to_ktime(lenght_ts);

			tick_program_power_baggy(lenght_kt);
			data->baggy_lenght = (unsigned int) (-1);

		}
		else
		{
			lenght_kt = tick_nohz_get_baggy_length();
		}

		/* determine the expected residency time */
		expected_ns = ktime_to_ns(ktime_add(tick_nohz_get_sleep_length(), lenght_kt)) >> 3;
		if ((expected_ns >> 32) != 0)
		{
			// we have an approximation of 2,4 %
			// for a duration greater than 34,36 sec
			expected_ns >>= 7;
			data->expected_us = (u32)(expected_ns);
		}
		else
		{
			data->expected_us = (u32)(expected_ns) / 125;
		}
//		data->expected_us =	(u32) ktime_to_ns(tick_nohz_get_sleep_length()) / 1000;

#ifdef CONFIG_PNX_POWER_TRACE
		pnx_dbg_put_element(pnx_cpu_menu_expected_name, data->expected_us, NULL);
//		pnx_dbg_put_element(pnx_cpu_menu_predicted_name, data->predicted_us, NULL);
//		pnx_dbg_put_element(pnx_cpu_menu_latency_name, latency_req, NULL);
#endif



		/* find the deepest idle state that satisfies our constraints */
		for (i = CPUIDLE_DRIVER_STATE_START + 1; i < dev->state_count; i++) {
			struct cpuidle_state *s = &dev->states[i];

			if (i > (CPUIDLE_DRIVER_STATE_START + data->deepest_idle_state_user))
				break;
			if (i > (CPUIDLE_DRIVER_STATE_START + data->deepest_idle_state_kernel))
				break;
			if (s->target_residency > data->expected_us)
				break;
//			if (s->target_residency > data->predicted_us)
//				break;
			if (s->exit_latency > latency_req)
				break;
		}

		data->last_state_idx = i - 1;
		return i - 1;
	}
	else
	{
		data->enable = 1;
		return dev->state_count-1;
	}
		
}

/**
 * menu_reflect - attempts to guess what happened after entry
 * @dev: the CPU
 *
 * NOTE: it's important to be fast here because this operation will add to
 *       the overall exit latency.
 */
static void menu_reflect(struct cpuidle_device *dev)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);
	int last_idx = data->last_state_idx;
	unsigned int last_idle_us = cpuidle_get_last_residency(dev);
	struct cpuidle_state *target = &dev->states[last_idx];
	unsigned int measured_us;

	/*
	 * Ugh, this idle state doesn't support residency measurements, so we
	 * are basically lost in the dark.  As a compromise, assume we slept
	 * for one full standard timer tick.  However, be aware that this
	 * could potentially result in a suboptimal state transition.
	 */
	if (unlikely(!(target->flags & CPUIDLE_FLAG_TIME_VALID)))
		last_idle_us = USEC_PER_SEC / HZ;

	/*
	 * measured_us and elapsed_us are the cumulative idle time, since the
	 * last time we were woken out of idle by an interrupt.
	 */
	if (data->elapsed_us <= data->elapsed_us + last_idle_us)
		measured_us = data->elapsed_us + last_idle_us;
	else
		measured_us = -1;

	/* Predict time until next break event */
	data->current_predicted_us = max(measured_us, data->last_measured_us);

	if (last_idle_us + BREAK_FUZZ <
	    data->expected_us - target->exit_latency) {
		data->last_measured_us = measured_us;
		data->elapsed_us = 0;
	} else {
		data->elapsed_us = measured_us;
	}
}

/**
 * Sysfs interface
 */
static ssize_t show_enable(struct kobject * kobj, struct attribute * attr ,char * buf)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);

	ssize_t ret;

	ret = sprintf(buf, "%d\n", data->enable);

	return ret;
}

static ssize_t store_enable(struct kobject * kobj, struct attribute * attr,
		     const char * buf, size_t count)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);

	unsigned int enable = simple_strtoul(buf, NULL, 10);
	
	data->enable = enable;

	return count;
}

static ssize_t show_baggy(struct kobject * kobj, struct attribute * attr ,char * buf)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);

	ssize_t ret;

	ret = sprintf(buf, "%d\n", data->baggy_enable);

	return ret;
}

static ssize_t store_baggy(struct kobject * kobj, struct attribute * attr,
		     const char * buf, size_t count)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);

	unsigned int enable = simple_strtoul(buf, NULL, 10);
	
	data->baggy_enable = enable;

	return count;
}

static ssize_t show_baggy_max(struct kobject * kobj, struct attribute * attr ,char * buf)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);

	ssize_t ret;

	ret = sprintf(buf, "%d\n", data->baggy_max);

	return ret;
}

static ssize_t store_baggy_max(struct kobject * kobj, struct attribute * attr,
		     const char * buf, size_t count)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);

	unsigned int max = simple_strtoul(buf, NULL, 10);
	
	data->baggy_max = max;

	return count;
}

static ssize_t show_deepest_idle_state(struct kobject * kobj, struct attribute * attr ,char * buf)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);

	ssize_t ret;

	ret = sprintf(buf, "%d\n", data->deepest_idle_state_user);

	return ret;
}

static ssize_t store_deepest_idle_state(struct kobject * kobj, struct attribute * attr,
		     const char * buf, size_t count)
{
	struct cpuidle_device *dev = per_cpu(cpuidle_devices, 0);
	struct menu_device *data = &__get_cpu_var(menu_devices);

	unsigned int state = simple_strtoul(buf, NULL, 10);
	
	if (state < dev->state_count)
		data->deepest_idle_state_user = state;

	return count;
}

struct cpuidle_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, struct attribute *, const char *, size_t);
};

#define define_one_rw(_name) \
	static struct cpuidle_attr attr_##_name = __ATTR(_name, 0644, show_##_name, store_##_name)

define_one_rw(enable);
define_one_rw(baggy);
define_one_rw(baggy_max);
define_one_rw(deepest_idle_state);

static struct attribute *cpuidle_menu_default_attrs[] = {
	&attr_baggy.attr,
	&attr_baggy_max.attr,
	&attr_enable.attr,
	&attr_deepest_idle_state.attr,
	NULL
};

static struct attribute_group dbs_attr_menu_group = {
	.attrs = cpuidle_menu_default_attrs,
	.name = "menu",
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void menu_device_suspend(struct early_suspend *pes)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);

	data->baggy_mode = 1;
}

static void menu_device_resume(struct early_suspend *pes)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);

	data->baggy_mode = 0;
}

static struct early_suspend menu_device = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 15,
	.suspend = menu_device_suspend,
	.resume = menu_device_resume,
};
#endif

int menu_set_extended_baggy(unsigned int lenght)
{
	struct menu_device *data = &__get_cpu_var(menu_devices);

	if (lenght < data->baggy_max)
		data->baggy_lenght = lenght;
	else
		data->baggy_lenght = data->baggy_max;

	return 0;
}

/**
 * menu_enable_device - scans a CPU's states and does setup
 * @dev: the CPU
 */
static int menu_enable_device(struct cpuidle_device *dev)
{
	struct sys_device *sys_dev = get_cpu_sysdev((unsigned long)dev->cpu);
	struct menu_device *data = &per_cpu(menu_devices, dev->cpu);

	memset(data, 0, sizeof(struct menu_device));

	data->enable = 1;
	data->baggy_enable = 2;
	data->baggy_mode = 0;
	data->baggy_max = 3*HZ/2;
	data->baggy_lenght = (unsigned int)(-1);
	data->deepest_idle_state_user = dev->state_count-2;
	data->deepest_idle_state_kernel = dev->state_count-2;

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&menu_device);
#endif
	
	if (sysfs_create_group(&sys_dev->kobj, &dbs_attr_menu_group))
		printk("Unable to register menu in sysfs\n");
	
	return 0;
}

/**
 * menu_disable_device - scans a CPU's states and does setup
 * @dev: the CPU
 */
static void menu_disable_device(struct cpuidle_device *dev)
{
	struct sys_device *sys_dev = get_cpu_sysdev((unsigned long)dev->cpu);
	struct menu_device *data = &per_cpu(menu_devices, dev->cpu);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&menu_device);
#endif

	memset(data, 0, sizeof(struct menu_device));

	sysfs_remove_group(&sys_dev->kobj, &dbs_attr_menu_group);
}

static struct cpuidle_governor menu_governor = {
	.name =		"menu",
	.rating =	20,
	.enable =	menu_enable_device,
	.disable =	menu_disable_device,
	.select =	menu_select,
	.reflect =	menu_reflect,
	.owner =	THIS_MODULE,
};

/**
 * set_deepest_idle_state - et the deepest idle state the system can enter
 *  state : the state level
 */

int set_deepest_idle_state(unsigned long state)
{
	struct menu_device *data = &per_cpu(menu_devices, 0);
	data->deepest_idle_state_kernel = state;

	return 0;
}

/**
 * init_menu - initializes the governor
 */
static int __init init_menu(void)
{
	return cpuidle_register_governor(&menu_governor);
}

/**
 * exit_menu - exits the governor
 */
static void __exit exit_menu(void)
{
	cpuidle_unregister_governor(&menu_governor);
}

MODULE_LICENSE("GPL");
module_init(init_menu);
module_exit(exit_menu);
