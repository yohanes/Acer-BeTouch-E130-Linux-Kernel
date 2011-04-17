/* *
 * tsc2007.c. Driver for Texas Instruments TSC2007 touchscreen
 * controller on I2C bus on AT91SAM9261 board.
 *
 * Author: Joseph Robert  (joseph_robert@mindtree.com)
 * Date:   12-06-2008
 * */
/* ACER Jen chang, 2009/06/15, IssueKeys:AU4.F-2, Touch panel driver porting { */
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
/* ACER Jen chang, 2009/09/29, IssueKeys:AU4.FC-78, Add early suspend for entering sleep mode { */
#include <linux/earlysuspend.h>
/* } ACER Jen Chang, 2009/09/29*/
#include "tsc2007.h"

//#define Debug_ts
#define MODULE_NAME  "tsc2007"
#define Demo_K2

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
static unsigned int short normal_i2c[] = { 0x48, I2C_CLIENT_END };

I2C_CLIENT_INSMOD_1 (tsc2007_ts);
#endif
/* } ACER Jen chang, 2010/03/24 */

/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Add for saving raw data { */
struct ts_event {
	u16	x;
	u16	y;
	u16	z1;
	u16 z2;
	u16 pressure;
	s16 update_x;
	s16 update_y;
};
/* } ACER Jen chang, 2009/12/23 */

/* ACER Jen chang, 2010/04/07, IssueKeys:A21.B-1, Modify for saving calibartion data { */
struct ts_calibration
{
	u16 cal_x[5];
	u16 cal_y[5];
	u8 cal_flag[5];
};

struct ts_calibration cal_data;

/* ACER Jen chang, 2010/04/14, IssueKeys:A21.B-276, Modify touch screen default calibration data to mapping 0~4096 pixel for different model { */
#if (defined ACER_L1_AU4) || (defined ACER_L1_K2)
u16 default_cal_point[5][2] = {{256, 181}, {3840, 181}, {2048, 1928}, {256, 3674}, {3840, 3674}};
#elif (defined ACER_L1_K3)
u16 default_cal_point[5][2] = {{192, 256}, {3904, 256}, {2048, 2048}, {192, 3840}, {3904, 3840}};
#endif
/* } ACER Jen Chang, 2009/08/27*/
/* } ACER Jen chang, 2010/04/07 */

/* driver data structure */
struct tsc2007_data
{
/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	struct i2c_client * client;
#else
	struct i2c_client client;
#endif
/* } ACER Jen chang, 2010/03/24 */
	/* ACER Jen chang, 2009/10/29, IssueKeys:AU4.FC-144, Add single thread workq for touch panel { */
	struct workqueue_struct *tsc2007_workqueue;
	/* } ACER Jen Chang, 2009/10/29*/
	struct delayed_work work;
	struct input_dev *idev;
	unsigned int pen_gpio;
	unsigned int pen_irq;
	enum power_down_mode pd;
	enum resolution_mode m;
	/* ACER Jen chang, 2009/11/05, IssueKeys:AU4.B-874, Add for recording irq state { */
	volatile int irq_depth;
	/* } ACER Jen Chang, 2009/11/05*/
	/* ACER Jen chang, 2009/10/06, IssueKeys:AU4.FC-107, Add for adjusting delay timer to adjust sampling interval { */
	unsigned int start_timer;
	unsigned int restart_timer;
	/* } ACER Jen Chang, 2009/10/06*/
	/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Add for pressure threshold and debug message { */
	unsigned int Ph_Threshold;
	unsigned int Pl_Threshold;
	unsigned int debug_state;
	/* } ACER Jen chang, 2009/12/23 */
	/* ACER Jen chang, 2010/02/26, IssueKeys:A43.B-813, Add for saving calibartion data { */
	unsigned int calibration_flag;
	int x_offset;
	int y_offset;
	int x_scale;
	int y_scale;
	int cal_x_offset;
	int cal_y_offset;
	int cal_x_scale;
	int cal_y_scale;
	/* } ACER Jen chang, 2010/02/26 */
	/* ACER Jen chang, 2009/09/29, IssueKeys:AU4.FC-78, Add early suspend define for entering sleep mode { */
	struct early_suspend early_suspend;
	/* } ACER Jen Chang, 2009/09/29*/
};

/* ACER Jen chang, 2009/09/29, IssueKeys:AU4.FC-78, Add early suspend function for entering sleep mode { */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void tsc2007_early_suspend(struct early_suspend *h);
static void tsc2007_late_resume(struct early_suspend *h);
#endif
/* } ACER Jen Chang, 2009/09/29*/

static struct i2c_driver tsc2007_driver;

static struct platform_device *tsc2007_pdev;


/* ACER Jen chang, 2009/10/29, IssueKeys:AU4.FC-144, Add single thread workq for touch panel { */
/***********************************************************************
 * Single thread for touch panel
 ***********************************************************************/
/*
 * Internal function. Schedule delayed work in the TSC2007 work queue.
 */
static int tsc2007_schedule_delayed_work(struct delayed_work *work, unsigned long delay)
{
	struct tsc2007_data *data = container_of(work, struct tsc2007_data, work);

	return queue_delayed_work(data->tsc2007_workqueue, work, delay);
}
/* } ACER Jen Chang, 2009/10/29*/

/* ACER Jen chang, 2009/10/06, IssueKeys:AU4.FC-107, Add sys_fs function for adjusting delay timer to adjust sampling interval { */
/***********************************************************************
 * DEVFS Management
 ***********************************************************************/
static ssize_t show_start_timer(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "start_timer: %d\n", data->start_timer);
}

static ssize_t set_start_timer(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int timer = simple_strtoul(buf, NULL, 0);
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

	data->start_timer = timer;

    return count;
}

static ssize_t show_restart_timer(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "restart_timer: %d\n", data->restart_timer);
}

static ssize_t set_restart_timer(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int timer = simple_strtoul(buf, NULL, 0);
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

	data->restart_timer = timer;

    return count;
}

/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Add to adjust presure threshold and enable/disable debug message { */
static ssize_t set_Ph_threshold(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int threshold = simple_strtoul(buf, NULL, 0);
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

	data->Ph_Threshold = threshold;

    return count;
}

static ssize_t show_Ph_threshold(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "Ph_threshold: %d\n", data->Ph_Threshold);
}

static ssize_t set_Pl_threshold(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int threshold = simple_strtoul(buf, NULL, 0);
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

	data->Pl_Threshold = threshold;

    return count;
}

static ssize_t show_Pl_threshold(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "Pl_Threshold: %d\n", data->Pl_Threshold);
}

/* ACER Jen chang, 2010/01/20, IssueKeys:A43.B-486, Add for enabling irq to let touch drive work { */
static ssize_t set_irq_depth(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = simple_strtoul(buf, NULL, 0);
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

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
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));
	char *b = buf;

	b += sprintf(b, "\033[1;34mNote: Don't issue both command unless touch driver hang!!\033[0m\n");
	b += sprintf(b, "=>enable_irq:echo 1 > irq_depth\n");
	b += sprintf(b, "=>disable_irq:echo 0 > irq_depth\n");
	b += sprintf(b, "\033[1;34mNow irq_depth=%d\033[0m\n", data->irq_depth);

	return b - buf;
}
/* } ACER Jen Chang, 2010/01/20*/


static ssize_t set_debug_state(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int state = simple_strtoul(buf, NULL, 0);
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

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
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));
	char bin_buf[9], *b = buf;
    unsigned int bin, i;

	b += sprintf(b, "\033[1;34mTsc2007 debug_state: bit-\033[0m\n");
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

	return b - buf;
}

/* ACER Jen chang, 2010/02/26, IssueKeys:A43.B-813, Add for get/set calibartion data { */
static ssize_t get_calibration_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));
	char *b = buf;

	b += sprintf(b, "%d %d %d %d\n",  data->cal_x_offset, data->cal_x_scale, data->cal_y_offset, data->cal_y_scale);

	return b - buf;
}

static ssize_t set_calibration_data(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));
	char buf_tmp[5];
    int x_offset, y_offset, x_scale, y_scale;
    int i = 0 , j = 0;

    /* Copy the first part of buf (before the white space) in buf1 */
    while(!isspace(buf[i]) && (i < count))
    {
        buf_tmp[i] = buf[i];
        i++;
    }
    /* Add EoS */
    buf_tmp[i]='\n'; /* Line Feed */
	x_offset = simple_strtol(buf_tmp, NULL, 0);

	i++; /* Skip the whitespace */
	j = 0;
    while(!isspace(buf[i]) && (i<count))
    {
        buf_tmp[j] = buf[i];
        j++;
        i++;
    }
	/* Add EoS */
    buf_tmp[j]='\n'; /* Line Feed */
	x_scale = simple_strtol(buf_tmp, NULL, 0);

	i++; /* Skip the whitespace */
	j = 0;
	/* Copy the first part of buf (before the white space) in buf1 */
    while(!isspace(buf[i]) && (i < count))
    {
        buf_tmp[j] = buf[i];
        j++;
        i++;
	}
    /* Add EoS */
    buf_tmp[j]='\n'; /* Line Feed */
	y_offset = simple_strtol(buf_tmp, NULL, 0);

	i++; /* Skip the whitespace */
	j = 0;
    while(!isspace(buf[i]) && (i<count))
    {
        buf_tmp[j] = buf[i];
        j++;
        i++;
    }
    /* Add EoS */
    buf_tmp[j]='\n'; /* Line Feed */
	y_scale = simple_strtol(buf_tmp, NULL, 0);

	if((x_offset > 4096) || (y_offset > 4096))
		return -EINVAL;

	data->x_offset = x_offset;
	data->x_scale = x_scale;
	data->y_offset = y_offset;
	data->y_scale = y_scale;

	return count;
}

static ssize_t get_status_calibration(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "calibration_flag=%d\n", data->calibration_flag);
}

/* ACER Jen chang, 2010/04/07, IssueKeys:A21.B-1, Modify for saving calibartion data { */
static ssize_t set_start_calibration(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int flag = simple_strtoul(buf, NULL, 0);
	struct tsc2007_data *data = i2c_get_clientdata(to_i2c_client(dev));

	if(flag == 1)
	{
		data->calibration_flag = 1;
		memset(&cal_data, 0, sizeof(cal_data));
	}
	else
	{
		data->calibration_flag = 0;
	}

    return count;
}
/* } ACER Jen chang, 2010/04/07 */

static DEVICE_ATTR(start_timer, S_IWUSR | S_IRUGO, show_start_timer, set_start_timer);
static DEVICE_ATTR(restart_timer, S_IRUGO | S_IWUSR, show_restart_timer, set_restart_timer);
static DEVICE_ATTR(ph_threshold, S_IRUGO | S_IWUSR, show_Ph_threshold, set_Ph_threshold);
static DEVICE_ATTR(pl_threshold, S_IRUGO | S_IWUSR, show_Pl_threshold, set_Pl_threshold);
static DEVICE_ATTR(debug_state, S_IRUGO | S_IWUSR, show_debug_state, set_debug_state);
/* ACER Jen chang, 2010/01/20, IssueKeys:A43.B-486, Add for enabling irq to let touch drive work { */
static DEVICE_ATTR(irq_depth, S_IRUGO | S_IWUSR, show_irq_depth, set_irq_depth);
/* } ACER Jen Chang, 2010/01/20*/
static DEVICE_ATTR(calibration_data, S_IRUGO | S_IWUSR, get_calibration_data, set_calibration_data);
static DEVICE_ATTR(start_calibration, S_IRUGO | S_IWUSR, get_status_calibration, set_start_calibration);

static struct attribute *tsc2007_sysfs_entries[] = {
	&dev_attr_start_timer.attr,
	&dev_attr_restart_timer.attr,
	&dev_attr_ph_threshold.attr,
	&dev_attr_pl_threshold.attr,
	&dev_attr_debug_state.attr,
/* ACER Jen chang, 2010/01/20, IssueKeys:A43.B-486, Add for enabling irq to let touch drive work { */
	&dev_attr_irq_depth.attr,
/* } ACER Jen Chang, 2010/01/20*/
	&dev_attr_calibration_data.attr,
	&dev_attr_start_calibration.attr,
	NULL
};
/* } ACER Jen chang, 2009/12/23 */
/* } ACER Jen chang, 2010/02/06 */

static const struct attribute_group tsc2007_attr_group = {
	.name	= "tsc2007",			/* put in device directory */
	.attrs = tsc2007_sysfs_entries,
};
/* } ACER Jen Chang, 2009/10/06*/

/************************************************************************
 *  Function to initialize the Power down mode & resolution modes.
 *  The required configuration values must be set up here by
 *  initializing the structure data using appropriate enums.
 *
 *  Parameters:
 *       input:
 *		client: The i2c_client device for our chip
 */
static inline void tsc2007_init_client (struct i2c_client *client)
{
	struct tsc2007_data *data = i2c_get_clientdata (client);
	data->pd = PD_POWERDOWN_ENABLEPENIRQ;

#ifdef BIT_MODE_12
	data->m = M_12BIT;
#else
	data->m = M_8BIT;
#endif

	/* ACER Jen chang, 2009/10/06, IssueKeys:AU4.FC-107, Set default sampling interval as 2 jiffaz(about 20ms) { */
	data->start_timer = START_TIMER_DELAY;
	data->restart_timer = RESTART_TIMER_DELAY;
	/* } ACER Jen Chang, 2009/10/06*/

	/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Initialize presure threshold and debug state { */
	data->Ph_Threshold = PRESURE_Hi_THRESHOLD;
	data->Pl_Threshold = PRESURE_Lo_THRESHOLD;

	data->debug_state = 0x10;
	/* } ACER Jen Chang, 2009/12/23*/

	/* ACER Jen chang, 2009/11/05, IssueKeys:AU4.B-874, Initialize data->depth as zero { */
	data->irq_depth = 0;
	/* } ACER Jen Chang, 2009/11/05*/

	/* ACER Jen chang, 2010/03/02, IssueKeys:A43.B-813, Set calibartion initialized data for pressing menu item easily { */
	data->calibration_flag = 0;
	data->cal_x_offset = CONFIG_INPUT_SCREEN_OFFSET_X;
	data->cal_y_offset = CONFIG_INPUT_SCREEN_OFFSET_Y;
	data->cal_x_scale = (ADC_MAX * 1000 / CONFIG_INPUT_SCREEN_SCALE_X);
	data->cal_y_scale = (ADC_MAX * 1000 / CONFIG_INPUT_SCREEN_SCALE_Y);
	data->x_offset = data->cal_x_offset;
	data->y_offset = data->cal_y_offset;
	data->x_scale = data->cal_x_scale;
	data->y_scale = data->cal_y_scale;
	/* } ACER Jen chang, 2010/03/02 */
	return;
}

/*  Funtion to issue and read the results for all the commands except the setup
 *  command
 *
 *  Parameters:
 *	 input: data: struct tsc2007_data structure
 *		cmd  : enum converer_function to select the function of the controller.
 *
 *   	 output:
 *		value: The result of the corresponding measurement based on the command byte sent.
 */

static inline int tsc2007_read (struct tsc2007_data *data, enum converter_function cmd)
{
	u8 command_byte;
	s32 value = 0;
/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	struct i2c_client *new_client = data->client;
#else
	struct i2c_client *new_client = &data->client;
#endif
/* } ACER Jen chang, 2010/03/24 */

	/* Command byte to be sent based on the structure data values */

	command_byte = TSC2007_CMD (cmd, data->pd, data->m);

	value = i2c_smbus_read_word_data(new_client, command_byte);
	if (value < 0)
		alert_tsc ("unable to receive data: %s\n", __FUNCTION__);

	return value;
}

/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Add for read raw data values to improve coding architecture { */
static void tsc2007_read_values(struct tsc2007_data *data, struct ts_event *tp)
{
	u8 measure_x =	TSC2007_CMD (MEAS_XPOS, data->pd, data->m);
	u8 measure_y =	TSC2007_CMD (MEAS_YPOS, data->pd, data->m);
	u32 x_value = 0;
	u32 y_value = 0;
#ifdef REPORT_ACTUAL_PRESSURE
	u32 z1_value = 0;
	u32 z2_value = 0;
	u8 measure_z1 =	TSC2007_CMD (MEAS_Z1, data->pd, data->m);
	u8 measure_z2 =	TSC2007_CMD (MEAS_Z2, data->pd, data->m);
#endif

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	y_value = i2c_smbus_read_word_data(data->client, measure_y);
#else
	y_value = i2c_smbus_read_word_data(&data->client, measure_y);
#endif
/* } ACER Jen chang, 2010/03/24 */
	if (y_value < 0)
		alert_tsc ("unable to receive data: %s\n", __FUNCTION__);

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	x_value = i2c_smbus_read_word_data(data->client, measure_x);
#else
	x_value = i2c_smbus_read_word_data(&data->client, measure_x);
#endif
/* } ACER Jen chang, 2010/03/24 */
	if (x_value < 0)
		alert_tsc ("unable to receive data: %s\n", __FUNCTION__);

	/**
	 * The values coming out of the TSC2007 are in big-endian format and
	 * ****NEED BYTE SWAPPING***** on little endian machines. Depending upon
	 * the resolution mode used proper bits have to be masked off before the
	 * values can be used.
	 * */
	if (data->m == M_12BIT)
	{
		/* Bit masking and data alignment for 12 bit mode */
		tp->y = (((be16_to_cpu (y_value)) >> 4) & VALUE_MASK);
		tp->x = (((be16_to_cpu (x_value)) >> 4) & VALUE_MASK);
	}/* No need of bit masking in case of 8 bit mode */
	else if (data->m == M_8BIT)
	{
		tp->y = y_value;
		tp->x = x_value;
	}

	/*  To Report constant pressure value in the range 1-15000
	 *  (MAX pressure recevied as per Platform data. 7500 is chosen
	 *  as an intermediate value in that range
	 */
	tp->pressure = FIXED_PRESSURE_VAL;

	/*  Measuring touch pressure can also be done with the TSC2007.
	 *  To determine pen touch, the pressure of the touch must
	 *  be determined
	 */
#ifdef REPORT_ACTUAL_PRESSURE
/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	z1_value = i2c_smbus_read_word_data(data->client, measure_z1);
#else
	z1_value = i2c_smbus_read_word_data(&data->client, measure_z1);
#endif
/* } ACER Jen chang, 2010/03/24 */
	if (z1_value < 0)
		alert_tsc ("unable to receive data: %s\n", __FUNCTION__);

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	z2_value = i2c_smbus_read_word_data(data->client, measure_z2);
#else
	z2_value = i2c_smbus_read_word_data(&data->client, measure_z2);
#endif
/* } ACER Jen chang, 2010/03/24 */
	if (z2_value < 0)
		alert_tsc ("unable to receive data: %s\n", __FUNCTION__);

	/* Bit masking and data alignment for 12 bit mode */
	if (data->m == M_12BIT)
	{
		tp->z1 = (((be16_to_cpu (z1_value)) >> 4) & VALUE_MASK);
		tp->z2 = (((be16_to_cpu (z2_value)) >> 4) & VALUE_MASK);
	}

	/* No need of bit masking in case of 8 bit mode */

	else if (data->m == M_8BIT)
	{
		tp->z1 = z1_value;
		tp->z2 = z2_value;
	}

	/* Actual pressure calculation using the formula to calculate Rtouch.
	   The pressure is not reported if in case z1=0 */
	if (z1_value)
	{
		/* ACER Jen chang, 2009/12/25, IssueKeys:AU4.B-2253, Modify formula to avoid calculating error because of variable type { */
		tp->pressure = ((((tp->z2 - tp->z1) * RX_PLATE_VAL) >> ADC_MAX_LEVEL) * tp->x) / tp->z1;
		tp->pressure = tp->pressure & VALUE_MASK ;
		/* } ACER Jen Chang, 2009/12/25*/
	}
#endif
}

/* ACER Jen chang, 2010/02/06, IssueKeys:A43.B-813, Get calibartion data to calculate coordinate  { */
//Add for normalize raw data values to improve coding architecture
static void tsc2007_normalize_value(struct tsc2007_data *data, struct ts_event *tp)
{
	tp->update_x = (tp->x - data->x_offset) * data->x_scale / 1000;
	tp->update_y = ((ADC_MAX - tp->y) - data->y_offset) * data->y_scale / 1000;

	if(tp->update_x < 1)
		tp->update_x = 1;
	else if(tp->update_x > 4096)
		tp->update_x = 4096;

	if(tp->update_y < 1)
		tp->update_y = 1;
	else if(tp->update_y > 4096)
		tp->update_y = 4096;
}

//Add for read raw data 3 times to get averaging closet data
static int tsc2007_averaging_closest_data(struct tsc2007_data *data, struct ts_event *tp)
{
	struct ts_event tp_reRead[3];
	int i, delta_x[3], delta_y[3], delta_z1[3], delta_z2[3];

	for(i = 0; i < 3; i++)
	{
		if (!IS_PEN_UP (data->pen_gpio))
		{
			tsc2007_read_values(data, &tp_reRead[i]);
			tsc2007_normalize_value(data, &tp_reRead[i]);

			if((data->debug_state & 0x4) == 0x4)
			{
				printk("\033[33;1m-rawdata-%d:x=%d,y=%d,z1=%d,z2=%d, pressure=%d-\033[m\n",
					i, tp_reRead[i].x, tp_reRead[i].y, tp_reRead[i].z1, tp_reRead[i].z2, tp_reRead[i].pressure);
			}
		}
		else
			return -1;
	}

	delta_x[0] = tp_reRead[0].update_x - tp_reRead[1].update_x;	//get the differentials
	delta_x[1] = tp_reRead[1].update_x - tp_reRead[2].update_x;
	delta_x[2] = tp_reRead[2].update_x - tp_reRead[0].update_x;

	delta_y[0] = tp_reRead[0].update_y - tp_reRead[1].update_y;	//get the differentials
	delta_y[1] = tp_reRead[1].update_y - tp_reRead[2].update_y;
	delta_y[2] = tp_reRead[2].update_y - tp_reRead[0].update_y;

	delta_z1[0] = tp_reRead[0].z1 - tp_reRead[1].z1;	//get the differentials
	delta_z1[1] = tp_reRead[1].z1 - tp_reRead[2].z1;
	delta_z1[2] = tp_reRead[2].z1 - tp_reRead[0].z1;

	delta_z2[0] = tp_reRead[0].z2 - tp_reRead[1].z2;	//get the differentials
	delta_z2[1] = tp_reRead[1].z2 - tp_reRead[2].z2;
	delta_z2[2] = tp_reRead[2].z2 - tp_reRead[0].z2;

	for(i = 0; i < 3; i++)
	{
		if((tp_reRead[i].z1 < Raw_Pressure_Lo_Threshold) || (tp_reRead[i].z2 > Raw_Pressure_Hi_Threshold)) //depend on experiment
			return -1;
		else
		{
			if(delta_x[i] < 0 )
				delta_x[i] = -delta_x[i];

			if(delta_y[i] < 0 )
				delta_y[i] = -delta_y[i];
		}
	}

	if(delta_x[0] < delta_x[1])
	{
		if(delta_x[0] < delta_x[2])
		{
			tp->update_x = tp_reRead[0].update_x + tp_reRead[1].update_x;
			tp->pressure = tp_reRead[0].pressure + tp_reRead[1].pressure;
		}
		else
		{
			tp->update_x = tp_reRead[0].update_x + tp_reRead[2].update_x;
			tp->pressure = tp_reRead[0].pressure + tp_reRead[2].pressure;
		}
	}
	else
	{
		if(delta_x[1] < delta_x[2])
		{
			tp->update_x = tp_reRead[1].update_x + tp_reRead[2].update_x;
			tp->pressure = tp_reRead[1].pressure + tp_reRead[2].pressure;
		}
		else
		{
			tp->update_x = tp_reRead[0].update_x + tp_reRead[2].update_x;
			tp->pressure = tp_reRead[0].pressure + tp_reRead[2].pressure;
		}
	}
	tp->update_x >>= 1;
	tp->pressure >>= 1;

	if(delta_y[0] < delta_y[1])
	{
		if(delta_y[0] < delta_y[2])
			tp->update_y = tp_reRead[0].update_y + tp_reRead[1].update_y;
		else
			tp->update_y = tp_reRead[0].update_y + tp_reRead[2].update_y;
	}
	else
	{
		if(delta_y[1] < delta_y[2])
			tp->update_y = tp_reRead[1].update_y + tp_reRead[2].update_y;
		else
			tp->update_y = tp_reRead[0].update_y + tp_reRead[2].update_y;
	}
	tp->update_y >>= 1;

	return 0;
}
/* } ACER Jen chang, 2010/02/06 */

//Add for report event to improve coding architecture
static void tsc2007_report_event(struct tsc2007_data *data, struct ts_event *tp, uint pen_status)
{
	if(pen_status == TOUCH_TRUE)
	{
		if((data->debug_state & 0x2) == 0x2)
		{
			printk("\033[31;1m-X=%d,Y=%d,PRESSURE=%d-\033[m\n",tp->update_x, tp->update_y, tp->pressure);
		}

		/*Reporting to input sub system */
		input_report_key (data->idev, BTN_TOUCH, TOUCH_TRUE);

		/* ACER Jen chang, 2009/08/27, IssueKeys:AU4.FC-46, Add touch screen default calibration data to mapping 0~4096 pixel { */
		input_report_abs (data->idev, ABS_X, tp->update_x);
		input_report_abs (data->idev, ABS_Y, tp->update_y);
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

/* ACER Jen chang, 2010/02/06, IssueKeys:A43.B-813, Calculate calibartion data { */
static void tsc2007_compute_calibration(struct tsc2007_data *data, struct ts_calibration cal_data)
{
	int x_offset = 0, y_offset = 0, x_scale[2] = {0}, y_scale[2] = {0};
	int i;

	x_offset = ((cal_data.cal_x[0] - default_cal_point[0][0]) +
				(cal_data.cal_x[3] - default_cal_point[3][0])) >> 1;

	y_offset = (((ADC_MAX - cal_data.cal_y[0]) - default_cal_point[0][1]) +
				((ADC_MAX - cal_data.cal_y[1]) - default_cal_point[1][1])) >> 1;

	for(i = 0; i < 5; i += 3)
	{
		x_scale[0] += cal_data.cal_x[i+1] - cal_data.cal_x[i];
		x_scale[1] += default_cal_point[i+1][0] - default_cal_point[i][0];
	}
	x_scale[0] >>= 1;
	x_scale[1] >>= 1;

	for(i = 3; i < 5; i++)
	{
		y_scale[0] += ((ADC_MAX - cal_data.cal_y[i]) - (ADC_MAX - cal_data.cal_y[i-3]));
		y_scale[1] += default_cal_point[i][1] - default_cal_point[i-3][1];
	}
	y_scale[0] >>= 1;
	y_scale[1] >>= 1;

/* ACER Jen chang, 2010/02/26, IssueKeys:A43.B-813, Saving calibartion data different from default data to avoid influencing coordinates immediately after calibrating { */
	data->cal_x_offset = x_offset;
	data->cal_y_offset = y_offset;
	data->cal_x_scale = 1000 * x_scale[1] / x_scale[0];
	data->cal_y_scale = 1000 * y_scale[1] / y_scale[0];

#if 0
	data->x_offset = x_offset;
	data->y_offset = y_offset;

	data->x_scale = 1000 * x_scale[1] / x_scale[0];
	data->y_scale = 1000 * y_scale[1] / y_scale[0];
#endif

	printk("--cal_data:x_offset=%d, x_scale=%d--\n", data->cal_x_offset, data->cal_x_scale);
	printk("--cal_data:y_offset=%d, y_scale=%d--\n", data->cal_y_offset, data->cal_y_scale);
/* } ACER Jen chang, 2010/02/26 */
}
/* } ACER Jen chang, 2010/02/06 */

/*  Timer function called whenever the touch is detected.This function is used
 *  to read the X and Y coordinates and also the Z1,Z2 values if Actual pressure
 *  is to be reported. Pen down values are reported to the subsystem until
 *  the pen is down & returns by enbling IRQ on pen up.
 *  parameters:
 *       input:
 *		arg: struct tsc2007_data structure
 */
/* ACER Jen chang, 2010/02/06, IssueKeys:A43.B-813, Get and calculate calibartion data { */
//static void tsc2007_timer (unsigned long arg)
static void tsc2007_timer(struct delayed_work *work)
{
	struct tsc2007_data *data = container_of(work, struct tsc2007_data, work);
	struct ts_event tp;
	u16 tmp_x, tmp_y;
	int ret, tp_index, i;

	if(IS_PEN_UP(data->pen_gpio))
	{
		input_report_key (data->idev, BTN_TOUCH, TOUCH_FALSE);
		input_report_abs (data->idev, ABS_PRESSURE, PRESSURE_FALSE);
		input_sync (data->idev);
		/* ACER Jen chang, 2010/04/21, IssueKeys:A21.B-117, Add irq_depth detect for avoiding unbalance irq { */
		if(data->irq_depth > 0)
		{
		enable_irq(data->pen_irq);
		data->irq_depth--;
		}
		/* } ACER Jen Chang, 2010/04/21*/
		return;
	}

	/* ACER Jen chang, 2010/04/07, IssueKeys:A21.B-1, Modify for saving calibartion data { */
	if(data->calibration_flag == 1)
	{
		if (!IS_PEN_UP (data->pen_gpio))
		{
			tsc2007_read_values(data, &tp);

			if((tp.z1 > Raw_Pressure_Lo_Threshold) && (tp.z2 < Raw_Pressure_Hi_Threshold) &&
				(tp.pressure < data->Ph_Threshold) && (tp.pressure > data->Pl_Threshold)) //depend on experiment
			{
				if(tp.x < 1365)
				{
					if(tp.y > 2730)
					{
						if(cal_data.cal_flag[0] == 0)
							tp_index = 0;
						else
							tp_index = 5;
					}
					else if(tp.y < 1365)
					{
						if(cal_data.cal_flag[3] == 0)
							tp_index = 3;
						else
							tp_index = 5;
					}
					else
						tp_index = 5;
				}
				else if(tp.x > 2730)
				{
					if(tp.y > 2730)
					{
						if(cal_data.cal_flag[1] == 0)
							tp_index = 1;
						else
							tp_index = 5;
					}
					else if(tp.y < 1365)
					{
						if(cal_data.cal_flag[4] == 0)
							tp_index = 4;
						else
							tp_index = 5;
					}
					else
						tp_index = 5;
				}
				else
				{
					if(tp.y > 1365 && tp.y < 2730)
					{
						if(cal_data.cal_flag[2] == 0)
							tp_index = 2;
						else
							tp_index = 5;
					}
					else
						tp_index = 5;
				}

				if(tp_index == 5)
				{
					/* ACER Jen chang, 2010/04/21, IssueKeys:A21.B-117, Add irq_depth detect for avoiding unbalance irq { */
					if(data->irq_depth > 0)
					{
					enable_irq(data->pen_irq);
					data->irq_depth--;
					}
					/* } ACER Jen Chang, 2010/04/21*/
					return;
				}
				tp.update_x = tp.x;
				tp.update_y = (ADC_MAX - tp.y);

				/*Reporting to input sub system */
				tsc2007_report_event(data, &tp, TOUCH_TRUE);

				cal_data.cal_x[tp_index] = tp.x;
				cal_data.cal_y[tp_index] = tp.y;
				cal_data.cal_flag[tp_index] = 1;
				printk("--cal_data:x=%d, y=%d--\n", cal_data.cal_x[tp_index], cal_data.cal_y[tp_index]);

				for(i = 0; i <= 4; i++)
				{
					if(cal_data.cal_flag[i] != 1)
					{
						printk("--check calibration--\n");
						break;
					}

					if(i >= 4)
					{
						tsc2007_compute_calibration(data, cal_data);
						data->calibration_flag = 0;
					}
				}

				/* ACER Jen chang, 2010/04/21, IssueKeys:A21.B-117, Add irq_depth detect for avoiding unbalance irq { */
				if(data->irq_depth > 0)
				{
				enable_irq(data->pen_irq);
				data->irq_depth--;
				}
				/* } ACER Jen Chang, 2010/04/21*/
				tsc2007_report_event(data, &tp, TOUCH_FALSE);
			}
			else
			{
				input_report_key (data->idev, BTN_TOUCH, TOUCH_FALSE);
				input_report_abs (data->idev, ABS_PRESSURE, PRESSURE_FALSE);
				input_sync (data->idev);
				/* ACER Jen chang, 2010/04/21, IssueKeys:A21.B-117, Add irq_depth detect for avoiding unbalance irq { */
				if(data->irq_depth > 0)
				{
				enable_irq(data->pen_irq);
				data->irq_depth--;
				}
				/* } ACER Jen Chang, 2010/04/21*/
			}
		}
		else
		{
			input_report_key (data->idev, BTN_TOUCH, TOUCH_FALSE);
			input_report_abs (data->idev, ABS_PRESSURE, PRESSURE_FALSE);
			input_sync (data->idev);
			/* ACER Jen chang, 2010/04/21, IssueKeys:A21.B-117, Add irq_depth detect for avoiding unbalance irq { */
			if(data->irq_depth > 0)
			{
			enable_irq(data->pen_irq);
			data->irq_depth--;
			}
			/* } ACER Jen Chang, 2010/04/21*/
		}
	}
	/* } ACER Jen chang, 2010/04/07 */
	else
	{
	/* Try to read X,Y,Z values */
	ret = tsc2007_averaging_closest_data(data, &tp);

	/* Check for prolonged touches i.e drags and drawings.
	 * If pen is still down restart the timer else report pen up and return
	 */
/* ACER Jen chang, 2009/09/22, IssueKeys:AU4.FC-62, Discard incorrect data when pressing touch panel { */
//	if (!IS_PEN_UP (data->pen_gpio))
	if (!IS_PEN_UP (data->pen_gpio) && ret == 0)
/* } ACER Jen Chang, 2009/09/22*/
	{
		if(tp.pressure < data->Ph_Threshold && tp.pressure > data->Pl_Threshold)
		{
			tsc2007_report_event(data, &tp, TOUCH_TRUE);
		}

		/* ACER Jen chang, 2009/10/29, IssueKeys:AU4.FC-144, Add single thread workq for touch panel { */
		//schedule_delayed_work(&data->work, data->restart_timer);
		tsc2007_schedule_delayed_work(&data->work, data->restart_timer);
		/* } ACER Jen Chang, 2009/10/29*/
	}
	else
	{
		tsc2007_report_event(data, &tp, TOUCH_FALSE);

		/* ACER Jen chang, 2010/04/21, IssueKeys:A21.B-117, Add irq_depth detect for avoiding unbalance irq { */
		if(data->irq_depth > 0)
		{
		enable_irq(data->pen_irq);
		data->irq_depth--;
		}
		/* } ACER Jen Chang, 2010/04/21*/
	}
}
}
/* } ACER Jen Chang, 2009/12/23*/
/* } ACER Jen chang, 2010/02/06 */

/*  IRQ handler function for penirq.The timer function is called here on pen
 *  down and also the pen up is reported here to the input subsystem.
 *  parameters:
 *	 input:
 *		irq: irq number
 * 		v: struct tsc2007_data structure
 *
 *	output:
 *		IRQ HANDLED status
 */

static irqreturn_t tsc2007_handle_penirq (int irq, void *v)
{
	struct tsc2007_data *data = (struct tsc2007_data *) v;

	disable_irq (irq);
	/* ACER Jen chang, 2009/11/05, IssueKeys:AU4.B-874, Add depth for counting enable/disable irq times { */
	data->irq_depth++;
	/* } ACER Jen Chang, 2009/11/05*/

	/* Start timer if touch is detected (pen down) */
	if (!IS_PEN_UP (data->pen_gpio))
	{
		/* ACER Jen chang, 2009/10/29, IssueKeys:AU4.FC-144, Add single thread workq for touch panel { */
		//schedule_delayed_work(&data->work, data->start_timer);
		tsc2007_schedule_delayed_work(&data->work, data->restart_timer);
		/* } ACER Jen Chang, 2009/10/29*/
	}
	else
	{
		/* ACER Jen chang, 2010/03/03, IssueKeys:A43.B-2476, Add irq_depth detect for avoiding unbalance irq { */
		if(data->irq_depth > 0)
		{
		enable_irq (irq);
		/* ACER Jen chang, 2009/11/05, IssueKeys:AU4.B-874, Add depth for counting enable/disable irq times { */
		data->irq_depth--;
		/* } ACER Jen Chang, 2009/11/05*/
		}
		/* } ACER Jen Chang, 2009/03/03*/
	}

	return IRQ_HANDLED;
}

/* *
 * Register the the TSC2007 to the kernel. Allocate a input device,
 * request IRQs and register.
 * parameters:
 *	input:
 *		data: tsc2007_data structure
 *	output:
 *		function exit status
 *
 * */

/* ACER Jen chang, 2009/10/29, IssueKeys:AU4.FC-144, Add single thread workq for touch panel { */
static int tsc2007_register_driver (struct tsc2007_data *data)
{
	struct input_dev *idev;
	int ret;

	idev = input_allocate_device ();

	if (idev == NULL)
	{
		alert_tsc ("Unable to allocate input device\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	idev->name = DRIVER_NAME;
/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	idev->dev.parent = &(data->client->dev);
#else
	idev->dev.parent = &((data->client).dev);
#endif
/* } ACER Jen chang, 2010/03/24 */
	idev->id.bustype = BUS_I2C;

	idev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	idev->keybit[BIT_WORD (BTN_TOUCH)] = BIT_MASK (BTN_TOUCH);
	idev->absbit[BIT_WORD (ABS_X)] = BIT_MASK (ABS_X);
	idev->absbit[BIT_WORD (ABS_Y)] = BIT_MASK (ABS_Y);
	idev->absbit[BIT_WORD (ABS_PRESSURE)] = BIT_MASK (ABS_PRESSURE);

	/* Informing the input subsystem the range of values that our controller
	 * can send.
	 */
	input_set_abs_params (idev, ABS_X, 0, ADC_MAX, 0, 0);
	input_set_abs_params (idev, ABS_Y, 0, ADC_MAX, 0, 0);
	input_set_abs_params (idev, ABS_PRESSURE, 0, ADC_PRESSURE_MAX, 0, 0);

	data->idev = idev;

	if (input_register_device (data->idev) != 0)
	{
		alert_tsc ("Unable to register input device: %s", __FUNCTION__);
		ret = -ENODEV;
		goto err_exit;
	}

	INIT_DELAYED_WORK(&data->work, tsc2007_timer);

	/* ACER Jen chang, 2009/10/29, IssueKeys:AU4.FC-144, Add single thread workq for touch panel { */
	data->tsc2007_workqueue = create_singlethread_workqueue("tsc2007_singlethread_wq");
	if (!(data->tsc2007_workqueue))
	{
		ret = -ENOMEM;
		goto err_exit;
	}
	/* } ACER Jen Chang, 2009/10/29*/

	/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Get pen_irq/pen_gpio from driver device for improving coding architecture { */
	data->pen_irq = platform_get_irq(tsc2007_pdev, 0);
	if(data->pen_irq < 0) {
		dev_err(&tsc2007_pdev->dev, "no irq in platform resources!\n");
		ret = -EIO;
		goto destroy_workqueue;
	}

	data->pen_gpio = EXTINT_TO_GPIO(data->pen_irq);
	/* } ACER Jen Chang, 2009/12/23*/

	/*  Set PB0 as GPIO input */
	ret = gpio_request(data->pen_gpio, MODULE_NAME);//EXTINT_TO_GPIO(data->pen_gpio));
	if(ret < 0)
	{
		alert_tsc ("Unable to request gpio\n");
		ret = -ENODEV;
		goto destroy_workqueue;
	}

	gpio_direction_input (data->pen_gpio);//EXTINT_TO_GPIO(data->pen_gpio), GPIO_DIR_INPUT);

	ret = request_irq(data->pen_irq, tsc2007_handle_penirq, 0, DRIVER_NAME, data);
	if (ret != 0)
	{
		alert_tsc ("Unable to grab IRQ\n");
		ret = -ENODEV;
		goto destroy_workqueue;
	}

	if (set_irq_type(data->pen_irq, IRQ_TYPE_EDGE_FALLING))
	{
		ret = -ENODEV;
		goto destroy_workqueue;
	}

/* ACER Jen chang, 2009/09/29, IssueKeys:AU4.FC-78, Add early suspend for entering sleep mode { */
#ifdef CONFIG_HAS_EARLYSUSPEND
/* ACER Jen chang, 2010/01/27, IssueKeys:A43.B-1149, Modify early suspend level to avoid hang { */
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 20;//EARLY_SUSPEND_LEVEL_MISC_DEVICE;
/* } ACER Jen Chang, 2010/01/27*/
	data->early_suspend.suspend = tsc2007_early_suspend;
	data->early_suspend.resume = tsc2007_late_resume;
	register_early_suspend(&data->early_suspend);
#endif
/* } ACER Jen Chang, 2009/09/29*/

	return 0;

destroy_workqueue:
	destroy_workqueue(data->tsc2007_workqueue);
err_exit:
	return ret;

}
/* } ACER Jen Chang, 2009/10/29*/

/*   I2C detect function is used to populate the client data ,probe the I2C
 *   device by sending a command and receiving an ack.Setup command is sent
 *   to configure the Rirq value and enable/use MAV filter.Attach the Device
 *   to the I2C bus and also to validate all the commands of the tsc2007
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
/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
static int tsc2007_probe(struct i2c_client *new_client, const struct i2c_device_id *id)
{
#else
static int tsc2007_detect (struct i2c_adapter *adapter, int address, int kind)
{
	struct i2c_client *new_client;
#endif
/* } ACER Jen chang, 2010/03/24 */
	struct tsc2007_data *data;
	s32 err = 0;
	u8 command_byte = 0;

	/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Add for improving coding architecture { */
	dbg_tsc("entering\n");
	if (!tsc2007_pdev) {
		printk(KERN_ERR "tsc2007: driver needs a platform_device!\n");
		err = -EIO;
		goto exit;
	}
	/* } ACER Jen Chang, 2009/12/23*/

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
	/* Check if the I2C Master supports the following functionalities*/
	if (!i2c_check_functionality (adapter, I2C_FUNC_SMBUS_BYTE_DATA
				| I2C_FUNC_I2C | I2C_FUNC_SMBUS_WORD_DATA))
	{
		alert_tsc ("I2C Functionality not supported\n");
		err = -EIO;
		goto exit;
	}
#endif
/* } ACER Jen chang, 2010/03/24 */

	data = kcalloc (1, sizeof (*data), GFP_KERNEL);

	if (NULL == data)
	{
		err = -ENOMEM;
		goto exit;
	}

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	i2c_set_clientdata(new_client, data);
	data->client = new_client;
#else
	new_client = &data->client;
	i2c_set_clientdata (new_client, data);
	new_client->addr = address;
	new_client->adapter = adapter;
	new_client->driver = &tsc2007_driver;
	new_client->flags = 0;
	strlcpy (new_client->name, "tsc2007", I2C_NAME_SIZE);
#endif
/* } ACER Jen chang, 2010/03/24 */

	/* Function to initialize the structure data */
	tsc2007_init_client (new_client);

	/*Try probing...... Send some command and wait for the ACK.If we have an
	  ack assume that its our device */
/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
	dbg_tsc ("%s: probing address: %#x\n", __FUNCTION__, address);
#endif
/* } ACER Jen chang, 2010/03/24 */
	command_byte = TSC2007_CMD (MEAS_TEMP0, data->pd, data->m);
	err = i2c_smbus_write_byte(new_client, command_byte);
	if(err)
	{
		alert_tsc ("%s\n", "Unable to probe a TSC2007 device on I2C bus");
		goto err_exit;
	}

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
	/*Device found. Attach it to the I2C bus */
	dbg_tsc ("%s: attaching a tsc2007 device on : %#x\n", __FUNCTION__, address);
	if ( i2c_attach_client (new_client))
	{
		alert_tsc ("%s: unable to attach device on : %#x\n", __FUNCTION__, address);
		goto err_exit;
	}
#endif
/* } ACER Jen chang, 2010/03/24 */
	/* ACER Jen chang, 2009/10/06, IssueKeys:AU4.FC-107, Register sys_fs group for adjusting delay timer to adjust sampling interval { */
	err = sysfs_create_group(&new_client->dev.kobj, &tsc2007_attr_group);
	if (err) {

	/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
	#ifdef CONFIG_I2C_NEW_PROBE
		printk(KERN_ERR "tsc2007: error creating sysfs group!\n");
		goto err_exit;
	#else
		alert_tsc ("%s: error creating sysfs group : %#x\n", __FUNCTION__, address);
		goto exit_detach;
	#endif
	/* } ACER Jen chang, 2010/03/24 */
	}
	/* } ACER Jen Chang, 2009/10/06*/

	/*SET UP COMMAND */

#ifdef SETUP_MAV_RIRQ
	command_byte =(u8) CURRENT_SETUP_SETTING;
	err = i2c_smbus_write_byte (new_client, command_byte);
	dbg_tsc("Setup Command 0x%x:%d\n", command_byte, err);

	/*  NOTE: Setup command has no ack and hence an error message
	 *  (i2c-adapter i2c-0: sendbytes: error - bailout) appears when
	 *  the module is inserted with the SETUP_MAV_RIRQ
	 *  MACRO defined.
	 */
#endif

	/* Option of validating all the commands of tsc2007 controller */

#ifdef VALIDATE_ALL_COMMANDS
	dbg_tsc ("TEMP0 = %#x\n", tsc2007_read (data, MEAS_TEMP0));
	dbg_tsc ("AUX0 = %#x\n", tsc2007_read (data, MEAS_AUX));
	dbg_tsc ("TEMP1 = %#x\n", tsc2007_read (data, MEAS_TEMP1));
	dbg_tsc ("XPOS = %#x\n", tsc2007_read (data, MEAS_XPOS));
	dbg_tsc ("YPOS = %#x\n", tsc2007_read (data, MEAS_YPOS));
	dbg_tsc ("ACTIVATED X DRIVERS = %#x\n",	tsc2007_read (data, ACTIVATE_X_DRIVERS));
	dbg_tsc ("ACTIVATED Y DRIVERS = %#x\n",	tsc2007_read (data, ACTIVATE_Y_DRIVERS));
	dbg_tsc ("ACTIVATED YnX DRIVERS = %#x\n", tsc2007_read (data, ACTIVATE_YnX_DRIVERS));
	dbg_tsc ("PRESSURE Z1 = %#x\n", tsc2007_read (data, MEAS_Z1));
	dbg_tsc ("PRESSURE Z2 = %#x\n", tsc2007_read (data, MEAS_Z2));
#endif

/* ACER Jen chang, 2009/10/06, IssueKeys:AU4.FC-107, Add registering error process { */
	err = tsc2007_register_driver (data);
	if (err != 0)
		goto exit_sysfs;

exit:
	return err;

exit_sysfs:
	sysfs_remove_group(&new_client->dev.kobj, &tsc2007_attr_group);

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
exit_detach:
	i2c_detach_client(new_client);
#endif
/* } ACER Jen chang, 2010/03/24 */

err_exit:
	kfree(data);
     return -ENODEV;
/* } ACER Jen Chang, 2009/10/06*/
}

/* Function to attach the i2c_client
 *
 * parameters:
 * 	input:
 * 		adapter: The global adapter structure (defined in linux\i2c.h)
 *
 * */
/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
static int tsc2007_attach (struct i2c_adapter *adapter)
{
	return i2c_probe (adapter, &addr_data, &tsc2007_detect);
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
static int __devexit tsc2007_remove(struct i2c_client *client)
{
#else
static int tsc2007_detach (struct i2c_client *client)
{
	int err = 0;
#endif
/* } ACER Jen chang, 2010/03/24 */
	struct tsc2007_data *data = i2c_get_clientdata (client);

	input_unregister_device (data->idev);
	input_free_device (data->idev);
	/* ACER Jen chang, 2009/10/29, IssueKeys:AU4.FC-144, Add single thread workq for touch panel { */
	destroy_workqueue(data->tsc2007_workqueue);
	/* } ACER Jen Chang, 2009/10/29*/
	free_irq (data->pen_irq, data);

	/* ACER Jen chang, 2009/10/06, IssueKeys:AU4.FC-107, Add detach sys_fs process { */
	sysfs_remove_group(&client->dev.kobj, &tsc2007_attr_group);
	/* } ACER Jen Chang, 2009/10/06*/

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifndef CONFIG_I2C_NEW_PROBE
	if (err = i2c_detach_client (client))
	{
		alert_tsc ("Device unregistration failed\n");
		return err;
	}
#endif
/* } ACER Jen chang, 2010/03/24 */

	return 0;
}

/* ACER Jen chang, 2009/08/27, IssueKeys:AU4.FC-47, Add touch screen suspend and resume mode for entering sleep { */
#ifdef	CONFIG_PM
static int tsc2007_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;

	struct tsc2007_data *data = i2c_get_clientdata(client);

	disable_irq(data->pen_irq);
	data->irq_depth++;

	/* ACER Jen chang, 2010/01/20, IssueKeys:A43.B-486, Modify to test touch driver hang issue { */
	cancel_delayed_work_sync(&data->work);

	#if 0
	/* ACER Jen chang, 2009/10/29, IssueKeys:AU4.FC-144, Add for ensuring that wrokq has finished { */
	ret = cancel_delayed_work(&data->work);
	if (ret == 0)
	{
		/* workq instance might be running, wait for it */
		flush_workqueue(data->tsc2007_workqueue);
	}
	/* } ACER Jen Chang, 2009/10/29*/
	/* ACER Jen chang, 2010/01/20, IssueKeys:A43.B-486, Modify to test touch driver hang issue { */
	#endif
	/* } ACER Jen Chang, 2010/01/20*/

	ret = i2c_smbus_write_byte (client, CURRENT_SETUP_SETTING);
	if (ret)
		printk(KERN_ERR "tsc2007_suspend: i2c_smbus_write_byte_data failed\n");

	return 0;
}

static int tsc2007_resume(struct i2c_client *client)
{
	struct tsc2007_data *data = i2c_get_clientdata(client);

	for(; data->irq_depth > 0; data->irq_depth--)
	{
		enable_irq(data->pen_irq);
	}

	/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Add to enable debug message from sysfs { */
	if((data->debug_state & 0x10) == 0x10)
		printk("--irq_depth=%d--\n", (data->irq_depth));
	/* } ACER Jen Chang, 2009/12/23*/

	return 0;
}
#else
#define tsc2007_suspend	NULL
#define tsc2007_resume	NULL
#endif
/* } ACER Jen Chang, 2009/08/27*/

/* ACER Jen chang, 2009/09/29, IssueKeys:AU4.FC-78, Add early suspend function for entering sleep mode { */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void tsc2007_early_suspend(struct early_suspend *h)
{
	int ret;
	struct tsc2007_data *data;
	data = container_of(h, struct tsc2007_data, early_suspend);

	disable_irq(data->pen_irq);
	data->irq_depth++;

	/* ACER Jen chang, 2010/01/20, IssueKeys:A43.B-486, Modify to test touch driver hang issue { */
	cancel_delayed_work_sync(&data->work);

	#if 0
	/* ACER Jen chang, 2009/10/29, IssueKeys:AU4.FC-144, Add for ensuring that wrokq has finished { */
	ret = cancel_delayed_work(&data->work);
	if (ret == 0)
	{
		/* workq instance might be running, wait for it */
		flush_workqueue(data->tsc2007_workqueue);
	}
	/* } ACER Jen Chang, 2009/10/29*/
	#endif
	/* } ACER Jen Chang, 2010/01/20*/

	/* ACER Jen chang, 2010/01/27, IssueKeys:A43.B-1149, Modify to test touch driver hang issue { */

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	ret = i2c_smbus_write_byte (data->client, CURRENT_SETUP_SETTING);
#else
	ret = i2c_smbus_write_byte (&data->client, CURRENT_SETUP_SETTING);
#endif
	/* } ACER Jen Chang, 2010/01/27*/
/* } ACER Jen chang, 2010/03/24 */

	/* ACER Jen chang, 2009/11/25, IssueKeys:AU4.FC-490, Remove error log of setup setting, because it won't ack anything { */
	if((data->debug_state & 0x8) == 0x8)
	printk("Tsc2007 enter early suspend!!\n");
	/* } ACER Jen Chang, 2009/11/25 */
}

static void tsc2007_late_resume(struct early_suspend *h)
{
	struct tsc2007_data *data;
	data = container_of(h, struct tsc2007_data, early_suspend);

	for(; data->irq_depth > 0; data->irq_depth--)
	{
		enable_irq(data->pen_irq);
	}

	/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Add to enable debug message from sysfs { */
	if((data->debug_state & 0x10) == 0x10)
		printk("--irq_depth=%d--\n", (data->irq_depth));
	/* } ACER Jen Chang, 2009/12/23*/
}
#endif
/* } ACER Jen Chang, 2009/09/29*/

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
static const struct i2c_device_id tsc2007_id[] = {
	{ "tsc2007", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tsc2007_id);

static struct i2c_driver tsc2007_driver = {
	.driver = {
		.name	= " tsc2007",
		.owner  = THIS_MODULE,
	},
	.id_table =  tsc2007_id,
	.probe	  =  tsc2007_probe,
	.remove	  = __devexit_p( tsc2007_remove),
	.suspend=  tsc2007_suspend,
	.resume	=  tsc2007_resume,
};
#else
static struct i2c_driver tsc2007_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name	= "tsc2007",
		.suspend= tsc2007_suspend,
		.resume	= tsc2007_resume,
	},
	.id		= I2C_DRIVERID_TSC2007,
	.attach_adapter	= tsc2007_attach,
	.detach_client	= tsc2007_detach,
};
#endif
/* } ACER Jen chang, 2010/03/24 */

/* platform driver, since i2c devices don't have platform_data */
static int __init tsc2007_plat_probe(struct platform_device *pdev)
{
	tsc2007_pdev = pdev;

	return 0;
}

static int tsc2007_plat_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver tsc2007_plat_driver = {
	.probe	= tsc2007_plat_probe,
	.remove	= tsc2007_plat_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name 	= "pnx-tsc2007",
	},
};

/* Init function of the driver called during module inserion */


static int __init tsc2007_init (void)
{
	int rc;

	if (!(rc = platform_driver_register(&tsc2007_plat_driver)))
	{
		printk("tsc2007 platform_driver_register OK !!!\n");
		if (!(rc = i2c_add_driver(&tsc2007_driver)))
		{
			printk("tsc2007 i2c_add_driver OK !!!\n");
		}
		else
		{
			printk(KERN_ERR "tsc2007 i2c_add_driver failed\n");
			platform_driver_unregister(&tsc2007_driver);
			return 	-ENODEV;
		}
	}
	else
	{
		printk("tsc2007 platform_driver_register Failed !!!\n");
	}

	return rc;

	//return i2c_add_driver (&tsc2007_driver);
}

/* EXit function of the driver called during module removal */

static void __exit tsc2007_exit (void)
{
	i2c_del_driver (&tsc2007_driver);
	platform_driver_unregister(&tsc2007_plat_driver);
}

module_init (tsc2007_init);
module_exit (tsc2007_exit);

MODULE_DESCRIPTION("TSC2007 TouchScreen Driver");
MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Joseph Robert:report bugs to joseph_robert@mindtree.com");
/* } ACER Jen Chang, 2009/06/15*/
