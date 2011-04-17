/*	tea5991dlh.c - ST MEMS motion sensor
 *	
 *	Based on tea5991dle driver written by Pasley Lin <pasleylin@tp.cmcs.com.tw>
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
#include "tea5991.h"
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <linux/pid.h>
#include <linux/miscdevice.h>

#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/delay.h>

#define DEBUG_TEA5991
#define PROCFILE_TEA5991


#ifdef DEBUG_TEA5991
#define dbg_lis(format, arg...)	printk(KERN_ERR format, ## arg)
#else
#define dbg_lis(format, arg...)	do{}while(0)
#endif


/*===============================================*/
/*ACER  Paul Lo, 2009/10/28, Add early suspend function for entering sleep mode { */
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>`
static int _inearlysuspend;
#endif
/* } ACER Paul Lo, 2009/10/28*/
/*===============================================*/

#define K2_FM_TEST
/* Unique ID allocation */
/*tea5991 FM IC*/
static unsigned short normal_i2c[] = { 0x11, I2C_CLIENT_END };

//static unsigned short ignore = I2C_CLIENT_END;    // new add

/*
static struct i2c_client_address_data addr_data = {    // new add
	.normal_i2c		= normal_i2c,
	.probe			= &ignore,
	.ignore			= &ignore,
};

*/

I2C_CLIENT_INSMOD;    // Owen

static struct i2c_client 	*tea5991_client;

static struct timer_list 	timer;
//spinlock_t          		lock;

static int tea5991_attach(struct i2c_adapter * adapter);
static int tea5991_detach(struct i2c_client *pClient);
static int tea5991_suspend( struct i2c_client * client, pm_message_t mesg);
static int tea5991_resume( struct i2c_client * client);
static int tea5991_power_up(void);
static int tea5991_power_up_chk(void);
static int create_tea5991_proc_file(void);
static int fm_read_reg(u8* wData, int byteLen);
static int fm_write_reg(u8* wData, int byteLen);


static struct i2c_driver tea5991_driver = {
	.driver = {
		.name	= "Tea5991_FM",   // tea5991_fm
	},
	.attach_adapter = tea5991_attach,
	.detach_client  = tea5991_detach,
};


struct FM_State
{
	int vol;
	int band;
	int muteState;
	int monoStereoState;
	int frequency;
	int rssiData;
	int rssiThreshold;
	int rdsState;
	char rdsData[12];
};

struct FM_State Tea5991State;


#define CMD_HEADER 0X00
#define RSP_HEADER 0X08

static int tea5991_power_up(void)
{
	u8 buffer[] = {0x00, 0x0a, 0x09, 0x37, 0x5d};
	int ret = 0;

	printk("Owen: power up tea5991\n");

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 5);

	printk("Owen: After Power-up, ret=%d \n", ret);

	
bail:
	return ret;
}

static int tea5991_power_down(void)
{
	u8 buffer[] = {0x00, 0x04, 0x08};
	int ret = 0;

	printk("Owen: power down tea5991\n");

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 3);

	printk("Owen: After Power-down, ret=%d \n", ret);

	
bail:
	return ret;
}

static int tea5991_SetBand(int radioRegion)    // 0-US/EN, 1-JPN, 2-CHINA
{
	u8 buffer[] = {0x00, 0x01, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x01, 0x9A};
	int ret = 0;

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	buffer[4] = radioRegion;

	ret = fm_write_reg(buffer, 9);
	printk("Owen: After FM set Band, ret=%d \n", ret);

bail:
	return ret;
}

static int tea5991_Tune(void)   
{
	int ret = 0;
	return ret;
}



static int tea5991_GetTunedFrequency(void)
{
	u8 buffer[] = {0x00, 0x1D, 0x18};
	int ret = 0;
	u8 getBuf[4];
	int tempChannel = 0;
	
	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 3);
	printk("Owen: After GetTunedFrequency's write, ret=%d \n", ret);

	ret = fm_read_reg(getBuf, 4);
	printk("Owen: After GetTunedFrequency's read, ret=%d \n", ret);

	tempChannel = getBuf[2]*16*16+getBuf[3];
	Tea5991State.frequency = (tempChannel/20) + 875;
	printk("Owen: get frequency = %d \n", Tea5991State.frequency);

bail:
	return ret;
}

static int tea5991_SetMonoStereoMode(int monoStereoState)
{
	u8 buffer[] = {0x00, 0x32, 0x1A, 0x00, 0x01, 0x00, 0x01};
	int ret = 0;

	buffer[4] = monoStereoState;   // 0-forced mono, 1-mono/stereo    ,2-forced stereo
	
	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 3);
	printk("Owen: After SetMonoStereoMode's write, ret=%d \n", ret);

bail:
	return ret;
}

static int tea5991_GetMonoStereoMode(void)  
{
	
	return Tea5991State.monoStereoState;
}


static int tea5991_SetMuteMode(int muteState)
{
	u8 buffer[] = {0x00, 0x03, 0x12, 0x00, 0x01, 0x00, 0x00};
	int ret = 0;

	buffer[4] = muteState;   // 0-Mute-unset, 1-Mute-set   
	
	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 3);
	printk("Owen: After SetMuteMode's write, ret=%d \n", ret);

bail:
	return ret;
}

	
static int tea5991_GetMuteMode(void)  
{
	return Tea5991State.muteState;
}

static int tea5991_SetRSSIThreshold(int rssiThreshold)
{
	u8 buffer[] = {0x00, 0x05, 0x19, 0x00, 0x00};
	int ret = 0;
	
	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}
	buffer[3] = ( rssiThreshold & 0xFF00 ) >>8;
	buffer[4] = rssiThreshold & 0xFF;

	ret = fm_write_reg(buffer, 3);
	printk("Owen: After SetRSSIThreshold's write, ret=%d \n", ret);

bail:
	return ret;
}

	
static int tea5991_GetRSSIThreshold(void)
{
	u8 buffer[] = {0x00, 0x1D, 0x18};
	u8 getBuf[6];
	int ret = 0;
	
	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 3);
	printk("Owen: After GetRSSIThreshold's write, ret=%d \n", ret);

	ret = fm_read_reg(getBuf, 6);
	printk("Owen: After GetRSSIThreshold's read, ret=%d \n", ret);

bail:
	return ret;
}


static int tea5991_goto_mode(void)
{

	u8 buffer[] = {0x00, 0x02, 0x09, 0x00, 0x01};
	int ret = 0;

	printk("Leon debug: tea5991_goto_mode tea5991\n");

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 5);
	printk("Owen: After gotoMode 's write, ret=%d \n", ret);

bail:
	return ret;
}


static int tea5991_SetAudioDAC(void)
{
	u8 buffer[5]= {0x00, 0x04, 0x11, 0x00, 0x01};
	int ret = 0;

	printk("Leon debug: tea5991_SetAudioDAC tea5991\n");

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 5);
	printk("Owen: After setAudioDAC, ret=%d \n", ret);

bail:
	return ret;
}



static int tea5991_SetFrequency(u8 *interChannel)
{
	u8 buffer[5];
	int ret = 0;

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}
	// $22 $00 $1E $19 $00 $40
	buffer[0] = 0x00;
	buffer[1] = 0x1E;
	buffer[2] = 0X19;
	buffer[3] = interChannel[0];
	buffer[4] = interChannel[1];

	ret = fm_write_reg(buffer, 5);
	printk("Owen: After SetChannel, ret=%d \n", ret);
	
bail:
	return ret;
}


static int tea5991_SetVolume(unsigned int vol)
{
	u8 buffer[5];
	int ret = 0;
	
	if ((vol < 0) || (vol > 32767))      // 32767 = 0x7FFF
	{
		printk("Owen: volume out of SPEC \n ");
		ret = -ENODEV;
		goto bail;
	}	
	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}
	// $22 $00 $1E $19 $00 $40
	buffer[0] = 0x00;
	buffer[1] = 0x01;
	buffer[2] = 0x11;

	if (vol== 1){
		buffer[3] = 0x00;
		buffer[4] = 0x00;
	}
	else if (vol==5){
		buffer[3] = 0x3F;
		buffer[4] = 0xFF;
	}
	else if (vol==10){
		buffer[3] = 0x7F;
		buffer[4] = 0xFF;
	}
	
	ret = fm_write_reg(buffer, 5);
	Tea5991State.vol = 10;
	printk("Owen: In tea5991_SetVolume, after write, ret=%d \n", ret);
		
bail:
	return ret;
}

static int tea5991_GetVolume(void)
{
	int ret = 0;
	return ret;
	
}


static int tea5991_Seek(void)
{
	u8 buffer[] = {0x00, 0x20, 0x1A, 0x00, 0x01, 0x00, 0x80};   // RSSI = 0x0080
	int ret = 0;
	

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}
	
	ret = fm_write_reg(buffer, 7);
	printk("Owen: In tea5991_Seek, after write, ret=%d \n", ret);
		
bail:
	return ret;
}

static int tea5991_StopSeek(void)
{
	u8 buffer[] =  {0x00, 0x1C, 0x18};
	int ret = 0;
	

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 3);
	printk("Owen: In tea5991_StopSeek, after write, ret=%d \n", ret);
		
bail:
	return ret;
}

static int tea5991_SeekUp(void)
{
	u8 buffer[] =  {0x00, 0x1F, 0x1C, 0x00, 0x00, 0x00, 0x01, 0x00, 0x45, 0x00, 0x40}; 
	//0x0045, 0x0040 -> noise coarse threshold and noise final threshold
	int ret = 0;
	

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 3);
	printk("Owen: In tea5991_SeekUp, after write, ret=%d \n", ret);
		
bail:
	return ret;
}

static int tea5991_SeekDown(void)
{
	u8 buffer[] =  {0x00, 0x1F, 0x1C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x45, 0x00, 0x40};
	//0x0045, 0x0040 -> noise coarse threshold and noise final threshold
	int ret = 0;
	

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 3);
	printk("Owen: In tea5991_SeekDown, after write, ret=%d \n", ret);
		
bail:
	return ret;
}


static int tea5991_EnableRDS(void)
{
	int ret = 0;
	return ret;
}

static int tea5991_DisableRDS(void)
{
	int ret = 0;
	return ret;
}


static int tea5991_SetRDSSystem(int rdsState)   // 0-off, 1-basic, 2-enhance
{
	u8 buffer[] = {0x00, 0x15, 0x19, 0x00, 0x01};
	int ret = 0;												

	buffer[4] = rdsState;
	
	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_write_reg(buffer, 5);
	printk("Owen: After SetRDSSYstem 's write, ret=%d \n", ret);
	Tea5991State.rdsState = rdsState;
	

bail:
	return ret;
}

	
static int tea5991_GetRDSSystem(void)
{
	int ret = 0;
	return ret;	
}

static int tea5991_GetRSSI(void)
{
	u8 getBuf[4];
	u8 setBuf[]= {0x00, 0x04, 0x18};
	int ret = 0;
	//int rssiData = 0;

	// i2c Data :   $06 $0E $00 $2B $59 $91 $00 $01 $00 $05 $00 $13
	printk("Owen: In tea5991_get_RSSI, now ready for write RSSI(1) \n");

	ret = fm_write_reg(setBuf, 3);	
	printk("Owen: In tea5991_get_RSSI, after get RSSI write, ret=%d \n", ret);	

	ret = fm_read_reg(getBuf, 4);
	printk("Owen: In tea5991_get_RSSI, after get RSSI read, ret=%d \n", ret);
	
	Tea5991State.rssiData = getBuf[2]*16*16+getBuf[3];
	printk("Owen: RSSI data = %d \n", Tea5991State.rssiData);
	
	return ret;
}


static int tea5991_deviceID_write(void)
{
	u8 buffer[3];
	int ret = 0;


	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}
	// $22 $00 $06 $08

	printk("Owen: check read Tea5991 chip-id seq \n");
	
	buffer[0] = 0x00;
	buffer[1] = 0x06;
	buffer[2] = 0x08;

	ret = fm_write_reg(buffer, 3);
	printk("Owen: After write in drviceIDwrite, ret=%d \n", ret);


bail:
	return ret;
}

static int tea5991_deviceID_read(void)
{
	u8 get_buf[12];
	u8 set_buf = RSP_HEADER;
	int ret = 0, i=0;

	// i2c Data :   $06 $0E $00 $2B $59 $91 $00 $01 $00 $05 $00 $13
	printk("Leon debug: tea5991_deviceID_read\n");

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_read_reg(get_buf, 12);
	printk("Owen: After write in drviceIDread, ret=%d \n", ret);

	return ret;


bail:
	return ret;
}

static int  tea5991_freq_test()
{
printk("Leon debug: tea5991_freq_test\n");

	#if 0
	tea5991_power_up();
	mdelay(1);
	tea5991_goto_mode();
		mdelay(1);
	tea5991_SetAudioDAC();
		mdelay(5);
	tea5991_SetChannel();
	#endif
	//tea5991_power_up_chk();
	tea5991_deviceID_write();
	//mdelay(5);   // Owen marked
	tea5991_deviceID_read();
	printk("Leon debug: tea5991_freq_test end\n");

	return 0;
}

static int tea5991_power_up_chk(void)
{
	u8 get_buf[3];
	u8 set_buf = RSP_HEADER;
	int ret = 0;

	printk("Leon debug: check tea5991 power up state \n");

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	ret = fm_read_reg(get_buf, 3);

	printk("Owen: after power_up_chk, ret=%d \n", ret);
	printk("Leon debug: return [0x%x] [0x%x] [0x%x]\n", get_buf[0], get_buf[1], get_buf[2]);

bail:
	return ret;
}



static int tea5991dle_dev_open( struct inode * inode, struct file * file )
{
	printk("Owen: in dev_open: tea5991DLH open \n" );

	if( ( file->f_flags & O_ACCMODE ) == O_WRONLY )
	{
		printk(KERN_ERR "tea5991DLH's device node is readonly\n" );
		return -1;
	}
	else
		return 0;
}


static int fm_write(struct file *fp, const char __user *buf, size_t count, loff_t *pos)
{
	int ret=0;

	u8 wData[count];
		
	if(copy_from_user(wData, buf, count))
	{
		printk("Owen: FM copy_from_user FAIL \n");
	}
	else {
		ret = fm_write_reg(wData, count);
	}
	
	return ret;	

}


static int fm_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
	int ret=0;
	/*
	struct FM_State Tea5991_temp;
	Tea5991_temp.vol = Tea5991State.vol;
	Tea5991_temp.rssiData = Tea5991State.rssiData;
	Tea5991_temp.band = Tea5991State.band;
	*/
	printk("Owen: Now in FM_READ \n");
	
	ret = copy_to_user(buf, &Tea5991State, sizeof(struct FM_State));	
	
	if (ret)
		printk("Owen: FM copy_to_user FAIL \n");

	
	return ret;
}


static int fm_read_reg(u8* wData, int byteLen)
{

	struct i2c_msg msgs[2];
	//u8 get_buf[byteLen];
	u8 set_buf = RSP_HEADER;
	int ret = 0;
	int i=0;
	

	if(!tea5991_client->adapter){
		ret = -ENODEV;
		goto bail;
	}

	msgs[0].addr = tea5991_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &set_buf;
	msgs[1].addr = tea5991_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = byteLen;
	msgs[1].buf = wData;   //get_buf
	
	//printk("Owen: read I2C_transfer, adapter=0x%x, addr=0x%x \n", tea5991_client->adapter, tea5991_client->addr);
	ret = i2c_transfer(tea5991_client->adapter, &msgs, 2);
	
	for (i=0; i<byteLen; i++)
		printk("Owen: After READ: ret=%d, wData[%d] = 0x%x \n", ret, i, wData[i]);
	
	
	return ret;

bail:
	printk(KERN_ERR "tea5991: detect client error\n");
	return ret;
}


static int fm_write_reg(u8* wData, int byteLen)
{
	struct i2c_msg msgs;
	//u8 get_buf[byteLen]; 
	int ret = 0;

	msgs.addr = tea5991_client->addr;
	msgs.flags = 0;
	msgs.len = byteLen;
	msgs.buf = wData;

       //printk("Owen: write: I2C_transfer, tea5991=0x%x, addr=0x%x \n", tea5991_client->adapter, tea5991_client->addr);

	ret = i2c_transfer(tea5991_client->adapter, &msgs, 1);

	printk("Owen: FM after WRITE: ret=%d \n", ret);
	  
	return ret;
}

static ssize_t tea5991dle_dev_read( struct file * file, char __user * buffer, size_t size, loff_t * f_pos )
{
	
	char cmd[ 128 ];

	if( sizeof(buffer) > 128 )
		return 0;

	if( copy_from_user( cmd, buffer, sizeof(buffer) ) )
		return -EFAULT;

	printk("owen: in tea5991dle_dev_read: cmd=%s \n", cmd);
}

#define POWERON				0x01
#define POWERDOWN			0x02
#define SETMUTEMODE		0x03
#define GETMUTEMODE		0x04
#define SETFREQUENCY		0x05
#define GETFREQUENCY		0x06
#define SETRSSITHRESHOLD	0x07
#define GETRSSITHRESHOLD	0x08
#define SETVOLUME			0x09
#define GETVOLUME			0x10
#define SEEK					0x11
#define STOPSEEK			0x12
#define ENABLERDS			0x13
#define DISABLERDS			0x14
#define SETRSSISYSTEM		0x15
#define GETRSSISYSTEM		0x16
#define GETRSSI				0x17
#define SEEKUP				0x18
#define SEEKDOWN			0x19


static int tea5991dle_dev_ioctl( struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg )
{
	printk("Owen: in tea5991_ioctl: cmd = %d, arg=0x%x; \n", cmd, arg);

	int interCH = 0;
	int ret = 0;
	u8 tCH[2];

	tCH[0] = 0x00; 
	tCH[1] = 0x64;
	
	switch (cmd)
	{
		case POWERON:
			printk("Owen: Now Power on \n");
			tea5991_power_up();
			msleep(1000);
			tea5991_goto_mode();  
			tea5991_SetAudioDAC();
			tea5991_SetFrequency(tCH);
			ret = tea5991_SetVolume(arg);
			break;
			
		case POWERDOWN:
			ret = tea5991_power_down();
			break;

		case SETMUTEMODE:
			ret = tea5991_SetMuteMode(arg);
			break;
		case GETMUTEMODE:
			break;

		case SETFREQUENCY:
			interCH = (arg - 875)*2;
			tCH[0] = (interCH & 0xFF00) >> 8;
			tCH[1] = interCH & 0xFF;						
			ret = tea5991_SetFrequency(tCH);			
			break;

		case GETFREQUENCY:
			ret = tea5991_GetTunedFrequency();
			break;
			
		case SETRSSITHRESHOLD:
			break;
			
		case GETRSSITHRESHOLD:
			break;
			
		case SETVOLUME:
			ret = tea5991_SetVolume(arg);
			break;
			
		case GETVOLUME:
			
		case SEEK:
			ret = tea5991_Seek();
			break;
			
		case STOPSEEK:
			ret = tea5991_StopSeek();
			break;
			
		case ENABLERDS:
			break;

		case DISABLERDS:
			break;
			
		case SETRSSISYSTEM:
			ret = tea5991_SetRDSSystem(arg);
			break;
			
		case GETRSSISYSTEM:
			ret = tea5991_GetRDSSystem();
			break;

		case GETRSSI:
			printk("Owen: Now Get RSSI \n");
			ret = tea5991_GetRSSI();
			break;
			
		case SEEKUP:
			ret = tea5991_SeekUp();
			break;
			
		case SEEKDOWN:
			ret = tea5991_SeekDown();
			break;
			
		default:
			printk(KERN_ERR "unknown command %d\n", cmd);
			return -1;
			break;
	}
	return ret;
}

static int tea5991dle_dev_release( struct inode * inode, struct file * filp )
{
	if (tea5991_power_down()) {
		printk("tea5991DLH: failed to power down\n");
		return -1;
	}
	return 0;
}

static const struct file_operations tea5991dle_dev_fops = {
	.open = tea5991dle_dev_open,
	.read = fm_read,
	.write = fm_write,
	.ioctl = tea5991dle_dev_ioctl,
	.release = tea5991dle_dev_release,
	.owner = THIS_MODULE,
	//.poll = tea5991dle_dev_poll,
};

static struct miscdevice tea5991_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "Tea5991_FM",
	.fops = &tea5991dle_dev_fops
};

#if 1
#define	tea5991_PROC_FILE	"driver/tea5991dlh"

static struct proc_dir_entry *tea5991_proc_file;

#ifdef K2_FM_TEST
static int tea5991_proc_read( struct file * filp, char * buffer, size_t length, loff_t * offset )
{
	printk("Leon debug: tea5991_proc_read tea5991_power_up\n");
	tea5991_power_up_chk();	
	//tea5991_power_up();	
	return 0;
}

#endif

static int tea5991_proc_read_1( struct file * filp, char * buffer, size_t length, loff_t * offset )
{
	u8 val;
	u8 chip_id = 0;

	dbg_lis("tea5991_proc_read()\n\n");

	// WHO_AM_I
	chip_id = i2c_smbus_read_byte_data(tea5991_client, 0xFF);

	if (chip_id != 0x00) {
		printk(KERN_ERR "tea5991DLH unavailable! (proc)\n");
		return -ENXIO;
	} else {
		dbg_lis("tea5991DLH(chip id:0x%02x) detected. (proc)\n", chip_id);
	}
	
	return 0;
}

static int tea5991_proc_write( struct file * filp, char * buffer, size_t length, loff_t * offset )
{
	char cmd[ 4 ];
	u8 setBuf[3];
	u8 getBuf[12];
	u8 tCH[2];
	int interChannel = 0;
	int tmpChannel = 0;
	int ret=0;

	if( length > 128 )
		return 0;

	if( copy_from_user( cmd, buffer, length ) )
		return -EFAULT;

	//tea5991_power_up();
		
/*
	if( strncmp( cmd, "poweron", 7 ) == 0 )
                     tea5991_power_up(1);

	else if( strncmp( cmd, "powerdown", 9 ) == 0 )
                     tea5991_power_down();
	
           else if( strncmp( cmd, "mode", 4 ) == 0 )
                     tea5991_goto_mode();           

           else if( strncmp( cmd, "setDAC", 6 ) == 0 )
                     tea5991_SetAudioDAC();

           else if( strncmp( cmd, "setCH", 5 ) == 0 )
                     tea5991_SetChannel(tCH);                                                                                     

           else if( strncmp( cmd, "RSSI", 4 ) == 0 )
           {
           		tCH[0] = 0x00; 
			tCH[1] = 0x68;
           		tea5991_goto_mode();  
			tea5991_SetAudioDAC();
			tea5991_SetChannel(tCH);
           		tea5991_getRSSI();
           }

           else if( strncmp( cmd, "poweroff", 8 ) == 0 )
                     tea5991_power_up(0); 

	   else if( strncmp( cmd, "readid", 6 ) == 0 )
           {
           		setBuf[0] = 0x00;
			setBuf[1] = 0x06;
			setBuf[2] = 0x08;
           		fm_write_reg(setBuf, 3);
			fm_read_reg(getBuf, 12);

           }
           else

                     printk( KERN_ERR "%s: not supported proc command\n", __func__ );

           return length;
*/
  
  	sscanf(cmd, "%d", &tmpChannel);
       if (tmpChannel == 0)
		tea5991_power_up();
	else if (tmpChannel == 11)
		tea5991_power_down();  
	else if (tmpChannel == 1)
		tea5991_SetVolume(1);
	else if (tmpChannel == 5)
		tea5991_SetVolume(5);
	else if (tmpChannel == 10)
		tea5991_SetVolume(10);
	else if (tmpChannel == 100)
		tea5991_Seek();
	else if (tmpChannel == 101)
		tea5991_StopSeek();		
	else if (tmpChannel == 102)
		tea5991_SetMonoStereoMode(1);
	else if (tmpChannel == 103)
		tea5991_GetMonoStereoMode();
	
	else {

	  	interChannel = (tmpChannel -875)*2;
		tCH[0] = ( interChannel & 0xFF00 ) >>8;
		tCH[1] = interChannel & 0xFF;
		printk("Owen: input=%d, tmpCH=0x%x, interCH[0]=0x%x, interCH[1]=0x%x \n", tmpChannel, interChannel, tCH[0], tCH[1]);
		
		tea5991_goto_mode();  
		tea5991_SetAudioDAC();
		tea5991_SetFrequency(tCH);
		tea5991_SetVolume(10);
		msleep(1000);
		tea5991_GetRSSI();
	}
	return length;
	

}

static struct file_operations tea5991_proc_fops = {
	.owner	= THIS_MODULE,
	#ifdef K2_FM_TEST
//	.read   = tea5991_power_up_chk,
	//.read   = tea5991_power_up,
	.read   = tea5991_freq_test,
	#else
	.read	= tea5991_proc_read,
	#endif
	.write	= tea5991_proc_write,
};

static int create_tea5991_proc_file(void)
{
	printk("Leon create_tea5991_proc_file 2\n");
	tea5991_proc_file = create_proc_entry(tea5991_PROC_FILE, 0644, NULL);
	if (!tea5991_proc_file) {
		printk(KERN_ERR "Create proc file for tea5991DLH failed\n");
		return -ENOMEM;
	}

	dbg_lis("tea5991DLH proc OK\n" );
	tea5991_proc_file->proc_fops = &tea5991_proc_fops;
	return 0;
}

static void remove_tea5991_proc_file(void)
{
	//remove_proc_entry(tea5991_PROC_FILE, &proc_root);
	return;
}
#endif /*PROCFILE_tea5991*/

/*===============================================*/
/*ACER  Paul Lo, 2009/10/28, Add early suspend function for entering sleep mode { */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void tea5991_early_suspend(struct early_suspend *h)
{
	dbg_lis("tea5991: entering early suspend!!\n");
	_inearlysuspend = 1;
	//tea5991_power_up(0);

	return 0;
}

static void tea5991_late_resume(struct early_suspend *h)
{
	dbg_lis("tea5991: exited early suspend!!\n");
	_inearlysuspend = 0;
	//tea5991_power_up(1);

	return 0;
}

static struct early_suspend tea5991_early_suspend_mode = {
	.level = EARLY_SUSPEND_LEVEL_MISC_DEVICE,
	.suspend = tea5991_early_suspend,
	.resume = tea5991_late_resume,
};
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/* } ACER Paul Lo, 2009/10/28*/

static int tea5991_detect_client(struct i2c_adapter *adap, int address, int kind)                                                               
{
	struct i2c_client *pClient ;
	int err, ret;
	dbg_lis("tea5991: tea5991_detect_client\n"); 
   
	if (!(pClient = kzalloc(sizeof(struct i2c_client),GFP_KERNEL))) {
		printk(KERN_ERR "Can't allocate memory for I2C client driver.");
		err = -ENOMEM;
		goto bail;
	}   
        
	pClient->addr = address;
	pClient->adapter = adap;
	pClient->driver = &tea5991_driver;
	pClient->flags = 0;
	strcpy(pClient->name, "Tea5991_FM");    // tea5991_FM

	err = i2c_attach_client(pClient);
	if (err){
		printk(KERN_ERR "tea5991: Can't register new I2C client driver.");
		kfree(pClient);
		goto bail;
	}   
	tea5991_client = pClient;
	printk("Owen: After i2c_attach_client, tea5991_client:0x%x \n", tea5991_client);
	printk(KERN_INFO "tea5991 tea5991_detect_client: address:0x%x \n", address);

	err = misc_register(&tea5991_device);
  	 if (err) {
		printk(KERN_ERR "tea5991: FM misc register failed\n");
		goto exit_detach;
	}

	err = create_tea5991_proc_file();
	if (err) {
		printk(KERN_ERR "%s: Failed to create tea5991 proc file.\n", __func__);
		goto exit_misc_device;
	}

/*ACER  Paul Lo, 2009/10/28, Add early suspend function for entering sleep mode { */
#ifdef CONFIG_HAS_EARLYSUSPEND
  _inearlysuspend = 0;
  register_early_suspend(&tea5991_early_suspend_mode);
#endif	
	
	return 0;

exit_misc_device:
	misc_deregister(&tea5991_device);

exit_detach:
	i2c_detach_client(pClient);
	
bail:
	printk(KERN_ERR "tea5991: detect client error\n");
	return -4;
}

/*===============================================*/

static int tea5991_attach(struct i2c_adapter * adapter)
{
	return i2c_probe(adapter, &addr_data, &tea5991_detect_client);
}

static int tea5991_detach(struct i2c_client *pClient) 
{
	int err;

	if (!pClient->adapter){    
		err = -ENODEV;    /* our client isn't attached */
		goto bail;
	}

	if ((err = i2c_detach_client(pClient))) {
		  printk(KERN_ERR "tea5991: Client deregistration failed, client not detached.");
		goto bail;
	}

/*===============================================*/
/*ACER  Paul Lo, 2009/10/28, Add early suspend function for entering sleep mode { */
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&tea5991_early_suspend_mode);
#endif
/* } ACER Paul Lo, 2009/10/28*/
/*===============================================*/

	tea5991_client = NULL;
	kfree(pClient);
	return 0;

bail:
	return err;
}

/* platform driver, since i2c devices don't have platform_data */
static struct platform_device *tea5991_pdev;

static int __init tea5991_plat_probe(struct platform_device *pdev)
{
	tea5991_pdev = pdev;

	return 0;
}

static int tea5991_plat_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver tea5991_plat_driver = {
	.probe	= tea5991_plat_probe,
	.remove	= tea5991_plat_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name 	= "pnx-tea5991",
	},
};

static int __init tea5991_init(void)
{
	int ret;

	if (!(ret = platform_driver_register(&tea5991_plat_driver)))
	{
		printk("tea5991 platform_driver_register OK !!!\n");
		if (!(ret = i2c_add_driver(&tea5991_driver)))
		{
			printk("tea5991 i2c_add_driver OK !!!\n");
		}
		else
		{
			printk(KERN_ERR "tea5991 i2c_add_driver failed\n");
			platform_driver_unregister(&tea5991_driver);
			return 	-ENODEV;
		}
	}
	else
	{
		printk("tea5991 platform_driver_register Failed !!!\n");
	}

	return ret;
}

static void __exit tea5991_exit(void)
{
	dbg_lis("tea5991DLH exit\n" );
	i2c_del_driver(&tea5991_driver);
	misc_deregister(&tea5991_device);
}

//subsys_initcall(tea5991_init);
module_init(tea5991_init);
//late_initcall(tea5991_init);
module_exit(tea5991_exit);

MODULE_AUTHOR( "Stefan Chuang <stefanchchuang@tp.cmcs.com.tw>" );
MODULE_DESCRIPTION( "tea5991FM driver" );
MODULE_LICENSE( "GPL" );
