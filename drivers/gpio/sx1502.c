/*	sx1502.c - SEMTECH GPIO expander
 *	
 *	Copyright (C) 2010 Selwyn Chen <SelwynChen@acertdc.com>
 *	Copyright (C) 2010 ACER Co., Ltd.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 */

#include <linux/init.h>
#include <linux/module.h>
//#include <linux/err.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <mach/gpio.h>
#include <linux/irq.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "sx1502.h"

//#define ACER_DEBUG_L1

#ifdef ACER_DEBUG_L1
#define dbg(fmt, args...) printk("%-25s: " fmt, __func__, ## args)
#else
#define dbg(fmt, args...) do{}while(0)
#endif

u8 JB_SENSE = 3;
static u8 jb_l = 0;
static u8 jb_r = 0;
static u8 jb_u = 0;
static u8 jb_d = 0;
static u8 jb_pressed = 0;


/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
static unsigned int short normal_i2c[] = { 0x20, I2C_CLIENT_END };
I2C_CLIENT_INSMOD_1(sx1502);
#endif
/* } ACER Jen chang, 2010/03/25 */


//driver data structure
struct sx1502_data
{
/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	struct i2c_client * client;
#else
	struct i2c_client client;
#endif
/* } ACER Jen chang, 2010/03/25 */
	struct input_dev *idev;
	struct work_struct work;
	struct workqueue_struct *sx1502_workqueue;
	unsigned int wlan_on;
	unsigned int flags;
	/* ACER Jen chang, 2010/03/25, to improve gpio coding architecture { */
	unsigned int io_gpio;
	unsigned int io_irq;
	/* } ACER Jen chang, 2010/03/25 */
};

static struct i2c_driver sx1502_driver;
static struct platform_device *sx1502_pdev;
struct sx1502_data *sx1502_global;

static inline int sx1502_read(struct sx1502_data *data, u8 reg)
{
	u32 ret;

/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	ret = i2c_smbus_read_byte_data(data->client, reg);
#else
	ret = i2c_smbus_read_byte_data(&data->client, reg);
#endif
/* } ACER Jen chang, 2010/03/25 */

	return ret;
}

static inline int sx1502_write(struct sx1502_data *data, u8 reg, u8 value)
{
	u32 ret;	

/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	ret = i2c_smbus_write_byte_data(data->client, reg, value);
#else
	ret = i2c_smbus_write_byte_data(&data->client, reg, value);
#endif
/* } ACER Jen chang, 2010/03/25 */
	
	return ret;
}

static void jb_clear(void)
{
	jb_l=0;
	jb_r=0;
	jb_u=0;
	jb_d=0;
}

void hs_power_switch(int power_on)
{
	u8 val;

	val = sx1502_read(sx1502_global, SX1502_REGDATA);

	if(power_on)
	{
		//pull high
		val |= HPH_PWD_N;
		sx1502_write(sx1502_global, SX1502_REGDATA, val);
	}
	else
	{
		//pull low
		val &= ~HPH_PWD_N;
		sx1502_write(sx1502_global, SX1502_REGDATA, val);
	}
}

void wlan_power_switch(int power_on)
{
	u8 val;

	val = sx1502_read(sx1502_global, SX1502_REGDATA);

	if(power_on)
	{
		//pull high
		val |= WLAN_POWERON;
		sx1502_write(sx1502_global, SX1502_REGDATA, val);
		sx1502_global->wlan_on = 1;
	}
	else
	{
		//pull low
		val &= ~WLAN_POWERON;
		sx1502_write(sx1502_global, SX1502_REGDATA, val);
		sx1502_global->wlan_on = 0;
	}
}

void jb_power_switch(int power_on)
{
	u8 val;

	val = sx1502_read(sx1502_global, SX1502_REGDATA);

	if(power_on)
	{
		//pull low
		val &= ~JB_POWER_CTRL;
		sx1502_write(sx1502_global, SX1502_REGDATA, val);
	}
	else
	{
		//pull high
		val |= JB_POWER_CTRL;
		sx1502_write(sx1502_global, SX1502_REGDATA, val);
	}
}

static ssize_t show_wlan_power(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sx1502_data *data = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "wlan_power: %d\n", sx1502_global->wlan_on);
}

static ssize_t set_wlan_power(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int wlan_status = simple_strtoul(buf, NULL, 0);
	struct sx1502_data *data = i2c_get_clientdata(to_i2c_client(dev));

	wlan_power_switch(wlan_status);
	sx1502_global->wlan_on = wlan_status;
	dbg("sx1502 debug: wlan power is set to %d\n", wlan_status);

	return count;
}

static ssize_t show_jb_counter(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", JB_SENSE);
}

static ssize_t set_jb_counter(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int jb_newcount = simple_strtoul(buf, NULL, 0);

	if(jb_newcount > 10)
	{
		jb_newcount = 10;
	}
	else if(jb_newcount < 1)
	{
		jb_newcount = 1;
	}

	JB_SENSE = jb_newcount;
	jb_clear();
	dbg("sx1502 debug: jogball counter is set to %d\n", JB_SENSE);

	return count;
}

static DEVICE_ATTR(wlan_enable, S_IWUSR | S_IRUGO, show_wlan_power, set_wlan_power);
static DEVICE_ATTR(jb_counter, S_IWUSR | S_IRUGO, show_jb_counter, set_jb_counter);

static struct attribute *sx1502_sysfs_entries[] = {
	&dev_attr_wlan_enable.attr,
	&dev_attr_jb_counter.attr,
	NULL
};

static const struct attribute_group sx1502_attr_group = {
	.name	= "sx1502",
	.attrs = sx1502_sysfs_entries,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sx1502_early_suspend(struct early_suspend *pes)
{
	u8 val;
	dbg("Selwyn debug: sx1502_early_suspend is called!\n");
	disable_irq(sx1502_global->io_irq);
	jb_power_switch(0);

	val = sx1502_read(sx1502_global, SX1502_REGDATA);
	val &= 0xE0;

	//for power saving
	sx1502_write(sx1502_global, SX1502_REGPULLUP, 0x00);
	sx1502_write(sx1502_global, SX1502_REGDIR, 0x00);
	sx1502_write(sx1502_global, SX1502_REGDATA, val);
}

static void sx1502_late_resume(struct early_suspend *pes)
{
	dbg("Selwyn debug: sx1502_late_resume is called!\n");
	sx1502_write(sx1502_global, SX1502_REGPULLUP, 0x10);
	sx1502_write(sx1502_global, SX1502_REGDIR, 0x1F);
	sx1502_write(sx1502_global, SX1502_REGINTSOURCE, 0xFF);

	jb_power_switch(1);
	sx1502_write(sx1502_global, SX1502_REGINTSOURCE, 0xFF);
	enable_irq(sx1502_global->io_irq);
}

static struct early_suspend sx1502_earlys = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 5,
	.suspend = sx1502_early_suspend,
	.resume = sx1502_late_resume,
};
#endif

static void sx1502_work(struct work_struct *work)
{
	u8 EVENT, val;
	struct sx1502_data *data = container_of(work, struct sx1502_data, work);

	dbg("sx1502_work is called!\n");

	//check which interrupt event occur
	EVENT = sx1502_read(data, SX1502_REGEVENTSTATUS);
	dbg("sx1502 event = %d!!!\n", EVENT);

	if((EVENT & JOG_BALL_L) && (!jb_pressed))
	{
		jb_l++;
		if(jb_l > JB_SENSE)
		{
			input_report_key( data->idev, KEY_LEFT, 1 );
			input_report_key( data->idev, KEY_LEFT, 0 );
			jb_clear();
		}
		dbg("JOG_BALL_L is called!\n");
	}

	if((EVENT & JOG_BALL_R) && (!jb_pressed))
	{
		jb_r++;
		if(jb_r > JB_SENSE)
		{
			input_report_key( data->idev, KEY_RIGHT, 1 );
			input_report_key( data->idev, KEY_RIGHT, 0 );
			jb_clear();
		}
		dbg("JOG_BALL_R is called!\n");
	}

	if((EVENT & JOG_BALL_U) && (!jb_pressed))
	{
		jb_u++;
		if(jb_u > JB_SENSE)
		{
			input_report_key( data->idev, KEY_UP, 1 );
			input_report_key( data->idev, KEY_UP, 0 );
			jb_clear();
		}
		dbg("JOG_BALL_U is called!\n");
	}

	if((EVENT & JOG_BALL_D) && (!jb_pressed))
	{
		jb_d++;
		if(jb_d > JB_SENSE)
		{
			input_report_key( data->idev, KEY_DOWN, 1 );
			input_report_key( data->idev, KEY_DOWN, 0 );
			jb_clear();
		}
		dbg("JOG_BALL_D is called!\n");
	}

	if(EVENT & JOG_BALL_S)
	{
		val = sx1502_read(sx1502_global, SX1502_REGDATA);
		if(val & JOG_BALL_S)
		{
			input_report_key( data->idev, KEY_AGAIN, 0 );
			jb_pressed = 0;
		}
		else
		{
			input_report_key( data->idev, KEY_AGAIN, 1 );
			jb_pressed = 1;
		}
		jb_clear();
	}

	//reset interrupt pin and clear corresponding bits status
	sx1502_write(data, SX1502_REGINTSOURCE, 0xFF);
}


static int sx1502_irq_handler(int irq, void *dev_id)
{
	dbg("sx1502_irq_handler is called!\n");

	disable_irq(irq);
	queue_work(sx1502_global->sx1502_workqueue, &sx1502_global->work);
	enable_irq(irq);

	return IRQ_HANDLED;
}

static int sx1502_register_driver(struct sx1502_data *data)
{
	int err = 0;
	int pin;

	//Prepare for input device
	data->idev = input_allocate_device();
	if (!data->idev)
		goto exit;

	data->idev->name = "sx1502";
	//data->idev->phys = "input1";
	data->idev->id.bustype = BUS_I2C;
/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	data->idev->dev.parent = &(data->client->dev);
#else
	data->idev->dev.parent = &((data->client).dev);
#endif
/* } ACER Jen chang, 2010/03/25 */

	set_bit(EV_KEY, data->idev->evbit);
	set_bit(KEY_UP, data->idev->keybit);
	set_bit(KEY_DOWN, data->idev->keybit);
	set_bit(KEY_LEFT, data->idev->keybit);
	set_bit(KEY_RIGHT, data->idev->keybit);
	set_bit(KEY_AGAIN, data->idev->keybit);

	//Register linux input device
	err = input_register_device(data->idev);
	if (err < 0)
	{
		printk("Unable to register sx1502 input device\n");
		goto exit;
	}

	/* ACER Jen chang, 2010/03/25, to improve gpio coding architecture { */
	data->io_irq = platform_get_irq(sx1502_pdev, 0);
	data->io_gpio = EXTINT_TO_GPIO(data->io_irq);

	err = pnx_gpio_request(data->io_gpio);
	if( err )
	{
		printk("sx1502: Can't get GPIO_A12 for keypad led\n");
		goto exit;
	}

	pnx_gpio_set_mode(data->io_gpio, GPIO_MODE_MUX0);
	pnx_gpio_set_direction(data->io_gpio, GPIO_DIR_INPUT);

	pin = pnx_gpio_read_pin(data->io_gpio);
	dbg("sx1502: initial GPIO_A12 value is %d!\n", pin);
	//set_irq_type(IRQ_EXTINT(6), (pin==0)?IRQ_TYPE_EDGE_RISING:IRQ_TYPE_EDGE_FALLING);	
	set_irq_type(data->io_irq, IRQ_TYPE_EDGE_FALLING);	

	err = request_irq(data->io_irq, &sx1502_irq_handler, 0, "sx1502_interrupt", NULL);
	if(err < 0)
		goto exit;
	/* } ACER Jen chang, 2010/03/25 */

exit:
	return err;
}

static void sx1502_init_registers(struct sx1502_data *data)
{	
	u8 jb_pin, val;

	dbg("entering sx1502_init_registers\n");

	jb_pin = sx1502_read(data, SX1502_REGDATA);
	//printk("sx1502_REGDATA val = 0x%x\n", jb_pin);
	
	sx1502_write(data, SX1502_REGPULLUP, 0X10);

	sx1502_write(data, SX1502_REGSENSEHIGH, 0x03);
	sx1502_write(data, SX1502_REGSENSELOW, 0xFF);

	//set gpio directory
	sx1502_write(data, SX1502_REGDIR, 0x1F);

	val = sx1502_read(data, SX1502_REGDIR);
	//printk("sx1502_REGDIR val = 0x%x\n", val);

	//set interrupt mask
	sx1502_write(data, SX1502_REGINTMASK, 0xE0);

	//enable jogball power
	jb_power_switch(1);

	//disable hph power
	hs_power_switch(0);
}

/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#define i2c_sx1502_suspend	NULL
#define i2c_sx1502_resume	NULL

#ifdef CONFIG_I2C_NEW_PROBE
static int sx1502_probe(struct i2c_client *new_client, const struct i2c_device_id *id)
{
#else
static int sx1502_detect(struct i2c_adapter *adapter, int address, int kind)
{
	struct i2c_client *new_client;
#endif
/* } ACER Jen chang, 2010/03/25 */
	struct sx1502_data *data;
	int err = 0;

	dbg("entering sx1502_detect\n");

	data = kcalloc(1, sizeof(*data), GFP_KERNEL);
	if(NULL == data)
	{
		err = -ENOMEM;
		goto err_exit;
	}

/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
	/* now we try to detect the chip */
#ifdef CONFIG_I2C_NEW_PROBE
	i2c_set_clientdata(new_client, data);
	data->client = new_client;
#else
	new_client = &data->client;
	i2c_set_clientdata(new_client, data);
	new_client->addr = address;
	new_client->adapter = adapter;
	new_client->driver = &sx1502_driver;
	new_client->flags = 0;
	strlcpy(new_client->name, "sx1502", I2C_NAME_SIZE);
#endif
/* } ACER Jen chang, 2010/03/25 */

/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
	/* register with i2c core */
	if ((err = i2c_attach_client(new_client))) {
		dev_err(&new_client->dev,
			"sx1502: error during i2c_attach_client()\n");
		goto exit_free;
	}
#endif
/* } ACER Jen chang, 2010/03/25 */

	INIT_WORK(&data->work, sx1502_work);

	data->sx1502_workqueue = create_singlethread_workqueue("sx1502_wq");
	if (!(data->sx1502_workqueue))
	{
		err = -ENOMEM;
		goto err_exit;
	}

	sx1502_global = data;

	/* initial the device */
	sx1502_init_registers(data);

	/* register input device */
	err = sx1502_register_driver(data);
	if(err != 0)
	/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
	{
	#ifdef CONFIG_I2C_NEW_PROBE
		goto exit_free;
	#else
		goto exit_detach;
	#endif
	}
	/* } ACER Jen chang, 2010/03/25 */

	err = sysfs_create_group(&new_client->dev.kobj, &sx1502_attr_group);
	if (err) {
		printk("sx1502 creating sysfs group error!\n");
		goto err_exit;
	}

	return 0;

err_exit:
	return err;
/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
exit_detach:
	i2c_detach_client(new_client);
#endif
/* } ACER Jen chang, 2010/03/25 */
exit_free:
	kfree(data);
	return err;
}

/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
static int sx1502_attach_adapter(struct i2c_adapter *adapter)
{
	dbg("entering sx1502_attach_adapter, calling i2c_probe\n");
	return i2c_probe(adapter, &addr_data, &sx1502_detect);
}
#endif
/* } ACER Jen chang, 2010/03/25 */

#ifdef CONFIG_I2C_NEW_PROBE
static int __devexit sx1502_remove(struct i2c_client *client)
{
#else
static int sx1502_detach_client(struct i2c_client *client)
{
	int err = 0;
#endif
/* } ACER Jen chang, 2010/03/25 */
	struct sx1502_data *data = i2c_get_clientdata(client);

	dbg("entering sx1502_detach_client\n");

	sysfs_remove_group(&client->dev.kobj, &sx1502_attr_group);

/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
	if((err = i2c_detach_client(client)))
	{
		dbg("sx1502 unregistration failed\n");
		return err;
	}
#endif
/* } ACER Jen chang, 2010/03/25 */

	kfree(data);
	return 0;
}

/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
static const struct i2c_device_id sx1502_id[] = {
	{ "sx1502", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sx1502_id);

static struct i2c_driver sx1502_driver = {
	.driver = {
		.name	= "sx1502",
		.owner  = THIS_MODULE,
	},
	.id_table =  sx1502_id,
	.probe	  =  sx1502_probe,
	.remove	  = __devexit_p( sx1502_remove),
	.suspend=  i2c_sx1502_suspend,
	.resume	=  i2c_sx1502_resume,
};
#else
static struct i2c_driver sx1502_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name	= "sx1502",
	},
	//.id		= I2C_DRIVERID_SX1502, //defined in i2c-id.h
	.attach_adapter	= sx1502_attach_adapter,
	.detach_client	= sx1502_detach_client,
};
#endif
/* } ACER Jen chang, 2010/03/25 */

/* platform driver, since i2c devices don't have platform_data */
static int __init sx1502_plat_probe(struct platform_device *pdev)
{
	sx1502_pdev = pdev;

	#ifdef CONFIG_HAS_EARLYSUSPEND
	printk("Selwyn debug: sx1502 register early suspend\n");
	register_early_suspend(&sx1502_earlys);
	#endif
	return 0;
}

static int sx1502_plat_remove(struct platform_device *pdev)
{
	#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&sx1502_earlys);
	#endif
	return 0;
}

static struct platform_driver sx1502_plat_driver = {
	.probe	= sx1502_plat_probe,
	.remove	= sx1502_plat_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name 	= "pnx-sx1502",
	},
};

static int __init sx1502_init(void)
{
	int rc;

	dbg("entering sx1502_init\n");
	if(!(rc = platform_driver_register(&sx1502_plat_driver)))
	{
		rc = i2c_add_driver(&sx1502_driver);

	}
	return rc;
}

static void __exit sx1502_exit(void)
{
	i2c_del_driver(&sx1502_driver);
	platform_driver_unregister(&sx1502_plat_driver);
}

module_init(sx1502_init);
module_exit(sx1502_exit);

MODULE_AUTHOR("Selwyn Chen");
MODULE_DESCRIPTION("SEMTECH GPIO Expanders");
MODULE_LICENSE("GPL");



