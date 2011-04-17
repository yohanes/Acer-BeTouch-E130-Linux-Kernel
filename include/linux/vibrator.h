/*
 * Vibrator Lowlevel Control Abstraction
 *
 */
/* ACER Jen chang, 2009/06/28, IssueKeys:AU2.B-38, Implement vibrator device driver for HAL interface function { */
#ifndef _LINUX_VIBRATOR_H
#define _LINUX_VIBRATOR_H

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/notifier.h>


struct vibrator_device;

struct vibrator_ops {
	/* enable the output and set the timer */
	void (*set_onoff)(struct vibrator_device *dev, int timeout);

	/* returns the current number of milliseconds remaining on the timer */
	int	(*get_time)(struct vibrator_device *dev);
};

/* This structure defines all the properties of a backlight */
struct vibrator_properties {

	struct work_struct work_vibrator_on;

	struct work_struct work_vibrator_off;

	struct hrtimer vibrator_timer;

	/* ACER Jen chang, 2009/09/02, IssueKeys:AU4.B-172, Add for recording setting timer from AP { */
	int value;
	/* } ACER Jen Chang, 2009/09/02 */
};



struct vibrator_device {

	const char	*name;

	struct mutex ops_lock;

	struct vibrator_properties props;

	struct vibrator_ops *ops;

	struct device dev;
};

extern struct vibrator_device *vibrator_device_register(const char *name,
	struct device *dev, void *devdata, struct vibrator_ops *ops);
extern void vibrator_device_unregister(struct vibrator_device *bd);

#define to_vibrator_device(obj) container_of(obj, struct vibrator_device, dev)

static inline void * vibrator_get_data(struct vibrator_device *vibrator_dev)
{
	return dev_get_drvdata(&vibrator_dev->dev);
}

#endif
/* } ACER Jen Chang, 2009/06/28*/

