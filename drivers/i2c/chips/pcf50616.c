/*
 * ============================================================================
 *
 * Filename:     pcf50616.c
 *
 * Description:  PMU50616 driver
 *
 * Version:      1.0
 * Created:      20.11.2009 11:14:40
 * Revision:     none
 * Compiler:     gcc
 *
 * Author:       Patrice Chotard (PCH), patrice.chotard@stericsson.com
 * Company:      ST-Ericssson Le Mans
 * 
 * Derived from PCF50633
 *
 * (C) 2006-2008 by Openmoko, Inc.
 * Author: Balaji Rao <balajirrao@openmoko.org>
 * All rights reserved.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * changelog:
 *
 * ============================================================================
 */

#if defined(CONFIG_NKERNEL)
#define VREG
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/rtc.h>
#include <linux/watchdog.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/timer.h>
#ifdef VREG 
#include <linux/wait.h>
#include <nk/vreg.h>
#if defined (CONFIG_NET_9P_XOSCORE)
#include <nk/adcclient.h>
#endif 
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
/* fix me to be put in correct interface file */
typedef void (*irquser_t)(void );

#endif

#include <linux/pcf506XX.h>

#include <mach/gpio.h>
//#include <asm-generic/rtc.h>

#ifdef CONFIG_SWITCH    
#include <linux/switch.h>
#endif

#include <mach/pcf50616.h>
//#include "charge.h"

//#define PMU_DEBUG_L1 1
//#define PMU_DEBUG_L2 1

#ifdef PMU_DEBUG_L1
#define dbgl1(fmt, args...) printk(KERN_DEBUG "%-25s: " fmt, __func__, ## args)
#else
#define dbgl1(fmt, args...) do{}while(0)
#endif

#ifdef PMU_DEBUG_L2
#define dbgl2(fmt, args...) printk(KERN_DEBUG "%-25s: " fmt, __func__, ## args)
#else
#define dbgl2(fmt, args...) do{}while(0)
#endif

/***********************************************************************
 * Static data / structures
 ***********************************************************************/
static unsigned long bootcause=0; /* concatenation of INT1 .. INT4 */
static unsigned long oocscause=0;
static unsigned int major=0;

#ifndef CONFIG_I2C_NEW_PROBE
static unsigned short normal_i2c[] = { 0x70, I2C_CLIENT_END };
I2C_CLIENT_INSMOD_1(pcf50616);
#endif

struct pcf50616_data {
#ifdef CONFIG_I2C_NEW_PROBE
	struct i2c_client * client;
#else
	struct i2c_client client;
#endif
	struct pcf50616_platform_data *pdata;
	struct backlight_device *keypadbl;
	struct mutex lock;
	unsigned int flags;
	unsigned int working;
	struct work_struct work;
	struct rtc_device *rtc;
	struct input_dev *input_dev;
	int onkey_seconds;
	struct {
		int enabled;
		int pending;
	} rtc_alarm;
	int irq;
    u_int8_t write_reg;
    u_int8_t write_val;
    u_int8_t read_reg;
	u_int8_t id;
	u_int8_t variant;
    struct {
        u_int8_t tvout_plugged;
        u_int8_t headset_plugged;
        u_int8_t button_pressed;
		struct delayed_work work_timer;
#ifdef CONFIG_SWITCH    
		struct switch_dev swheadset;
#endif
    } accessory;
#ifdef VREG
	void *vregdesc;
	int irq_pending;
    irquser_t irqusercallback;
	/* work queue detected to the the irq enable */
	struct work_struct work_enable_irq;
#endif
    itcallback pcf50616_itcallback;
    itcallback pcf50616_usbcallback;
	struct {
		u_int8_t state;
		unsigned int fast_charge;
		unsigned int long_charge;
		unsigned int discharge;
		unsigned long endtime;
		struct delayed_work work_timer;
	}bbcc;
	int gpios;
    struct workqueue_struct *workqueue;
};
static struct i2c_driver pcf50616_driver;

struct pcf50616_data *pcf50616_global;
EXPORT_SYMBOL(pcf50616_global);

static struct platform_device *pcf50616_pdev;

/* This callback function cannot be put in pcf50616_data structute because pcf50616 drivers is installed after pnx_kbd */
static itcallback pcf50616_onkey;

/***********************************************************************
 * Constant data
 ***********************************************************************/
#define PCF50616_F_CHG_MAIN_PLUGGED 0x00000001  /* Main charger present */
#define PCF50616_F_CHG_USB_PLUGGED	0x00000002  /* Charger USB present  */
#define PCF50616_F_CHG_MAIN_ENABLED 0x00000004  /* Main charge enabled  */
#define PCF50616_F_CHG_USB_ENABLED  0x00000008  /* USB charge enabled   */
#define PCF50616_F_CHG_THERM_LIMIT  0x00000010  /* Thermal limite on    */
#define PCF50616_F_CHG_BATTFULL     0x00000020  /* Battery Full         */
#define PCF50616_F_CHG_CV_REACHED	0x00000040	/* Charger swtiches from CC to CV */
#define PCF50616_F_RTC_SECOND	    0x00000080
#define PCF50616_ACC_INIT_TIME      2*HZ
#define PCF50616_NOACC_DETECTED     0x84
#define PCF50616_BBCC_24H           24*60*60*HZ /* 24 hours */
#define PCF50616_BBCC_6H            6*60*60*HZ  /* 6 hours  */
#define PCF50616_BBCC_1H            1*60*60*HZ  /* 1 hour   */
#define PCF50616_BBCC_LONG_CHARGE   0x01
#define PCF50616_BBCC_FAST_CHARGE   0x02
#define PCF50616_BBCC_DISCHARGE     0x03

static int reg_write(struct pcf50616_data *pcf, u_int8_t reg, u_int8_t val);

/***********************************************************************
 * Low-Level routines
 ***********************************************************************/
#ifdef VREG
/***********************************************************************
 * external function used by IOCTL or by driver call
 ************************************************************************/
#if defined (CONFIG_NET_9P_XOSCORE)
static ulong batt_temp;
static ulong batt_volt;

static void pcf5062x_adc_temperature(unsigned long temp)
{
    batt_temp = temp;
	dbgl1("temperature %ld \n",temp);
}

static void  pcf5062x_adc_voltage(unsigned long volt)
{
    batt_volt = volt;
	dbgl2("volt %ld \n",volt);
}

static void  pcf5062x_adc_accessory(unsigned long uohms)
{
    ulong mohms = uohms/1000;
	char *env[] = {"TVOUT=plugged", NULL};
	struct kobject *pkobj = &(pcf50616_pdev->dev.kobj);

	dbgl1("Resistance = %lu uOhms\n",uohms);
	dbgl1("Resistance = %lu mOhms\n",mohms);
	reg_write(pcf50616_global, PCF50616_REG_OOCC, PCF50616_OOCC_REC_EN| PCF50616_OOCC_REC_MOD);

    if ( mohms<500 ) {
        dbgl1("Tvout plugged\n");
        pcf50616_global->accessory.tvout_plugged = 1;
		kobject_uevent_env(pkobj, KOBJ_CHANGE, env);
    } else
        if ( mohms<3000 ) {
            dbgl1("Headset plugged\n");
            pcf50616_global->accessory.headset_plugged = 1;
            input_event(pcf50616_global->input_dev, EV_SW, SW_HEADPHONE_INSERT, 1);
#ifdef CONFIG_SWITCH
			switch_set_state(&pcf50616_global->accessory.swheadset, pcf50616_global->accessory.headset_plugged );
#endif
        } else
            if ( mohms<4300000 ) {
                dbgl1("Headset button pressed\n");
	            pcf50616_global->accessory.button_pressed = 1;
                input_report_key(pcf50616_global->input_dev, KEY_MUTE, 1);
				if ( pcf50616_global->accessory.headset_plugged == 0)
				{
					/* Notify headset plugged in case of headset plugged with key pressed */
		            pcf50616_global->accessory.headset_plugged = 1;
		            input_event(pcf50616_global->input_dev, EV_SW, SW_HEADPHONE_INSERT, 1);
#ifdef CONFIG_SWITCH
					switch_set_state(&pcf50616_global->accessory.swheadset, pcf50616_global->accessory.headset_plugged );
#endif

				}

            }
}

#endif
void pcf5062x_bb_read(unsigned char addr, unsigned char *dest, unsigned char len)
{
	vreg_user_read(pcf50616_global->vregdesc,addr,dest,len);		
	return;
}

void pcf5062x_bb_write(unsigned char addr, unsigned char *src, unsigned char len)
{
	vreg_user_write(pcf50616_global->vregdesc,addr,src,len);		
	return;
}
int pcf50616_reg_configure(void *desc)
{
	int ret=0;
	ret+=vreg_user_desc(desc,PCF50616_REG_ID,1,VR_READ,0,0);	
	ret+=vreg_driver_desc(desc,PCF50616_REG_ID,1,VR_READ,0,0);
	/* config modem only */
	/* config for SIM register */
	ret+=vreg_user_desc(desc,PCF50616_REG_SIMREGC1,1, VR_READ | VR_WRITE | VR_EXCLU,0,0);
	ret+=vreg_user_desc(desc,PCF50616_REG_SIMREGS1,1, VR_READ | VR_WRITE | VR_EXCLU,0,0);
	/* config for regu modem */
	ret+=vreg_user_desc(desc,PCF50616_REG_DCD1C1,PCF50616_REG_DCD2C1-PCF50616_REG_DCD1C1,VR_READ | VR_WRITE | VR_EXCLU, 0, 0);
	ret+=vreg_driver_desc(desc,PCF50616_REG_DCD2C1,PCF50616_REG_DCD2DVS4-PCF50616_REG_DCD2C1+1,VR_READ | VR_WRITE | VR_EXCLU, 0, 0);

	ret+=vreg_driver_desc(desc,PCF50616_REG_D1C,PCF50616_REG_D7C-PCF50616_REG_D1C+1,VR_READ | VR_WRITE | VR_EXCLU, 0, 0);
	ret+=vreg_driver_desc(desc,PCF50616_REG_RF1C,PCF50616_REG_RFGAIN-PCF50616_REG_RF1C+1,VR_READ | VR_WRITE | VR_EXCLU, 0,0);
	/* interruption config  */
	{
	unsigned char pol[4]={0xff,0xff,0xff,0xff};
	unsigned char mask[4]={0xf1,0xff,0xff,0xff};
	ret+=vreg_user_desc(desc,PCF50616_REG_INT1,4, VR_READ | VR_CLEAR | VR_MASK,mask,0);
	ret+=vreg_user_desc(desc,PCF50616_REG_INT1M,4, VR_WRITE | VR_READ | VR_POLARITY | VR_MASK ,mask,pol);
	ret+=vreg_driver_desc(desc,PCF50616_REG_INT1,4, VR_READ | VR_CLEAR ,0,0);
	ret+=vreg_driver_desc(desc,PCF50616_REG_INT1M,4, VR_WRITE | VR_READ | VR_POLARITY ,0,pol);
	ret+=vreg_driver_desc(desc,PCF50616_REG_HCC,1, VR_READ | VR_WRITE | VR_EXCLU,0,0);
	/* map IOREG / USB REG */
	ret+=vreg_driver_desc(desc,PCF50616_REG_IOC,1, VR_READ | VR_WRITE | VR_EXCLU,0,0);

	}
	/* config specific for RECC */
	{
#ifndef RTK_TEST
		unsigned char pol[1]={1<<6};
		unsigned char mask[1]={1<<6};
		unsigned char mask1[1]={0xff};
		ret+=vreg_user_desc(desc, PCF50616_REG_RECC1,
				1,VR_POLARITY | VR_MASK | VR_WRITE | VR_READ ,
				mask,pol);

		ret+=vreg_driver_desc(desc, PCF50616_REG_RECC1,
				1,VR_POLARITY | VR_MASK | VR_WRITE | VR_READ ,
				mask1,pol);
#else
	ret+=vreg_user_desc(desc, PCF50616_REG_RECC1,
				1, VR_WRITE | VR_READ ,
				0,0);
#endif
	}
	/* fix register used by driver only */
    /* OOC, BVMC */
	ret+=vreg_driver_desc(desc,PCF50616_REG_OOCC,3,VR_WRITE | VR_READ | VR_EXCLU,0,0);    
	ret+=vreg_driver_desc(desc,PCF50616_REG_BVMC,1,VR_WRITE | VR_READ | VR_EXCLU,0,0);    
	/* RTC */
	ret+=vreg_driver_desc(desc,PCF50616_REG_RTC1,8,VR_WRITE | VR_READ ,0,0);
	ret+=vreg_user_desc(desc,PCF50616_REG_RTC1,8, VR_READ ,0,0);
	/* CBC BBC ADC PWMS TSI */
#ifndef RTK_TEST	
	ret+=vreg_driver_desc(desc,PCF50616_REG_CBCC1,	PCF50616_REG_BBCC-PCF50616_REG_CBCC1+1
			,VR_WRITE | VR_READ | VR_EXCLU,0,0); 
#else
	ret+=vreg_user_desc(desc,PCF50616_REG_CBCC1,	PCF50616_REG_BBCC-PCF50616_REG_CBCC1+1
			,VR_WRITE | VR_READ | VR_EXCLU,0,0); 
#endif 

	return ret;
}	
/** 
 * @brief block waiting thread till interruption occcurance
 * 
 * @return 
 */
void pcf5062x_bb_int(irquser_t callback)
{
	if (pcf50616_global->irq_pending==0)
	{
		pcf50616_global->irqusercallback=callback;
	}
	else
	{
	pcf50616_global->irq_pending=0;
	pcf50616_global->irqusercallback=0;
	callback();
	}
}
static void pcf50616_irq_enable(struct work_struct *work_enable_irq)
{
	struct pcf50616_data *pcf =
			container_of(work_enable_irq, struct pcf50616_data, work_enable_irq);
	enable_irq(pcf->irq);
}

void pcf50616_user_irq_handler(struct pcf50616_data *pcf)
{
	if (vreg_user_pending(pcf->vregdesc,PCF50616_REG_INT1,4))
			{
		pcf->irq_pending=1;/* unblock the user waiting for interruption */
		if (pcf->irqusercallback)
		{
			pcf->irqusercallback();
			pcf->irq_pending=0;
			pcf->irqusercallback=0;
		}
	}
}
/* routine to be provided to vreg library */
/* fix me this routine can be improved by grouping i2c access instead of doing one after the other */
int pcf5062x_reg_nwrite(void *p, unsigned long reg, unsigned char *data, unsigned long len)
{
	struct pcf50616_data *pcf=p;
#if 1
	if (len==1)	
	{
#ifdef CONFIG_I2C_NEW_PROBE
		i2c_smbus_write_byte_data(pcf->client, reg, data[0]);
#else
		i2c_smbus_write_byte_data(&pcf->client, reg, data[0]);
#endif
		return 1;
	}
	else
		/* useful at initialization step following call perform memcpy */
#ifdef CONFIG_I2C_NEW_PROBE
		if (i2c_smbus_write_i2c_block_data(pcf->client,reg,(u8) len, data)<0)return 0;
#else
		if (i2c_smbus_write_i2c_block_data(&pcf->client,reg,(u8) len, data)<0)return 0;
#endif
		else return len;
#else
	int i;
	for(i=0;i<len;i++)
	{
#ifdef CONFIG_I2C_NEW_PROBE
	i2c_smbus_write_byte_data(pcf->client, (reg+i),data[i]);
#else
	i2c_smbus_write_byte_data(&pcf->client, (reg+i),data[i]);
#endif
	}
    return len;
#endif 
}

int pcf5062x_reg_nread(void *p, unsigned long reg, unsigned char *data, unsigned long len)
{
	int i;
	struct pcf50616_data *pcf=p;
	for(i=0;i<len;i++)
	{
#ifdef CONFIG_I2C_NEW_PROBE
	data[i]=(unsigned char)i2c_smbus_read_byte_data(pcf->client, (reg+i));
#else
	data[i]=(unsigned char)i2c_smbus_read_byte_data(&pcf->client, (reg+i));
#endif
	}
	return len;
}
#else
void pcf5062x_bb_int(void * callback){}
void pcf5062x_bb_read(unsigned char addr, unsigned char *dest, unsigned char len){}
void pcf5062x_bb_write(unsigned char addr, unsigned char *src, unsigned char len){}
#endif

static inline int __reg_nwrite(struct pcf50616_data *pcf, u_int8_t reg,  u_int8_t *val, unsigned long size )
{
#ifndef VREG
	return i2c_smbus_write_block_data(&pcf->client, reg,(u8) size, val);
#else
	return vreg_driver_write(pcf->vregdesc,reg,val,size );
#endif
}

static inline int __reg_write(struct pcf50616_data *pcf, u_int8_t reg, u_int8_t val)
{
#ifndef VREG
	return i2c_smbus_write_byte_data(&pcf->client, reg, val);
#else
	return vreg_driver_write(pcf->vregdesc,reg,&val,1);
#endif
}

static int reg_write(struct pcf50616_data *pcf, u_int8_t reg, u_int8_t val)
{
	int ret;

	mutex_lock(&pcf->lock);
	ret = __reg_write(pcf, reg, val);
	mutex_unlock(&pcf->lock);

	return ret;
}

#ifdef VREG 
static inline int32_t __reg_rawread(struct pcf50616_data *pcf, u_int8_t reg)
{
	int32_t ret;
#ifdef CONFIG_I2C_NEW_PROBE
	ret=i2c_smbus_read_byte_data(pcf->client, reg);
#else
	ret=i2c_smbus_read_byte_data(&pcf->client, reg);
#endif
	return ret;
}
static u_int8_t reg_rawread(struct pcf50616_data *pcf, u_int8_t reg)
{
	int32_t ret;
	mutex_lock(&pcf->lock);
	ret = __reg_rawread(pcf, reg);
	mutex_unlock(&pcf->lock);
	return ret & 0xff;
}
static inline int32_t __reg_rawwrite(struct pcf50616_data *pcf, u_int8_t reg, u_int8_t val)
{
	int32_t ret;
#ifdef CONFIG_I2C_NEW_PROBE
	ret= i2c_smbus_write_byte_data(pcf->client, reg, val);
#else
	ret= i2c_smbus_write_byte_data(&pcf->client, reg, val);
#endif
	return ret;
}
static int32_t  reg_rawwrite(struct pcf50616_data *pcf, u_int8_t reg, u_int8_t val)
{
	int32_t ret;
	mutex_lock(&pcf->lock);
	ret = __reg_rawwrite(pcf, reg, val);
	mutex_unlock(&pcf->lock);
	return ret;
}
#endif 
static inline int32_t __reg_read(struct pcf50616_data *pcf, u_int8_t reg)
{
	int32_t ret;
#ifndef VREG
	ret=i2c_smbus_read_byte_data(&pcf->client, reg);
#else
	vreg_driver_read(pcf->vregdesc,reg,&ret,1);
#endif
	return ret;
}

static u_int8_t reg_read(struct pcf50616_data *pcf, u_int8_t reg)
{
	int32_t ret;

	mutex_lock(&pcf->lock);
	ret = __reg_read(pcf, reg);
	mutex_unlock(&pcf->lock);

	return ret & 0xff;
}


static int reg_set_bit_mask(struct pcf50616_data *pcf, u_int8_t reg, u_int8_t mask, u_int8_t val)
{
	int ret;
	u_int8_t tmp;

	val &= mask;

	mutex_lock(&pcf->lock);
	
	tmp = __reg_read(pcf, reg);
	tmp &= ~mask;
	tmp |= val;
	ret = __reg_write(pcf, reg, tmp);

	mutex_unlock(&pcf->lock);

	return ret;
}

static int reg_clear_bits(struct pcf50616_data *pcf, u_int8_t reg, u_int8_t val)
{
	int ret;
	u_int8_t tmp;

	mutex_lock(&pcf->lock);

	tmp = __reg_read(pcf, reg);
	tmp &= ~val;
	ret = __reg_write(pcf, reg, tmp);

	mutex_unlock(&pcf->lock);

	return ret;
}

/***********************************************************************
 * General PMU management
 * sysfs callback functions to Access tp PMU register via i2c
 ***********************************************************************/
static ssize_t show_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    u_int8_t bin,i;
    char bin_buf[9];
	struct pcf50616_data *pcf = i2c_get_clientdata(to_i2c_client(dev));

    bin = pcf->read_reg;
    for (i=0;i<8;i++)
    {
        bin_buf[7-i]=((bin%2)==1?'1':'0');
        bin/=2;
    }
    bin_buf[i]=0;

	return sprintf(buf, "%u (0x%02X) (%8s)\n", pcf->read_reg, pcf->read_reg, bin_buf);
}
#ifdef VREG
static ssize_t set_rawread(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	unsigned long reg = simple_strtoul(buf, NULL, 0);

	if (reg > 0xff)
		return -EINVAL;
    pcf->read_reg = reg_rawread(pcf, reg);

    return count;
}
static ssize_t set_rawwrite(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
    char buf1[5], buf2[5];
    int i=0 , j=0;
    unsigned long reg, val;

    /* Copy the first part of buf (before the white space) in buf1 */
    while ( !isspace(buf[i]) && (i<count) )
    {
        buf1[i]=buf[i];
        i++;
    }
    /* Add EoS */
    buf1[i]='\n'; /* Line Feed */

    /* Skip the whitespace */
    i++;

    while ( !isspace(buf[i]) && (i<count) )
    {
        buf2[j]=buf[i];
        j++;
        i++;
    }
    /* Add EoS */
    buf2[j]='\n'; /* Line Feed */

	reg = simple_strtoul(buf1, NULL, 0);
	val = simple_strtoul(buf2, NULL, 0);

	if ((val > 0xff) | (reg>0xff) )
		return -EINVAL;

	pcf->write_reg = reg;
	pcf->write_val = val;
	reg_rawwrite(pcf,reg,val);
	return count;
}

#endif 
static ssize_t set_read(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	unsigned long reg = simple_strtoul(buf, NULL, 0);

	if (reg > 0xff)
		return -EINVAL;

    pcf->read_reg = reg_read(pcf, reg);

    return count;
}

static ssize_t show_write(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pcf50616_data *pcf = i2c_get_clientdata(to_i2c_client(dev));
	return sprintf(buf, "Reg=%u (0x%02X), Val=%u (0x%02X)\n", pcf->write_reg, pcf->write_reg, pcf->write_val, pcf->write_val);
}

static ssize_t set_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
    char buf1[5], buf2[5];
    int i=0 , j=0;
    unsigned long reg, val;

    /* Copy the first part of buf (before the white space) in buf1 */
    while ( !isspace(buf[i]) && (i<count) )
    {
        buf1[i]=buf[i];
        i++;
    }
    /* Add EoS */
    buf1[i]='\n'; /* Line Feed */

    /* Skip the whitespace */
    i++;

    while ( !isspace(buf[i]) && (i<count) )
    {
        buf2[j]=buf[i];
        j++;
        i++;
    }
    /* Add EoS */
    buf2[j]='\n'; /* Line Feed */

	reg = simple_strtoul(buf1, NULL, 0);
	val = simple_strtoul(buf2, NULL, 0);

	if ((val > 0xff) || (reg>0xff) )
		return -EINVAL;

	pcf->write_reg = reg;
	pcf->write_val = val;
	reg_write(pcf,reg,val);
	return count;
}

/***********************************************************************
 * Regulators management by sysfs
 ***********************************************************************/

static u_int8_t pcf50616_dxreg_voltage(unsigned int millivolts)
{
    /* Cf PCF50616 User Manual p106 for details */
	if (millivolts < 1200)
		return 0;
	else if (millivolts > 3300)
		return 0x18;

	millivolts -= 900;
	return (u_int8_t)((millivolts/100));
}

static unsigned int pcf50616_dxreg_2voltage(u_int8_t bits)
{
	bits = (bits & 0x1F);
	return 900 + (bits * 100);
}

/* manage only register not handled by RTK */
static const u_int8_t regulator_registers[__NUM_PCF506XX_REGULATORS] = {
	[PCF506XX_REGULATOR_D1REG]	= PCF50616_REG_D1C,
	[PCF506XX_REGULATOR_D2REG]	= PCF50616_REG_D2C,
	[PCF506XX_REGULATOR_D3REG]	= PCF50616_REG_D3C,
	[PCF506XX_REGULATOR_D4REG]	= PCF50616_REG_D4C,
	[PCF506XX_REGULATOR_D5REG]	= PCF50616_REG_D5C,
	[PCF506XX_REGULATOR_D6REG]	= PCF50616_REG_D6C,
	[PCF506XX_REGULATOR_D7REG]	= PCF50616_REG_D7C,
	[PCF506XX_REGULATOR_IOREG]	= PCF50616_REG_IOC,
	[PCF506XX_REGULATOR_USBREG]	= PCF50616_REG_USBC,
	[PCF506XX_REGULATOR_HCREG]	= PCF50616_REG_HCC,
};

static const  enum pcf50616_regulator_enable regulator_mode[__NUM_PCF506XX_REGULATORS_MODE]= {
	[PCF506XX_REGU_ECO] = PCF50616_REGULATOR_ECO,
	[PCF506XX_REGU_OFF] = PCF50616_REGULATOR_OFF,
	[PCF506XX_REGU_ON] = PCF50616_REGULATOR_ON,
	[PCF506XX_REGU_ECO_ON_ON_ON] = PCF50616_REGULATOR_ECO_ON_ON_ON,
	[PCF506XX_REGU_OFF_ON_ON_OFF] = PCF50616_REGULATOR_OFF_ON_ON_OFF,
	[PCF506XX_REGU_OFF_OFF_ON_ON] = PCF50616_REGULATOR_OFF_OFF_ON_ON,
	[PCF506XX_REGU_ECO_ECO_ON_ON] = PCF50616_REGULATOR_ECO_ECO_ON_ON,
	[PCF506XX_REGU_ECO_ON_ON_ECO] = PCF50616_REGULATOR_ECO_ON_ON_ECO,
};


/* pcf50616_onoff_set input parameters:
 *
 * pcf : pointer on pcf50616_data structure
 * reg : ID of regulator to manage, defined in include/linux/pcf506XX.h
 * mode: mode to set regulator: Off, Eco, On, cf include/linux/pcf506XX.h
 * 
 */

int pcf506XX_onoff_set(struct pcf50616_data *pcf,
		       enum pcf506XX_regulator_id reg, enum pcf506XX_regulator_mode mode)
{
	u_int8_t addr;

	if ((reg >= __NUM_PCF506XX_REGULATORS) ||
		(mode >=__NUM_PCF506XX_REGULATORS_MODE) ||
        (!pcf->pdata->rails[reg].used))
		return -EINVAL;

	if ( reg==PCF506XX_REGULATOR_USBREG) {
		addr = regulator_registers[reg];
		switch (mode) 
		{
		case PCF506XX_REGU_OFF :
			reg_clear_bits(pcf, addr, PCF50616_REGULATOR_ON_MASK);
			break;
		case PCF506XX_REGU_ECO :
			reg_set_bit_mask(pcf, addr, PCF50616_REGU_USB_ON_MASK|PCF50616_REGU_USB_ECO_MASK , 
										PCF50616_REGU_USB_ON | PCF50616_REGU_USB_ECO);
			break;
		case PCF506XX_REGU_ON:
			reg_set_bit_mask(pcf, addr, PCF50616_REGU_USB_ON_MASK|PCF50616_REGU_USB_ECO_MASK, PCF50616_REGU_USB_ON);

			break;
		/* all other command are not supported for this regu */
		default: return -EINVAL;
		}
	} 
	else {
		addr = regulator_registers[reg];
		switch (mode) 
		{
		case PCF506XX_REGU_OFF :
           reg_clear_bits(pcf, addr, PCF50616_REGULATOR_ON | PCF50616_REGULATOR_ECO);
          break;
		case PCF506XX_REGU_ECO : 
		   reg_set_bit_mask(pcf, addr, PCF50616_REGULATOR_ON_MASK, PCF50616_REGULATOR_ECO);
		   break;
		case  PCF506XX_REGU_ON :
           reg_set_bit_mask(pcf, addr, PCF50616_REGULATOR_ON_MASK, PCF50616_REGULATOR_ON);
           break;
         default : return -EINVAL;
		}
	}
	return 0;
}
EXPORT_SYMBOL(pcf506XX_onoff_set);

int pcf506XX_onoff_get(struct pcf50616_data *pcf, enum pcf506XX_regulator_id reg)
{
	u_int8_t val, addr;

	if ((reg >= __NUM_PCF506XX_REGULATORS) || (!pcf->pdata->rails[reg].used))
		return -EINVAL;

	/* the *REGC2 register is always one after the *REGC1 register except for USB*/
	if ( reg == PCF506XX_REGULATOR_USBREG) {
		addr = regulator_registers[reg];
		val = (reg_read(pcf, addr) & (PCF50616_REGU_USB_ON_MASK | PCF50616_REGU_USB_ECO_MASK));
	} 
	else {
		addr = regulator_registers[reg];
		val = reg_read(pcf, addr) & PCF50616_REGULATOR_ON_MASK;
	}
	return val;
}
EXPORT_SYMBOL(pcf506XX_onoff_get);

int pcf506XX_voltage_set(struct pcf50616_data *pcf,
			 enum pcf506XX_regulator_id reg,
			 unsigned int millivolts)
{
	u_int8_t volt_bits;
	u_int8_t regnr;

	dbgl1("pcf=%p, reg=%d, mvolts=%d\n", pcf, reg, millivolts);

	if ((reg >= __NUM_PCF506XX_REGULATORS) || (!pcf->pdata->rails[reg].used))
		return -EINVAL;

	regnr = regulator_registers[reg];
    
	if (millivolts > pcf->pdata->rails[reg].voltage.max)
		millivolts = pcf->pdata->rails[reg].voltage.max;
	
	switch (reg) {
	case PCF506XX_REGULATOR_D1REG:
	case PCF506XX_REGULATOR_D2REG:
	case PCF506XX_REGULATOR_D3REG:
	case PCF506XX_REGULATOR_D4REG:
	case PCF506XX_REGULATOR_D5REG:
	case PCF506XX_REGULATOR_D6REG:
	case PCF506XX_REGULATOR_D7REG:
	case PCF506XX_REGULATOR_IOREG:
		volt_bits = pcf50616_dxreg_voltage(millivolts);
		dbgl1("pcf50616_dxreg_voltage(%umV)=0x%02X\n", millivolts, volt_bits);
		break;
	case PCF506XX_REGULATOR_HCREG:
        /* Special case for HCREG that supports only few values */
        if ( millivolts < 2600 ) {
            millivolts = 1800;
			volt_bits = PCF50616_HCC_HCREGVOUT_1V8;
        } else
            if ( millivolts < 3000 ) {
                millivolts=2600;
				volt_bits = PCF50616_HCC_HCREGVOUT_2V6;
            } else
                if ( millivolts < 3200) {
                    millivolts = 3000;
					volt_bits = PCF50616_HCC_HCREGVOUT_3V0;
                } else {
                    millivolts = 3200;
					volt_bits = PCF50616_HCC_HCREGVOUT_3V2;
				}
		dbgl1("pcf50616_dxreg_voltage(%umV)=0x%02X\n", millivolts, volt_bits);
		break;
	default:
		return -EINVAL;
	}

	return reg_write(pcf, regnr, volt_bits);
}
EXPORT_SYMBOL(pcf506XX_voltage_set);

unsigned int pcf506XX_voltage_get(struct pcf50616_data *pcf,
			 enum pcf506XX_regulator_id reg)
{
	u_int8_t volt_bits;
	u_int8_t regnr;
	unsigned int rc = 0;

	if ((reg >= __NUM_PCF506XX_REGULATORS) || (!pcf->pdata->rails[reg].used))
		return -EINVAL;

	regnr = regulator_registers[reg];
	volt_bits = reg_read(pcf, regnr);

	dbgl1("pcf=%p, reg=0x%02X, data=0x%02X\n", pcf, regnr, volt_bits);

	switch (reg) {
	case PCF506XX_REGULATOR_D1REG:
	case PCF506XX_REGULATOR_D2REG:
	case PCF506XX_REGULATOR_D3REG:
	case PCF506XX_REGULATOR_D4REG:
	case PCF506XX_REGULATOR_D5REG:
	case PCF506XX_REGULATOR_D6REG:
	case PCF506XX_REGULATOR_D7REG:
	case PCF506XX_REGULATOR_IOREG:
		rc = pcf50616_dxreg_2voltage(volt_bits);
		break;
	case PCF506XX_REGULATOR_HCREG:
        if ( volt_bits == PCF50616_HCC_HCREGVOUT_1V8 ) {
            rc= 1800;
        } else
            if ( volt_bits == PCF50616_HCC_HCREGVOUT_2V6 ) {
                rc=2600;
            } else
                if ( volt_bits == PCF50616_HCC_HCREGVOUT_3V0 ) {
                    rc= 3000;
                } else {
                    rc= 3200;
				}
		break;
	default:
		return -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(pcf506XX_voltage_get);

/* following are the sysfs callback functions to get or set PMU regulator via i2c */
static int reg_id_by_name(const char *name)
{
	int reg_id;

	if (!strcmp(name, "voltage_d1reg") || !strcmp(name, "regu_d1_onoff"))
		reg_id = PCF506XX_REGULATOR_D1REG;
	else if (!strcmp(name, "voltage_d3reg") || !strcmp(name, "regu_d3_onoff"))
		reg_id = PCF506XX_REGULATOR_D3REG;
	else if (!strcmp(name, "voltage_d4reg") || !strcmp(name, "regu_d4_onoff"))
		reg_id = PCF506XX_REGULATOR_D4REG;
	else if (!strcmp(name, "voltage_d5reg") || !strcmp(name, "regu_d5_onoff"))
		reg_id = PCF506XX_REGULATOR_D5REG;
	else if (!strcmp(name, "voltage_d6reg") || !strcmp(name, "regu_d6_onoff"))
		reg_id = PCF506XX_REGULATOR_D6REG;
	else if (!strcmp(name, "voltage_d7reg") || !strcmp(name, "regu_d7_onoff"))
		reg_id = PCF506XX_REGULATOR_D7REG;
	else if (!strcmp(name, "voltage_ioreg") || !strcmp(name, "regu_io_onoff"))
		reg_id = PCF506XX_REGULATOR_IOREG;
	else if (!strcmp(name, "voltage_usbreg") || !strcmp(name, "regu_usb_onoff"))
		reg_id = PCF506XX_REGULATOR_USBREG;
	else if (!strcmp(name, "voltage_hcreg") || !strcmp(name, "regu_hc_onoff"))
		reg_id = PCF506XX_REGULATOR_HCREG;
	else
		reg_id = -1;

	return reg_id;
}

static ssize_t show_onoff(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	unsigned int reg_id;
	u_int8_t val;
	
	reg_id = reg_id_by_name(attr->attr.name);
	if (reg_id < 0)
		return 0;
	
	val=pcf506XX_onoff_get(pcf, reg_id);

	if (reg_id == PCF506XX_REGULATOR_USBREG)
	{
		switch(val){
			case 0: 
					return sprintf(buf, "\033[1;34m%s is \033[2;31mOFF\033[0m\n",attr->attr.name);
					break;
			case PCF50616_REGU_USB_ON_MASK: 
					return sprintf(buf, "\033[1;34m%s is \033[2;32mON\033[0m\n",attr->attr.name);
					break;
			case (PCF50616_REGU_USB_ON_MASK | PCF50616_REGU_USB_ECO_MASK): 
					return sprintf(buf, "\033[1;34m%s is \033[2;33mECO\033[0m\n",attr->attr.name);
					break;
			default : return 0;
		}
	}
	else
	{
		switch (val&PCF50616_REGULATOR_ON_MASK) {
		case 0: 
			return sprintf(buf, "\033[1;34m%s is \033[2;31mOFF\033[0m\n",attr->attr.name);
			break;
		case PCF50616_REGULATOR_ECO:
			return sprintf(buf, "\033[1;34m%s is \033[2;33mECO\033[0m\n",attr->attr.name);
			break;
		case PCF50616_REGULATOR_ON:
			return sprintf(buf, "\033[1;34m%s is \033[2;32mON\033[0m\n",attr->attr.name);
			break;
		default : return sprintf(buf, "\033[1;34m%s is \033[2;33m%x\033[0m\n",attr->attr.name, (val&PCF50616_REGULATOR_ON_MASK));
		}
	}
}

static ssize_t set_onoff(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	unsigned long onoff = simple_strtoul(buf, NULL, 10);
	unsigned int reg_id;

	reg_id = reg_id_by_name(attr->attr.name);
	if (reg_id < 0)
		return -EIO;

	dbgl1("attempting to witch set %s(%d) %s\n", attr->attr.name,
               reg_id, onoff==0?"OFF":onoff==1?"ECO":"ON");

	pcf506XX_onoff_set(pcf, reg_id, onoff);

	return count;
}

static ssize_t show_vreg(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	unsigned int reg_id;
	u_int8_t val;

	reg_id = reg_id_by_name(attr->attr.name);
	if (reg_id < 0)
		return 0;

	val=pcf506XX_onoff_get(pcf, reg_id);
	if (reg_id == PCF506XX_REGULATOR_USBREG)
	{
		switch(val){
			case 0: 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;31m(but regulator is OFF)\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			case PCF50616_REGU_USB_ON_MASK: 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;31m\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			case (PCF50616_REGU_USB_ON_MASK | PCF50616_REGU_USB_ECO_MASK): 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;33m(but regulator is ECO)\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			default : return 0;
		}
	}
	else
	{
		switch(val&PCF50616_REGULATOR_ON_MASK){
			case 0: 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;31m(but regulator is OFF)\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			case PCF50616_REGULATOR_ECO: 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;33m(but regulator is ECO)\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			case PCF50616_REGULATOR_ON: 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;31m\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			default : return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;33m(but regulator is %x)\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id), (val&PCF50616_REGULATOR_ON_MASK));
		}
	}
}

static ssize_t set_vreg(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	unsigned long mvolts = simple_strtoul(buf, NULL, 10);
	unsigned int reg_id;

	reg_id = reg_id_by_name(attr->attr.name);
	if (reg_id < 0)
		return -EIO;

	dbgl1("attempting to set %s(%d) to %lu mvolts\n", attr->attr.name,
		reg_id, mvolts);

	if (mvolts == 0) {
		pcf506XX_onoff_set(pcf, reg_id, 0);
	} else {
		if (pcf506XX_voltage_set(pcf, reg_id, mvolts) < 0) {
			dev_warn(dev, "refusing to set %s(%d) to %lu mvolts "
				 "(max=%u)\n", attr->attr.name, reg_id, mvolts,
				 pcf->pdata->rails[reg_id].voltage.max);
			return -EINVAL;
		}
		pcf506XX_onoff_set(pcf, reg_id, 1);
	}

	return count;
}

/***********************************************************************
 * Charger Control by sysfs
 ***********************************************************************/

/* Enable/disable charging via main charger */
void pcf506XX_main_charge_enable(int on)
{
	if (!(pcf50616_global->pdata->used_features & PCF506XX_FEAT_CBC))
		return;

	if (on) {
        reg_set_bit_mask(pcf50616_global, PCF50616_REG_CBCC1, PCF50616_CBCC1_CHGENA, PCF50616_CBCC1_CHGENA);
		pcf50616_global->flags |= PCF50616_F_CHG_MAIN_ENABLED;
	} else {
        reg_clear_bits(pcf50616_global, PCF50616_REG_CBCC1, PCF50616_CBCC1_CHGENA);
	    pcf50616_global->flags &= ~PCF50616_F_CHG_CV_REACHED;
	    pcf50616_global->flags &= ~PCF50616_F_CHG_BATTFULL;
		pcf50616_global->flags &= ~PCF50616_F_CHG_MAIN_ENABLED;
	}
}
EXPORT_SYMBOL(pcf506XX_main_charge_enable);

void pcf506XX_SetUsbSuspend(int val)
{
	/* check if USB charge is enable */
	if (pcf50616_global->flags & PCF50616_F_CHG_USB_ENABLED)
	{
		/* set PWREN4 PMU pins relative to suspend state */
		gpio_set_value(pcf50616_global->pdata->usb_suspend_gpio,val);
	}
}
EXPORT_SYMBOL(pcf506XX_SetUsbSuspend);

/* Enable/disable charging via USB charger */
void pcf506XX_usb_charge_enable(int on)
{
	if (!(pcf50616_global->pdata->used_features & PCF506XX_FEAT_CBC))
		return;

	if (on) {
        reg_set_bit_mask(pcf50616_global, PCF50616_REG_CBCC1, PCF50616_CBCC1_USBENA, PCF50616_CBCC1_USBENA);
		pcf50616_global->flags |= PCF50616_F_CHG_USB_ENABLED;

		if (pcf50616_global->pdata->reduce_wp !=NULL)
		{
			/* alert RTK that USB charge if enable/disable			*/
			/* in order to adapt the DVM working point number		*/
			/* when reduce working point is ON, the GPIO which		*/
			/* toggle the PWREN4 PMU pin is no more used for DVM	*/
			/* but used for enable/disable the charger				*/
			pcf50616_global->pdata->reduce_wp(on); 
			/* request the GPIO previously used by RTK to toggle the PWREN4 */
			/* this GPIO is now used to enable/disable the USB suspend		*/
			gpio_request(pcf50616_global->pdata->usb_suspend_gpio,pcf50616_pdev->name);
		}
		
	} else {
		if (pcf50616_global->pdata->reduce_wp !=NULL)
		{
			pcf50616_global->pdata->reduce_wp(on); 
			gpio_free(pcf50616_global->pdata->usb_suspend_gpio);
		}
        reg_clear_bits(pcf50616_global, PCF50616_REG_CBCC1, PCF50616_CBCC1_USBENA);
	    pcf50616_global->flags &= ~PCF50616_F_CHG_CV_REACHED;
	    pcf50616_global->flags &= ~PCF50616_F_CHG_BATTFULL;
		pcf50616_global->flags &= ~PCF50616_F_CHG_USB_ENABLED;
	}
}
EXPORT_SYMBOL(pcf506XX_usb_charge_enable);


static ssize_t show_chgmode(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	char *b = buf;
	u_int8_t cbcc1 = reg_read(pcf, PCF50616_REG_CBCC1);
	u_int8_t cbcc2 = reg_read(pcf, PCF50616_REG_CBCC2);
	u_int8_t cbcc3 = reg_read(pcf, PCF50616_REG_CBCC3);
	u_int8_t cbcc4 = reg_read(pcf, PCF50616_REG_CBCC4);
	u_int8_t cbcc5 = reg_read(pcf, PCF50616_REG_CBCC5);
	u_int8_t cbcc6 = reg_read(pcf, PCF50616_REG_CBCC6);
	u_int8_t cbcs1 = reg_read(pcf, PCF50616_REG_CBCS1);

    const char * const  autores[4]={"OFF","ON, Vres=Vmax-3%","ON, Vres=Vmax-6%","ON, Vres=Vmax-4.5%"};

    b += sprintf(b, "\033[1;34mBattery Charger Configuration:\033[0m\n");
    if (cbcc1 & PCF50616_CBCC1_CHGENA)
        b += sprintf(b, "Charging via main charger    : \033[2;32menabled\033[0m\n");
    else
        b += sprintf(b, "Charging via main charger    : \033[2;31mdisabled\033[0m\n");
    if (cbcc1 & PCF50616_CBCC1_USBENA)
        b += sprintf(b, "Charging via USB charger     : \033[2;32menabled\033[0m\n");
    else
        b += sprintf(b, "Charging via USB charger     : \033[2;31mdisabled\033[0m\n");
    if (cbcc1 & PCF50616_CBCC1_AUTOCC)
        b += sprintf(b, "Fast charge                  : \033[2;32menabled\033[0m\n");
    else
        b += sprintf(b, "Fast charge                  : \033[2;31mdisabled\033[0m\n");
    if (cbcc1 & PCF50616_CBCC1_AUTOSTOP) {
        b += sprintf(b, "Auto stop charge             : \033[2;32menabled\033[0m\n");
        if (cbcc1 & PCF50616_CBCC1_AUTORES_MASK)
            b += sprintf(b, "Automatic charge resume      : \033[2;33m%s\033[0m\n",autores[((cbcc1&PCF50616_CBCC1_AUTORES_MASK)&PCF50616_CBCC1_AUTORES_MASK)>>4]);
    }
    else
        b += sprintf(b, "Auto stop charge             : \033[2;31mdisabled\033[0m\n");
    if (cbcc1 & PCF50616_CBCC1_WDTIME_5H)
        b += sprintf(b, "Charge time                  : \033[2;31mlimited to 5 hours\033[0m\n");
    else
        b += sprintf(b, "Charge time                  : \033[2;32mnot limited\033[0m\n");

    b += sprintf(b, "Vmax level setting           = \033[2;33m4.%02uV\033[0m\n",2*(((cbcc2&PCF50616_CBCC2_VBATMAX_MASK)>>3)-1));

    b += sprintf(b, "Precharge cur. main charger  = \033[2;33m%umA\033[0m\n",cbcc3*1136/255);

    b += sprintf(b, "Precharge cur. USB charger   = \033[2;33m%umA\033[0m\n",cbcc4*1136/255);
    b += sprintf(b, "Trickle charge current       = \033[2;33m%umA\033[0m\n",((cbcc5&0x7F)+1)*1136/128);

	if (cbcc6 & PCF50616_CBCC6_OVPENA)
        b += sprintf(b, "Over volt. prot. Main Charger: \033[2;32menabled\033[0m\n");
    else
        b += sprintf(b, "Over volt. prot. Main Charger: \033[2;31mdisabled\033[0m\n");

    if (cbcc6 & PCF50616_CBCC6_CVMOD)
        b += sprintf(b, "Charge current limited by    : \033[2;32mexternal charger\033[0m\n");
    else
        b += sprintf(b, "Charge current limited by    : \033[2;31mPMU\033[0m\n");

    b += sprintf(b, "\033[1;34m\nBattery Charger Status:\033[0m\n");

    if (cbcs1 & PCF50616_CBCS1_BATFUL)
        b += sprintf(b, "\033[32mBattery Full\033[0m\n");
    if (cbcs1 & PCF50616_CBCS1_TLMT)
        b += sprintf(b, "\033[31mThermal Limiting Active\033[0m\n");
    if (cbcs1 & PCF50616_CBCS1_WDEXP)
        b += sprintf(b, "\033[31mCBC Watchdog Timer expired\033[0m\n");
    if (cbcs1 & PCF50616_CBCS1_ILMT)
        b += sprintf(b, "\033[32mCharge current lower than trickle current\033[0m\n");
    else
        b += sprintf(b, "\033[31mCharge current higher than trickle current\033[0m\n");
    if (cbcs1 & PCF50616_CBCS1_VLMT)
        b += sprintf(b, "\033[32mVoltage higher than Vmax\033[0m\n");
    else
        b += sprintf(b, "\033[31mVoltage lower than Vmax\033[0m\n");
    if (cbcs1 & PCF50616_CBCS1_CHGOVP)
        b += sprintf(b, "\033[31mCharger in over voltage protection\033[0m\n");
    if (cbcs1 & PCF50616_CBCS1_RESSTAT)
        b += sprintf(b, "\033[32mCharging has been resumed\033[0m\n");

	return b-buf;
}

static ssize_t set_chgmode(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	if (strcmp(buf, "MCBOFF\n")==0) {
        dbgl1("MCBOFF received\n");
		pcf506XX_main_charge_enable(0);
    }
	else if (strcmp(buf, "MCBON\n")==0) {
        dbgl1("MCBON received\n");
		pcf506XX_main_charge_enable(1);
    }
    else if (strcmp(buf, "USBOFF\n")==0) {
        dbgl1("USBOFF received\n");
        pcf506XX_usb_charge_enable(0);
    }
    else if (strcmp(buf, "USBON\n")==0) {
        dbgl1("USBON received\n");
        pcf506XX_usb_charge_enable(1);
    }
    else
        dbgl1("%s received\n",buf);

	return count;
}


static const char *chgstate_names[] = {
	"Main charger plugged",
	"USB charger plugged",
	"Charge by Main charger enabled",
	"Charge by USB charger enabled",
	"Thermal limiting activated",
	"Battery Full",
	"CV mode",
	"CC mode",
};

static ssize_t show_chgstate(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);

	char *b = buf;
	int i;

    b += sprintf(b, "\033[1;34mBattery Charger State:\033[0m\n");

	/* Display messages up to Battery Full */
	for (i = 0; i <  ARRAY_SIZE(chgstate_names)-2; i++)
		if (pcf->flags & (1 << i) )
			b += sprintf(b, "\033[1;32m%s\033[0m\n", chgstate_names[i]);

	/* Special management for CC/CV mode */
	if ( (pcf->flags & PCF50616_F_CHG_MAIN_PLUGGED) || (pcf->flags & PCF50616_F_CHG_USB_PLUGGED) )
	{
		if ( pcf->flags & PCF50616_F_CHG_CV_REACHED )
			b += sprintf(b, "\033[1;32m%s\033[0m\n", chgstate_names[i]);
		else
			b += sprintf(b, "\033[1;32m%s\033[0m\n", chgstate_names[i+1]);
	}

	if (b > buf)
		b += sprintf(b, "\n");

	return b - buf;
}

#if defined(CONFIG_NKERNEL) && defined (CONFIG_NET_9P_XOSCORE)
static ssize_t show_battvolt(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
    adc_aread(FID_VBAT,pcf5062x_adc_voltage);

	return sprintf(buf, "Battery voltage=%ldmV\n",batt_volt/1000);
}

static ssize_t show_batttemp(struct device *dev, struct device_attribute *attr,
                             char *buf)
{
    adc_aread(FID_TEMP,pcf5062x_adc_temperature);

    batt_temp /= 1000;
	return sprintf(buf, "Battery temperature=%02lu.%02lu°C\n",batt_temp/1000,batt_temp%1000);
}

static ssize_t show_accessory(struct device *dev, struct device_attribute *attr,
                             char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);

	if ( !strcmp(attr->attr.name, "tvout") )
	{
		if (pcf->accessory.tvout_plugged)
			return sprintf(buf, "plugged\n");
		else
			return sprintf(buf, "unplugged\n");
	} else
		if ( !strcmp(attr->attr.name, "headset") )
		{
			if (pcf->accessory.headset_plugged)
				return sprintf(buf, "plugged\n");
			else
				return sprintf(buf, "unplugged\n");
		} else
			if ( !strcmp(attr->attr.name, "headsetkey") )
			{
				if (pcf->accessory.button_pressed)
					return sprintf(buf, "pressed\n");
				else
					return sprintf(buf, "released\n");
			}


	return sprintf(buf, "Battery temperature=%02lu.%02lu°C\n",batt_temp/1000,batt_temp%1000);
}
#endif /* CONFIG_NKERNEL */

/* Backup Battery*/
static ssize_t show_bbccstate(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	unsigned long timeleft = (pcf->bbcc.endtime - jiffies)/HZ;
	int time_h, time_m, time_s;
	char *b = buf;
	u_int8_t bbcc = reg_read(pcf, PCF50616_REG_BBCC);

	const char * const charge_mode[4] = {"UNKNOWN", "long ", "fast ", "long dis"};
	const char * const charge_cur[4] =  {"50uA", "100uA", "200uA", "400uA"};

	time_h = timeleft/3600;
	time_m = (timeleft-(time_h*3600))/60;
    time_s = (timeleft-(time_h*3600))%60;

	b += sprintf(b, "\033[1;34mBackup Battery Configuration:\n");
	b += sprintf(b, "Backup Battery Charge is \033[1;31m%s, ",bbcc&PCF50616_BBCC_BBCE?"Enabled":"disabled");
	b += sprintf(b, "\033[1;34m(\033[1;31m%s\033[1;34m, \033[1;31m%s\033[1;34m)\n", bbcc&PCF50616_BBCC_BBCV?"3V3":"2V5",charge_cur[(bbcc&PCF50616_BBCC_CURRENT_MASK)>>2]);

	b += sprintf(b, "Backup battery is in \033[1;31m%scharge \033[1;34mmode, \033[1;31m%02uH%02u'%02us \033[1;34mremaining\n",charge_mode[pcf->bbcc.state], time_h, time_m, time_s);
	b += sprintf(b, "\033[1;34mLong charge timer : \033[1;31m%us\n", pcf->bbcc.long_charge/HZ);
	b += sprintf(b, "\033[1;34mFast charge timer : \033[1;31m%us\n", pcf->bbcc.fast_charge/HZ);
	b += sprintf(b, "\033[1;34mDischarge timer   : \033[1;31m%us\n\n", pcf->bbcc.discharge/HZ);
	b += sprintf(b, "\033[1;32mTo modify bbcc state and timings:\n");
	b += sprintf(b, "\033[0;37mecho state timing > bbcc \033[1;34m(state=1,2 or 3, timing in second)\n");
	b += sprintf(b, "state=1 -> long charge mode\n");
	b += sprintf(b, "state=2 -> fast charge mode\n");
	b += sprintf(b, "state=3 -> discharge mode\n\033[0m");

	return b-buf;
}

static ssize_t set_bbccstate(struct device *dev, struct device_attribute *attr, 
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
    char buf1[5], buf2[10];
	int i=0, j=0;
    unsigned long cmd, timing;

    /* Copy the first part of buf (before the white space) in buf1 */
    while ( !isspace(buf[i]) && (i<count) )
    {
        buf1[i]=buf[i];
        i++;
    }
    /* Add EoS */
    buf1[i]='\n'; /* Line Feed */

    /* Skip the whitespace */
    i++;

    while ( !isspace(buf[i]) && (i<count) )
    {
        buf2[j]=buf[i];
        j++;
        i++;
    }
    /* Add EoS */
    buf2[j]='\n'; /* Line Feed */

	cmd = simple_strtoul(buf1, NULL, 0);
	timing = simple_strtoul(buf2, NULL, 0);

	if ((cmd > 3) || (cmd==0) || (timing==0))
	{
		printk("Error in arguments: cmd=%lu, timing=%lu",cmd, timing);
		return -EINVAL;
	}

	pcf->bbcc.state = cmd;
	cancel_delayed_work_sync(&pcf->bbcc.work_timer);

	switch (cmd) {
	case PCF50616_BBCC_LONG_CHARGE:
		pcf->bbcc.long_charge = timing*HZ;
		queue_delayed_work(pcf->workqueue, &pcf->bbcc.work_timer, pcf->bbcc.long_charge);
	    reg_set_bit_mask(pcf, PCF50616_REG_BBCC, PCF50616_BBCC_BBCE, PCF50616_BBCC_BBCE);    
		break;
	case PCF50616_BBCC_FAST_CHARGE:
		pcf->bbcc.fast_charge = timing*HZ;
		queue_delayed_work(pcf->workqueue, &pcf->bbcc.work_timer, pcf->bbcc.fast_charge);
	    reg_set_bit_mask(pcf, PCF50616_REG_BBCC, PCF50616_BBCC_BBCE, PCF50616_BBCC_BBCE);    
		break;
	case PCF50616_BBCC_DISCHARGE:
		pcf->bbcc.discharge = timing*HZ;
		queue_delayed_work(pcf->workqueue, &pcf->bbcc.work_timer, pcf->bbcc.discharge);
        reg_clear_bits(pcf, PCF50616_REG_BBCC, PCF50616_BBCC_BBCE);
		break;
	};

	/* Modify entime according to new timer valuer */
	pcf->bbcc.endtime = jiffies + timing*HZ;

	return count;
}


/***********************************************************************
 * DEVFS Management
 ***********************************************************************/
/* with VREG a direct read to pmu register without VREG abstraction exists for test in sysv */
#ifdef VREG 
static DEVICE_ATTR(rawread, S_IWUSR | S_IRUGO, show_read, set_rawread);
static DEVICE_ATTR(rawwrite, S_IWUSR | S_IRUGO, show_write, set_rawwrite);
#endif 
static DEVICE_ATTR(read, S_IRUGO | S_IWUSR, show_read, set_read);
static DEVICE_ATTR(write, S_IRUGO | S_IWUSR, show_write, set_write);
static DEVICE_ATTR(regu_d1_onoff, S_IRUGO | S_IWUSR, show_onoff, set_onoff);
static DEVICE_ATTR(regu_d3_onoff, S_IRUGO | S_IWUSR, show_onoff, set_onoff);
static DEVICE_ATTR(regu_d2_onoff, S_IRUGO | S_IWUSR, show_onoff, set_onoff);
static DEVICE_ATTR(regu_d4_onoff, S_IRUGO | S_IWUSR, show_onoff, set_onoff);
static DEVICE_ATTR(regu_d5_onoff, S_IRUGO | S_IWUSR, show_onoff, set_onoff);
static DEVICE_ATTR(regu_d6_onoff, S_IRUGO | S_IWUSR, show_onoff, set_onoff);
static DEVICE_ATTR(regu_d7_onoff, S_IRUGO | S_IWUSR, show_onoff, set_onoff);
static DEVICE_ATTR(regu_io_onoff, S_IRUGO | S_IWUSR, show_onoff, set_onoff);
static DEVICE_ATTR(regu_usb_onoff, S_IRUGO | S_IWUSR, show_onoff, set_onoff);
static DEVICE_ATTR(regu_hc_onoff, S_IRUGO | S_IWUSR, show_onoff, set_onoff);
static DEVICE_ATTR(voltage_d1reg, S_IRUGO | S_IWUSR, show_vreg, set_vreg);
static DEVICE_ATTR(voltage_d3reg, S_IRUGO | S_IWUSR, show_vreg, set_vreg);
static DEVICE_ATTR(voltage_d4reg, S_IRUGO | S_IWUSR, show_vreg, set_vreg);
static DEVICE_ATTR(voltage_d5reg, S_IRUGO | S_IWUSR, show_vreg, set_vreg);
static DEVICE_ATTR(voltage_d6reg, S_IRUGO | S_IWUSR, show_vreg, set_vreg);
static DEVICE_ATTR(voltage_d7reg, S_IRUGO | S_IWUSR, show_vreg, set_vreg);
static DEVICE_ATTR(voltage_ioreg, S_IRUGO | S_IWUSR, show_vreg, set_vreg);
static DEVICE_ATTR(voltage_usbreg, S_IRUGO | S_IWUSR, show_vreg, set_vreg);
static DEVICE_ATTR(voltage_hcreg, S_IRUGO | S_IWUSR, show_vreg, set_vreg);
static DEVICE_ATTR(chgstate, S_IRUGO, show_chgstate, NULL);
#if defined(CONFIG_NKERNEL) && defined (CONFIG_NET_9P_XOSCORE)
static DEVICE_ATTR(battvolt, S_IRUGO, show_battvolt, NULL);
static DEVICE_ATTR(batttemp, S_IRUGO, show_batttemp, NULL);
static DEVICE_ATTR(tvout, S_IRUGO, show_accessory, NULL);
static DEVICE_ATTR(headset, S_IRUGO, show_accessory, NULL);
static DEVICE_ATTR(headsetkey, S_IRUGO, show_accessory, NULL);
#endif
static DEVICE_ATTR(chgmode, S_IRUGO | S_IWUSR, show_chgmode, set_chgmode);
static DEVICE_ATTR(bbcc, S_IRUGO | S_IWUSR, show_bbccstate, set_bbccstate);

static struct attribute *pcf50616_pcf_sysfs_entries[32] = {
	&dev_attr_read.attr,
	&dev_attr_write.attr,
	&dev_attr_regu_d1_onoff.attr,
	&dev_attr_regu_d2_onoff.attr,
	&dev_attr_regu_d3_onoff.attr,
	&dev_attr_regu_d4_onoff.attr,
	&dev_attr_regu_d5_onoff.attr,
	&dev_attr_regu_d6_onoff.attr,
	&dev_attr_regu_d7_onoff.attr,
	&dev_attr_regu_io_onoff.attr,
	&dev_attr_regu_usb_onoff.attr,
	&dev_attr_regu_hc_onoff.attr,
	&dev_attr_voltage_d1reg.attr,
	&dev_attr_voltage_d3reg.attr,
	&dev_attr_voltage_d4reg.attr,
	&dev_attr_voltage_d5reg.attr,
	&dev_attr_voltage_d6reg.attr,
	&dev_attr_voltage_d7reg.attr,
	&dev_attr_voltage_ioreg.attr,
	&dev_attr_voltage_usbreg.attr,
	&dev_attr_voltage_hcreg.attr,
#if defined(CONFIG_NKERNEL) && defined (CONFIG_NET_9P_XOSCORE)
    &dev_attr_battvolt.attr,
    &dev_attr_batttemp.attr,
    &dev_attr_tvout.attr,
    &dev_attr_headset.attr,
    &dev_attr_headsetkey.attr,
#endif
#ifdef VREG
	&dev_attr_rawread.attr,
	&dev_attr_rawwrite.attr,
#endif 
	&dev_attr_bbcc.attr,
	NULL
};

static const struct attribute_group pcf50616_attr_group = {
	.name	= NULL,			/* put in device directory */
	.attrs = pcf50616_pcf_sysfs_entries,
};

static void populate_sysfs_group(struct pcf50616_data *pcf)
{
	int i = 0;
	struct attribute **attr;

	for (attr = pcf50616_pcf_sysfs_entries; *attr; attr++)
		i++;

	if (pcf->pdata->used_features & PCF506XX_FEAT_CBC) {
		pcf50616_pcf_sysfs_entries[i++] = &dev_attr_chgstate.attr;
		pcf50616_pcf_sysfs_entries[i++] = &dev_attr_chgmode.attr;
	}
}

/***********************************************************************
 * Charge Management, cf spec VYn_ps18857
 ***********************************************************************/
static void pcf50616_void(u_int16_t x)
{
    dbgl1("WARNING !!! Function called before drvchr init !!!\n");   
};

static void pcf50616_usb_void(u_int16_t x)
{
    dbgl1("WARNING !!! Function called before usb driver init !!!\n");   
};

void pcf506XX_setAutoStop(u_int8_t on)
{
    if (on)
    {
        reg_set_bit_mask(pcf50616_global, PCF50616_REG_CBCC1, PCF50616_CBCC1_AUTOSTOP, PCF50616_CBCC1_AUTOSTOP);
        reg_set_bit_mask(pcf50616_global, PCF50616_REG_CBCC1, PCF50616_CBCC1_AUTORES_MASK, PCF50616_CBCC1_AUTORES_ON45);
    } else {
        reg_clear_bits(pcf50616_global, PCF50616_REG_CBCC1, PCF50616_CBCC1_AUTOSTOP);
    }
}

void pcf506XX_setVmax(u_int8_t vmax)
{
    reg_set_bit_mask(pcf50616_global, PCF50616_REG_CBCC2, PCF50616_CBCC2_VBATMAX_MASK, vmax);
}

void pcf506XX_setMainChgCur(u_int8_t cur)
{
    reg_write(pcf50616_global, PCF50616_REG_CBCC3, cur);
}

void pcf506XX_setUsbChgCur(u_int8_t cur)
{
    reg_write(pcf50616_global, PCF50616_REG_CBCC4, cur);
}

void pcf506XX_setTrickleChgCur(u_int8_t cur)
{
	if (cur <= 0x1F)
	{
	    reg_write(pcf50616_global, PCF50616_REG_CBCC5, cur);
	}
	else
	{
		printk("Error in argument: cur=%d is over 0x1F",cur);
	}
}

u_int8_t pcf506XX_getIlimit(void)
{
    u_int8_t value;

    value = reg_read(pcf50616_global,PCF50616_REG_CBCS1);
    return (value & PCF50616_CBCS1_ILMT);
}

u_int8_t pcf506XX_mainChgPlugged(void)
{
    return (pcf50616_global->flags & PCF50616_F_CHG_MAIN_PLUGGED);
}

u_int8_t pcf506XX_usbChgPlugged(void)
{
    return (pcf50616_global->flags & PCF50616_F_CHG_USB_PLUGGED);
}

u_int8_t pcf506XX_CVmodeReached(void)
{
    return (pcf50616_global->flags & PCF50616_F_CHG_CV_REACHED);
}

void pcf506XX_disableBattfullInt(void)
{
    reg_set_bit_mask(pcf50616_global, PCF50616_REG_INT4M, PCF50616_INT4_BATFUL, PCF50616_INT4_BATFUL);    
}

void pcf506XX_enableBattfullInt(void)
{
    reg_clear_bits(pcf50616_global, PCF50616_REG_INT4M, PCF50616_INT4_BATFUL);
}

void pcf506XX_registerChgFct(itcallback chgfct)
{
    pcf50616_global->pcf50616_itcallback = chgfct;
}

void pcf506XX_unregisterChgFct(void)
{
    pcf50616_global->pcf50616_itcallback = pcf50616_void;
}

void pcf506XX_registerUsbFct(itcallback usbfct)
{
    pcf50616_global->pcf50616_usbcallback = usbfct;
}

void pcf506XX_unregisterUsbFct(void)
{
    pcf50616_global->pcf50616_usbcallback = pcf50616_usb_void;
}

void pcf506XX_registerOnkeyFct(itcallback onkeyfct)
{
    pcf50616_onkey = onkeyfct;
}

void pcf506XX_unregisterOnkeyFct(void)
{
    pcf50616_onkey = pcf50616_void;
}


EXPORT_SYMBOL(pcf506XX_CVmodeReached);
EXPORT_SYMBOL(pcf506XX_usbChgPlugged);
EXPORT_SYMBOL(pcf506XX_mainChgPlugged);
EXPORT_SYMBOL(pcf506XX_getIlimit);
EXPORT_SYMBOL(pcf506XX_enableBattfullInt);
EXPORT_SYMBOL(pcf506XX_setMainChgCur);
EXPORT_SYMBOL(pcf506XX_setTrickleChgCur);
EXPORT_SYMBOL(pcf506XX_setUsbChgCur);
EXPORT_SYMBOL(pcf506XX_setVmax);
EXPORT_SYMBOL(pcf506XX_setAutoStop);
EXPORT_SYMBOL(pcf506XX_disableBattfullInt);
EXPORT_SYMBOL(pcf506XX_registerChgFct);
EXPORT_SYMBOL(pcf506XX_unregisterChgFct);
EXPORT_SYMBOL(pcf506XX_registerUsbFct);
EXPORT_SYMBOL(pcf506XX_unregisterUsbFct);

/***********************************************************************
 * Backup Battery Management
 ***********************************************************************/
static void pcf50616_bbcc_work_timer(struct work_struct *work_timer)
{

	switch (pcf50616_global->bbcc.state) {
	case PCF50616_BBCC_LONG_CHARGE:
		pcf50616_global->bbcc.endtime = jiffies + pcf50616_global->bbcc.discharge;
		schedule_delayed_work(&pcf50616_global->bbcc.work_timer, pcf50616_global->bbcc.discharge);
        reg_clear_bits(pcf50616_global, PCF50616_REG_BBCC, PCF50616_BBCC_BBCE);
		pcf50616_global->bbcc.state = PCF50616_BBCC_DISCHARGE;
		break;
	case PCF50616_BBCC_FAST_CHARGE:
		pcf50616_global->bbcc.endtime = jiffies + pcf50616_global->bbcc.discharge;
		schedule_delayed_work(&pcf50616_global->bbcc.work_timer, pcf50616_global->bbcc.discharge);
		reg_clear_bits(pcf50616_global, PCF50616_REG_BBCC, PCF50616_BBCC_BBCE);
		pcf50616_global->bbcc.state = PCF50616_BBCC_DISCHARGE;
		break;

	case PCF50616_BBCC_DISCHARGE:
		pcf50616_global->bbcc.endtime = jiffies + pcf50616_global->bbcc.fast_charge;
		schedule_delayed_work(&pcf50616_global->bbcc.work_timer, pcf50616_global->bbcc.fast_charge);
	    reg_set_bit_mask(pcf50616_global, PCF50616_REG_BBCC, PCF50616_BBCC_BBCE, PCF50616_BBCC_BBCE);    
		pcf50616_global->bbcc.state = PCF50616_BBCC_FAST_CHARGE;
		break;
	}

	dbgl1("New charge mode is: %u\n", pcf50616_global->bbcc.state);
	dbgl1("Current time is %lu, next expired timer is %lu\n", jiffies, pcf50616_global->bbcc.endtime);

}

/* as this function is called from a botton half implemented on a work queue 
 * the launch of the work queue is always successfull */
static void pcf50616_work(struct work_struct *work)
{
	struct pcf50616_data *pcf = container_of(work, struct pcf50616_data, work);
    u_int16_t batteryInt = 0;
	u_int8_t int1, int2, int3, int4;
	char *env[] = {"TVOUT=unplugged", NULL};
	struct kobject *pkobj = &(pcf50616_pdev->dev.kobj);
	pcf->working = 1;

    /* Read interrupt MASK for debug */
	int1 = __reg_read(pcf, PCF50616_REG_INT1M);
	int2 = __reg_read(pcf, PCF50616_REG_INT2M);
	int3 = __reg_read(pcf, PCF50616_REG_INT3M);
	int4 = __reg_read(pcf, PCF50616_REG_INT4M);

	dbgl2("INT1M=0x%02x INT2M=0x%02x INT3M=0x%02x INT4M=0x%02x\n",int1, int2, int3, int4);

	int1 = __reg_read(pcf, PCF50616_REG_INT1);
	int2 = __reg_read(pcf, PCF50616_REG_INT2);
	int3 = __reg_read(pcf, PCF50616_REG_INT3);
	int4 = __reg_read(pcf, PCF50616_REG_INT4);
#ifdef VREG
	pcf50616_user_irq_handler(pcf);
#endif 

	dbgl2("INT1=0x%02x INT2=0x%02x INT3=0x%02x INT4=0x%02x\n",int1, int2, int3, int4);

	if (int1 & PCF50616_INT1_LOWBAT) {
		/* BVM detected low battery voltage */
		dbgl1("LOWBAT\n");
        batteryInt |= PCF506XX_LOWBAT;

		dbgl2("SIGPWR(init) ");
		kill_cad_pid(SIGPWR, 0);
		/* Tell PMU we are taking care of this */
		reg_set_bit_mask(pcf, PCF50616_REG_OOCC,
				 PCF50616_OOCC_TOT_RST,
				 PCF50616_OOCC_TOT_RST);
	}

	if (int1 & PCF50616_INT1_SECOND) {
		/* RTC periodic second interrupt */
		dbgl1("SECOND\n");
		if (pcf->flags & PCF50616_F_RTC_SECOND)
			rtc_update_irq(pcf->rtc, 1,
				       RTC_PF | RTC_IRQF);
    }


    if (int1 & PCF50616_INT1_MINUTE) {
		dbgl1("MINUTE\n");
	}
    
	if (int1 & PCF50616_INT1_ALARM) {
		dbgl1("ALARM\n");
		if (pcf->pdata->used_features & PCF506XX_FEAT_RTC)
			rtc_update_irq(pcf->rtc, 1,
				       RTC_AF | RTC_IRQF);
		pcf->rtc_alarm.pending = 1;
	}
	
	if (int1 & PCF50616_INT1_ONKEYF) {
		/* ONKEY falling edge (start of button press) */
		dbgl1("ONKEYF\n");
//		input_report_key(pcf->input_dev, KEY_STOP, 1);
	    pcf50616_onkey(1);
	}

	if (int1 & PCF50616_INT1_ONKEYR) {
		/* ONKEY rising edge (end of button press) */
		dbgl1("ONKEYR\n");
		pcf->onkey_seconds = 0;
//		input_report_key(pcf->input_dev, KEY_STOP, 0);
	    pcf50616_onkey(0);
	}

	if (int1 & PCF50616_INT1_ONKEY1S) {
		dbgl1("ONKEY1S\n");
		pcf->onkey_seconds++;
		if (pcf->onkey_seconds >=
		    pcf->pdata->onkey_seconds_required) {
			/* Ask init to do 'ctrlaltdel' */
			dbgl1("SIGINT(init) ");
			kill_cad_pid(SIGPWR, 0);
			/* FIXME: what if userspace doesn't shut down? */
		}
	}

	if (int1 & PCF50616_INT1_HIGHTMP) {
		dbgl1("HIGHTMP\n");
        batteryInt |= PCF506XX_HIGHTMP;
    }

    /* INT2 management */
	if (int2 & PCF50616_INT2_EXTONR) {
		dbgl1("EXTONR\n");
	}

	if (int2 & PCF50616_INT2_EXTONF) {
		dbgl1("EXTONF\n");
	}

#if defined(CONFIG_NKERNEL)
	if (int2 & PCF50616_INT2_RECLF) {
        dbgl1("RECLF: check resistance\n");
#ifdef CONFIG_NET_9P_XOSCORE
 		/* Set REC1 in continuous mode and mic bias enabled */
        reg_set_bit_mask(pcf, PCF50616_REG_OOCC, PCF50616_OOCC_REC_MOD | PCF50616_OOCC_MICB_EN | PCF50616_OOCC_REC_EN, PCF50616_OOCC_MICB_EN | PCF50616_OOCC_REC_EN  );
        
        adc_aread(FID_ACC,pcf5062x_adc_accessory);
#endif
	}

	if (int2 & PCF50616_INT2_RECLR) {
        dbgl1("RECLR\n");
        if (pcf->accessory.headset_plugged == 1) { 
            /* Headset plugged + RECLR => headset button released */
            pcf50616_global->accessory.button_pressed = 0;
            dbgl1("RECLR: Headset button released\n");
            input_report_key(pcf->input_dev, KEY_MUTE, 0);
        } else {
            /* TVout cable unplugged */
            pcf->accessory.tvout_plugged = 0;
            dbgl1("RECLR: Tvout unplugged\n");
			kobject_uevent_env(pkobj, KOBJ_CHANGE, env);
        }
	}

	if (int2 & PCF50616_INT2_RECHF) {
        dbgl1("RECLF: check resistance\n");
#ifdef CONFIG_NET_9P_XOSCORE
		adc_aread(FID_ACC,pcf5062x_adc_accessory);
#endif
	}
#endif /* CONFIG_NKERNEL */

	if (int2 & PCF50616_INT2_RECHR) {
        dbgl1("REC2HR\n");
        /* If current state of tvout is plugged, skip RECHR int */
        if (pcf->accessory.tvout_plugged == 1)
        {
            pcf->accessory.tvout_plugged = 0;
            dbgl1("REC2HR: Tvout unplugged\n");
			kobject_uevent_env(pkobj, KOBJ_CHANGE, env);
        } else {
            /* Headset unplugged */
            pcf->accessory.headset_plugged = 0;
            dbgl1("REC2HR: Headset unplugged\n");
            input_event(pcf->input_dev, EV_SW, SW_HEADPHONE_INSERT, 0);
#ifdef CONFIG_SWITCH
	switch_set_state(&pcf50616_global->accessory.swheadset, pcf50616_global->accessory.headset_plugged );
#endif

        }
	}

	if (int2 & PCF50616_INT2_VMAX) {
		dbgl1("VMAX\n");
		pcf->flags |= PCF50616_F_CHG_CV_REACHED;
        batteryInt |= PCF506XX_VMAX;
	}

	if (int2 & PCF50616_INT2_CHGWD) {
		dbgl1("CHGWD\n");
        batteryInt |= PCF506XX_CHGWD;
	}


    /* INT3 management */
	if (int3 & PCF50616_INT3_SIMUV) {
		dbgl1("SIMUV: USIMREG reported an under-voltage error\n");
	}

	if (int3 & PCF50616_INT3_INSERT) {
		dbgl1("INSERT: SIM cart has been inserted\n");
	}

	if (int3 & PCF50616_INT3_EXTRACT) {
		dbgl1("EXTRACT: SIM card has been removed\n");
	}

	if (int3 & PCF50616_INT3_OVPCHGON) {
		dbgl1("OVPCHGON: Charging is continued after OVP has been resolved\n");
        batteryInt |= PCF506XX_OVPCHGON;
	}

	if (int3 & PCF50616_INT3_OVPCHGOFF) {
		dbgl1("OVPCHGOFF: Charging is stopped dur to OVP condition\n");
        batteryInt |= PCF506XX_OVPCHGOFF;
	}

	if (int3 & PCF50616_INT3_MCHGINS) {
        dbgl1("MCHINS\n");
		/* charger insertion detected */
		pcf->flags |= PCF50616_F_CHG_MAIN_PLUGGED;
        batteryInt |= PCF506XX_MCHINS;
	}

	if (int3 & PCF50616_INT3_MCHGRM) {
		dbgl1("MCHRM\n");
		/* charger removal detected */
        pcf->flags &= ~PCF50616_F_CHG_MAIN_PLUGGED;
        batteryInt |= PCF506XX_MCHRM;
	}

    /* INT4 management */
	if (int4 & PCF50616_INT4_CHGRES) {
		dbgl1("CHGRES\n");
        batteryInt |= PCF506XX_CHGRES;
		/* battery voltage < autoes voltage threshold */
	}

	if (int4 & PCF50616_INT4_THLIMON) {
		dbgl1("THLIMON\n");
        batteryInt |= PCF506XX_THLIMON;
		pcf->flags |= PCF50616_F_CHG_THERM_LIMIT;
	}

	if (int4 & PCF50616_INT4_THLIMOFF) {
		dbgl1("THLIMOFF\n");
        batteryInt |= PCF506XX_THLIMOFF;
		pcf->flags &= ~PCF50616_F_CHG_THERM_LIMIT;
		/* FIXME: signal this to userspace */
	}
    
	if (int4 & PCF50616_INT4_BATFUL) {
		dbgl1("BATFUL: fully charged battery \n");
        batteryInt |= PCF506XX_BATFUL;
		pcf->flags |= PCF50616_F_CHG_BATTFULL;
	}

	if (int4 & PCF50616_INT4_UCHGINS) {
		dbgl1("UCHGINS\n");
		pcf->flags |= PCF50616_F_CHG_USB_PLUGGED;
        batteryInt |= PCF506XX_UCHGINS;
	    pcf->pcf50616_usbcallback(1);
	}

    if (int4 & PCF50616_INT4_UCHGRM) {
		dbgl1("UCHGRM\n");
		pcf->flags &= ~PCF50616_F_CHG_USB_PLUGGED;
        batteryInt |= PCF506XX_UCHGRM;
	    pcf->pcf50616_usbcallback(0);
	}



    pcf->pcf50616_itcallback(batteryInt);
	pcf->working = 0;
	input_sync(pcf->input_dev);
#ifdef CONFIG_I2C_NEW_PROBE
	put_device(&pcf->client->dev);
#else
	put_device(&pcf->client.dev);
#endif
#ifndef VREG 
	enable_irq(pcf->irq);
#else
	/* as this function is called from a botton half implemented on a work queue 
 * the launch of the work queue is always successfull */
    queue_work(pcf->workqueue, &pcf->work_enable_irq);
#endif 

}

static int pcf50616_schedule_work(struct pcf50616_data *pcf)
{
	int status;

#ifdef CONFIG_I2C_NEW_PROBE
	get_device(&pcf->client->dev);
#else
	get_device(&pcf->client.dev);
#endif
    status = queue_work(pcf->workqueue, &pcf->work);

	if (!status )
		dbgl2("pcf 50616 work item may be lost\n");
	return status;
}

static irqreturn_t pcf50616_irq(int irq, void *_pcf)
{
	struct pcf50616_data *pcf = _pcf;
    u_int8_t pmu_int;

	dbgl2("entering(irq=%u, pcf=%p): scheduling work\n", irq, _pcf);

    /* Read interrupt status to manage the pending interrupt */
	pmu_int = gpio_get_value(EXTINT_TO_GPIO(irq));

    if (pmu_int == 0)
    {
        pcf50616_schedule_work(pcf);
        /* Disable any further interrupts until we have processed */
        /* the current one */

        disable_irq(irq);
    }
/*     else */
/*         dbgl1("EXTINT is read as 1, no interrupt to manage\n"); */


	return IRQ_HANDLED;
}


/***********************************************************************
 * RTC interface implementation
 ***********************************************************************/
static int pcf50616_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	u_long counter;
	u_int8_t rtc1,rtc2,rtc3,rtc4;

	mutex_lock(&pcf->lock);

	/* Get value from PCF50616 reg */
	rtc1 = (u_int8_t)__reg_read(pcf, PCF50616_REG_RTC1);
	rtc2 = (u_int8_t)__reg_read(pcf, PCF50616_REG_RTC2);
	rtc3 = (u_int8_t)__reg_read(pcf, PCF50616_REG_RTC3);
	rtc4 = (u_int8_t)__reg_read(pcf, PCF50616_REG_RTC4);

	/* Compute the counter value */
	counter = (u_long)(rtc1<<24)+(u_long)(rtc2<<16)+(u_long)(rtc3<<8)+(u_long)rtc4;

	/* Convert the counter in GMT info */
	rtc_time_to_tm(counter,tm);

	mutex_unlock(&pcf->lock);

	return 0;
}

static int pcf50616_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);

	u_long counter;
    int ret;
	u_int8_t rtc1,rtc2,rtc3,rtc4;

	mutex_lock(&pcf->lock);

	/* Convert current RTC time in secondes */
	ret = rtc_tm_to_time(tm, &counter);

	/* Split the counter to store it in the RTC registers */
	rtc1 = (u_int8_t)((counter>>24)&0xFF);
	rtc2 = (u_int8_t)((counter>>16)&0xFF);
	rtc3 = (u_int8_t)((counter>>8)&0xFF);
	rtc4 = (u_int8_t)((counter)&0xFF);

	/* Write the values in the registers */
	ret = __reg_write(pcf, PCF50616_REG_RTC1,rtc1);
	ret = __reg_write(pcf, PCF50616_REG_RTC2,rtc2);
	ret = __reg_write(pcf, PCF50616_REG_RTC3,rtc3);
	ret = __reg_write(pcf, PCF50616_REG_RTC4,rtc4);

	mutex_unlock(&pcf->lock);

	return 0;
}

/*
 * Calculate the next alarm time given the requested alarm time mask
 * and the current time.
 */
static void rtc_next_alarm_time(struct rtc_time *next, struct rtc_time *now, struct rtc_time *alrm)
{
	unsigned long next_time;
	unsigned long now_time;

	next->tm_year = now->tm_year;
	next->tm_mon = now->tm_mon;
	next->tm_mday = now->tm_mday;
	next->tm_hour = alrm->tm_hour;
	next->tm_min = alrm->tm_min;
	next->tm_sec = alrm->tm_sec;

	rtc_tm_to_time(now, &now_time);
	rtc_tm_to_time(next, &next_time);

	if (next_time < now_time) {
		/* Advance one day */
		next_time += 60 * 60 * 24;
		rtc_time_to_tm(next_time, next);
	}
}

/**
 * pcf50616_fill_with_next_date - sets the next possible expiry date in alrm
 * @alrm: contains the sec, min and hour part of the time to be set
 *
 * This function finds the next possible expiry date and fills it into alrm.
 */
static int pcf50616_rtc_fill_with_next_date(struct device *dev, struct rtc_wkalrm *alrm)
{
    struct rtc_time next, now;

	/* read the current time */
	pcf50616_rtc_read_time(dev, &now);

	rtc_next_alarm_time(&next, &now, &alrm->time);

	alrm->time = next;
	return 0;
}

static int pcf50616_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	u_int8_t rtc1a,rtc2a,rtc3a,rtc4a;
	u_long counter;

	mutex_lock(&pcf->lock);

	/* Get value from PCF50616 reg */
	rtc1a = (u_int8_t)__reg_read(pcf, PCF50616_REG_RTC1A);
	rtc2a = (u_int8_t)__reg_read(pcf, PCF50616_REG_RTC2A);
	rtc3a = (u_int8_t)__reg_read(pcf, PCF50616_REG_RTC3A);
	rtc4a = (u_int8_t)__reg_read(pcf, PCF50616_REG_RTC4A);

	/* Compute the counter value */
	counter = (u_long)(rtc1a<<24)+(u_long)(rtc2a<<16)+(u_long)(rtc3a<<8)+(u_long)rtc4a;

	/* Convert the counter in GMT info */
	rtc_time_to_tm(counter,&alrm->time);

	alrm->enabled = pcf->rtc_alarm.enabled;
	alrm->pending = pcf->rtc_alarm.pending;

	dbgl1("Alarm %s\n", alrm->enabled?"Enabled":"Disabled");
	dbgl1("Alarm %s\n", alrm->pending?"Pending":"Not Pending");

	mutex_unlock(&pcf->lock);

	return 0;
}

/**
 * pcf50616_set_alarm - set the alarm in the RTC
 * @alrm: the time when the alarm should expire
 *
 * This function gets called from two different places in
 * linux/arch/arm/common/rtctime.c.
 *
 * 1) from the ioctl RTC_ALM_SET
 * 2) from the ioctl RTC_WKALM_SET
 *
 * 1):
 *
 *  - only seconds, minutes and hour are set in alrm, everything else is -1
 *  - should expire at the next possible date
 *  - assuming the alarm in non-persistent and should be activated immediately
 *
 * 2):
 *
 *  - should expire at the exact date alrm contains
 *
 */
static int pcf50616_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);
	u_long counter;
	int ret;
	u_int8_t rtc1a,rtc2a,rtc3a,rtc4a;
	u_int8_t irqmask;

	mutex_lock(&pcf->lock);

	alrm->pending = 0;
	if (alrm->time.tm_year == -1) {
		/* called from 1) - (see doc above this function) */
		ret = pcf50616_rtc_fill_with_next_date(dev, alrm);
		
		alrm->enabled = 1; /* non-persistant */
	}

	/* disable alarm interrupt */
	irqmask = __reg_read(pcf, PCF50616_REG_INT1M);
	irqmask |= PCF50616_INT1_ALARM;
    dbgl1("INT1 irq mask = %02X\n",irqmask);
	__reg_write(pcf, PCF50616_REG_INT1M, irqmask);

	/* Convert current RTC time in secondes */
	ret = rtc_tm_to_time(&alrm->time, &counter);

	/* Split the counter to store it in the RTC registers */
	rtc1a = (u_int8_t)((counter>>24)&0xFF);
	rtc2a = (u_int8_t)((counter>>16)&0xFF);
	rtc3a = (u_int8_t)((counter>>8)&0xFF);
	rtc4a = (u_int8_t)((counter)&0xFF);

	/* Write the values in the registers */
	ret = __reg_write(pcf, PCF50616_REG_RTC1A,rtc1a);
	ret = __reg_write(pcf, PCF50616_REG_RTC2A,rtc2a);
	ret = __reg_write(pcf, PCF50616_REG_RTC3A,rtc3a);
	ret = __reg_write(pcf, PCF50616_REG_RTC4A,rtc4a);

 	if (alrm->enabled) { 
		/* (re-)enaable alarm interrupt */
		irqmask = __reg_read(pcf, PCF50616_REG_INT1M);
		irqmask &= ~PCF50616_INT1_ALARM;
        dbgl1("INT1 irq mask = %02X\n",irqmask);
		__reg_write(pcf, PCF50616_REG_INT1M, irqmask);
 	} 

	pcf->rtc_alarm.enabled = alrm->enabled;
	pcf->rtc_alarm.pending = alrm->pending;


	dbgl1("Alarm %s\n", alrm->enabled?"Enabled":"Disabled");
	dbgl1("Alarm %s\n", alrm->pending?"Pending":"Not Pending");
	mutex_unlock(&pcf->lock);

	return 0;
}

static int pcf50616_rtc_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50616_data *pcf = i2c_get_clientdata(client);

	switch (cmd) {
	case RTC_PIE_OFF:
	case RTC_UIE_OFF:
		/* disable periodic interrupt (hz tick) */
		pcf->flags &= ~PCF50616_F_RTC_SECOND;
		reg_set_bit_mask(pcf, PCF50616_REG_INT1M, PCF50616_INT1_SECOND, PCF50616_INT1_SECOND);
		return 0;
	case RTC_PIE_ON:
	case RTC_UIE_ON:
		/* ensable periodic interrupt (hz tick) */
		pcf->flags |= PCF50616_F_RTC_SECOND;
		reg_clear_bits(pcf, PCF50616_REG_INT1M, PCF50616_INT1_SECOND);
		return 0;

	case RTC_AIE_ON:
/* 		local_irq_save(flags); */
/* 		int_enabled |= RTC_AIE_MASK; */
/* 		local_irq_restore(flags); */
		reg_clear_bits(pcf, PCF50616_REG_INT1M, PCF50616_INT1_ALARM);
		break;

	case RTC_AIE_OFF:
/* 		local_irq_save(flags); */
/* 		int_enabled &= ~RTC_AIE_MASK; */
/* 		local_irq_restore(flags); */
		reg_set_bit_mask(pcf, PCF50616_REG_INT1M, PCF50616_INT1_ALARM, PCF50616_INT1_ALARM);
		break;

/* 	case RTC_UIE_ON: */
/* 		local_irq_save(flags); */
/* 		int_enabled |= RTC_UIE_MASK; */
/* 		local_irq_restore(flags); */
/* 		pcf50623_send_ioctl(RTCIO_UIE_ON); */
		break;

/* 	case RTC_UIE_OFF: */
/* 		local_irq_save(flags); */
/* 		int_enabled &= ~RTC_UIE_MASK; */
/* 		local_irq_restore(flags); */
/* 		break; */

	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

static struct rtc_class_ops pcf50616_rtc_ops = {
	.read_time	= pcf50616_rtc_read_time,
	.set_time	= pcf50616_rtc_set_time,
	.read_alarm	= pcf50616_rtc_read_alarm,
	.set_alarm	= pcf50616_rtc_set_alarm,
	.ioctl		= pcf50616_rtc_ioctl,
};

/***********************************************************************
 * IOCTL management
 ***********************************************************************/
static int pcf50616_pmu_open(struct inode * inode, struct file * instance)
{
	dbgl1 ( KERN_INFO "pcf50616_pmu_open\n" );
  
	return 0 ;
}

static int pcf50616_pmu_close(struct inode * inode, struct file * instance)
{
	dbgl1 ( KERN_INFO "pcf50616_pmu_close\n" );
  
	return 0 ;
}


static int pcf50616_pmu_ioctl(struct inode *inode, struct file *instance,
                              unsigned int cmd, unsigned long arg)
{
    struct ioctl_write_reg swr;
	void __user *argp = (void __user *)arg;
    int reg, res;
    int value;

	dbgl1 ( KERN_INFO "pcf50616_pmu_ioctl, cmd=%u\n",cmd );

	switch (cmd) {
	case PCF506XX_IOCTL_READ_REG:
		res=copy_from_user(&reg, argp, sizeof(int));
		if (res)
			return -EFAULT;
		value = reg_read(pcf50616_global, reg);
		if (copy_to_user((void *)arg, &value, sizeof(value)))
			return -EFAULT;
		break;

	case PCF506XX_IOCTL_WRITE_REG:
		res=copy_from_user(&swr, argp, sizeof(swr));
		if (res)
			return -EFAULT;
		dbgl1("IOCTL write reg. Addr=0x%02X, Value=%u (0x%02X)\n", swr.address, swr.value, swr.value);
		return reg_write(pcf50616_global, swr.address, swr.value);
		break;

	case PCF506XX_IOCTL_BOOTCAUSE:
		{
			unsigned long bootcausereport=0;
			unsigned char int1,int2,int3,int4,oocs1,oocs2;
			int1 = bootcause & 0xff;
			int2 = (bootcause >> 8)& 0xff;
			int3 = (bootcause >> 16) & 0xff;
			int4 = (bootcause >> 24) & 0xff;
			oocs1 = oocscause &0xff;
			oocs2 = oocscause &0xff;

			/*bootcause contains the initial status */
			/* if neither onkey cause or alarm is set and non charger usb or main charger is plugged */
			/* system starts following several usb or charger removal*/ 
			/* boot cause bit map enum pcf50616_bootcause */
			if (int1 & 	PCF50616_INT1_ONKEYF) bootcausereport |= PCF506XX_ONKEY;
			if (int1 & 	PCF50616_INT1_ALARM)  bootcausereport |= PCF506XX_ALARM;
			if (oocs2 & PCF50616_OOCS2_UCHG_OK ) bootcausereport |= PCF506XX_USB;
			if (oocs1 & PCF50616_OOCS1_MCHGOK ) bootcausereport |= PCF506XX_MCH;

			switch (int4 &(PCF50616_INT4_UCHGRM|PCF50616_INT4_UCHGINS)) 
			{
			case (PCF50616_INT4_UCHGRM |PCF50616_INT4_UCHGINS) :
				bootcausereport |=  PCF506XX_USB_GLITCHES;
				break;
			case (PCF50616_INT4_UCHGRM ): 
				bootcausereport |=  PCF506XX_USB_GLITCH;
				break;
			}
			switch (int3 & (PCF50616_INT3_MCHGINS|PCF50616_INT3_MCHGRM)) 
			{
			case (PCF50616_INT3_MCHGINS|PCF50616_INT3_MCHGRM) :
				bootcausereport |=  PCF506XX_MCH_GLITCHES;
				break;
			case (PCF50616_INT3_MCHGRM): 
				bootcausereport |=  PCF506XX_MCH_GLITCH;
				break;
			}

			if (copy_to_user((void *)arg, &bootcausereport, sizeof(bootcausereport)))
				return -EFAULT;
			break;
		}

	default:
		dbgl1("Command unknown\n");
		break;

	}
    
    return 0;
}

static struct file_operations pcf50616_pmu_ops = {
	.owner		=	THIS_MODULE,
	.ioctl		=	pcf50616_pmu_ioctl,
	.open		= 	pcf50616_pmu_open,
	.release	=	pcf50616_pmu_close,
};

/***********************************************************************
 * Backlight device
 ***********************************************************************/

static int pcf50616_kbl_get_intensity(struct backlight_device *bd)
{
	u_int8_t intensity = reg_read(pcf50616_global, PCF50616_REG_D6C)& PCF50616_REGULATOR_ON;

	return intensity;
}

static int pcf50616_kbl_set_intensity(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;
	int ret;

	if (bd->props.power != FB_BLANK_UNBLANK)
		intensity = 0;
	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		intensity = 0;

	if (intensity == 0 )
		ret = reg_clear_bits(pcf50616_global, PCF50616_REG_D6C, PCF50616_REGULATOR_ON);
	else
		ret = reg_set_bit_mask(pcf50616_global, PCF50616_REG_D6C, PCF50616_REGULATOR_ON, PCF50616_REGULATOR_ON);


	return ret;
}

static struct backlight_ops pcf50616_kbl_ops = {
	.get_brightness	= pcf50616_kbl_get_intensity,
	.update_status	= pcf50616_kbl_set_intensity,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void pcf50616_kbl_suspend(struct early_suspend *pes)
{
	pcf50616_global->keypadbl->props.brightness = 0;
	pcf50616_kbl_set_intensity(pcf50616_global->keypadbl);
}

static void pcf50616_kbl_resume(struct early_suspend *pes)
{
	pcf50616_global->keypadbl->props.brightness = 1;
	pcf50616_kbl_set_intensity(pcf50616_global->keypadbl);
}

static struct early_suspend pcf50616_kbl_earlys = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 5,
	.suspend = pcf50616_kbl_suspend,
	.resume = pcf50616_kbl_resume,
};
#endif

/***********************************************************************
 * Driver initialization
 ***********************************************************************/

static struct platform_device pnx6708_pm_dev = {
	.name		="pnx-pmu",
};


static void pcf50616_work_timer(struct work_struct *work_timer)
{
	u_int8_t oocs1;

	oocs1   = reg_read(pcf50616_global, PCF50616_REG_OOCS1);
	dbgl1(KERN_INFO "RECL =%d RECH=%d\n",((oocs1 & PCF50616_OOCS1_RECL) ? 1: 0), ((oocs1 & PCF50616_OOCS1_RECH) ? 1: 0));
	if ( (oocs1 & PCF50616_NOACC_DETECTED) != PCF50616_NOACC_DETECTED)
	{
#ifdef CONFIG_NET_9P_XOSCORE
		if ( adc_aread(FID_ACC,pcf5062x_adc_accessory) != 0)
		{
			/* adc not ready yet, restart the timer */
			queue_delayed_work(pcf50616_global->workqueue, &pcf50616_global->accessory.work_timer, PCF50616_ACC_INIT_TIME);
		}
#endif
	}
}

static void pcf50616_init_registers(struct pcf50616_data *data)
{
//	int ret;
	struct rtc_time rtc_tm;
	u_long counter;
	u_int8_t int1, int2, int3, int4;
	u_int8_t rtc1, rtc2, rtc3, rtc4, rtc1a;
    u_int8_t oocs1, oocs2, id_var;
	u_int8_t i;

   /* Read interrupt register to in order to get boot cause */
   /* readind is done prior to enable any extint that trigger a interrruption */
    mutex_lock(&data->lock);
	id_var = __reg_read(data, PCF50616_REG_ID);
	int1   = __reg_read(data, PCF50616_REG_INT1);
	int2   = __reg_read(data, PCF50616_REG_INT2);
	int3   = __reg_read(data, PCF50616_REG_INT3);
	int4   = __reg_read(data, PCF50616_REG_INT4);
	rtc1   = __reg_read(data, PCF50616_REG_RTC1);
	rtc2   = __reg_read(data, PCF50616_REG_RTC2);
	rtc3   = __reg_read(data, PCF50616_REG_RTC3);
	rtc4   = __reg_read(data, PCF50616_REG_RTC4);
	rtc1a  = __reg_read(data, PCF50616_REG_RTC1A);
    oocs1   = __reg_read(data, PCF50616_REG_OOCS1);
    oocs2   = __reg_read(data, PCF50616_REG_OOCS2);

    mutex_unlock(&data->lock);
 
	/* configure interrupt mask */
    /* Enable all interrupt but INT1_SECOND and minute */
    mutex_lock(&data->lock);
	__reg_write(data, PCF50616_REG_OOCC, PCF50616_OOCC_REC_EN|PCF50616_OOCC_REC_MOD);
    __reg_write(data, PCF50616_REG_RECC1, 0xB0);

	__reg_write(data, PCF50616_REG_INT1M, PCF50616_INT1_SECOND | PCF50616_INT1_MINUTE);
	__reg_write(data, PCF50616_REG_INT2M, 0x00);
	__reg_write(data, PCF50616_REG_INT3M, 0x00);
	__reg_write(data, PCF50616_REG_INT4M, PCF50616_INT4_BATFUL);
	mutex_unlock(&data->lock);

    bootcause = int1 | (int2<<8) | (int3<<16) | (int4<<24);
    oocscause = oocs1 | oocs2<<8;
	printk(KERN_INFO "INT1=0x%02x INT2=0x%02x INT3=0x%02x INT4=0x%02x OOCS1=0x%02x OOCS2=0x%02x bootcause=0x%04lx \n",int1, int2, int3, int4, oocs1, oocs2, bootcause);
    /* Check current OOCS configuration to know wether a charger is plugged or not */

    if (oocs1 & PCF50616_OOCS1_MCHGOK) {
		data->flags |= PCF50616_F_CHG_MAIN_PLUGGED;
    } else {
        data->flags &= ~PCF50616_F_CHG_MAIN_PLUGGED;
        reg_clear_bits(data, PCF50616_REG_CBCC1, PCF50616_CBCC1_CHGENA);
    }
    
    if (oocs2 & PCF50616_OOCS2_UCHG_OK) {
		data->flags |= PCF50616_F_CHG_USB_PLUGGED;
    } else {        
        data->flags &= ~PCF50616_F_CHG_USB_PLUGGED;
        reg_clear_bits(data, PCF50616_REG_CBCC1, PCF50616_CBCC1_USBENA);
    }

	if ((pcf50616_global->pdata->used_features & PCF506XX_FEAT_CBC))
	{
		/* set CBCMOD to direct control by PWREN4 */
		reg_set_bit_mask(pcf50616_global, PCF50616_REG_CBCC5, PCF50616_CBCC5_CBCMOD_MASK, PCF50616_CBCC5_CBCMOD_DIRECT_CONTROL);
	}
	
	data->id = (u_int8_t)(id_var>>4);
	data->variant = (u_int8_t)(id_var&0x0F);

	printk(KERN_INFO "PMU50616 ID=%02u, VARIANT=%02u\n",data->id, data->variant);
	/* Initialize Config relative to Modem */
	{
		struct pmu_reg_table *table;
		int table_size;
		switch (id_var &0xF0) 
		{
		case 0x10 : /* PMU50616 N1A*/
			table=data->pdata->reg_table;
			table_size=data->pdata->reg_table_size;
			break;
		default :
			table=NULL;
			printk(KERN_ERR "Unknowm pmu Version !\n");
			break;
		}
		if (table)
		{
			mutex_lock(&data->lock);
			for ( i=0; i <table_size;i++)
			{
				if (__reg_nwrite(data,table[i].addr,table[i].value,table[i].size)!=table[i].size)
					printk(KERN_ERR "pcf50616 Modem Init %x ,%x failed  \n",
							table[i].addr,table[i].size);

			}
			mutex_unlock(&data->lock);
		}
	}

	/* Initializes Regulator voltage */
	for ( i=0; i<__NUM_PCF506XX_REGULATORS; i++ )
	{
		if (data->pdata->rails[i].used)
		{
			pcf506XX_voltage_set(data, i, data->pdata->rails[i].voltage.init);
			pcf506XX_onoff_set(data, i, data->pdata->rails[i].mode);
		}
	}

	/* Manage accessory detection at startup */
	oocs1   = reg_read(data, PCF50616_REG_OOCS1);
	dbgl1(KERN_INFO "RECL=%d RECH=%d\n",((oocs1 & PCF50616_OOCS1_RECL)? 1:0),((oocs1 & PCF50616_OOCS1_RECH)? 1:0));
	if ( (oocs1 & PCF50616_NOACC_DETECTED) != (PCF50616_OOCS1_RECL | PCF50616_OOCS1_RECH))
	{
#ifdef CONFIG_NET_9P_XOSCORE
		/* An accessory is detected, find which */
		if ( adc_aread(FID_ACC,pcf5062x_adc_accessory) != 0)
		{
			INIT_DELAYED_WORK(&data->accessory.work_timer, pcf50616_work_timer);
			queue_delayed_work(data->workqueue, &data->accessory.work_timer, PCF50616_ACC_INIT_TIME);
		}
#endif
	}

	/* Compute the counter value */
	counter = (u_long)(rtc1<<24)+(u_long)(rtc2<<16)+(u_long)(rtc3<<8)+(u_long)rtc4;
	dbgl1("Current counter time is %lu\n",counter);

	/* Start Backup Battery Management */
	INIT_DELAYED_WORK(&data->bbcc.work_timer, pcf50616_bbcc_work_timer);

	data->bbcc.fast_charge = PCF50616_BBCC_1H;
	data->bbcc.long_charge = PCF50616_BBCC_6H;
	data->bbcc.discharge   = PCF50616_BBCC_24H;

	/* If product woke up on alarm, don't care of rtc1a value */
	if ((int1 & 0x08) && (rtc1a==0))
	{
		printk("******        RTC ALARM WAKE-UP !!!!        ******\n");
		reg_write(data, PCF50616_REG_RTC1A, 0x55);
		rtc1a = 0x55;
	}

	if (counter > 0x7FFFFFFF)
	{
		/* RTC timer upper than 0x7FFFFFFF , that means that the backup battery is fully discharged */
		/* Start a 6 hours charge ... */
		dbgl1("Write 0x%02X in 0x%02X\n", PCF50616_BBCC_BBCE|PCF50616_BBCC_BBCC_50|PCF50616_BBCC_BBCV, PCF50616_REG_BBCC);
		reg_write(data, PCF50616_REG_BBCC, PCF50616_BBCC_BBCE|PCF50616_BBCC_BBCC_50|PCF50616_BBCC_BBCV);
		pcf50616_global->bbcc.endtime =  jiffies + PCF50616_BBCC_6H;
		data->bbcc.state = PCF50616_BBCC_LONG_CHARGE;
		queue_delayed_work(data->workqueue,&data->bbcc.work_timer, PCF50616_BBCC_6H);

		/* ... and set a correct date in RTC */
        rtc_tm.tm_mday = 29;
        rtc_tm.tm_mon  = 5 - 1;
        rtc_tm.tm_year = 2009 - 1900,
        rtc_tm.tm_hour = 8;
        rtc_tm.tm_min  = 0;
        rtc_tm.tm_sec  = 0;

#ifdef CONFIG_I2C_NEW_PROBE
		pcf50616_rtc_set_time(&data->client->dev, &rtc_tm);
#else
		pcf50616_rtc_set_time(&data->client.dev, &rtc_tm);
#endif
	} 
	else
	{
		/* RTC timer is ok, start a 24 hour discharge */
		dbgl1("Write 0x%02X in 0x%02X\n", PCF50616_BBCC_BBCC_50|PCF50616_BBCC_BBCV, PCF50616_REG_BBCC);
		reg_write(data, PCF50616_REG_BBCC, PCF50616_BBCC_BBCC_50|PCF50616_BBCC_BBCV);

		pcf50616_global->bbcc.endtime =  jiffies + PCF50616_BBCC_24H;
		data->bbcc.state = PCF50616_BBCC_DISCHARGE;
		queue_delayed_work(data->workqueue, &data->bbcc.work_timer, PCF50616_BBCC_24H);
	}
}

static void pcf50616_poweroff(void)
{
	u_int8_t occmask = __reg_read(pcf50616_global, PCF50616_REG_OOCC);
	occmask = occmask | PCF50616_OOCC_GO_STDBY;
	reg_write(pcf50616_global, PCF50616_REG_OOCC,  occmask);
}

#ifdef CONFIG_SWITCH
static ssize_t pcf50616_print_switch_headset_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%s\n", "h2w");
}

static ssize_t pcf50616_print_switch_headset_state(struct switch_dev *sdev, char *buf)
{
/*	struct fsg_dev	*fsg = container_of(accessory.swheadset, struct fsg_dev, accessory.swheadset);*/
	return sprintf(buf, "%d\n", pcf50616_global->accessory.headset_plugged  );
}
#endif

#ifdef CONFIG_I2C_NEW_PROBE
static int pcf50616_probe(struct i2c_client *new_client, const struct i2c_device_id *id)
{
#else
static int pcf50616_detect(struct i2c_adapter *adapter, int address, int kind)
{
	struct i2c_client *new_client;
#endif
	struct pcf50616_data *data;
	int err = 0;
	int irq;
    int pmu_int = 0;
    u_int8_t pmu_id = 0;


	dbgl1("entering\n");
	if (!pcf50616_pdev) {
		printk(KERN_ERR "pcf50616: driver needs a platform_device!\n");
		return -EIO;
	}

	irq = platform_get_irq(pcf50616_pdev, 0);
	if (irq < 0) {
		dev_err(&pcf50616_pdev->dev, "no irq in platform resources!\n");
		return -EIO;
	}

	/* At the moment, we only support one PCF50616 in a system */
	if (pcf50616_global) {
		dev_err(&pcf50616_pdev->dev,
			"currently only one chip supported\n");
		return -EBUSY;
	}

	if (!(data = kzalloc(sizeof(*data), GFP_KERNEL)))
		return -ENOMEM;

	mutex_init(&data->lock);
	INIT_WORK(&data->work, pcf50616_work);
#ifdef VREG
	INIT_WORK(&data->work_enable_irq,pcf50616_irq_enable);
#endif
	data->workqueue = create_workqueue("pcf50616_wq");
	data->irq = irq;
	data->working = 0;
	data->onkey_seconds = 0;

	data->pdata = pcf50616_pdev->dev.platform_data;

#ifdef CONFIG_I2C_NEW_PROBE
	i2c_set_clientdata(new_client, data);
	data->client = new_client;
#else
	new_client = &data->client;
	i2c_set_clientdata(new_client, data);
	new_client->addr = address;
	new_client->adapter = adapter;
	new_client->driver = &pcf50616_driver;
	new_client->flags = 0;
	strlcpy(new_client->name, "pnx-pmu", I2C_NAME_SIZE);
#endif
#ifdef CONFIG_SWITCH
    /* register headset switch */
	data->accessory.swheadset.name = "h2w";
	data->accessory.swheadset.print_name = pcf50616_print_switch_headset_name;
	data->accessory.swheadset.print_state = pcf50616_print_switch_headset_state;
	err = switch_dev_register(&data->accessory.swheadset);
	if (err < 0)
    {
        printk("error register headset switch class \n");
    }
#endif

	/* now we try to detect the chip */

#ifndef CONFIG_I2C_NEW_PROBE
	/* register with i2c core */
	if ((err = i2c_attach_client(new_client))) {
		dev_err(&new_client->dev,
			"error during i2c_attach_client()\n");
		goto exit_free;
	}
#endif
	pcf50616_global = data;

	populate_sysfs_group(data);

	err = sysfs_create_group(&new_client->dev.kobj, &pcf50616_attr_group);
	if (err) {
		dev_err(&new_client->dev, "error creating sysfs group\n");
#ifdef CONFIG_I2C_NEW_PROBE
		goto exit_free;
#else
		goto exit_detach;
#endif
	}

    /* USe /dev/input/event1 to send pmu events */
	data->input_dev = input_allocate_device();
	if (!data->input_dev)
		goto exit_sysfs;

	data->input_dev->name = "PNX-PMU events";
	data->input_dev->phys = "input1";
	data->input_dev->id.bustype = BUS_I2C;
	data->input_dev->dev.parent = &new_client->dev;

    /* Event EV_SW is used to forward Headset plugged detection */
	data->input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_PWR) | BIT(EV_SW);

    /* Use KEY_STOP, KEY_MUTE */
//	set_bit(KEY_STOP, data->input_dev->keybit);
	set_bit(KEY_MUTE, data->input_dev->keybit);
	set_bit(KEY_POWER2, data->input_dev->keybit);
	set_bit(KEY_BATTERY, data->input_dev->keybit);

    /* Use SW_HEADPHONE_INSERT */
	set_bit(SW_HEADPHONE_INSERT, data->input_dev->swbit);

    /* /dev/input/eventX, X is automatically incremented by the kernel */
	if (input_register_device(data->input_dev))
		goto exit_sysfs;
#ifdef VREG
	data->irq_pending=0;
	data->vregdesc=vreg_alloc(data,"PMU",0,0xff,pcf5062x_reg_nwrite,pcf5062x_reg_nread);
	if  (!data->vregdesc)
		goto exit_input;
	if (pcf50616_reg_configure(data->vregdesc)) 
		goto exit_input;
	data->irqusercallback=0;
#endif 

	/* configure PMU irq */
	/* register GPIO in input */
	gpio_request(EXTINT_TO_GPIO(irq), pcf50616_pdev->name);
	pnx_gpio_set_mode(EXTINT_TO_GPIO(irq), GPIO_MODE_MUX0);
	gpio_direction_input(EXTINT_TO_GPIO(irq));

    /* Read device ID */
    pmu_id = reg_read(data, PCF50616_REG_ID);

	/* Register PMU as RTC device to manage rtc */
	if (data->pdata->used_features & PCF506XX_FEAT_RTC) {
		data->rtc = rtc_device_register("pnx-pmu", &new_client->dev,
						&pcf50616_rtc_ops, THIS_MODULE);
		if (IS_ERR(data->rtc)) {
			err = PTR_ERR(data->rtc);
			goto exit_irq;
		}
	}

	if (data->pdata->used_features & PCF506XX_FEAT_KEYPAD_BL) {
//		data->keypadbl = backlight_device_register("keyboard-backlight",
		data->keypadbl = backlight_device_register("keypad-bl",
							    &new_client->dev,
							    data,
							    &pcf50616_kbl_ops);
		if (!data->keypadbl)
			goto exit_rtc;
		/* FIXME: are we sure we want default == off? */
		data->keypadbl->props.max_brightness = 1;
		data->keypadbl->props.power = FB_BLANK_UNBLANK;
		data->keypadbl->props.fb_blank = FB_BLANK_UNBLANK;
		data->keypadbl->props.brightness =0;
		backlight_update_status(data->keypadbl);
		pcf50616_kbl_set_intensity(data->keypadbl);
	}
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&pcf50616_kbl_earlys);
#endif

	/* Register PMU as dev device */
	major = register_chrdev(0, "pmu", &pcf50616_pmu_ops);

	if( major <0 ) {
		printk(KERN_ERR "Not able to register_chrdev: %i\n",major);
        goto exit_bl;
    };

	printk("PMU dev device Major = %u\n",major);

    /* Set PMU register in correct initial state */
    if (pcf50616_global->pcf50616_itcallback==NULL)
        pcf50616_global->pcf50616_itcallback=pcf50616_void;

	if (pcf50616_global->pcf50616_usbcallback==NULL)
	{
	    pcf50616_global->pcf50616_usbcallback=pcf50616_usb_void;
	}
    
    if (pcf50616_onkey==NULL)
       pcf50616_onkey=pcf50616_void;

	pcf50616_init_registers(data);

    pnx6708_pm_dev.dev.parent = &new_client->dev;
    platform_device_register(&pnx6708_pm_dev);
    pm_power_off=pcf50616_poweroff;
    /* initialize irq  */
	pmu_int = gpio_get_value(EXTINT_TO_GPIO(irq));

    printk(KERN_INFO "PCF50616: driver ID is:%u (0x%02X)\nPCF50616: EXTINT is at %u\n",pmu_id,pmu_id,pmu_int);

    /* Configure irq for PNX */
	err = request_irq(irq, pcf50616_irq, 0, "pnx-pmu", data);
	if (err < 0)
	{
        printk(KERN_INFO "PCF50616 : request irq failed\n");
		goto exit_input;
	}
#ifdef VREG
 	set_irq_type(irq, IRQ_TYPE_LEVEL_LOW);
#endif 

	return 0;
exit_bl:
	if (data->pdata->used_features & PCF506XX_FEAT_KEYPAD_BL)
		backlight_device_unregister(pcf50616_global->keypadbl);
exit_rtc:
	if (data->pdata->used_features & PCF506XX_FEAT_RTC)
		rtc_device_unregister(pcf50616_global->rtc);
exit_irq:
	free_irq(pcf50616_global->irq, pcf50616_global);
exit_input:
	input_unregister_device(data->input_dev);
exit_sysfs:
/* 	pm_power_off = NULL; */
	sysfs_remove_group(&new_client->dev.kobj, &pcf50616_attr_group);
#ifndef CONFIG_I2C_NEW_PROBE
exit_detach:
	i2c_detach_client(new_client);
#endif
exit_free:
	kfree(data);
	pcf50616_global = NULL;
	return err;
}

#ifdef CONFIG_I2C_NEW_PROBE
static int __devexit pcf50616_remove(struct i2c_client *client)
#else
static int pcf50616_attach_adapter(struct i2c_adapter *adapter)
{
	dbgl1("entering, calling i2c_probe\n");
	return i2c_probe(adapter, &addr_data, &pcf50616_detect);
}

static int pcf50616_detach_client(struct i2c_client *client)
#endif
{
	struct pcf50616_data *pcf = i2c_get_clientdata(client);

	dbgl1("entering\n");

/* 	apm_get_power_status = NULL; */

	free_irq(pcf->irq, pcf);

	input_unregister_device(pcf->input_dev);

#ifdef CONFIG_SWITCH
	switch_dev_unregister(&pcf->accessory.swheadset);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&pcf50616_kbl_earlys);
#endif

    unregister_chrdev(major,"pmu");

	if (pcf->pdata->used_features & PCF506XX_FEAT_RTC)
		rtc_device_unregister(pcf->rtc);

	platform_device_unregister(&pnx6708_pm_dev);

	sysfs_remove_group(&client->dev.kobj, &pcf50616_attr_group);

	pm_power_off = NULL;
	
#ifndef CONFIG_I2C_NEW_PROBE
	i2c_detach_client(client);
#endif
	kfree(pcf);

	return 0;
}

#define pcf50616_suspend NULL
#define pcf50616_resume NULL

#ifdef CONFIG_I2C_NEW_PROBE
static const struct i2c_device_id pcf50616_id[] = {
	{ "pcf50616", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pcf50616_id);

static struct i2c_driver pcf50616_driver = {
	.driver = {
		.name	= "pcf50616",
		.owner  = THIS_MODULE,
	},
	.id_table = pcf50616_id,
	.probe	  = pcf50616_probe,
	.remove	  = __devexit_p(pcf50616_remove),
	.suspend= pcf50616_suspend,
	.resume	= pcf50616_resume,
};
#else
static struct i2c_driver pcf50616_driver = {
	.driver = {
		.name	= "pnx-pmu",
		.suspend= pcf50616_suspend,
		.resume	= pcf50616_resume,
	},
	.id		= I2C_DRIVERID_PCF50616,
	.attach_adapter	= pcf50616_attach_adapter,
	.detach_client	= pcf50616_detach_client,
};
#endif
/* platform driver, since i2c devices don't have platform_data */
static int __init pcf50616_plat_probe(struct platform_device *pdev)
{
	struct pcf50616_platform_data *pdata = pdev->dev.platform_data;

	if (!pdata)
		return -ENODEV;

	pcf50616_pdev = pdev;

	return 0;
}

static int pcf50616_plat_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver pcf50616_plat_driver = {
	.probe	= pcf50616_plat_probe,
	.remove	= pcf50616_plat_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name 	= "pnx-pmu",
	},
};

static int __init pcf50616_init(void)
{
	int rc;

	if (!(rc = platform_driver_register(&pcf50616_plat_driver)))
	{
		printk("platform_driver_register OK !!!\n");
		if (!(rc = i2c_add_driver(&pcf50616_driver)))
		{
			printk("i2c_add_driver OK !!!\n");
		}
		else
		{
			printk(KERN_ERR "i2c_add_driver failed\n");
			platform_driver_unregister(&pcf50616_plat_driver);
			return 	-ENODEV;
		}
	}
	else
	{
		printk("platform_driver_register Failed !!!\n");
	}

	return rc;
}

static void __exit pcf50616_exit(void)
{
	i2c_del_driver(&pcf50616_driver);
	platform_driver_unregister(&pcf50616_plat_driver);
}

module_init(pcf50616_init);
module_exit(pcf50616_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrice Chotard");
MODULE_DESCRIPTION("PCF50616 PMU driver");

