/*
 * linux/drivers/video/pnx/busses/lcdctrl.c
 *
 * LCD ctrl driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <video/pnx/lcdbus.h>
#include <video/pnx/lcdctrl.h>

struct bus_type lcdctrl_bustype = {
	.name = "lcdctrl",
};

int lcdctrl_device_register(struct lcdctrl_device *ldev)
{
	struct device *dev = &ldev->dev;

	pr_debug("%s()\n", __FUNCTION__);

	dev->bus = &lcdctrl_bustype;
	return device_register(dev);
}
EXPORT_SYMBOL(lcdctrl_device_register);

void lcdctrl_device_unregister(struct lcdctrl_device *ldev)
{
	struct device *dev = &ldev->dev;

	pr_debug("%s()\n", __FUNCTION__);
	device_unregister(dev);
}
EXPORT_SYMBOL(lcdctrl_device_unregister);

/* ------------------------------------------------------------------------- */

static int __init lcdctrl_init(void)
{
	pr_debug("%s()\n", __FUNCTION__);
	return bus_register(&lcdctrl_bustype);
}

subsys_initcall(lcdctrl_init);

static void __exit lcdctrl_exit(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	bus_unregister(&lcdctrl_bustype);
}

module_exit(lcdctrl_exit);

MODULE_AUTHOR("Dirk Hoerner, Faouaz TENOUTIT, ST-Ericsson");
MODULE_DESCRIPTION("LCDCTRL infrastructure");
MODULE_LICENSE("GPL");
