/* include/asm/mach-msm/msm_vibrator.c
 *
 * Copyright (C) 2009 ACER, Inc.
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2007 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/* ACER Jen chang, 2009/06/28, IssueKeys:AU2.B-38, Implement vibrator device driver for HAL interface function { */
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/err.h>

#include <linux/vibrator.h>

static ssize_t show_vibrator_timer(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct vibrator_device *vib = to_vibrator_device(dev);

	if (hrtimer_active(&vib->props.vibrator_timer))
	{
		ktime_t r = hrtimer_get_remaining(&vib->props.vibrator_timer);

		return sprintf(buf, "vibrator timer: %d\n", (r.tv.sec * 1000 + r.tv.nsec / 1000000));
	}
	else
		return sprintf(buf, "vibrator timer: %d\n", 0);
}

static ssize_t set_vibrator_onoff(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = -ENXIO;
	char *endp;
	unsigned long vib_timer;
	size_t size = endp - buf;
	struct vibrator_device *vib = to_vibrator_device(dev);

	//poor test 0624
	//printk("---set_vibrator_onoff--1---\n");
	//poor test 0624

	vib_timer = simple_strtoul(buf, &endp, 0);

	//poor test 0624
	//printk("---set_vibrator_onoff--vib_timer=%d---\n", vib_timer);
	//poor test 0624
	
	mutex_lock(&vib->ops_lock);
	if (vib->ops) {
	//poor test 0624
	//printk("---set_vibrator_onoff--3---\n");
	//poor test 0624
		pr_debug("vibrator: set vibrator: %d\n", vib_timer);
		vib->ops->set_onoff(vib, vib_timer);
	//poor test 0624
	//printk("---set_vibrator_onoff--4---\n");
	//poor test 0624	
		rc = count;
	}
	mutex_unlock(&vib->ops_lock);

	return rc;
}

static struct class *vibrator_class;

static void vibrator_device_release(struct device *dev)
{
	struct vibrator_device *vib = to_vibrator_device(dev);
	kfree(vib);
}

static struct device_attribute vibrator_device_attributes[] = {
	__ATTR(vibtimer, S_IRUGO, show_vibrator_timer, NULL),
	__ATTR(vibonoff, S_IWUSR, NULL, set_vibrator_onoff),
	__ATTR_NULL,
};

/**
 * backlight_device_register - create and register a new object of
 *   backlight_device class.
 * @name: the name of the new object(must be the same as the name of the
 *   respective framebuffer device).
 * @parent: a pointer to the parent device
 * @devdata: an optional pointer to be stored for private driver use. The
 *   methods may retrieve it by using bl_get_data(bd).
 * @ops: the backlight operations structure.
 *
 * Creates and registers new backlight device. Returns either an
 * ERR_PTR() or a pointer to the newly allocated device.
 */
struct vibrator_device *vibrator_device_register(const char *name,
		struct device *parent, void *devdata, struct vibrator_ops *ops)
{
	struct vibrator_device *new_vib;
	int rc;

	pr_debug("vibrator_device_register: name=%s\n", name);

	new_vib = kzalloc(sizeof(struct vibrator_device), GFP_KERNEL);
	if (!new_vib)
		return ERR_PTR(-ENOMEM);

	mutex_init(&new_vib->ops_lock);

	new_vib->dev.class = vibrator_class;
	new_vib->dev.parent = parent;
	new_vib->dev.release = vibrator_device_release;
	strlcpy(new_vib->dev.bus_id, name, BUS_ID_SIZE);
	dev_set_drvdata(&new_vib->dev, devdata);

	rc = device_register(&new_vib->dev);
	if (rc) {
		kfree(new_vib);
		return ERR_PTR(rc);
	}

	new_vib->ops = ops;

	return new_vib;
}
EXPORT_SYMBOL(vibrator_device_register);

/**
 * backlight_device_unregister - unregisters a backlight device object.
 * @bd: the backlight device object to be unregistered and freed.
 *
 * Unregisters a previously registered via backlight_device_register object.
 */
void vibrator_device_unregister(struct vibrator_device *vib)
{
	if (!vib)
		return;

	mutex_lock(&vib->ops_lock);
	vib->ops = NULL;
	mutex_unlock(&vib->ops_lock);

	device_unregister(&vib->dev);
}
EXPORT_SYMBOL(vibrator_device_unregister);

static void __exit vibrator_class_exit(void)
{
	class_destroy(vibrator_class);
}

static int __init vibrator_class_init(void)
{
	vibrator_class = class_create(THIS_MODULE, "vibrator");
	if (IS_ERR(vibrator_class)) {
		printk(KERN_WARNING "Unable to create vibrator class; errno = %ld\n",
				PTR_ERR(vibrator_class));
		return PTR_ERR(vibrator_class);
	}

	vibrator_class->dev_attrs = vibrator_device_attributes;
}

/*
 * if this is compiled into the kernel, we need to ensure that the
 * class is registered before users of the class try to register lcd's
 */
postcore_initcall(vibrator_class_init);
module_exit(vibrator_class_exit);
/* } ACER Jen Chang, 2009/06/28*/

