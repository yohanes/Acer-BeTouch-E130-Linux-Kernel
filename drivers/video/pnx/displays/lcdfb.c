/*
 * linux/drivers/video/pnx/displays/lcdfb.c
 *
 * lcdfb driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <video/pnx/lcdctrl.h>

struct bus_type lcdfb_bustype = {
	.name = "lcdfb",
};


/*
 * lcdfb_device_register
 */
int lcdfb_device_register(struct lcdfb_device *ldev)
{
	struct device *dev = &ldev->dev;

	pr_debug("%s()\n", __FUNCTION__);

	dev->bus = &lcdfb_bustype;
	return device_register(dev);
}
EXPORT_SYMBOL(lcdfb_device_register);


/*
 * lcdfb_device_unregister
 */
void lcdfb_device_unregister(struct lcdfb_device *ldev)
{
	struct device *dev = &ldev->dev;

	pr_debug("%s()\n", __FUNCTION__);
	device_unregister(dev);
}
EXPORT_SYMBOL(lcdfb_device_unregister);

/* ------------------------------------------------------------------------- */

static int __init lcdfb_init(void)
{
	pr_debug("%s()\n", __FUNCTION__);
	return bus_register(&lcdfb_bustype);
}

subsys_initcall(lcdfb_init);

static void __exit lcdfb_exit(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	bus_unregister(&lcdfb_bustype);
}

module_exit(lcdfb_exit);

MODULE_AUTHOR("Dirk HOERNER, Faouaz TENOUTIT, ST-Ericsson");
MODULE_DESCRIPTION("LCDFB infrastructure");
MODULE_LICENSE("GPL");
