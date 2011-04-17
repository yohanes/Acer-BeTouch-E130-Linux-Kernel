/* magsensor.c - YAMAHA MS-3C compass driver
 * 
 * Copyright (C) 2009-2010 ACER Corporation.
 * Author: Owen YC Chang
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

#define OwenDebug 0
#define DEBUG  0

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/module.h>
#include <linux/device.h>   
#include <linux/proc_fs.h>

//#include <mach/msm_iomap.h>
//#include <mach/msm_smd.h>


/* ACER; 2009/6/10 { */
/* implement suspend/resume */
#define FEATURE_SLEEP	1
/* } ACER; 2009/6/10 */


#define PROCFILE_MS3C 1

extern struct i2c_client* get_lis331_client(void);

static struct i2c_client *ms3c_i2c_client = NULL;
static struct i2c_client *lis331x_i2c_client = NULL;

struct mag_sensor_context {
#ifdef CONFIG_I2C_NEW_PROBE
	struct i2c_client *client;
#else
	struct i2c_client client;
#endif
	struct i2c_client *activeSlave;
/* ACER; 2009/6/10 { */
/* implement suspend/resume */
#if FEATURE_SLEEP
	unsigned char bIsIdle;
#endif
/* } ACER; 2009/6/10 */
}; 

#ifndef lis331_I2C_ADDR
#define lis331_I2C_ADDR		0x19    // org: 0x38
#endif

#ifndef MS3CDRV_I2C_SLAVE_ADDRESS
#define MS3CDRV_I2C_SLAVE_ADDRESS 0x2e    // org:0x2e->OK
#endif

#define IOCTL_SET_ACTIVE_SLAVE	0x0706     // org: 0x0706 (check it in i2c-dev.h, you can see the list of i2c address)

static int ms3c_read_reg(u8* wData, int byteLen)
{
	struct mag_sensor_context *data = i2c_get_clientdata (ms3c_i2c_client);
	struct i2c_msg msgs;
	//u8 get_buf[byteLen];  
	int ret = 0;
	
	msgs.addr = data->activeSlave->addr;
	msgs.flags = I2C_M_RD;
	msgs.len = byteLen;
	msgs.buf = wData; 

	ret = i2c_transfer(data->activeSlave->adapter, &msgs, 1);

	//printk("Owen: after READ: ret=%d \n", ret);//mark by HC Lai for avoiding flooding message
	/*
	if(ret)
	{
		printk("Owen: compass_read_reg failed\n");
	}
	*/
	
	return ret;
}


static int ms3c_write_reg(u8* wData, int byteLen)
{
	struct mag_sensor_context *data = i2c_get_clientdata (ms3c_i2c_client);
	struct i2c_msg msgs;
	int ret = 0;

	msgs.addr = data->activeSlave->addr;
	msgs.flags = 0;
	msgs.len = byteLen;
	msgs.buf = wData;

	ret = i2c_transfer(data->activeSlave->adapter, &msgs, 1);

	//printk("Owen: after WRITE: ret=%d \n", ret);//mark by HC Lai for avoiding flooding message
	/*
	if(ret)
	{
		printk("Owen: compass_write_reg failed \n");
	}
		*/
	return ret;
}


static u8* read_compassid(void)
{
	u8 initReg = 0xd0;
	struct i2c_msg msgs[2];
	u8 get_buf[2];   // 3
	int ret = 0;
	u8* compassid;

	msgs[0].addr = ms3c_i2c_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &initReg;
	msgs[1].addr = ms3c_i2c_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2; 
	msgs[1].buf = get_buf;

	ret = i2c_transfer(ms3c_i2c_client->adapter, msgs, 2);
	if(ret)
	{
		printk("Owen: READ MS3C CHIP-ID failed\n");
	}
	else
	{
		printk("Owen: return [0x%x] [0x%x]  \n", get_buf[0], get_buf[1]);  
	}
	*compassid = get_buf[1];
	return compassid;
}

static int compass_write(struct file *fp, const char __user *buf, size_t count, loff_t *pos)
{
	struct mag_sensor_context *data = i2c_get_clientdata (ms3c_i2c_client);
	int ret=0;
	#if OwenDebug
	int ow=0;
	#endif
	u8 wData[count];

	if(( data->activeSlave == lis331x_i2c_client) || (data->activeSlave == ms3c_i2c_client))
	{		
		if(copy_from_user(wData, buf, count))
		{
			if (data->activeSlave == lis331x_i2c_client)
				printk("LIS331: In WRITE: copy from user FAIL \n");
			else 
				printk("MS3C: In WRITE: copy from user FAIL \n");

			ret = -EFAULT;
			return ret;
		}
		else 
		{
			ret = ms3c_write_reg(wData, count);
			#if OwenDebug						
			for (ow=0; ow<count; ow++)
			{
				if (magsensor_ctx.activeSlave == lis331x_i2c_client) 
					printk("Owen: lis331: write Data[%d] =0x%x, ret=%d \n", ow, wData[ow], ret);
				else
					printk("Owen: ms3c: write Data[%d] =0x%x, ret=%d \n", ow, wData[ow], ret);
			}
			#endif
		}
	}
	else
	{
		printk("ActiveSlave err \n");
		ret = -EFAULT;
	}
	
	if (ret == 1)
		return 0;
	else 
		return ret;
}

static int compass_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
	struct mag_sensor_context *data = i2c_get_clientdata (ms3c_i2c_client);
	int ret=0;
	u8 wData[count];
	#if OwenDebug
	int ow=0;
	#endif

	if(( data->activeSlave == lis331x_i2c_client) || (data->activeSlave == ms3c_i2c_client))
	{
		ret = ms3c_read_reg(wData, count);
		if (ret < 0) //HC Lai modify
		{
			if (data->activeSlave == lis331x_i2c_client)
				printk("i2c LIS331 read FAIL \n");
			else 
				printk("i2c MS3C read FAIL \n");
			return ret;
		}

		if (copy_to_user(buf, wData, count))//HC Lai modify
		{
			if (data->activeSlave == lis331x_i2c_client)
				printk("LIS331: After READ copy to user FAIL \n");
			else 
				printk("MS3C: After READ copy to user FAIL \n");

			return ret;
		}
		#if OwenDebug
		for (ow=0; ow<count; ow++)
		{
			if (data->activeSlave == lis331x_i2c_client)
				printk("Owen: LIS331 read: Data[%d] =0x%x, ret=%d \n",  ow, wData[ow], ret);
			else
				printk("Owen: MS3C read: Data[%d] =0x%x, ret=%d \n", ow, wData[ow], ret);
		}
		#endif
	}
	else
	{
		printk("ActiveSlave err \n");
		ret = -EFAULT;
	}
	
	if (ret == 1)
		return 0;
	else 
		return ret;
}

/* ACER; 2009/6/10 { */
/* implement suspend/resume */

#if FEATURE_SLEEP
/*
	#define lis331_MODE_NORMAL	0
	#define lis331_MODE_SLEEP	2

	#define lis331_CONF2_REG	0x30    // org: 0x15, 0x30 for INT1_CFG
	#define lis331_CTRL_REG		0x20    // org: 0x0a, 0x20 for CTRL_REG1

	#define WAKE_UP_REG		lis331_CONF2_REG
	#define WAKE_UP_POS		0
	#define WAKE_UP_LEN		1
	#define WAKE_UP_MSK		0x01

	#define SLEEP_REG		lis331_CTRL_REG
	#define SLEEP_POS		0
	#define SLEEP_LEN		1
	#define SLEEP_MSK		0x01

	#define lis331_SET_BITSLICE(regvar, bitname, val) \
					(regvar & ~bitname##_MSK) | ((val<<bitname##_POS)&bitname##_MSK)
	
	static int enter_mode(unsigned char mode)
	{
		printk("Owen: enter_mode(set mode??)\n");
		unsigned char data1, data2;
		int ret;

		if(mode==lis331_MODE_NORMAL || mode==lis331_MODE_SLEEP) 
		{
			if((ret=compass_read_reg(lis331x_i2c_client, WAKE_UP_REG, &data1, 1)))
				return ret;
		#if DEBUG
			printk(KERN_INFO "gsensor: WAKE_UP_REG (0x%x)\r\n", data1);
		#endif
			data1 = lis331_SET_BITSLICE(data1, WAKE_UP, mode);

			if((ret=compass_read_reg(lis331x_i2c_client, SLEEP_REG, &data2, 1)))
				return ret;
		#if DEBUG
			printk(KERN_INFO "gsensor: SLEEP_REG (0x%x)\r\n", data2);		
		#endif
			data2 = lis331_SET_BITSLICE(data2, SLEEP, (mode>>1));

			if((ret=compass_write_reg(lis331x_i2c_client, WAKE_UP_REG, &data1, 1)))
				return ret;
			if((ret=compass_write_reg(lis331x_i2c_client, SLEEP_REG, &data2, 1)))
				return ret;
		}

		return 0;
	}
	*/
#endif

static int start_suspend(void)
{
	printk(KERN_INFO "compass: start_suspend\r\n");

	//return enter_mode(lis331_MODE_SLEEP);   //org: unmark // for no lis331_i2c_client, add it will be reboot
	return 0;
}

static int start_resume(void)
{
	printk(KERN_INFO "compass: start_resume\r\n");
	
	//return enter_mode(lis331_MODE_NORMAL);  // org: unmark // for no lis331_i2c_client, add it will be reboot
	return 0;
	
}

static int compass_open(struct inode *inode, struct file *file)
{
	struct mag_sensor_context *data = i2c_get_clientdata (ms3c_i2c_client);

	if(data->activeSlave != NULL)
	{
		printk(KERN_ERR "warning: compass reopen\r\n");
		return 0;
	}

	//magsensor_ctx.activeSlave = ms3c_i2c_client;   // org: mark
	//magsensor_ctx.activeSlave = lis331x_i2c_client;   // org: unmark

/* ACER; 2009/6/10 { */
/* implement suspend/resume */
#if FEATURE_SLEEP
	if(data->bIsIdle == true)
	{
		if(start_resume()) {
			printk(KERN_ERR "magsensor: start_resume() fail\r\n");
			return -EIO;
		}
		else {
			//printk("Owen: magsensor_ctx.bIsIdle = false \n");
			data->bIsIdle = false;
		}
	}
#endif
/* } ACER; 2009/6/10 */
	
	return 0;
}

static int compass_release(struct inode *inode, struct file *file)
{
	struct mag_sensor_context *data = i2c_get_clientdata (ms3c_i2c_client);

	printk(KERN_INFO "compass_release()\r\n");
	
	if(data->activeSlave == NULL)
	{
		printk(KERN_ERR "warning: compass reclose\r\n");
	}

	data->activeSlave = NULL;

/* ACER; 2009/6/10 { */
/* implement suspend/resume */
#if FEATURE_SLEEP
	
	if(data->bIsIdle == false)
	{
		if(start_suspend()) {
			printk(KERN_ERR "magsensor: start_suspend() fail\r\n");
			printk("Owen: start_suspend() fail ");
		}
		else {
			data->bIsIdle = true;
		}
	}
#endif
/* } ACER; 2009/6/10 */

	return 0;
}

static int compass_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mag_sensor_context *data = i2c_get_clientdata (ms3c_i2c_client);

	// printk("Owen: in compass_ioctl, arg=%d; cmd=%d \n", arg, cmd);
#if DEBUG
	printk(KERN_INFO "compass_ioctl(): cmd (0x%X)\r\n", cmd);
#endif

	switch (cmd)
	{
	case IOCTL_SET_ACTIVE_SLAVE:
#if DEBUG
		printk(KERN_INFO "compass_ioctl(): set active slave (0x%X)\r\n", (unsigned int)arg);
#endif
		switch (arg)
			{
		case lis331_I2C_ADDR:
					if(lis331x_i2c_client == NULL)
			lis331x_i2c_client = get_lis331_client();
					data->activeSlave = lis331x_i2c_client;
			//printk("Owen: in ioctl, g_client = 0x%x, \n", lis331x_i2c_client);
#if DEBUG
			printk(KERN_INFO "compass_ioctl() active slave=0x%X (gsensor)\r\n", (unsigned int)magsensor_ctx.activeSlave);
#endif
			break;
			
		case MS3CDRV_I2C_SLAVE_ADDRESS:
					data->activeSlave = ms3c_i2c_client;
			//printk("Owen: in ioctl, ms3c_i2c_client = 0x%x, \n", ms3c_i2c_client);
#if DEBUG
			printk(KERN_INFO "compass_ioctl() active slave=0x%X (ecompass)\r\n", (unsigned int)magsensor_ctx.activeSlave);
#endif
			break;
		
		default:
			break;
		}
		break;
		
	default:
		break;
	}

	return 0;
}

#ifdef PROCFILE_MS3C
	static int ms3c_proc_read(void)
	{
	struct mag_sensor_context *data = i2c_get_clientdata (ms3c_i2c_client);

		u8 readReg = 0xd0;
		u8 recvData[2];

		if ( ms3c_i2c_client == NULL )	/* No global client pointer? */
			return -EINVAL;

	data->activeSlave = ms3c_i2c_client;

		ms3c_write_reg(&readReg, 1);
		ms3c_read_reg(recvData, 2);

		printk("eCompass stored Reg = 0x%2x \n", recvData[0]);
		printk("eCompass Chip-ID = 0x%2x \n", recvData[1]);
		
		return 0;
	}

	static int ms3c_proc_write(void)
	{
		int ret = 0;
		u8* wData;
		int byteLen = 1;

		*wData = 0x00;
		ret = ms3c_write_reg(wData, byteLen);

		return ret;
	}

#define	 MS3C_PROC_FILE	"driver/ms3ctest"

	static struct proc_dir_entry *ms3c_proc_file;

	static struct file_operations ms3c_proc_fops = {
		.owner	= THIS_MODULE,
		.read	= ms3c_proc_read,
		.write	= ms3c_proc_write,
	};

	static int create_ms3c_proc_file(void)
	{
		ms3c_proc_file = create_proc_entry(MS3C_PROC_FILE, 0644, NULL);
		if (!ms3c_proc_file) {
			printk(KERN_ERR "Create proc file for MS3C failed\n");
			return -ENOMEM;
		}

		ms3c_proc_file->proc_fops = &ms3c_proc_fops;
		return 0;
	}
#endif   // of proc_read

static struct file_operations compass_fops = {
	.owner 		= THIS_MODULE,
	.open 		= compass_open,
	.release 		= compass_release,
	.ioctl 		= compass_ioctl,
	.read 		= compass_read,
	.write 		= compass_write,	
};

//static unsigned short ignore = I2C_CLIENT_END;    // new add

#ifndef CONFIG_I2C_NEW_PROBE
static unsigned short normal_i2c[] = { 0x2E, I2C_CLIENT_END };    // new add
I2C_CLIENT_INSMOD;
#endif

static struct miscdevice compass_device = {    
.minor = MISC_DYNAMIC_MINOR,
	.name = "compass",
	.fops = &compass_fops
};

/*
static struct i2c_client_address_data addr_data = {    // new add
	.normal_i2c		= normal_i2c,
	.probe			= &ignore,
	.ignore			= &ignore,
};
*/


#ifndef CONFIG_I2C_NEW_PROBE
static int ms3c_attach(struct i2c_adapter * adapter)
{
	int ret = 0;

	ret = i2c_probe(adapter, &addr_data, &ms3c_detect_client);

	if (ret != 0)
		printk( KERN_INFO "ms3c: i2c_probe nothing\n");
	else
	       printk("Owen: ms3c: sensor _i2c_probe_adapter: ret:0x%x \n", ret);


	return ret;
}
#endif

#ifdef CONFIG_I2C_NEW_PROBE
static int __devexit ms3c_remove(struct i2c_client *client)
#else
static int ms3c_detach(struct i2c_client *client)
#endif
{
	int err;

	if (!client->adapter){
		err = -ENODEV;    /* our client isn't attached */
		goto bail;
	}

#ifndef CONFIG_I2C_NEW_PROBE
	if ((err = i2c_detach_client(client))) {
		  printk(KERN_ERR "ms3c: Client deregistration failed, client not detached.");
		goto bail;
	}
#endif

/*===============================================*/
/*ACER  Owen, 2009/10/28, Add early suspend function for entering sleep mode { */
#if 0 //#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&lis331_early_suspend_mode);
#endif
/* } ACER Owen, 2009/10/28*/
/*===============================================*/

	ms3c_i2c_client = NULL;
	kfree(client);
	return 0;

bail:
	return err;
}

#ifndef CONFIG_I2C_NEW_PROBE
static int ms3c_remove(struct i2c_client *client)
{	
	printk("Owen: in magsensor ms3c_remove \n");
	//i2c_detach_client(client);
	return 0;
}
#endif

static int ms3c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int ms3c_resume(struct i2c_client *client)
{
	return 0;
}

#ifdef CONFIG_I2C_NEW_PROBE
static int ms3c_probe(struct i2c_client *new_client, const struct i2c_device_id *id)
{
#else
static int ms3c_detect_client(struct i2c_adapter *adap, int address, int kind)                                                               
{
	struct i2c_client *new_client ;
#endif
	struct mag_sensor_context *data;
	int err;
   
	if (!(data = kzalloc(sizeof(*data),GFP_KERNEL))) {
		printk(KERN_ERR "Can't allocate memory for I2C client driver.");
		err = -ENOMEM;
		goto bail;
	}   
        
#ifdef CONFIG_I2C_NEW_PROBE
	i2c_set_clientdata(new_client, data);
	data->client = new_client;
#else
	new_client->addr = address;
	new_client->adapter = adap;
	new_client->driver = &ms3c_driver;
	new_client->flags = 0;
	strcpy(new_client->name, "MS3C");  // compass

	err = i2c_attach_client(new_client);
	if (err){
		printk(KERN_ERR "MS3C: Can't register new I2C client driver.");
		kfree(new_client);
		goto bail;
	}   
#endif

	ms3c_i2c_client = new_client;

#ifndef CONFIG_I2C_NEW_PROBE
	printk(KERN_INFO "MS3C ms3c_detect_client: address:0x%x \n", address);
#endif
	printk("Owen: After detach, ms3c_client:0x%x \n", ms3c_i2c_client);

	err = misc_register(&compass_device);
	if (err) {
	        printk(KERN_ERR "Owen: ecompass_device misc register failed\n");

#ifdef CONFIG_I2C_NEW_PROBE
		goto err_exit;
#else
		goto exit_detach;
#endif
	}
	
	#ifdef PROCFILE_MS3C
	err = create_ms3c_proc_file();
	if (err) {
			printk(KERN_ERR "%s: Failed to create MS3C proc file.\n", __func__);

		goto exit_misc_device;
		}
	#endif

	data->activeSlave = NULL;
#if FEATURE_SLEEP
	data->bIsIdle = false;
#endif

	printk("Owen: compass driver OK \n");

	return 0;

bail:
	return err;

exit_misc_device:
	misc_deregister(&compass_device);

#ifndef CONFIG_I2C_NEW_PROBE
exit_detach:
	i2c_detach_client(new_client);
#endif

err_exit:
	kfree(data);
	return -ENODEV;
}

#ifdef CONFIG_I2C_NEW_PROBE
static const struct i2c_device_id ms3c_id[] = {
	{ "MS3C", 0 },
	{ }
};

static struct i2c_driver ms3c_driver = {

	.driver = {
		   	.name 	= "MS3C",               //org: MS3C
		   	.owner  = THIS_MODULE,
			},
	.id_table = ms3c_id,
	.probe    = ms3c_probe,
	.remove   = __devexit_p(ms3c_remove),
	.suspend  = ms3c_suspend,
	.resume = ms3c_resume,
};
#else
static struct i2c_driver ms3c_driver = {

	.driver = {
		   	.name 	= "MS3C",               //org: MS3C
			.remove 	= ms3c_remove,
			.suspend	= ms3c_suspend,
			.resume	= ms3c_resume,
			},

	.attach_adapter	= ms3c_attach,
	.detach_client		= ms3c_detach,
	.id_table 			= ms3c_id,
};
#endif

static int profile = 1;

module_param(profile, int, S_IRUGO);

static int __init magsensor_init(void)
{
	int ret;	  
	
	ret = i2c_add_driver(&ms3c_driver);
	if (ret){
		printk(KERN_WARNING "MS3C: e-Compass Driver registration failed, module not inserted.\n");
	}
		
	return ret;
}

static void __exit magsensor_exit(void)
{
	i2c_del_driver(&ms3c_driver);
	//i2c_del_driver(&lis331_driver);
	misc_deregister(&compass_device);
}

module_init(magsensor_init);   // org
//late_initcall(magsensor_init);
module_exit(magsensor_exit);

MODULE_AUTHOR("OwenYCChang <owenycchang@acertdc.com>");
MODULE_DESCRIPTION("YAMAHA MS-3C compass driver");
MODULE_LICENSE("GPL");
