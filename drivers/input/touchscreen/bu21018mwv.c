/* *
 * bu21018mwv.c. Driver for Texas Instruments BU21018MWV touchscreen
 * controller on I2C bus on AT91SAM9261 board.
 *
 * Author: Jen Chang  (JenCJChang@acertdc.com)
 * Date:   04-09-2010
 * */
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/byteorder/generic.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include "bu21018mwv.h"

#define MODULE_NAME  "bu21018mwv"

#ifndef CONFIG_I2C_NEW_PROBE
static unsigned int short normal_i2c[] = { 0xB8, I2C_CLIENT_END };
I2C_CLIENT_INSMOD_1 (bu21018mwv_ts);
#endif

struct ts_event {
	u16	x[2];
	u16	y[2];
	u16 pressure;
	s16 update_x[2];
	s16 update_y[2];
};

/* driver data structure */
struct bu21018mwv_data
{
#ifdef CONFIG_I2C_NEW_PROBE
	struct i2c_client * client;
#else
	struct i2c_client client;
#endif
	struct workqueue_struct *bu21018mwv_workqueue;
	struct delayed_work work;
	struct input_dev *idev;
	unsigned int pen_gpio;
	unsigned int pen_irq;
	volatile int irq_depth;
	unsigned int start_timer;
	unsigned int restart_timer;
	unsigned int debug_state;
	unsigned int calibration_flag;
	unsigned int program_pld;
	unsigned char *program;
	struct early_suspend early_suspend;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bu21018mwv_early_suspend(struct early_suspend *h);
static void bu21018mwv_late_resume(struct early_suspend *h);
#endif

static struct i2c_driver bu21018mwv_driver;

static struct platform_device *bu21018mwv_pdev;

/***********************************************************************
 * Single thread for touch panel
 ***********************************************************************/
/*
 * Internal function. Schedule delayed work in the BU21018MWV work queue.
 */
static int bu21018mwv_schedule_delayed_work(struct delayed_work *work, unsigned long delay)
{
	struct bu21018mwv_data *data = container_of(work, struct bu21018mwv_data, work);

	return queue_delayed_work(data->bu21018mwv_workqueue, work, delay);
}

/***********************************************************************
 * DEVFS Management
 ***********************************************************************/
static ssize_t show_start_timer(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bu21018mwv_data *data = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "start_timer: %d\n", data->start_timer);
}

static ssize_t set_start_timer(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int timer = simple_strtoul(buf, NULL, 0);
	struct bu21018mwv_data *data = i2c_get_clientdata(to_i2c_client(dev));

	data->start_timer = timer;

    return count;
}

static ssize_t show_restart_timer(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bu21018mwv_data *data = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "restart_timer: %d\n", data->restart_timer);
}

static ssize_t set_restart_timer(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int timer = simple_strtoul(buf, NULL, 0);
	struct bu21018mwv_data *data = i2c_get_clientdata(to_i2c_client(dev));

	data->restart_timer = timer;

    return count;
}

static ssize_t set_irq_depth(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = simple_strtoul(buf, NULL, 0);
	struct bu21018mwv_data *data = i2c_get_clientdata(to_i2c_client(dev));

	if(value == 1)
	{
		enable_irq(data->pen_irq);
		data->irq_depth--;
		printk("--enable_irq: irq_depth=%d--\n", data->irq_depth);
	}
	else if(value == 0)
	{
		disable_irq(data->pen_irq);
		data->irq_depth++;
		printk("--disable_irq: irq_depth=%d--\n", data->irq_depth);
	}

    return count;
}

static ssize_t show_irq_depth(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bu21018mwv_data *data = i2c_get_clientdata(to_i2c_client(dev));
	char *b = buf;

	b += sprintf(b, "\033[1;34mNote: Don't issue both command unless touch driver hang!!\033[0m\n");
	b += sprintf(b, "=>enable_irq:echo 1 > irq_depth\n");
	b += sprintf(b, "=>disable_irq:echo 0 > irq_depth\n");
	b += sprintf(b, "\033[1;34mNow irq_depth=%d\033[0m\n", data->irq_depth);

	return b - buf;
}

static ssize_t set_debug_state(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int state = simple_strtoul(buf, NULL, 0);
	struct bu21018mwv_data *data = i2c_get_clientdata(to_i2c_client(dev));

	if(state == 0)
		data->debug_state = 0x1;
	else
	{
		data->debug_state &= ~(0x1);
		data->debug_state |= (1 << state);
	}

    return count;
}

static ssize_t show_debug_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bu21018mwv_data *data = i2c_get_clientdata(to_i2c_client(dev));
	char bin_buf[9], *b = buf;
    unsigned int bin, i;

	b += sprintf(b, "\033[1;34mbu21018mwv debug_state: bit-\033[0m\n");
	b += sprintf(b, "0:clear all log\n");
	b += sprintf(b, "1:update data\n");
	b += sprintf(b, "2:raw data\n");
	b += sprintf(b, "3:early_suspend\n");
	b += sprintf(b, "4:late_resume\n");

    bin = data->debug_state;
    for (i = 0; i < 8; i++)
    {
        bin_buf[7-i] = ((bin%2) == 1? '1':'0');
        bin /= 2;
    }
    bin_buf[i] = 0;

	b += sprintf(b, "\033[1;34mNow Debug_state is:(%8s)\033[0m\n", bin_buf);
	b += sprintf(b, "\033[1;32m IC_MAX_X=%d, IC_MAX_Y=%d\033[0m\n", IC_MAX_X, IC_MAX_Y);

	return b - buf;
}

static DEVICE_ATTR(start_timer, S_IWUSR | S_IRUGO, show_start_timer, set_start_timer);
static DEVICE_ATTR(restart_timer, S_IRUGO | S_IWUSR, show_restart_timer, set_restart_timer);
static DEVICE_ATTR(debug_state, S_IRUGO | S_IWUSR, show_debug_state, set_debug_state);
static DEVICE_ATTR(irq_depth, S_IRUGO | S_IWUSR, show_irq_depth, set_irq_depth);

static struct attribute *bu21018mwv_sysfs_entries[] = {
	&dev_attr_start_timer.attr,
	&dev_attr_restart_timer.attr,
	&dev_attr_debug_state.attr,
	&dev_attr_irq_depth.attr,
	NULL
};

static const struct attribute_group bu21018mwv_attr_group = {
	.name	= "bu21018mwv",			/* put in device directory */
	.attrs = bu21018mwv_sysfs_entries,
};

/************************************************************************
 *  Function to initialize default value.
 *  The required configuration values must be set up here by
 *  initializing the structure data using appropriate enums.
 *
 *  Parameters:
 *       input:
 *		client: The i2c_client device for our chip
 */
static inline void bu21018mwv_init_client (struct i2c_client *client)
{
	struct bu21018mwv_data *data = i2c_get_clientdata (client);

	data->start_timer = START_TIMER_DELAY;
	data->restart_timer = RESTART_TIMER_DELAY;

	data->debug_state = 0x10;
	data->irq_depth = 0;

	data->program = firmware_code;
}

/************************************************************************
 *  Function to initialize configuration BU21013MWV.
 *
 *  Parameters:
 *       input:
 *		client: The i2c_client device for our chip
 */
static inline void bu21018mwv_download_firmware (struct i2c_client *client)
{
	struct bu21018mwv_data *data = i2c_get_clientdata (client);

	int i = 0;

	/* Softreset */
	i2c_smbus_write_byte_data(client, ALL_RST_REG, 0x01);
	i2c_smbus_write_byte_data(client, ALL_RST_REG, 0x00);

	/* Download Code from Flash to BU21018MWV1 */
	i2c_smbus_write_byte_data(client, PROG_LD_REG, 0x02);
	i2c_smbus_write_byte_data(client, PADDR_H_REG, 0x00);
	i2c_smbus_write_byte_data(client, PADDR_L_REG, 0x00);

	for(i = 0; i < 16384; i++)
	{
		if(i < CODESIZE)
		{
			i2c_smbus_write_byte_data(client, PDATA_REG, data->program[i]);
		}
		else
		{
			i2c_smbus_write_byte_data(client, PDATA_REG, 0xFF);
		}
	}
	i2c_smbus_write_byte_data(client, PROG_LD_REG, 0x00);

	printk("-bu21018mwv download finish!!-\n");
}

static inline void bu21018mwv_chip_initialization (struct i2c_client *client)
{
	/* Init BU21018MWV1 */
	i2c_smbus_write_byte_data(client, SW_RST_REG, 0x01);
	i2c_smbus_write_byte_data(client, SENS_START_REG, 0x00);
	i2c_smbus_write_byte_data(client, CLK_DIV1_REG, 0x00);
	i2c_smbus_write_byte_data(client, CLK_DIV2_REG, 0x00);
	i2c_smbus_write_byte_data(client, SLP_EN_REG, 0x00);
	i2c_smbus_write_byte_data(client, SLP_TIME_REG, 0x00);
	i2c_smbus_write_byte_data(client, SLP_LEVEL_REG, 0x0F);
	i2c_smbus_write_byte_data(client, INT_MODE_REG, 0x00);
	i2c_smbus_write_byte_data(client, INT_MASK_REG, 0x08);
	i2c_smbus_write_byte_data(client, INT_CLR_REG, 0x00);
	i2c_smbus_write_byte_data(client, GAIN_A_REG, 0x13);
	i2c_smbus_write_byte_data(client, GAIN_B_REG, 0x12);
	i2c_smbus_write_byte_data(client, FILTER_EN_REG, 0x03);
	i2c_smbus_write_byte_data(client, FILTER1_REG, 0x08);
	i2c_smbus_write_byte_data(client, FILTER2_REG, 0x0F);

	i2c_smbus_write_byte_data(client, ON_TH_A_REG, 0x30);
	i2c_smbus_write_byte_data(client, OFF_TH_A_REG, 0x29);
	i2c_smbus_write_byte_data(client, ON_TH2_B_REG, 0x30);
	i2c_smbus_write_byte_data(client, OFF_TH2_B_REG, 0x29);
	i2c_smbus_write_byte_data(client, OFF_DAC_SW_REG, 0x00);
	i2c_smbus_write_byte_data(client, SENSOR_EN0_REG, 0xF8);
	i2c_smbus_write_byte_data(client, SENSOR_EN1_REG, 0xCF);
	i2c_smbus_write_byte_data(client, SENSOR_EN2_REG, 0xFF);
	i2c_smbus_write_byte_data(client, SENSOR_EN3_REG, 0xFF);
	i2c_smbus_write_byte_data(client, SENSOR_EN4_REG, 0x03);
	i2c_smbus_write_byte_data(client, ASSIGN_CHA_REG, 0xF8);
	i2c_smbus_write_byte_data(client, ASSIGN_CHB_REG, 0x0F);
	i2c_smbus_write_byte_data(client, ASSIGN_CHC_REG, 0x00);
	i2c_smbus_write_byte_data(client, ASSIGN_CHD_REG, 0xFE);
	i2c_smbus_write_byte_data(client, ASSIGN_CHE_REG, 0x03);
	i2c_smbus_write_byte_data(client, XY_MODE_REG, 0x00);
	i2c_smbus_write_byte_data(client, FILTER3_REG, 0x0F);

	i2c_smbus_write_byte_data(client, X_EDGE_REG, 0x00);
	i2c_smbus_write_byte_data(client, Y_EDGE_REG, 0x00);

	i2c_smbus_write_byte_data(client, SW_RST_REG, 0x00);
	i2c_smbus_write_byte_data(client, SENS_START_REG, 0x01);
}

static void bu21018mwv_read_values(struct bu21018mwv_data *data, struct ts_event *tp)
{
	s32 x_value[4], y_value[4];

	//X1
	x_value[0] = i2c_smbus_read_byte_data(data->client, X1H_REG);
	x_value[1] = i2c_smbus_read_byte_data(data->client, X1L_REG);
	tp->x[0] = ((x_value[0] & VALUE_MASK) << 8) + (x_value[1] & VALUE_MASK);

	//Y1
	y_value[0] = i2c_smbus_read_byte_data(data->client, Y1H_REG);
	y_value[1] = i2c_smbus_read_byte_data(data->client, Y1L_REG);
	tp->y[0] = ((y_value[0] & VALUE_MASK) << 8) + (y_value[1] & VALUE_MASK);

	//X2
	x_value[2] = i2c_smbus_read_byte_data(data->client, X2H_REG);
	x_value[3] = i2c_smbus_read_byte_data(data->client, X2L_REG);
	tp->x[1] = ((x_value[2] & VALUE_MASK) << 8) + (x_value[3] & VALUE_MASK);

	//Y2
	y_value[2] = i2c_smbus_read_byte_data(data->client, Y2H_REG);
	y_value[3] = i2c_smbus_read_byte_data(data->client, Y2L_REG);
	tp->y[1] = ((y_value[2] & 0xFF) << 8) + (y_value[3] & 0xFF);

	/* Fix it--read register to show real pressure */
	tp->pressure = 0;

	if((data->debug_state & 0x4) == 0x4)
	{
		printk("\033[33;1m-rawdata1-X_H=%d,X_L=%d,Y_H=%d,Y_L=%d-\033[m\n",
			x_value[0], x_value[1], y_value[0], y_value[1]);
		printk("\033[33;1m-rawdata2-X_H=%d,X_L=%d,Y_H=%d,Y_L=%d-\033[m\n",
			x_value[2], x_value[3], y_value[3], y_value[3]);
	}

}

static int bu21018mwv_normalize_value(struct bu21018mwv_data *data, struct ts_event *tp)
{
	int i;

	for(i = 0; i < 2; i++)
	{
		if(tp->x[0] == 0 || tp->y[0] == 0)
			return -1;

		tp->update_x[i] = ((IC_MAX_X - tp->x[i]) * DISPLAY_MAX) / IC_MAX_X;
		tp->update_y[i] = (tp->y[i] * DISPLAY_MAX) / IC_MAX_Y;

		if(tp->update_x[i] < 1)
			tp->update_x[i] = 1;
		else if(tp->update_x[i] > 4096)
			tp->update_x[i] = 4096;

		if(tp->update_y[i] < 1)
			tp->update_y[i] = 1;
		else if(tp->update_y[i] > 4096)
			tp->update_y[i] = 4096;
	}

	return 0;
}

//Add for report event to improve coding architecture
static void bu21018mwv_report_event(struct bu21018mwv_data *data, struct ts_event *tp, uint pen_status)
{
	if(pen_status == TOUCH_TRUE)
	{
		if((data->debug_state & 0x2) == 0x2)
		{
			printk("\033[31;1m-X=%d,Y=%d,PRESSURE=%d-\033[m\n",tp->update_x[0], tp->update_y[0], tp->pressure);
		}

		/*Reporting to input sub system */
		input_report_key (data->idev, BTN_TOUCH, TOUCH_TRUE);

		/* ACER Jen chang, 2009/08/27, IssueKeys:AU4.FC-46, Add touch screen default calibration data to mapping 0~4096 pixel { */
		input_report_abs (data->idev, ABS_X, tp->update_x[0]);
		input_report_abs (data->idev, ABS_Y, tp->update_y[0]);
		/* } ACER Jen Chang, 2009/08/27*/

		input_report_abs (data->idev, ABS_PRESSURE, tp->pressure);
		input_sync (data->idev);
	}
	else
	{
		input_report_key (data->idev, BTN_TOUCH, TOUCH_FALSE);
		input_report_abs (data->idev, ABS_PRESSURE, PRESSURE_FALSE);
		input_sync (data->idev);
	}
}

/*  Timer function called whenever the touch is detected.This function is used
 *  to read the X and Y coordinates and also the Z1,Z2 values if Actual pressure
 *  is to be reported. Pen down values are reported to the subsystem until
 *  the pen is down & returns by enbling IRQ on pen up.
 *  parameters:
 *       input:
 *		arg: struct bu21018mwv_data structure
 */
static void bu21018mwv_timer(struct delayed_work *work)
{
	struct bu21018mwv_data *data = container_of(work, struct bu21018mwv_data, work);
	struct ts_event tp;
	s32 ret;

	/* INT_STS */
	ret = i2c_smbus_read_byte_data(data->client, INT_STS_REG);
	if (ret < 0)
		alert_mesg ("unable to receive data: %s\n", __FUNCTION__);

	if((ret & 0x8) == 0x8)
	{
		if(IS_PEN_UP(data->pen_gpio))
		{
			input_report_key (data->idev, BTN_TOUCH, TOUCH_FALSE);
			input_report_abs (data->idev, ABS_PRESSURE, PRESSURE_FALSE);
			input_sync (data->idev);
			if(data->irq_depth > 0)
			{
				enable_irq(data->pen_irq);
				data->irq_depth--;
			}
			return;
		}

		i2c_smbus_write_byte_data(data->client, INT_CLR_REG, 0x8);
		bu21018mwv_read_values(data, &tp);
		ret = bu21018mwv_normalize_value(data, &tp);

		/* Check for prolonged touches i.e drags and drawings.
		 * If pen is still down restart the timer else report pen up and return
		 */
		if (!IS_PEN_UP (data->pen_gpio) && ret == 0)
		{
			bu21018mwv_report_event(data, &tp, TOUCH_TRUE);
			bu21018mwv_schedule_delayed_work(&data->work, data->restart_timer);
		}
		else
		{
			bu21018mwv_report_event(data, &tp, TOUCH_FALSE);
			if(data->irq_depth > 0)
			{
				enable_irq(data->pen_irq);
				data->irq_depth--;
			}
		}
	}
	else if((ret & 0x10) == 0x10)
	{
		i2c_smbus_write_byte_data(data->client, INT_CLR_REG, 0x10);
		i2c_smbus_write_byte_data(data->client, PROG_LD_REG, 0x00);
		printk("--Clear PLD--\n");
	}
	else if((ret & 0x80) == 0x80)
	{
		i2c_smbus_write_byte_data(data->client, INT_CLR_REG, 0x80);
		printk("--Clear ERR--\n");
	}
}

/*  IRQ handler function for penirq.The timer function is called here on pen
 *  down and also the pen up is reported here to the input subsystem.
 *  parameters:
 *	 input:
 *		irq: irq number
 * 		v: struct bu21018mwv_data structure
 *
 *	output:
 *		IRQ HANDLED status
 */

static irqreturn_t bu21018mwv_handle_penirq (int irq, void *v)
{
	struct bu21018mwv_data *data = (struct bu21018mwv_data *) v;

	disable_irq (irq);
	data->irq_depth++;
	/* Start timer if touch is detected (pen down) */
	if (!IS_PEN_UP (data->pen_gpio))
	{
		bu21018mwv_schedule_delayed_work(&data->work, data->restart_timer);
	}
	else
	{
		if(data->irq_depth > 0)
		{
			enable_irq (irq);
			data->irq_depth--;
		}
	}
	return IRQ_HANDLED;
}

/* *
 * Register the the bu21018mwv to the kernel. Allocate a input device,
 * request IRQs and register.
 * parameters:
 *	input:
 *		data: bu21018mwv_data structure
 *	output:
 *		function exit status
 *
 * */

static int bu21018mwv_register_driver (struct bu21018mwv_data *data)
{
	struct input_dev *idev;
	int ret;

	idev = input_allocate_device ();

	if (idev == NULL)
	{
		alert_mesg ("Unable to allocate input device\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	idev->name = DRIVER_NAME;
#ifdef CONFIG_I2C_NEW_PROBE
	idev->dev.parent = &(data->client->dev);
#else
	idev->dev.parent = &((data->client).dev);
#endif

	idev->id.bustype = BUS_I2C;

	idev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	idev->keybit[BIT_WORD (BTN_TOUCH)] = BIT_MASK (BTN_TOUCH);
	idev->absbit[BIT_WORD (ABS_X)] = BIT_MASK (ABS_X);
	idev->absbit[BIT_WORD (ABS_Y)] = BIT_MASK (ABS_Y);
	idev->absbit[BIT_WORD (ABS_PRESSURE)] = BIT_MASK (ABS_PRESSURE);

	/* Informing the input subsystem the range of values that our controller can send. */
	input_set_abs_params (idev, ABS_X, 0, DISPLAY_MAX, 0, 0);
	input_set_abs_params (idev, ABS_Y, 0, DISPLAY_MAX, 0, 0);
	input_set_abs_params (idev, ABS_PRESSURE, 0, ADC_PRESSURE_MAX, 0, 0);

	data->idev = idev;

	if (input_register_device (data->idev) != 0)
	{
		alert_mesg ("Unable to register input device: %s", __FUNCTION__);
		ret = -ENODEV;
		goto err_exit;
	}

	INIT_DELAYED_WORK(&data->work, bu21018mwv_timer);

	data->bu21018mwv_workqueue = create_singlethread_workqueue("bu21018mwv_singlethread_wq");
	if (!(data->bu21018mwv_workqueue))
	{
		ret = -ENOMEM;
		goto err_exit;
	}

	data->pen_irq = platform_get_irq(bu21018mwv_pdev, 0);
	if(data->pen_irq < 0) {
		dev_err(&bu21018mwv_pdev->dev, "no irq in platform resources!\n");
		ret = -EIO;
		goto destroy_workqueue;
	}

	data->pen_gpio = EXTINT_TO_GPIO(data->pen_irq);

	/*  Set PB0 as GPIO input */
	ret = gpio_request(data->pen_gpio, MODULE_NAME);
	if(ret < 0)
	{
		alert_mesg ("Unable to request gpio\n");
		ret = -ENODEV;
		goto destroy_workqueue;
	}

	gpio_direction_input (data->pen_gpio);

	ret = request_irq(data->pen_irq, bu21018mwv_handle_penirq, 0, DRIVER_NAME, data);
	if (ret != 0)
	{
		alert_mesg ("Unable to grab IRQ\n");
		ret = -ENODEV;
		goto destroy_workqueue;
	}

	if (set_irq_type(data->pen_irq, IRQ_TYPE_EDGE_FALLING))
	{
		ret = -ENODEV;
		goto destroy_workqueue;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 20;//EARLY_SUSPEND_LEVEL_MISC_DEVICE;
	data->early_suspend.suspend = bu21018mwv_early_suspend;
	data->early_suspend.resume = bu21018mwv_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	/* Host interface download firmware */
	bu21018mwv_download_firmware(data->client);

	/* Function to initialize chip */
	bu21018mwv_chip_initialization (data->client);

	bu21018mwv_schedule_delayed_work(&data->work, data->restart_timer);

	return 0;

destroy_workqueue:
	destroy_workqueue(data->bu21018mwv_workqueue);
err_exit:
	return ret;
}

/*   I2C detect function is used to populate the client data ,probe the I2C
 *   device by sending a command and receiving an ack.Setup command is sent
 *   to configure the Rirq value and enable/use MAV filter.Attach the Device
 *   to the I2C bus and also to validate all the commands of the bu21018mwv
 *   controller.
 *
 *   parameters:
 *   	  input:
 *   	  	adapter: Global structure adapter (defined in linux/i2c.h),
 *   	  	address: i2c address,
 *   	  	kind   : kind of probing (forceful detection in this case).
 *       output:
 *       	function exit status
 *
 */
#ifdef CONFIG_I2C_NEW_PROBE
static int bu21018mwv_probe(struct i2c_client *new_client, const struct i2c_device_id *id)
{
#else
static int bu21018mwv_detect (struct i2c_adapter *adapter, int address, int kind)
{
	struct i2c_client *new_client;
#endif
	struct bu21018mwv_data *data;
	s32 err = 0;

	dbg_mesg("bu21018mwv_probe()!!\n");

	if (!bu21018mwv_pdev) {
		printk(KERN_ERR "bu21018mwv: driver needs a platform_device!\n");
		err = -EIO;
		goto exit;
	}

#ifndef CONFIG_I2C_NEW_PROBE
	/* Check if the I2C Master supports the following functionalities*/
	if (!i2c_check_functionality (adapter, I2C_FUNC_SMBUS_BYTE_DATA
				| I2C_FUNC_I2C | I2C_FUNC_SMBUS_WORD_DATA))
	{
		alert_mesg ("I2C Functionality not supported\n");
		err = -EIO;
		goto exit;
	}
#endif

	data = kcalloc (1, sizeof (*data), GFP_KERNEL);

	if (NULL == data)
	{
		err = -ENOMEM;
		goto exit;
	}

#ifdef CONFIG_I2C_NEW_PROBE
	i2c_set_clientdata(new_client, data);
	data->client = new_client;
#else
	new_client = &data->client;
	i2c_set_clientdata (new_client, data);
	new_client->addr = address;
	new_client->adapter = adapter;
	new_client->driver = &bu21018mwv_driver;
	new_client->flags = 0;
	strlcpy (new_client->name, "bu21018mwv", I2C_NAME_SIZE);
#endif

	/* Function to initialize the structure data */
	bu21018mwv_init_client (new_client);

#ifndef CONFIG_I2C_NEW_PROBE
	/*Device found. Attach it to the I2C bus */
	dbg_mesg ("%s: attaching a bu21018mwv device on : %#x\n", __FUNCTION__, address);
	if ( i2c_attach_client (new_client))
	{
		alert_mesg ("%s: unable to attach device on : %#x\n", __FUNCTION__, address);
		goto err_exit;
	}
#endif

	err = sysfs_create_group(&new_client->dev.kobj, &bu21018mwv_attr_group);
	if (err) {
	#ifdef CONFIG_I2C_NEW_PROBE
		printk(KERN_ERR "bu21018mwv: error creating sysfs group!\n");
		goto err_exit;
	#else
		alert_mesg ("%s: error creating sysfs group : %#x\n", __FUNCTION__, address);
		goto exit_detach;
	#endif
	}

	err = bu21018mwv_register_driver (data);
	if (err != 0)
		goto exit_sysfs;

exit:
	return err;

exit_sysfs:
	sysfs_remove_group(&new_client->dev.kobj, &bu21018mwv_attr_group);

#ifndef CONFIG_I2C_NEW_PROBE
exit_detach:
	i2c_detach_client(new_client);
#endif

err_exit:
	kfree(data);
     return -ENODEV;
}

/* Function to attach the i2c_client
 *
 * parameters:
 * 	input:
 * 		adapter: The global adapter structure (defined in linux\i2c.h)
 *
 * */
#ifndef CONFIG_I2C_NEW_PROBE
static int bu21018mwv_attach (struct i2c_adapter *adapter)
{
	return i2c_probe (adapter, &addr_data, &bu21018mwv_detect);
}
#endif

/*Function to detach the i2c_client
 *
 * parameters:
 *     output:
 *     		client: The i2c client device for our chip
 *
 * */
#ifdef CONFIG_I2C_NEW_PROBE
static int __devexit bu21018mwv_remove(struct i2c_client *client)
{
#else
static int bu21018mwv_detach (struct i2c_client *client)
{
	int err = 0;
#endif
	struct bu21018mwv_data *data = i2c_get_clientdata (client);

	input_unregister_device (data->idev);
	input_free_device (data->idev);
	destroy_workqueue(data->bu21018mwv_workqueue);
	free_irq (data->pen_irq, data);

	sysfs_remove_group(&client->dev.kobj, &bu21018mwv_attr_group);

#ifndef CONFIG_I2C_NEW_PROBE
	if (err = i2c_detach_client (client))
	{
		alert_mesg ("Device unregistration failed\n");
		return err;
	}
#endif

	return 0;
}

#ifdef	CONFIG_PM
static int bu21018mwv_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;

	struct bu21018mwv_data *data = i2c_get_clientdata(client);

	disable_irq(data->pen_irq);
	data->irq_depth++;

	cancel_delayed_work_sync(&data->work);

	return 0;
}

static int bu21018mwv_resume(struct i2c_client *client)
{
	struct bu21018mwv_data *data = i2c_get_clientdata(client);

	for(; data->irq_depth > 0; data->irq_depth--)
	{
		enable_irq(data->pen_irq);
	}

	if((data->debug_state & 0x10) == 0x10)
		printk("--irq_depth=%d--\n", (data->irq_depth));

	return 0;
}
#else
#define bu21018mwv_suspend	NULL
#define bu21018mwv_resume	NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bu21018mwv_early_suspend(struct early_suspend *h)
{
	int ret;
	struct bu21018mwv_data *data;
	data = container_of(h, struct bu21018mwv_data, early_suspend);

#if 0
	disable_irq(data->pen_irq);
	data->irq_depth++;

	cancel_delayed_work_sync(&data->work);
#endif
	if((data->debug_state & 0x8) == 0x8)
		printk("bu21018mwv enter early suspend!!\n");
}

static void bu21018mwv_late_resume(struct early_suspend *h)
{
	struct bu21018mwv_data *data;
	data = container_of(h, struct bu21018mwv_data, early_suspend);

#if 0
	for(; data->irq_depth > 0; data->irq_depth--)
	{
		enable_irq(data->pen_irq);
	}
#endif
	if((data->debug_state & 0x10) == 0x10)
		printk("--irq_depth=%d--\n", (data->irq_depth));
}
#endif

#ifdef CONFIG_I2C_NEW_PROBE
static const struct i2c_device_id bu21018mwv_id[] = {
	{ "bu21018mwv", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bu21018mwv_id);

static struct i2c_driver bu21018mwv_driver = {
	.driver = {
		.name	= "bu21018mwv",
		.owner  = THIS_MODULE,
	},
	.id_table =  bu21018mwv_id,
	.probe	  =  bu21018mwv_probe,
	.remove	  = __devexit_p( bu21018mwv_remove),
	.suspend=  bu21018mwv_suspend,
	.resume	=  bu21018mwv_resume,
};
#else
static struct i2c_driver bu21018mwv_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name	= "bu21018mwv",
		.suspend= bu21018mwv_suspend,
		.resume	= bu21018mwv_resume,
	},
	.id		= I2C_DRIVERID_BU21018MWV,
	.attach_adapter	= bu21018mwv_attach,
	.detach_client	= bu21018mwv_detach,
};
#endif

/* platform driver, since i2c devices don't have platform_data */
static int __init bu21018mwv_plat_probe(struct platform_device *pdev)
{
	bu21018mwv_pdev = pdev;

	return 0;
}

static int bu21018mwv_plat_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver bu21018mwv_plat_driver = {
	.probe	= bu21018mwv_plat_probe,
	.remove	= bu21018mwv_plat_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name 	= "pnx-bu21018mwv",
	},
};

/* Init function of the driver called during module inserion */
static int __init bu21018mwv_init (void)
{
	int rc;

	if (!(rc = platform_driver_register(&bu21018mwv_plat_driver)))
	{
		printk("bu21018mwv platform_driver_register OK !!!\n");
		if (!(rc = i2c_add_driver(&bu21018mwv_driver)))
		{
			printk("bu21018mwv i2c_add_driver OK !!!\n");
		}
		else
		{
			printk(KERN_ERR "bu21018mwv i2c_add_driver failed\n");
			platform_driver_unregister(&bu21018mwv_driver);
			return 	-ENODEV;
		}
	}
	else
	{
		printk("bu21018mwv platform_driver_register Failed !!!\n");
	}

	return rc;

	//return i2c_add_driver (&bu21018mwv_driver);
}

/* EXit function of the driver called during module removal */
static void __exit bu21018mwv_exit (void)
{
	i2c_del_driver (&bu21018mwv_driver);
	platform_driver_unregister(&bu21018mwv_plat_driver);
}

module_init (bu21018mwv_init);
module_exit (bu21018mwv_exit);

MODULE_DESCRIPTION("BU21018MWV TouchScreen Driver");
MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Jen Chang:report bugs to JenCJChang@acertdc.com");
