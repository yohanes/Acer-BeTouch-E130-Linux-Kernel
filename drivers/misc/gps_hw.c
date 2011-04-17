/*
 * drivers/misc/gps_hw.c
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Thomas VIVET  for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/gps_hw.h>
#include <linux/gns7560.h>
#include <mach/gpio.h>

#if 1
#define DBG(format, arg...) printk(KERN_ALERT \
	"%s - " format "\n", __func__, ## arg)
#else
#define DBG(fmt...) do { } while (0)
#endif

#define MODULE_NAME  "gps_hw"
#define GPS_HW_MINOR      223

static unsigned gps_pwr;
static unsigned gps_rst;


#define GPS_PWR_GPIO      gps_pwr
#define GPS_RST_GPIO      gps_rst;

static int gps_hw_ioctl(struct inode *inode,
			struct file *file,
			unsigned int cmd,
			unsigned long arg)
{
	int ret = 0;

	switch (cmd) {

	case GPS_POWER_ON:
		{
			DBG("GPS_POWER_ON");
			__gpio_set_value(gps_pwr, 1);
			break;
		}

	case GPS_POWER_OFF:
		{
			DBG("GPS_POWER_OFF");
			__gpio_set_value(gps_pwr, 0);
			break;
		}

	case GPS_RESET_ON:
		{
			DBG("GPS_RESET_ON");
			__gpio_set_value(gps_rst, 1);
			break;
		}

	case GPS_RESET_OFF:
		{
			DBG("GPS_RESET_OFF");
			__gpio_set_value(gps_rst, 0);
			break;
		}

	default:
		DBG("Unknown ioctl %d", cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}


static int gps_hw_open(struct inode *inode, struct file *file)
{
	return 0;
}


static ssize_t gps_hw_read(struct file *fd,
		char __user *buff, size_t size, loff_t *myLoff)
{
	return 0;
}


static ssize_t gps_hw_write(struct file *fd,
		const char __user *buff, size_t size, loff_t *myLoff)
{
	return 0;
}

const static struct file_operations gps_hw_fops = {
	.owner    = THIS_MODULE,
	.ioctl    = gps_hw_ioctl,
	.open     = gps_hw_open,
	.read     = gps_hw_read,
	.write    = gps_hw_write,
};
static struct miscdevice gps_hw_dev = {
	.minor    = GPS_HW_MINOR,
	.name     = "gps_hw",
	.fops     = &gps_hw_fops,
};

static int gps_hw_probe(struct platform_device *pdev)
{
	int rv;
	struct gps_pdata *pdata = (struct gps_pdata *)  pdev->dev.platform_data;
	gps_rst = pdata->gpo_rst;
	gps_pwr = pdata->gpo_pwr;
	rv = misc_register(&gps_hw_dev);
	if (rv)
		goto end;
	/* Setup GPS_RST_GPIO */
	rv = gpio_request(gps_rst, pdev->name);
	if (rv) {
		printk(KERN_ALERT "ERROR %d: Request GPS_RST_GPIO %d\n",
				rv, gps_rst);
		goto end;
	}
	gpio_direction_output(gps_rst, 1);

	/* Setup GPS_PWR_GPIO */
	rv = gpio_request(gps_pwr, pdev->name);
	if (rv) {
		printk(KERN_ALERT "ERROR %d: Request GPS_PWR_GPIO %d\n", rv,
				gps_pwr);
		goto end;
	}
	gpio_direction_output(gps_pwr, 1);
end:
	return rv;

}

static int gps_hw_remove(struct platform_device *pdev)
{
	int rv = 0;
	rv = misc_deregister(&gps_hw_dev);
	DBG("misc_deregister=%d", rv);
	gpio_free(gps_pwr);
	gpio_free(gps_rst);
	printk(KERN_INFO "GPS Driver Exit\n");
	return rv;

}


static struct platform_driver gps_hw_driver = {
	.probe = gps_hw_probe,
	.remove = gps_hw_remove,
	.driver = {
		.name = "gps_hw",
		.owner = THIS_MODULE,
	},
};


static int gps_hw_init(void)
{
	platform_driver_register(&gps_hw_driver);
	return 0;
}


static void gps_hw_exit(void)
{
	platform_driver_unregister(&gps_hw_driver);
}


module_init(gps_hw_init);
module_exit(gps_hw_exit);

MODULE_AUTHOR("STE");
MODULE_LICENSE("GPL");
