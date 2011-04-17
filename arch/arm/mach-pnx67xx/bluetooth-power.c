#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

static int bluetooth_power_state;
static int (*power_control)(int enable);

static DEFINE_SPINLOCK(bt_power_lock);

static int bluetooth_power_param_set(const char *val, struct kernel_param *kp)
{
	int ret;

	printk(KERN_DEBUG
		"%s: previous power_state=%d\n",
		__func__, bluetooth_power_state);

	/* lock change of state and reference */
	spin_lock(&bt_power_lock);
	ret = param_set_bool(val, kp);
	if (power_control) {
		if (!ret)
			ret = (*power_control)(bluetooth_power_state);
		else
			printk(KERN_ERR "%s param set bool failed (%d)\n",
					__func__, ret);
	} else {
		printk(KERN_INFO
			"%s: deferring power switch until probe\n",
			__func__);
	}
	spin_unlock(&bt_power_lock);
	printk(KERN_INFO
		"%s: current power_state=%d\n",
		__func__, bluetooth_power_state);
	return ret;
}

module_param_call(power, bluetooth_power_param_set, param_get_bool,
		  &bluetooth_power_state, S_IRUGO | S_IWUSR);

static int __init_or_module bt_power_probe(struct platform_device *pdev)
{
	int ret = 0;

	printk(KERN_DEBUG "%s\n", __func__);

	if (!pdev->dev.platform_data) {
		printk(KERN_ERR "%s: platform data not initialized\n",
				__func__);
		return -ENOSYS;
	}

	spin_lock(&bt_power_lock);
	power_control = pdev->dev.platform_data;

	if (bluetooth_power_state) {
		printk(KERN_INFO
			"%s: handling deferred power switch\n",
			__func__);
	}
	ret = (*power_control)(bluetooth_power_state);
	spin_unlock(&bt_power_lock);
	return ret;
}

static int bt_power_remove(struct platform_device *pdev)
{
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);
	if (!power_control) {
		printk(KERN_ERR "%s: power_control function not initialized\n",
				__func__);
		return -ENOSYS;
	}
	spin_lock(&bt_power_lock);
	bluetooth_power_state = 0;
	ret = (*power_control)(bluetooth_power_state);
	power_control = NULL;
	spin_unlock(&bt_power_lock);

	return ret;
}

static struct platform_driver bt_power_driver = {
	.probe = bt_power_probe,
	.remove = bt_power_remove,
	.driver = {
		.name = "bt_power",
		.owner = THIS_MODULE,
	},
};

static int __init_or_module bluetooth_power_init(void)
{
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);
	ret = platform_driver_register(&bt_power_driver);
	return ret;
}
//
static void __exit bluetooth_power_exit(void)
{
	printk(KERN_DEBUG "%s\n", __func__);
	platform_driver_unregister(&bt_power_driver);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ACER_EdLiu");
MODULE_DESCRIPTION("TI Bluetooth power control driver");
MODULE_VERSION("1.00");
MODULE_PARM_DESC(power, "TI Bluetooth power switch (bool): 0,1=off,on");

module_init(bluetooth_power_init);
module_exit(bluetooth_power_exit);
