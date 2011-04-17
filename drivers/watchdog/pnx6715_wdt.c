/*
 *  drivers/watchdog/pnx6715_wdt.c
 *
 *  PNX6715 watchdog driver
 *
 *  Author:     Johan Palsson
 *  Copyright:  ST-Ericsson (c) 2010
 *
 *  This WDT driver includes two watchdogs.
 *  The MODEM watchdog will increment a shared variable using a timer 
 *  that will be checked by MODEM.
 *  The Software watchdog will be kicked by a userspace daemon.
 *  Both watchdogs will trigger a blocking defence if they expire
 *
 *  Based on softdog.c by Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#include <linux/module.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>

#include <nk/xos_area.h>

#ifdef CONFIG_PNX6715_WATCHDOG_TEST
/* Sysfs include */
#include <linux/sysfs.h>
#include <linux/kobject.h>

/* Interrupt include */
#include <asm/mach/irq.h>
#endif

#define PNXWDT "PNX Watchdog: "
#define LINUX_WDT_AREA_NAME "PNX6715_WDT"
#define MODEM_WDT_TIMEOUT		10
#define SOFT_WDT_TIMEOUT	60

static int nowayout = WATCHDOG_NOWAYOUT;

struct watchdog_data {
	volatile unsigned long watchdog;
	volatile unsigned long timeout;
};

struct modem_watchdog_data {
	xos_area_handle_t shared;
	struct  watchdog_data* base_adr;
};

static unsigned long save_watchdog;
static struct platform_driver pnx6715_wdt_driver;


static void modem_wdt_keepalive(unsigned long data);
static void soft_wdt_fire(unsigned long data);

/* Watchdog monitored by MODEM */
static struct modem_watchdog_data modem_watchdog_data;
static struct timer_list modem_timer = 
	TIMER_INITIALIZER(modem_wdt_keepalive, 0, 0);
static unsigned long modem_timeout = MODEM_WDT_TIMEOUT;
static unsigned long modem_kick = 1;
static unsigned int modem_refresh_active = 1;

/* Software watchdog for user space */
static int soft_timeout = SOFT_WDT_TIMEOUT;
static struct timer_list wdt_soft_timer = 
	TIMER_INITIALIZER(soft_wdt_fire, 0, 0);
static unsigned long driver_open, orphan_timer;
static char expect_close;
static int soft_ignore_wdt = 0;


/**************************************/
/* Start modem watchdog functionality */
/**************************************/
static int modem_wdt_init_data(void)
{
	int ret = 0;

	modem_watchdog_data.shared = xos_area_connect(LINUX_WDT_AREA_NAME, 
			sizeof( struct watchdog_data));
	if (modem_watchdog_data.shared) {
		modem_watchdog_data.base_adr = xos_area_ptr(modem_watchdog_data.shared);
		modem_watchdog_data.base_adr->watchdog = modem_kick++;
		modem_watchdog_data.base_adr->timeout = modem_timeout;
	} else {
		printk(KERN_CRIT PNXWDT "failed to connect to xos %s area\n", 
				LINUX_WDT_AREA_NAME);
		ret = -1; /* failed */
	}
	return ret;
}

static void modem_wdt_keepalive(unsigned long data)
{
	if (modem_watchdog_data.shared && modem_refresh_active) {
		modem_watchdog_data.base_adr->watchdog = modem_kick++;
		mod_timer(&modem_timer, round_jiffies(jiffies + modem_timeout * HZ));
	}
}
/************************************/
/* End modem watchdog functionality */
/************************************/


/********************/
/* sysfs definition */
/********************/
#ifdef CONFIG_PNX6715_WATCHDOG_TEST
extern struct kobject *pnx_kobj;

static void test_wdt_print_testinfo(void)
{
	printk(PNXWDT "Test cases:\n");
	printk(PNXWDT "Write 1 to stop updating modem watchdog\n");
	printk(PNXWDT "Write 2 to turn off modem and Linux interrupts\n");
	printk(PNXWDT "Write 3 to turn off Linux interrupts\n");
	printk(PNXWDT "Write 4 to enter loop with interrupts on\n");
}
static ssize_t test_wdt_show(struct kobject *kobj,
	 struct kobj_attribute *attr, char *buf)
{
	int len = sprintf(buf,"Use the following format for the tests\n");
	test_wdt_print_testinfo();
	return len;
}

static ssize_t test_wdt_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	if (!strncasecmp(buf, "1", 1)) {
		modem_refresh_active = 0;
	} else if (!strncasecmp(buf, "2", 1)) {
		unsigned long flags;
		hw_raw_local_irq_save(flags);
		while(1);
	} else if (!strncasecmp(buf, "3", 1)) {
		local_irq_disable();
		while(1);
	} else if (!strncasecmp(buf, "4", 1)) {
		while(1);
	}else {
		test_wdt_print_testinfo();
	}
	return count;
}

#define PNX_ATTR_RW(_name) \
static struct kobj_attribute _name##_attr = \
	__ATTR(_name, 0644, _name##_show, _name##_store)

PNX_ATTR_RW(test_wdt);

static struct attribute * attrs[] = {
		&test_wdt_attr.attr,
		NULL,
};

static struct attribute_group test_wdt_attr_group = {
	.attrs = attrs,
	.name = "test_wdt",
};

static int __init test_wdt_sysfs_init(void)
{
	return sysfs_create_group(pnx_kobj,&test_wdt_attr_group);
}

#endif
/************************/
/* end sysfs definition */
/************************/


/*****************************************/
/* Start software watchdog functionality */
/*****************************************/

/*
 *	If the timer expires..
 */
static void soft_wdt_fire(unsigned long data)
{
	if (test_and_clear_bit(0, &orphan_timer))
		module_put(THIS_MODULE);
	
	if (soft_ignore_wdt)
		printk(KERN_CRIT PNXWDT "Soft watchdog triggered - ignored.\n");
	else 		
		panic("Software watchdog triggered\n");
}

/*
 *	Softdog operations
 */
static int soft_wdt_keepalive(void)
{
	mod_timer(&wdt_soft_timer, round_jiffies(jiffies + soft_timeout * HZ));
	return 0;
}

static int  soft_wdt_stop(void)
{
	printk(KERN_DEBUG PNXWDT "soft_wdt_stop\n");
	del_timer(&wdt_soft_timer);
	return 0;
}

static int soft_wdt_set_timeout(int time)
{
	printk(KERN_DEBUG PNXWDT "soft_wdt_set_timeout\n");
	if ((time < 0x01) || (time > 0xFF)) /* Between 1s and 255s */
		return -EINVAL;

	soft_timeout = time;
	return 0;
}

/*
 *	/dev/watchdog handling
 */
static int soft_wdt_open(struct inode *inode, struct file *file)
{
	printk(KERN_DEBUG PNXWDT "soft_wdt_open\n");
	if (test_and_set_bit(0, &driver_open))
		return -EBUSY;
	if (!test_and_clear_bit(0, &orphan_timer))
		__module_get(THIS_MODULE);
	/*
	 *	Activate timer
	 */
	soft_wdt_keepalive();
	return nonseekable_open(inode, file);
}

static int soft_wdt_release(struct inode *inode, struct file *file)
{
	printk(KERN_DEBUG PNXWDT "soft_wdt_release\n");
	/*
	 *	Shut off the timer.
	 * 	Lock it in if it's a module and we set nowayout
	 */
	if (expect_close == 42) {
		soft_wdt_stop();
		module_put(THIS_MODULE);
	} else {
		printk(KERN_CRIT PNXWDT
			"Unexpected close, not stopping watchdog!\n");
		set_bit(0, &orphan_timer);
		soft_wdt_keepalive();
	}
	clear_bit(0, &driver_open);
	expect_close = 0;
	return 0;
}

static ssize_t soft_wdt_write(struct file *file, const char __user *data,
						size_t len, loff_t *ppos)
{
	/*
	 *	Refresh the timer.
	 */
	if (len) {
		if (!nowayout) {
			size_t i;

			/* In case it was set long ago */
			expect_close = 0;

			for (i = 0; i != len; i++) {
				char c;

				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 42;
			}
		}
		soft_wdt_keepalive();
	}
	return len;
}

static long soft_wdt_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	int new_timeout;
	static const struct watchdog_info ident = {
		.options =	WDIOF_SETTIMEOUT |
					WDIOF_KEEPALIVEPING |
					WDIOF_MAGICCLOSE,
		.firmware_version =	0,
		.identity =		"Software Watchdog",
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, &ident, sizeof(ident)) ? -EFAULT : 0;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);
	case WDIOC_KEEPALIVE:
		soft_wdt_keepalive();
		return 0;
	case WDIOC_SETTIMEOUT:
		if (get_user(new_timeout, p))
			return -EFAULT;
		if (soft_wdt_set_timeout(new_timeout))
			return -EINVAL;
		soft_wdt_keepalive();
		/* Fall */
	case WDIOC_GETTIMEOUT:
		return put_user(soft_timeout, p);
	default:
		return -ENOTTY;
	}
}

/*
 *	Notifier for system down
 */
static int soft_wdt_notify_sys(struct notifier_block *this, unsigned long code,
	void *unused)
{
	printk(KERN_DEBUG PNXWDT "soft_wdt_notify_sys\n");

	if (code == SYS_DOWN || code == SYS_HALT)
		/* Turn the watchdog off */
		soft_wdt_stop();
	return NOTIFY_DONE;
}


static int pnx6715_wdt_suspend(struct platform_device *pdev,
				      pm_message_t state)
{
        save_watchdog =	modem_watchdog_data.base_adr->watchdog;
	modem_watchdog_data.base_adr->watchdog = 0;
	return 0;
}


int pnx6715_wdt_resume(struct platform_device *pdev)
{
	modem_watchdog_data.base_adr->watchdog = save_watchdog;
	return 0;
}
/*
 *	Kernel Interfaces
 */
static const struct file_operations soft_wdt_fops = {
	.owner			= THIS_MODULE,
	.llseek			= no_llseek,
	.write			= soft_wdt_write,
	.unlocked_ioctl	= soft_wdt_ioctl,
	.open			= soft_wdt_open,
	.release		= soft_wdt_release,
};

static struct notifier_block soft_wdt_notifier = {
	.notifier_call	= soft_wdt_notify_sys,
};
/***************************************/
/* End software watchdog functionality */
/***************************************/

static struct miscdevice wdt_miscdev = {
	.minor	= WATCHDOG_MINOR,
	.name	= "watchdog",
	.fops	= &soft_wdt_fops,
};


static struct platform_driver pnx6715_wdt_driver = {
	.suspend	= pnx6715_wdt_suspend,
	.resume		= pnx6715_wdt_resume,
	.driver		= {
		.owner = THIS_MODULE,
		.name = "watchdog",
	},
};

static struct platform_device pnx6715_wdt_device = {
	.name = "watchdog",
};

static int __init pnx_wdt_init(void)
{
	int ret;

	ret = register_reboot_notifier(&soft_wdt_notifier);
	if (ret) {
		printk(KERN_CRIT PNXWDT
			"cannot register reboot notifier (err=%d)\n", ret);
		goto err_out;
	}




	ret = misc_register(&wdt_miscdev);
	if (ret < 0) { 
		printk(KERN_CRIT PNXWDT
			"cannot register miscdev on minor=%d (err=%d)\n",
						WATCHDOG_MINOR, ret);
		goto err_out_reboot;
	}
	

	ret = modem_wdt_init_data();
	if (ret < 0) 
		goto err_out_init;
	
	ret = platform_device_register(&pnx6715_wdt_device);
	if (ret)
		goto err_out;

	ret = platform_driver_register(&pnx6715_wdt_driver);
	if (ret)
		goto err_out;

	
	mod_timer(&modem_timer, round_jiffies(jiffies + modem_timeout * HZ));

#ifdef CONFIG_PNX6715_WATCHDOG_TEST
	test_wdt_sysfs_init();
#endif
	return 0;

err_out_init:
	misc_deregister(&wdt_miscdev);
err_out_reboot:
	unregister_reboot_notifier(&soft_wdt_notifier);
err_out:
	printk(KERN_DEBUG PNXWDT "end pnx_wdt_probe\n");
	return ret;
}

static void __exit pnx_wdt_exit(void)
{
	if (timer_pending(&modem_timer))
		del_timer(&modem_timer);

	misc_deregister(&wdt_miscdev);
	unregister_reboot_notifier(&soft_wdt_notifier);
}

module_init(pnx_wdt_init);
module_exit(pnx_wdt_exit);

MODULE_AUTHOR("Johan Palsson <johan.palsson@stericsson.com>");
MODULE_DESCRIPTION("Driver for the PNX watchdog");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);

