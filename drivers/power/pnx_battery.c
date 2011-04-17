/*
 * linux/drivers/power/pnx_battery.c
 *
 * Battery measurement code for pnx system solution
 *
 * based on tosa_battery.c
 *
 * Copyright (C) 2009 Olivier Clergeaud <olivier.clergeaud@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include "drvchg.h"
#include "battmon.h"

static struct work_struct bat_work;
//struct mutex work_lock;

static int bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
static int ac_status = -1;
static int usb_status = -1;
static int pnx_batt_level = 50;


/// +ACER_AUx PenhoYu, Include the acer_battery function
#ifdef ACER_CHG_BAT_FUN
#include "acer_battery.h"
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, Include the acer_battery function

static unsigned long pnx_read_bat(struct power_supply *bat_ps)
{
/// +ACER_AUx PenhoYu, Include the acer_battery function
#ifdef ACER_CHG_BAT_FUN
	return g_batt_volt / 1000;
#else	// ACER_CHG_BAT_FUN
	return drvchg_get_voltage();
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, Include the acer_battery function
}

static unsigned long pnx_read_temp(struct power_supply *bat_ps)
{
	/* The temperature value received is in thousandth of degree Celsisus */
	/* the Linux power supply class needs a value in tenth of degree Celsius */
/// +ACER_AUx PenhoYu, for fix A41.B-674, A21.B-358
#ifdef ACER_CHG_BAT_FUN
	int i = ((isChgFlag(CHGF_P_OTP)) ? 2 :
			 (isChgFlag(CHGF_P_WDT)) ? 4 :
			 (isChgFlag(CHGF_P_APC)) ? 6 :
			 (isChgFlag(CHGF_P_OCP | CHGF_P_OVP | CHGF_P_THL | CHGF_P_BID)) ? 8 : 0) + isChgFlag(CHGF_750MA);
	return pnx_batt_temp + i;
#else	// ACER_CHG_BAT_FUN
	return (unsigned long)(drvchg_get_temperature()/100);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, for fix A41.B-674, A21.B-358
}

static int pnx_bat_get_property(struct power_supply *bat_ps,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	uint8_t charge = drvchg_get_charge_state();

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:

/// +ACER_AUx PenhoYu, for ACER_AUx to show the not-charging warning icon. (SW:OTP,OVP)
#ifdef ACER_CHG_BAT_FUN
		if (pnx_batt_level == 100 && charge)
		{
			val->intval = drvchg_get_charging_state() ? POWER_SUPPLY_STATUS_FULL : POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
		else
		{
			val->intval = ((g_temp_index == 10) ? POWER_SUPPLY_STATUS_UNKNOWN :
				((pcf506XX_mainChgPlugged()) ?
					(drvchg_get_charging_state() ? POWER_SUPPLY_STATUS_CHARGING : POWER_SUPPLY_STATUS_NOT_CHARGING) :
					POWER_SUPPLY_STATUS_DISCHARGING));
		}

		if (showdbglv(DBGMSG_NOTTIFY))
			printk(KERN_INFO "<BAT> pnx_bat_get_property - POWER_SUPPLY_PROP_STATUS (%d-%d-%ld) : %d\n",
				charge, drvchg_get_charging_state(), g_temp_index, val->intval);
#else	// ACER_CHG_BAT_FUN
		if (pnx_batt_level == 100 && charge)
		{
			val->intval = POWER_SUPPLY_STATUS_FULL;
		}
		else
		{
			val->intval = charge ? POWER_SUPPLY_STATUS_CHARGING : POWER_SUPPLY_STATUS_DISCHARGING;
		}
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, for ACER_AUx to show the not-charging warning icon. (SW:OTP,OVP)
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = drvchg_get_charge_state();
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = pnx_batt_level;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = pnx_read_bat(bat_ps);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = pnx_read_temp(bat_ps);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
/// +ACER_AUx PenhoYu, add abnormal charge report mechanism
#ifdef ACER_CHG_BAT_FUN
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = isChgFlag(0xFF);
		break;
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, add abnormal charge report mechanism
	default:
		return -EINVAL;
	}
	return 0;
}

static void pnx_bat_external_power_changed(struct power_supply *bat_ps)
{
	schedule_work(&bat_work);
}

static char *status_text[] = {
	[POWER_SUPPLY_STATUS_UNKNOWN] =		"Unknown",
	[POWER_SUPPLY_STATUS_CHARGING] =	"Charging",
	[POWER_SUPPLY_STATUS_DISCHARGING] =	"Discharging",
};

static void pnx_bat_update(struct power_supply *ps)
{
	int old_status = bat_status;

//	mutex_lock(&work_lock);

	bat_status = drvchg_get_charge_state() ?
				    POWER_SUPPLY_STATUS_CHARGING :
				    POWER_SUPPLY_STATUS_DISCHARGING;

	if (old_status != bat_status) {
		pr_debug("%s %s -> %s\n", ps->name,
				status_text[old_status],
				status_text[bat_status]);
	}

	power_supply_changed(ps);

//	mutex_unlock(&work_lock);
}

static enum power_supply_property pnx_bat_main_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
/// +ACER_AUx PenhoYu, add abnormal charge report mechanism
#ifdef ACER_CHG_BAT_FUN
	POWER_SUPPLY_PROP_ONLINE,
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, add abnormal charge report mechanism
};

static int pnx_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_MAINS)
			val->intval = ac_status;
		else
			val->intval = usb_status;
/// +ACER_AUx PenhoYu, show online state message
#ifdef ACER_CHG_BAT_FUN
		if (showdbglv(DBGMSG_NOTTIFY))
			printk(KERN_INFO "<BAT> pnx_power_get_property(\"%s\") : %d - ac(%d), usb(%d)\n", psy->name, val->intval, ac_status, usb_status);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, show online state message
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static char *pnx_power_supplied_to[] = {
	"main-battery",
	"backup-battery",
};

static enum power_supply_property pnx_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static struct power_supply pnx_psy_ac = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.supplied_to = pnx_power_supplied_to,
	.num_supplicants = ARRAY_SIZE(pnx_power_supplied_to),
	.properties = pnx_power_props,
	.num_properties = ARRAY_SIZE(pnx_power_props),
	.get_property = pnx_power_get_property,
	.external_power_changed = pnx_bat_external_power_changed,
};

static struct power_supply pnx_psy_usb = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.supplied_to = pnx_power_supplied_to,
	.num_supplicants = ARRAY_SIZE(pnx_power_supplied_to),
	.properties = pnx_power_props,
	.num_properties = ARRAY_SIZE(pnx_power_props),
	.get_property = pnx_power_get_property,
	.external_power_changed = pnx_bat_external_power_changed,
};

struct power_supply bat_ps = {
	.name					= "battery",
	.type					= POWER_SUPPLY_TYPE_BATTERY,
	.properties				= pnx_bat_main_props,
	.num_properties			= ARRAY_SIZE(pnx_bat_main_props),
	.get_property			= pnx_bat_get_property,
	.external_power_changed = pnx_bat_external_power_changed,
	.use_for_apm			= 1,
};

static void pnx_bat_level(int level)
{
	pnx_batt_level = level;
	pnx_bat_update(&bat_ps);
}

static void update_charger(unsigned int source)
{
	switch (source) {
	case POWER_SUPPLY_TYPE_BATTERY:
		ac_status=0;
/// +ACER_AUx PenhoYu, if usb unmout, must notify to remove USB Storage.
#ifdef ACER_CHG_BAT_FUN
		if (usb_status == 1) {
			usb_status=0;
			pnx_bat_update(&pnx_psy_usb);
		}
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, if usb unmout, must notify to remove USB Storage.
		usb_status=0;
		pnx_bat_update(&bat_ps);
		break;

	case POWER_SUPPLY_TYPE_MAINS:
/// +ACER_AUx PenhoYu, [PRM] Start the AC/USB power supply recognition mechanism
		usb_status=0;
/// -ACER_AUx PenhoYu, [PRM] Start the AC/USB power supply recognition mechanism
		ac_status=1;
		pnx_bat_update(&pnx_psy_ac);
		break;

	case POWER_SUPPLY_TYPE_USB:
		usb_status=1;
		ac_status=0;
		pnx_bat_update(&pnx_psy_usb);
		break;

/// +ACER_AUx PenhoYu, update the battery state
#ifdef ACER_CHG_BAT_FUN
	case POWER_SUPPLY_BATTERY_UPDATA:
		mod_timer(&g_battpoll_timer,
		          jiffies + msecs_to_jiffies(500));
		break;

	case POWER_SUPPLY_START_CHARGE:
		mod_timer(&g_otp_timer,
		          jiffies + msecs_to_jiffies(500));
		break;

	case POWER_SUPPLY_CK_BATID:
		adc_aread(FID_BATPACK, otp_adc_batid);
		break;
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, update the battery state

	default:
		break;
	}
}

static void pnx_bat_work(struct work_struct *work)
{
	u_int8_t source = drvchg_get_charge_state();
	
	printk(KERN_DEBUG "pnx_bat_work\n");

	if (source==POWER_SUPPLY_TYPE_MAINS)
	{
		ac_status=1;
		usb_status=0;
	} 
	else
		if (source==POWER_SUPPLY_TYPE_USB)
		{
			ac_status=0;
			usb_status=1;
		}
		else
		{
			ac_status=0;
			usb_status=0;	
		}

	pnx_bat_update(&pnx_psy_ac);
	pnx_bat_update(&pnx_psy_usb);
	pnx_bat_update(&bat_ps);
}

#ifdef CONFIG_PM
static int pnx_bat_suspend(struct platform_device *dev, pm_message_t state)
{
	flush_scheduled_work();
	return 0;
}

static int pnx_bat_resume(struct platform_device *dev)
{
	schedule_work(&bat_work);
	return 0;
}
#else
#define pnx_bat_suspend NULL
#define pnx_bat_resume NULL
#endif

static ssize_t set_level(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
/// +ACER_AUx PenhoYu, Control the Debug Message.
#ifdef ACER_CHG_BAT_FUN
	ulong dbglv = showdbglv(0xffffffff);
	if (*buf == 'n') {
		dbglv &= 0xffff0000;
		if ('0' <= (*(buf + 1)) && (*(buf + 1) <= '9')) setdbglv(dbglv | ((*(buf + 1) - '0') * 16));
		else setdbglv(dbglv + 1);
	}
	else if (*buf == 'f') setdbglv(dbglv & 0xffff0000);
	else if (*buf == 's') ctrlCharge(0, CHGF_P_APC);
	else if (*buf == 'r') ctrlCharge(1, CHGF_P_APC);
	else if (*buf == 'm') setdbglv(dbglv | DBGFLAG_MONKEY);
	else {
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, Control the Debug Message.
	unsigned long value = simple_strtoul(buf, NULL, 10);

	if (value < 0)
		value=0;

	if (value > 100)
		value = 100;

	pnx_batt_level = value;
	
	pnx_bat_update(&bat_ps);
/// +ACER_AUx PenhoYu, Control the Debug Message.
#ifdef ACER_CHG_BAT_FUN
	}
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, Control the Debug Message.
	
	return count;
}

static ssize_t show_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "\033[1;34mBattery level is %02u%% \033[0m\n",pnx_batt_level);
}

static DEVICE_ATTR(level, S_IRUGO | S_IWUGO, show_level, set_level);

static struct attribute *pnx_sysfs_entries[2] = {
	&dev_attr_level.attr,
	NULL
};

static const struct attribute_group pnx_attr_group = {
	.name	= NULL,			/* put in device directory */
	.attrs = pnx_sysfs_entries,
};

static int __devinit pnx_bat_probe(struct platform_device *dev)
{
	int ret = 0;

//    if (!machine_is_pnx())
//        return -ENODEV;

	printk(KERN_DEBUG "pnx_bat_probe\n");
//	mutex_init(&work_lock);

	INIT_WORK(&bat_work, pnx_bat_work);

/// +ACER_AUx PenhoYu, initial the data that is used by ACER_CHG_BAT_FUN
#ifdef ACER_CHG_BAT_FUN
	bibatchg_registerbattery(&pnx_batt_level);

	INIT_WORK(&g_battpoll_work, battery_pooling_work_proc);
	setup_timer(&g_battpoll_timer, battery_polling_timer, 0);
	mod_timer(&g_battpoll_timer,
	          jiffies + msecs_to_jiffies(QBAT_INIT_TIMER));

	INIT_WORK(&g_otp_work, otp_work_proc);
	setup_timer(&g_otp_timer, otp_timer_func, 0);
	// for fix A41.B-674, A21.B-358
	mod_timer(&g_otp_timer,
	          jiffies + msecs_to_jiffies(QBID_INIT_TIMER));

	g_bat_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	g_bat_early_suspend.suspend = pnxbat_early_suspend;
	g_bat_early_suspend.resume = pnxbat_late_resume;
	register_early_suspend(&g_bat_early_suspend);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, initial the data that is used by ACER_CHG_BAT_FUN

//	drvchg_registerOnlineFct(update_charger);	// move to below for fix SYScs45197

	ret = power_supply_register(&dev->dev, &bat_ps);
	if (ret) {
		printk(KERN_ERR "failed to register %s power supply\n",
		bat_ps.name);
	}

	ret = power_supply_register(&dev->dev, &pnx_psy_ac);
	if (ret) {
		printk(KERN_ERR "failed to register %s power supply\n",
		pnx_psy_ac.name);
	}

	ret = power_supply_register(&dev->dev, &pnx_psy_usb);
	if (ret) {
		printk(KERN_ERR "failed to register %s power supply\n",
			pnx_psy_usb.name);
	}

	drvchg_registerOnlineFct(update_charger);	// move from above for fix SYScs45197

/// +ACER_AUx PenhoYu, 
#ifdef ACER_CHG_BAT_FUN
	if ((ac_status == -1) || (usb_status == -1))
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, 
	schedule_work(&bat_work);

	ret = sysfs_create_group(&dev->dev.kobj, &pnx_attr_group);
	if (ret) {
		dev_err(&dev->dev, "error creating sysfs group\n");
	}

	return ret;
}

static int __devexit pnx_bat_remove(struct platform_device *dev)
{
	printk(KERN_DEBUG "pnx_bat_remove\n");
	sysfs_remove_group(&dev->dev.kobj, &pnx_attr_group);
	power_supply_unregister(&pnx_psy_usb);
	power_supply_unregister(&pnx_psy_ac);
	power_supply_unregister(&bat_ps);
/// +ACER_AUx PenhoYu, release the data that is used by ACER_CHG_BAT_FUN
#ifdef ACER_CHG_BAT_FUN
	unregister_early_suspend(&g_bat_early_suspend);
	del_timer_sync(&g_otp_timer);
	del_timer_sync(&g_battpoll_timer);
	bibatchg_unregisterbattery();
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, release the data that is used by ACER_CHG_BAT_FUN
	return 0;
}

static struct platform_driver pnx_bat_driver = {
	.driver.name	= "pnx-battery",
	.driver.owner	= THIS_MODULE,
	.probe		= pnx_bat_probe,
	.remove		= __devexit_p(pnx_bat_remove),
	.suspend	= pnx_bat_suspend,
	.resume		= pnx_bat_resume,
};

static int __init pnx_bat_init(void)
{
	int ret;
	printk(KERN_DEBUG "pnx_bat_init\n");
	
	ret=platform_driver_register(&pnx_bat_driver);

	if (ret != 0)
	{
		printk(KERN_DEBUG "platform_driver_register failed !\n");
		return ret;
	}

/// +ACER_AUx PenhoYu
#ifndef ACER_CHG_BAT_FUN
	battmon_registerLevelFct(pnx_bat_level);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu
	
	return ret;
}

static void __exit pnx_bat_exit(void)
{
	printk(KERN_DEBUG "pnx_bat_exit\n");

	battmon_unregisterLevelFct();
	platform_driver_unregister(&pnx_bat_driver);
}

module_init(pnx_bat_init);
module_exit(pnx_bat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Olivier Clergeaud <olivier.clergeaud@stericsson.com>");
MODULE_DESCRIPTION("PNX battery driver");

/// +ACER_AUx PenhoYu, Include the acer_battery function
#ifdef ACER_CHG_BAT_FUN
#include "acer_battery.c"
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, Include the acer_battery function
