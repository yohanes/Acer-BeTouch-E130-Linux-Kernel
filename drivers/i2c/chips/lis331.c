/*	lis331dlh.c - ST MEMS motion sensor
 *
 *	Based on lis331dle driver written by Pasley Lin <pasleylin@tp.cmcs.com.tw>
 *
 *     Copyright (C) 2009 Stefan Chuang <stefanchchuang@tp.cmcs.com.tw>
 *     Copyright (C) 2009 Chi Mei Communication Systems Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; version 2 of the License.
 */

#undef DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/major.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include "lis331.h"
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <linux/pid.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>    //AnsonLin 200114 , STE patch for kerror channel , DDTS 31925 , issue key: AU4.B-2386

#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/delay.h>

#define DEBUG_LIS331
#define PROCFILE_LIS331

#ifdef DEBUG_LIS331
#define dbg_lis(format, arg...)	printk(KERN_DEBUG format, ## arg)
#else
#define dbg_lis(format, arg...)	do{}while(0)
#endif

/* Unique ID allocation */
#ifndef CONFIG_I2C_NEW_PROBE
static unsigned short normal_i2c[] = { 0x19, I2C_CLIENT_END };
I2C_CLIENT_INSMOD;
#endif

//static irq_handler_t lis331_isr( int irq, void * dev_id);

static struct i2c_client 	*g_client;

static int lis331_read(u8 reg, u8 *pval)
{
	int ret;

	if ( g_client == NULL )	/* No global client pointer? */
		return -EINVAL;

	ret = i2c_smbus_read_byte_data( g_client, reg );

	if (ret >= 0)
	{
		*pval = ret;
		ret = 0;
	}
	else
		ret = -EIO;

	return ret;
}

static int lis331_write(u8 reg, u8 val)
{
	int ret;

	if( g_client == NULL )	/* No global client pointer? */
		return -1;

	ret = i2c_smbus_write_byte_data( g_client, reg, val );
	if( ret == 0 )	ret = 0;
	else	ret = -EIO;

	return ret;
}

void lis331_read_xyz(struct lis331_xyz_data * xyz)
{
	struct lis_platform_data *data = container_of(xyz, struct lis_platform_data, xyz_data);
	u8 val = 0;
	short x = 0, y = 0, z = 0;

	memset(xyz, 0, sizeof(struct lis331_xyz_data));

	lis331_read(OUT_X_L, &val);
	x |= val;
	//dbg_lis("LIS331DLH X_L: %d\n", (signed short)val);
	lis331_read(OUT_X_H, &val);
	//dbg_lis("LIS331DLH X_H: %d\n", (signed short)val);
	x |= (val << 8);
	val = 0;

	lis331_read(OUT_Y_L, &val);
	y |= val;
	//dbg_lis("LIS331DLH Y_L: %d\n", (signed short)val);
	lis331_read(OUT_Y_H, &val);
	//dbg_lis("LIS331DLH Y_H: %d\n", (signed short)val);
	y |= (val << 8);
	val = 0;

	lis331_read(OUT_Z_L, &val);
	z |= val;
	//dbg_lis("LIS331DLH Z_L: %d\n", (signed short)val);
	lis331_read(OUT_Z_H, &val);
	//dbg_lis("LIS331DLH Z_H: %d\n", (signed short)val);
	z |= (val << 8);

	//dbg_lis("LIS331DLH X: %d\n", (signed short)x);
	//dbg_lis("LIS331DLH Y: %d\n", (signed short)y);
	//dbg_lis("LIS331DLH Z: %d\n", (signed short)z);

	spin_lock(&data->lock);
/* ACER Erace.Ma@20100209, GSensor layout in K2 is clockwise 90 degrees than AU4*/
#if defined ACER_L1_K2
	xyz->x = (y >> 4);
	xyz->y = -(x >> 4);
#else
	xyz->x = -(x >> 4);
	xyz->y = -(y >> 4);
#endif
/* ACER Erace.Ma@20100209 */
	xyz->z = (z >> 4);
	spin_unlock(&data->lock);
}

static int lis331_selftest(int status)
{
        u8 val;

        if (status)
        {
                lis331_read(CTRL_REG4, &val);
                val |= CTRL_REG4_ST;
                if (lis331_write(CTRL_REG4, val) < 0)
                {
                        printk(KERN_ERR "LIS331: enable selftest failed!\n");
                        return -1;
                }
                dbg_lis("LIS331: enable selftest OK!\n");
        }
        else
        {
                lis331_read(CTRL_REG4, &val);
                val &= ~(CTRL_REG4_ST);
                if (lis331_write(CTRL_REG4, val) < 0)
                {
                        printk(KERN_ERR "LIS331: disable selftest failed!\n");
                        return -1;
                }
                dbg_lis("LIS331: disable selftest OK!\n");
        }

        return 0;
}

static void lis331_reset( void)
{
	u8 val;

	dbg_lis("LIS331: reset!!!!\n");

	lis331_read(INT1_CFG, &val);
	val |= INT1_CFG_XHIE | INT1_CFG_YHIE | INT1_CFG_ZHIE;
	lis331_write(INT1_CFG, val);

	lis331_read(INT1_THS, &val);
	val = 20;
	lis331_write(INT1_THS, val);

	lis331_read(INT1_DURATION, &val);
	val = 16;
	lis331_write(INT1_DURATION, val);
}

static int lis331_power_up(int status)
{
	struct lis_platform_data *data = i2c_get_clientdata(g_client);
	char tmp;

	if ((status == 1) && (data->power_state == 0))
	{
		dbg_lis("LIS331: power UP\n");
		lis331_read(CTRL_REG1, &tmp);
		tmp |= CTRL_REG1_PM0 | CTRL_REG1_DR0; // power up and data rate set to 100hz
		if (lis331_write(CTRL_REG1, tmp) < 0)
		{
			printk(KERN_ERR "LIS331: power up FAILED!\n");
			return -1;
		}
		lis331_reset();
		mod_timer(&data->timer, jiffies + (HZ / 5));
		/* ACER OwenYCChang AU2.B-3983 add for check Lis331 power status @2010/02/06*/
		mutex_lock(&data->update_lock);
		data->power_state = 1;
    	mutex_unlock(&data->update_lock);
		/* End of Owen */
	}
	else if ((status == 0) && (data->power_state == 1))
	{
		/* ACER OwenYCChang AU2.B-3983 add for check Lis331 power status @2010/02/06*/
		mutex_lock(&data->update_lock);
		data->power_state = 0;
    	mutex_unlock(&data->update_lock);
		/* End of owen */
		dbg_lis("LIS331: power DOWN\n");
		lis331_read(CTRL_REG1, &tmp);
		tmp &= ~(CTRL_REG1_PM0);
		if (lis331_write(CTRL_REG1, tmp) < 0)
			printk(KERN_ERR "LIS331: power down FAILED!\n");
	}
//	else
//		printk(KERN_ERR "LIS331: unknown power_up param: %d\n", status);

	return 0;
}

/* ACER OwenYCChang AU2.B-3983 add for ecoompass used @2010/02/06*/
struct i2c_client* get_lis331_client(void)
{
	lis331_power_up(1);

	return g_client;
}
/* End of OwenYCChang AU2.B-3983 @2010/02/06 */

static int lis331_who_am_i(struct lis_platform_data *data)
{
/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Modify to meet new i2c architecture { */
#ifdef CONFIG_I2C_NEW_PROBE
	struct i2c_client *new_client = data->client;
#else
	struct i2c_client *new_client = &data->client;
#endif
/* } ACER Jen chang, 2010/03/24 */
	printk( KERN_INFO "lis331 who am i\n" );

	data->chipid = i2c_smbus_read_byte_data( new_client, WHO_AM_I );

	if ( data->chipid != I_AM_LIS331_DLH )
	{
		printk(KERN_ERR "LIS331DLH unavailable!\n");
		return -ENXIO;
	} else
		printk( KERN_INFO "LIS331(chip id:0x%02x) detected.\n", data->chipid );

	return 0;
}

static int lis331dle_dev_open( struct inode * inode, struct file * file )
{
	dbg_lis("LIS331DLH open\n" );

	if (lis331_power_up(1))
	{
		printk(KERN_ERR "LIS331DLH: failed to power on\n");
		return -1;
	}

	if( ( file->f_flags & O_ACCMODE ) == O_WRONLY )
	{
		printk(KERN_ERR "LIS331DLH's device node is readonly\n" );
		return -1;
	}
	else
		return 0;
}

static ssize_t lis331dle_dev_read( struct file * file, char __user * buffer, size_t size, loff_t * f_pos )
{
        /* 20100114 AnsonLin for AU4.B-2386 begin */
        /* STE patch for kerror channel , DDTS 31925 , issue key: AU4.B-2386 */
	struct lis_platform_data *data = i2c_get_clientdata(g_client);
	struct lis331_xyz_data xyz_temp;
	int  ret = 0;

	spin_lock(&data->lock);
	xyz_temp.x = data->xyz_data.x;
	xyz_temp.y = data->xyz_data.y;
	xyz_temp.z = data->xyz_data.z;
	spin_unlock(&data->lock);

	ret = copy_to_user(buffer, &xyz_temp, sizeof(struct lis331_xyz_data));

        /* 20100114 AnsonLin for AU4.B-2386 end */
	return sizeof(struct lis331_xyz_data);

}

#define POWERON			0x01
#define POWERDOWN		0x02
#define SELFTEST_ENABLE		0x03
#define SELFTEST_DISABLE	0x04
static int lis331dle_dev_ioctl( struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg )
{
	switch (cmd)
	{
		case POWERON:
			lis331_power_up(1);
			break;
		case POWERDOWN:
			lis331_power_up(0);
			break;
		case SELFTEST_ENABLE:
			lis331_selftest(1);
			break;
		case SELFTEST_DISABLE:
			lis331_selftest(0);
			break;
		default:
			printk(KERN_ERR "unknown command %d\n", cmd);
			return -1;
			break;
	}
	return 0;
}

static int lis331dle_dev_release( struct inode * inode, struct file * filp )
{
	if (lis331_power_up(0)) {
		printk("LIS331DLH: failed to power down\n");
		return -1;
	}
	return 0;
}

static const struct file_operations lis331dle_dev_fops = {
	.open = lis331dle_dev_open,
	.read = lis331dle_dev_read,
	.ioctl = lis331dle_dev_ioctl,
	.release = lis331dle_dev_release,
	.owner = THIS_MODULE,
	//.poll = lis331dle_dev_poll,
};

static struct miscdevice gsensor_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &lis331dle_dev_fops
};

#ifdef PROCFILE_LIS331
#define	LIS331_PROC_FILE	"driver/lis331dlh"

static struct proc_dir_entry *lis331_proc_file;

static int lis331_proc_read( struct file * filp, char * buffer, size_t length, loff_t * offset )
{
	struct lis_platform_data *data = i2c_get_clientdata(g_client);

	u8 val;
	short x, y ,z;

	dbg_lis("lis331_proc_read()\n\n");

	lis331_power_up(1);

	lis331_who_am_i(data);

	spin_lock(&data->lock);
	x = data->xyz_data.x;
	y = data->xyz_data.y;
	z = data->xyz_data.z;
	spin_unlock(&data->lock);

	printk("LIS331DLH X: %d\n", (signed short)x);
	printk("LIS331DLH Y: %d\n", (signed short)y);
	printk("LIS331DLH Z: %d\n\n", (signed short)z);

	lis331_read(STATUS_REG, &val);
	dbg_lis("LIS331DLH STATUS_REG: 0x%x\n", val);

	lis331_read(CTRL_REG1, &val);
	dbg_lis("LIS331DLH CTRL_REG1: 0x%x\n", val);

	lis331_read(CTRL_REG2, &val);
	dbg_lis("LIS331DLH CTRL_REG2: 0x%x\n", val);

	lis331_read(CTRL_REG3, &val);
	dbg_lis("LIS331DLH CTRL_REG3: 0x%x\n", val);

	lis331_read(CTRL_REG4, &val);
	dbg_lis("LIS331DLH CTRL_REG4: 0x%x\n", val);

	lis331_read(CTRL_REG5, &val);
	dbg_lis("LIS331DLH CTRL_REG5: 0x%x\n", val);

	lis331_read(INT1_CFG, &val);
	dbg_lis("LIS331DLH INT1_CFG: 0x%x\n", val);

	lis331_read(INT1_THS, &val);
	dbg_lis("LIS331DLH INT1_THS: 0x%x\n", val);

	lis331_read(INT1_SOURCE, &val);
	dbg_lis("LIS331DLH INT1_SOURCE: 0x%x\n", val);

	lis331_read(INT1_DURATION, &val);
	dbg_lis("LIS331DLH INT1_DURATION: 0x%x\n", val);
	return 0;
}

static int lis331_proc_write( struct file * filp, char * buffer, size_t length, loff_t * offset )
{
	char cmd[ 128 ];

	if( length > 128 )
		return 0;

	if( copy_from_user( cmd, buffer, length ) )
		return -EFAULT;

	if( strncmp( cmd, "poweron", 7 ) == 0 )
		lis331_power_up(1);
	else if( strncmp( cmd, "poweroff", 8 ) == 0 )
		lis331_power_up(0);
	else
		printk( KERN_ERR "%s: not supported proc command\n", __func__ );

	return length;
}

static struct file_operations lis331_proc_fops = {
	.owner	= THIS_MODULE,
	.read	= lis331_proc_read,
	.write	= lis331_proc_write,
};

static int create_lis331_proc_file(void)
{
	lis331_proc_file = create_proc_entry(LIS331_PROC_FILE, 0644, NULL);
	if (!lis331_proc_file) {
		printk(KERN_ERR "Create proc file for LIS331DLH failed\n");
		return -ENOMEM;
	}

	dbg_lis("LIS331DLH proc OK\n" );
	lis331_proc_file->proc_fops = &lis331_proc_fops;
	return 0;
}

static void remove_lis331_proc_file(struct proc_dir_entry *proc_file)
{
	remove_proc_entry(proc_file->name, &proc_file);
}
#endif /*PROCFILE_LIS331*/

#define lis331_suspend	NULL
#define lis331_resume	NULL

/*===============================================*/
/*ACER  Paul Lo, 2009/10/28, Add early suspend function for entering sleep mode { */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void lis331_early_suspend(struct early_suspend *h)
{
	struct lis_platform_data *data = container_of(h, struct lis_platform_data, early_suspend);

	dbg_lis("LIS331: entering early suspend!!\n");
	data->_inearlysuspend = 1;
	//lis331_power_up(0);
}

static void lis331_late_resume(struct early_suspend *h)
{
	struct lis_platform_data *data = container_of(h, struct lis_platform_data, early_suspend);

	dbg_lis("LIS331: exited early suspend!!\n");
	data->_inearlysuspend = 0;
	//lis331_power_up(1);
}
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/* } ACER Paul Lo, 2009/10/28*/
/*===============================================*/

static void lis331_timer(unsigned long arg)
{
	struct lis_platform_data *data = i2c_get_clientdata(g_client);

	queue_work(data->lis331_wq, &data->work_data); //AnsonLin 200114 , STE patch for kerror channel , DDTS 31925 , issue key: AU4.B-2386
}

static void soft_irq(struct work_struct *work)
{
	struct lis_platform_data *data = container_of(work, struct lis_platform_data, work_data);
	u8 val;

	lis331_read(STATUS_REG, &val);
	if (val & STATUS_REG_ZYXDA)
		lis331_read_xyz(&data->xyz_data);

  if(!data->_inearlysuspend)
  {
	mod_timer(&data->timer, jiffies + ( HZ / 5));
	}

	return;
}

#ifdef CONFIG_I2C_NEW_PROBE
static int lis331_probe(struct i2c_client *new_client, const struct i2c_device_id *id)
{
#else
static int lis331_detect_client(struct i2c_adapter *adap, int address, int kind)
{
	struct i2c_client *new_client ;
#endif
	struct lis_platform_data *data;
	int err;

	dbg_lis("LIS331: lis331_detect_client\n");

	data = kcalloc (1, sizeof (*data), GFP_KERNEL);
	if (NULL == data)
	{
		printk(KERN_ERR "LIS331: Can't allocate memory for lis331 data.");
		err = -ENOMEM;
		goto bail;
	}

#ifdef CONFIG_I2C_NEW_PROBE
	i2c_set_clientdata(new_client, data);
	data->client = new_client;
#else
	new_client = &data->client;
	i2c_set_clientdata (new_client, data);
	new_client->addr = address;
	new_client->adapter = adap;
	new_client->driver = &lis331_driver;
	new_client->flags = 0;
	strcpy(new_client->name, "lis331_gsensor");

	err = i2c_attach_client(new_client);
	if (err){
		printk(KERN_ERR "LIS331: Can't register new I2C client driver.");
		goto err_exit;
	}
#endif

	g_client = new_client;

#ifndef CONFIG_I2C_NEW_PROBE
	printk(KERN_INFO "LIS331 lis331_detect_client: address:0x%x \n", address);
#endif

	err = lis331_who_am_i(data);
	if( err != 0 )
	{
		printk("Owen: LIS331 after check who_am_i, ret=%d \n", err);
	#ifdef CONFIG_I2C_NEW_PROBE
		goto err_exit;
	#else
		goto exit_detach;
	#endif
	}

	err = misc_register(&gsensor_device);
        if (err) {
                printk(KERN_ERR "LIS331: g-sensor_device misc register failed\n");

	#ifdef CONFIG_I2C_NEW_PROBE
		goto err_exit;
	#else
		goto exit_detach;
	#endif
        }

#ifdef PROCFILE_LIS331
	err = create_lis331_proc_file();
	if (err) {
		printk(KERN_ERR "%s: Failed to create LIS331 proc file.\n",	__func__);

		goto exit_misc_device;
	}
#endif

	data->power_state = 0;
	data->chipid = 0;

	printk("LIS331: i2c_add_driver() OK!\n" );

		/*ACER  Paul Lo, 2009/10/28, Add early suspend function for entering sleep mode { */
  #ifdef CONFIG_HAS_EARLYSUSPEND
	data->_inearlysuspend = 0;
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_MISC_DEVICE;
	data->early_suspend.suspend = lis331_early_suspend;
	data->early_suspend.resume = lis331_late_resume;
	register_early_suspend(&data->early_suspend);
#endif
 /* } ACER Paul Lo, 2009/10/28*/

	spin_lock_init(&data->lock);
	data->lis331_wq = create_workqueue("lis331_wq");  //AnsonLin 200114 , STE patch for kerror channel , DDTS 31925 , issue key: AU4.B-2386
	INIT_WORK(&data->work_data, soft_irq);
	setup_timer(&data->timer, lis331_timer, NULL);
	mutex_init(&data->update_lock);

	return 0;

bail:
	return err;

exit_misc_device:
	misc_deregister(&gsensor_device);

#ifndef CONFIG_I2C_NEW_PROBE
exit_detach:
	i2c_detach_client(new_client);
#endif

err_exit:
	kfree(data);
	return -ENODEV;
}

#ifndef CONFIG_I2C_NEW_PROBE
static int lis331_attach(struct i2c_adapter * adapter)
{
	int ret = 0;

	ret = i2c_probe(adapter, &addr_data, &lis331_detect_client);
	if (ret != 0)
		printk( KERN_INFO "LIS331: i2c_probe nothing\n");
	else
		dbg_lis("LIS331: sensor_i2c_probe_adapter: ret:0x%x \n", ret);

	return ret;
}
#endif

#ifdef CONFIG_I2C_NEW_PROBE
static int __devexit lis331_remove(struct i2c_client *client)
#else
static int lis331_detach(struct i2c_client *client)
#endif
{
	struct lis_platform_data *data = i2c_get_clientdata (client);

	int err;

	if (!client->adapter){
		err = -ENODEV;    /* our client isn't attached */
		goto bail;
	}

#ifndef CONFIG_I2C_NEW_PROBE
	if ((err = i2c_detach_client(client))) {
		  printk(KERN_ERR "LIS331: Client deregistration failed, client not detached.");
		goto bail;
	}
#endif

/*===============================================*/
/*ACER  Paul Lo, 2009/10/28, Add early suspend function for entering sleep mode { */
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
/* } ACER Paul Lo, 2009/10/28*/
/*===============================================*/

	g_client = NULL;
	kfree(client);
	return 0;

bail:
	return err;
}

#ifdef CONFIG_I2C_NEW_PROBE
static const struct i2c_device_id lis331_id[] = {
	{ "lis331_gsensor", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lis331_id);

static struct i2c_driver lis331_driver = {
	.driver = {
		.name	= " lis331_gsensor",
		.owner  = THIS_MODULE,
	},
	.id_table =  lis331_id,
	.probe	  =  lis331_probe,
	.remove	  = __devexit_p( lis331_remove),
	.suspend=  lis331_suspend,
	.resume	=  lis331_resume,
};
#else
static struct i2c_driver lis331_driver = {
	.driver = {
		.name	= "lis331_gsensor",
	},
	.attach_adapter = lis331_attach,
	.detach_client  = lis331_detach,
};
#endif

static int __init lis331_init(void)
{
	int ret;

	dbg_lis("LIS331DLH init\n" );

	ret = i2c_add_driver(&lis331_driver);
	if (ret) {
		printk(KERN_WARNING "LIS331: Driver registration failed, module not inserted.\n");
	}

	return ret;
}

static void __exit lis331_exit(void)
{
	dbg_lis("LIS331DLH exit\n" );
	i2c_del_driver(&lis331_driver);
	misc_deregister(&gsensor_device);
#ifdef PROCFILE_LIS331
	remove_lis331_proc_file(lis331_proc_file);
#endif
}


module_init(lis331_init);
module_exit(lis331_exit);

//dark add 2010.01.13 for Gsensor feature
#ifndef FEATURE_GSENSOR_DISABLE
//subsys_initcall(lis331_init);

#endif

MODULE_AUTHOR( "Stefan Chuang <stefanchchuang@tp.cmcs.com.tw>" );
MODULE_DESCRIPTION( "LIS331DLH driver" );
MODULE_LICENSE( "GPL" );
