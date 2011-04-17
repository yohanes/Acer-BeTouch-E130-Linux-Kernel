/* STE PCF50623 PMU DRIVER
*
*  Author:     Olivier Clergeaud
*  Copyright (C) 2010 ST-Ericsson
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
#include <linux/delay.h>
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
#define ACER_L1_K3
#define ACER_L1_CHANGED

#include <linux/pcf506XX.h>

#include <mach/gpio.h>

#ifdef CONFIG_SWITCH    
#include <linux/switch.h>
#endif

#include <mach/pcf50623.h>

/* ACER Jen chang, 2009/06/24, IssueKeys:AU2.B-38, Implement vibrator device driver for HAL interface function { */
#include <linux/vibrator.h>
/* } ACER Jen Chang, 2009/06/24*/
#include <linux/reboot.h> //20100201 add by HC Lai for POWER-ON in POC
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


#define HS_MIC_DETECT

#define MODULE_NAME  "pcf50623"
//Selwyn add 20100723
//#define HS_MIC_DETECT
//Selwyn add 20100723
/***********************************************************************
 * Static data / structures
 ***********************************************************************/
static unsigned long bootcause=0; /* concatenation of INT1 .. INT4 */
static unsigned long oocscause=0;
static unsigned int major=0;
//Selwyn modified for headset detect
#if defined (ACER_L1_CHANGED)
int hs_detect_wait=0;
int hs_irq=-1;
int HS_MIC=0;
int HS_FIX=0;
#ifdef HS_MIC_DETECT
int HS_STATE=-1;
int HS_BOOT_FIRST=1;
#endif
#else
int HS_MIC=0;
#endif
//~Selwyn modified


struct pcf50623_data {
#ifdef CONFIG_I2C_NEW_PROBE
	struct i2c_client * client;
#else
	struct i2c_client client;
#endif
	struct pcf50623_platform_data *pdata;
	struct backlight_device *keypadbl;
/* ACER Jen chang, 2009/07/29, IssueKeys:AU4.B-137, Creat device for controlling charger indicated led { */
	struct backlight_device *chargerbl;
/* } ACER Jen Chang, 2009/07/29 */
/* ACER Jen chang, 2009/06/24, IssueKeys:AU2.B-38, Implement vibrator device driver for HAL interface function { */
	struct vibrator_device *vibrator;
/* } ACER Jen Chang, 2009/06/24*/
	struct mutex lock;
	unsigned int flags;
	unsigned int working;
	struct work_struct work;
	//Selwyn modified for headset detect
#if defined (ACER_L1_CHANGED)
	struct delayed_work delay_work;
#endif
	//~Selwyn modified
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
		unsigned int working;	
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
    itcallback pcf50623_itcallback;
    itcallback pcf50623_usbcallback;
	struct {
		u_int8_t state;
		unsigned int fast_charge;
		unsigned int long_charge;
		unsigned int discharge;
		unsigned long endtime;
		struct delayed_work work_timer;
		unsigned int working;	
	}bbcc;
	struct gpio_bank bank;
	int gpios;
    struct workqueue_struct *workqueue;

};
static struct i2c_driver pcf50623_driver;

struct pcf50623_data *pcf50623_global;
EXPORT_SYMBOL(pcf50623_global);

static struct platform_device *pcf50623_pdev;

/* This callback function cannot be put in pcf50623_data structute because pcf50623 drivers is installed after pnx_kbd */
static itcallback pcf50623_onkey;

/***********************************************************************
 * Constant data
 ***********************************************************************/
#define PCF50623_F_CHG_MAIN_PLUGGED 0x00000001  /* Main charger present */
#define PCF50623_F_CHG_USB_PLUGGED	0x00000002  /* Charger USB present  */
#define PCF50623_F_CHG_MAIN_ENABLED 0x00000004  /* Main charge enabled  */
#define PCF50623_F_CHG_USB_ENABLED  0x00000008  /* USB charge enabled   */
#define PCF50623_F_CHG_THERM_LIMIT  0x00000010  /* Thermal limite on    */
#define PCF50623_F_CHG_BATTFULL     0x00000020  /* Battery Full         */
#define PCF50623_F_CHG_CV_REACHED	0x00000040	/* Charger swtiches from CC to CV */
#define PCF50623_F_RTC_SECOND	    0x00000080  /* RTC Second requested by RTC alarm */
#define PCF50623_F_PWR_PRESSED      0x00000100  /* ONKEY Pressed        */
#define PCF50623_ACC_INIT_TIME      2*HZ
#define PCF50623_NOACC_DETECTED     0x06
#define PCF50623_BBCC_24H           24*60*60*HZ /* 24 hours */
#define PCF50623_BBCC_6H            6*60*60*HZ  /* 6 hours  */
#define PCF50623_BBCC_1H            1*60*60*HZ  /* 1 hour   */
#define PCF50623_BBCC_LONG_CHARGE   0x01
#define PCF50623_BBCC_FAST_CHARGE   0x02
#define PCF50623_BBCC_DISCHARGE     0x03

static int reg_write(struct pcf50623_data *pcf, u_int8_t reg, u_int8_t val);

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
	struct kobject *pkobj = &(pcf50623_pdev->dev.kobj);

	dbgl1("Resistance = %lu uOhms\n",uohms);
	dbgl1("Resistance = %lu mOhms\n",mohms);
	/* Selwyn 2010/01/25 marked for hook key detect issue */
	//reg_write(pcf50623_global, PCF50623_REG_RECC1, 0x60);
	/* ~Selwyn 2010/01/25 marked */

#if !defined (ACER_L1_CHANGED)
    if ( mohms<500 ) {
        dbgl1("Tvout plugged\n");
        pcf50623_global->accessory.tvout_plugged = 1;
        kobject_uevent_env(pkobj, KOBJ_CHANGE, env);
    } else if ( mohms<3000 ) {
            dbgl1("Headset plugged\n");
            pcf50623_global->accessory.headset_plugged = 1;
		//input_event(pcf50623_global->input_dev, EV_SW, SW_HEADPHONE_INSERT, 1);
#ifdef CONFIG_SWITCH
			switch_set_state(&pcf50623_global->accessory.swheadset, pcf50623_global->accessory.headset_plugged );
#endif
	} else if ( mohms<4300000 ) {
                dbgl1("Headset button pressed\n");
	            pcf50623_global->accessory.button_pressed = 1;
                input_report_key(pcf50623_global->input_dev, KEY_MUTE, 1);
		input_sync(pcf50623_global->input_dev);
				if ( pcf50623_global->accessory.headset_plugged == 0)
				{
					/* Notify headset plugged in case of headset plugged with key pressed */
		            pcf50623_global->accessory.headset_plugged = 1;
			//input_event(pcf50623_global->input_dev, EV_SW, SW_HEADPHONE_INSERT, 1);
#ifdef CONFIG_SWITCH
					switch_set_state(&pcf50623_global->accessory.swheadset, pcf50623_global->accessory.headset_plugged );
#endif
				}

            }
#else
//Selwyn 20091001 modified
	if ( mohms<3000 )
	{
		HS_MIC = 1;
		#ifdef HS_MIC_DETECT
		if(HS_BOOT_FIRST)
		{
			HS_BOOT_FIRST = 0;
			HS_FIX=1;
			dbgl1("Selwyn debug: headset with mic plug set state - boot first\n");
			return;
		}
		else if(pcf50623_global->accessory.headset_plugged)
		{
			if(HS_STATE != pcf50623_global->accessory.headset_plugged)
			{	
			dbgl1("Selwyn debug: headset plug set state\n");
			HS_STATE = pcf50623_global->accessory.headset_plugged;	
			#ifdef CONFIG_SWITCH
			switch_set_state(&pcf50623_global->accessory.swheadset, pcf50623_global->accessory.headset_plugged);
			#endif
			}
		}
		#endif
	}
	else if( mohms<4300000 )
	{
		//Selwyn modified for headset detect
		#ifdef HS_MIC_DETECT
		if(HS_BOOT_FIRST)
		{
			HS_BOOT_FIRST = 0;
			HS_FIX=2;
			dbgl1("Selwyn debug: headset without mic plug set state - boot first\n");
			return;
		}
		#endif
		if((pcf50623_global->accessory.headset_plugged == 1) && (!hs_detect_wait))
		//~Selwyn modified
		{
			if(HS_MIC)
			{
			dbgl1("Headset button pressed\n");
			pcf50623_global->accessory.button_pressed = 1;
			input_report_key(pcf50623_global->input_dev, KEY_MUTE, 1);
			input_sync(pcf50623_global->input_dev);
			}
			#ifdef HS_MIC_DETECT
			else
			{
				if(HS_STATE != pcf50623_global->accessory.headset_plugged)
				{	
				dbgl1("Selwyn debug: headset plug set state\n");
				pcf50623_global->accessory.headset_plugged = 2;
				HS_STATE = pcf50623_global->accessory.headset_plugged;	
				#ifdef CONFIG_SWITCH
				switch_set_state(&pcf50623_global->accessory.swheadset, pcf50623_global->accessory.headset_plugged);
				#endif
				}
			}
			#endif
		}
	}

#endif
//~Selwyn modified
}

#endif
void pcf5062x_bb_read(unsigned char addr, unsigned char *dest, unsigned char len)
{
	vreg_user_read(pcf50623_global->vregdesc,addr,dest,len);		
	return;
}

void pcf5062x_bb_write(unsigned char addr, unsigned char *src, unsigned char len)
{
	vreg_user_write(pcf50623_global->vregdesc,addr,src,len);		
	return;
}
int pcf50623_reg_configure(void *desc)
{
	int ret=0;
	ret+=vreg_user_desc(desc,PCF50623_REG_ID,1,VR_READ,0,0);	
	ret+=vreg_driver_desc(desc,PCF50623_REG_ID,1,VR_READ,0,0);
	/* config modem only */
	/* config gpio */
	ret+=vreg_driver_desc(desc,PCF50623_REG_GPIO1C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);
    ret+=vreg_driver_desc(desc,PCF50623_REG_GPIO2C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);
	ret+=vreg_driver_desc(desc,PCF50623_REG_GPIO3C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);
	ret+=vreg_driver_desc(desc,PCF50623_REG_GPIO4C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);
	
	ret+=vreg_user_desc(desc,PCF50623_REG_GPIO5C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);
	ret+=vreg_user_desc(desc,PCF50623_REG_GPIO6C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);
	
	ret+=vreg_driver_desc(desc,PCF50623_REG_GPO1C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);
	ret+=vreg_driver_desc(desc,PCF50623_REG_GPO2C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);
	ret+=vreg_driver_desc(desc,PCF50623_REG_GPO3C1,1,VR_READ | VR_WRITE | VR_EXCLU,0,0);

/// +ACER_AUx PenhoYu, enable PMU GPIO4 for detect OVP_FAULT of RT9718
	ret+=vreg_driver_desc(desc,PCF50623_REG_GPIOS,1,VR_READ | VR_EXCLU,0,0);
/// -ACER_AUx PenhoYu, enable PMU GPIO4 for detect OVP_FAULT of RT9718

	/* config for SIM register */
	ret+=vreg_user_desc(desc,PCF50623_REG_USIMREGC1,1, VR_READ | VR_WRITE | VR_EXCLU,0,0);
	ret+=vreg_user_desc(desc,PCF50623_REG_USIMDETC,1, VR_READ | VR_WRITE | VR_EXCLU,0,0);
	/* config for regu modem */
	ret+=vreg_user_desc(desc,PCF50623_REG_DCD1C1,PCF50623_REG_DCD2C1-PCF50623_REG_DCD1C1,VR_READ | VR_WRITE | VR_EXCLU, 0, 0);
	ret+=vreg_driver_desc(desc,PCF50623_REG_DCD2C1,PCF50623_REG_DCD3C4-PCF50623_REG_DCD2C1+1,VR_READ | VR_WRITE | VR_EXCLU, 0, 0);

	ret+=vreg_driver_desc(desc,PCF50623_REG_D1REGC1,PCF50623_REG_D7REGC3-PCF50623_REG_D1REGC1+1,VR_READ | VR_WRITE | VR_EXCLU, 0, 0);
	ret+=vreg_driver_desc(desc,PCF50623_REG_RF1REGC1,PCF50623_REG_RF4REGC3-PCF50623_REG_RF1REGC1+1,VR_READ | VR_WRITE | VR_EXCLU, 0,0);
	/* interruption config  */
	{
	unsigned char pol[4]={0xff,0xff,0xff,0xff};
	unsigned char mask[4]={0xf1,0xff,0xff,0xff};
	ret+=vreg_user_desc(desc,PCF50623_REG_INT1,4, VR_READ | VR_CLEAR | VR_MASK,mask,0);
	ret+=vreg_user_desc(desc,PCF50623_REG_INT1M,4, VR_WRITE | VR_READ | VR_POLARITY | VR_MASK ,mask,pol);
	ret+=vreg_driver_desc(desc,PCF50623_REG_INT1,4, VR_READ | VR_CLEAR ,0,0);
	ret+=vreg_driver_desc(desc,PCF50623_REG_INT1M,4, VR_WRITE | VR_READ | VR_POLARITY ,0,pol);
	ret+=vreg_driver_desc(desc,PCF50623_REG_HCREGC1,3, VR_READ | VR_WRITE | VR_EXCLU,0,0);
	/* map IOREG / USB REG */
	ret+=vreg_driver_desc(desc,PCF50623_REG_IOREGC1,5, VR_READ | VR_WRITE | VR_EXCLU,0,0);
	}
	/* config specific for RECC */
	{
#ifndef RTK_TEST
		unsigned char pol[1]={1<<6};
		unsigned char mask[1]={1<<6};
		unsigned char mask1[1]={0xff};
		ret+=vreg_user_desc(desc, PCF50623_REG_RECC1,
				1,VR_POLARITY | VR_MASK | VR_WRITE | VR_READ ,
				mask,pol);

		ret+=vreg_driver_desc(desc, PCF50623_REG_RECC1,
				1,VR_POLARITY | VR_MASK | VR_WRITE | VR_READ ,
				mask1,pol);
		ret+=vreg_driver_desc(desc,PCF50623_REG_RECC2,2,VR_WRITE | VR_READ ,0,0);

#else
	ret+=vreg_user_desc(desc, PCF50623_REG_RECC1,
				1, VR_WRITE | VR_READ ,
				0,0);
    ret+=vreg_user_desc(desc,PCF50623_REG_RECC2,2,VR_WRITE | VR_READ ,0,0);
#endif
	}
	/* fix register used by driver only */
    /* OOC, BVMC */
	ret+=vreg_driver_desc(desc,PCF50623_REG_OOCC1,5,VR_WRITE | VR_READ | VR_EXCLU,0,0);    
	/* RTC */
	ret+=vreg_driver_desc(desc,PCF50623_REG_RTC1,8,VR_WRITE | VR_READ ,0,0);
	ret+=vreg_user_desc(desc,PCF50623_REG_RTC1,8, VR_READ ,0,0);
	/* CBC BBC ADC PWMS TSI */
#ifndef RTK_TEST	
	ret+=vreg_driver_desc(desc,PCF50623_REG_CBCC1,	PCF50623_REG_LEDCC	-PCF50623_REG_CBCC1+1
			,VR_WRITE | VR_READ | VR_EXCLU,0,0); 
#else
	ret+=vreg_user_desc(desc,PCF50623_REG_CBCC1,	PCF50623_REG_LEDCC-PCF50623_REG_CBCC1+1
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
	if (pcf50623_global->irq_pending==0)
	{
		pcf50623_global->irqusercallback=callback;
	}
	else
	{
	pcf50623_global->irq_pending=0;
	pcf50623_global->irqusercallback=0;
	callback();
	}
}
static void pcf50623_irq_enable(struct work_struct *work_enable_irq)
{
	struct pcf50623_data *pcf =
			container_of(work_enable_irq, struct pcf50623_data, work_enable_irq);
	enable_irq(pcf->irq);
}

void pcf50623_user_irq_handler(struct pcf50623_data *pcf)
{
	if (vreg_user_pending(pcf->vregdesc,PCF50623_REG_INT1,4))
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
	struct pcf50623_data *pcf=p;

	/* Wait MICBIAS settle time when switch to sampled mode */
	if ((reg == PCF50623_REG_RECC1) && (data[0] & 0x40))
		mdelay(10);

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
	struct pcf50623_data *pcf=p;
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

static inline int __reg_nwrite(struct pcf50623_data *pcf, u_int8_t reg,  u_int8_t *val, unsigned long size )
{
#ifndef VREG
	return i2c_smbus_write_block_data(&pcf->client, reg,(u8) size, val);
#else
	return vreg_driver_write(pcf->vregdesc,reg,val,size );
#endif
}

static inline int __reg_write(struct pcf50623_data *pcf, u_int8_t reg, u_int8_t val)
{
#ifndef VREG
	return i2c_smbus_write_byte_data(&pcf->client, reg, val);
#else
	return vreg_driver_write(pcf->vregdesc,reg,&val,1);
#endif
}

static int reg_write(struct pcf50623_data *pcf, u_int8_t reg, u_int8_t val)
{
	int ret;

	mutex_lock(&pcf->lock);
	ret = __reg_write(pcf, reg, val);
	mutex_unlock(&pcf->lock);

	return ret;
}

#ifdef VREG 
static inline int32_t __reg_rawread(struct pcf50623_data *pcf, u_int8_t reg)
{
	int32_t ret;
#ifdef CONFIG_I2C_NEW_PROBE
	ret=i2c_smbus_read_byte_data(pcf->client, reg);
#else
	ret=i2c_smbus_read_byte_data(&pcf->client, reg);
#endif
	return ret;
}
static u_int8_t reg_rawread(struct pcf50623_data *pcf, u_int8_t reg)
{
	int32_t ret;
	mutex_lock(&pcf->lock);
	ret = __reg_rawread(pcf, reg);
	mutex_unlock(&pcf->lock);
	return ret & 0xff;
}
static inline int32_t __reg_rawwrite(struct pcf50623_data *pcf, u_int8_t reg, u_int8_t val)
{
	int32_t ret;

	/* Wait MICBIAS settle time when switch to sampled mode */
	if ((reg == PCF50623_REG_RECC1) && (val & 0x40))
		mdelay(10);

#ifdef CONFIG_I2C_NEW_PROBE
	ret= i2c_smbus_write_byte_data(pcf->client, reg, val);
#else
	ret= i2c_smbus_write_byte_data(&pcf->client, reg, val);
#endif
	return ret;
}
static int32_t  reg_rawwrite(struct pcf50623_data *pcf, u_int8_t reg, u_int8_t val)
{
	int32_t ret;
	mutex_lock(&pcf->lock);
	ret = __reg_rawwrite(pcf, reg, val);
	mutex_unlock(&pcf->lock);
	return ret;
}
#endif 
static inline int32_t __reg_read(struct pcf50623_data *pcf, u_int8_t reg)
{
	int32_t ret;
#ifndef VREG
	ret=i2c_smbus_read_byte_data(&pcf->client, reg);
#else
	vreg_driver_read(pcf->vregdesc,reg,&ret,1);
#endif
	return ret;
}

static u_int8_t reg_read(struct pcf50623_data *pcf, u_int8_t reg)
{
	int32_t ret;

	mutex_lock(&pcf->lock);
	ret = __reg_read(pcf, reg);
	mutex_unlock(&pcf->lock);

	return ret & 0xff;
}


static int reg_set_bit_mask(struct pcf50623_data *pcf, u_int8_t reg, u_int8_t mask, u_int8_t val)
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

static int reg_clear_bits(struct pcf50623_data *pcf, u_int8_t reg, u_int8_t val)
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
	struct pcf50623_data *pcf = i2c_get_clientdata(to_i2c_client(dev));

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
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
	unsigned long reg = simple_strtoul(buf, NULL, 0);

	if (reg > 0xff)
		return -EINVAL;
    pcf->read_reg = reg_rawread(pcf, reg);

    return count;
}
static ssize_t set_rawwrite(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
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
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
	unsigned long reg = simple_strtoul(buf, NULL, 0);

	if (reg > 0xff)
		return -EINVAL;

    pcf->read_reg = reg_read(pcf, reg);

    return count;
}

static ssize_t show_write(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pcf50623_data *pcf = i2c_get_clientdata(to_i2c_client(dev));
	return sprintf(buf, "Reg=%u (0x%02X), Val=%u (0x%02X)\n", pcf->write_reg, pcf->write_reg, pcf->write_val, pcf->write_val);
}

static ssize_t set_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
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

static u_int8_t pcf50623_dxreg_voltage(unsigned int millivolts)
{
    /* Cf PCF50623 User Manual p106 for details */
	if (millivolts < 1200)
		return 0;
	else if (millivolts > 3300)
		return 0x1B;

	millivolts -= 600;
	return (u_int8_t)((millivolts/100)<<2);
}

static unsigned int pcf50623_dxreg_2voltage(u_int8_t bits)
{
	bits = (bits & 0x7C)>>2;
	return 600 + (bits * 100);
}

/* manage only register not handled by RTK */
static const u_int8_t regulator_registers[__NUM_PCF506XX_REGULATORS] = {
	[PCF506XX_REGULATOR_D1REG]	= PCF50623_REG_D1REGC1,
	[PCF506XX_REGULATOR_D3REG]	= PCF50623_REG_D3REGC1,
	[PCF506XX_REGULATOR_D4REG]	= PCF50623_REG_D4REGC1,
	[PCF506XX_REGULATOR_D5REG]	= PCF50623_REG_D5REGC1,
	[PCF506XX_REGULATOR_D6REG]	= PCF50623_REG_D6REGC1,
	[PCF506XX_REGULATOR_D7REG]	= PCF50623_REG_D7REGC1,
	[PCF506XX_REGULATOR_IOREG]	= PCF50623_REG_IOREGC1,
	[PCF506XX_REGULATOR_USBREG]	= PCF50623_REG_USBREGC1,
	[PCF506XX_REGULATOR_HCREG]	= PCF50623_REG_HCREGC1,
};

static const  enum pcf50623_regulator_enable regulator_mode[__NUM_PCF506XX_REGULATORS_MODE]= {
	[PCF506XX_REGU_ECO] = PCF50623_REGULATOR_ECO,
	[PCF506XX_REGU_OFF] = PCF50623_REGULATOR_OFF,
	[PCF506XX_REGU_ON] = PCF50623_REGULATOR_ON,
	[PCF506XX_REGU_ON_OFF] = PCF50623_REGULATOR_ON_OFF,
	[PCF506XX_REGU_OFF_ON] = PCF50623_REGULATOR_OFF_ON,
	[PCF506XX_REGU_ECO_OFF] = PCF50623_REGULATOR_ECO_OFF,
	[PCF506XX_REGU_ON_ECO] = PCF50623_REGULATOR_ON_ECO,
	[PCF506XX_REGU_ECO_ON] = PCF50623_REGULATOR_ECO_ON
};


/* pcf506XX_onoff_set input parameters:
 *
 * pcf : pointer on pcf50623_data structure
 * reg : ID of regulator to manage, defined in include/linux/pcf50623.h
 * mode: mode to set regulator: Off, Eco, On, cf include/linux/pcf50623.h
 * 
 */

int pcf506XX_onoff_set(struct pcf50623_data *pcf,
		       enum pcf506XX_regulator_id reg, enum pcf506XX_regulator_mode mode)
{
	u_int8_t addr;

	if ((reg >= __NUM_PCF506XX_REGULATORS) ||
		(mode >=__NUM_PCF506XX_REGULATORS_MODE) ||
        (!pcf->pdata->rails[reg].used))
		return -EINVAL;

	/* the *REGC2 Register is always one after the *REGC1 register excpet for USB */
	if ( reg==PCF506XX_REGULATOR_USBREG) {
		addr = regulator_registers[reg];
		switch (mode) 
		{
		case PCF506XX_REGU_OFF :
			reg_clear_bits(pcf, addr, PCF50623_REGU_USB_ON|PCF50623_REGU_USB_ECO);
			break;
		case PCF506XX_REGU_ECO :
			reg_set_bit_mask(pcf, addr, PCF50623_REGU_USB_ECO, PCF50623_REGU_USB_ECO);
			break;
		case PCF506XX_REGU_ON:
			reg_set_bit_mask(pcf, addr, PCF50623_REGU_USB_ON|PCF50623_REGU_USB_ECO, PCF50623_REGU_USB_ON);

			break;
		/* all other command are not supported for this regu */
		default: return -EINVAL;
		}
	} 
	else {
		addr = regulator_registers[reg] + 1;
		switch (mode) 
		{
		case PCF506XX_REGU_OFF :
           reg_clear_bits(pcf, addr, PCF50623_REGULATOR_ON | PCF50623_REGULATOR_ECO);
          break;
		case PCF506XX_REGU_ECO : 
		   reg_set_bit_mask(pcf, addr, PCF50623_REGULATOR_ON_MASK, PCF50623_REGULATOR_ECO);
		   break;
		case  PCF506XX_REGU_ON :
           reg_set_bit_mask(pcf, addr, PCF50623_REGULATOR_ON_MASK, PCF50623_REGULATOR_ON);
           break;
         default : 
            if (!pcf->pdata->rails[reg].gpiopwn) return -EINVAL;
		 reg_set_bit_mask(pcf, addr, PCF50623_REGULATOR_ON_MASK, regulator_mode[mode]);
		}
	}
	return 0;
}
EXPORT_SYMBOL(pcf506XX_onoff_set);

int pcf506XX_onoff_get(struct pcf50623_data *pcf, enum pcf506XX_regulator_id reg)
{
	u_int8_t val, addr;

	if ((reg >= __NUM_PCF506XX_REGULATORS) || (!pcf->pdata->rails[reg].used))
		return -EINVAL;

	/* the *REGC2 register is always one after the *REGC1 register except for USB*/
	if ( reg == PCF506XX_REGULATOR_USBREG) {
		addr = regulator_registers[reg];
		val = (reg_read(pcf, addr) & (PCF50623_REGU_USB_ON_MASK | PCF50623_REGU_USB_ECO_MASK));
	} 
	else {
		addr = regulator_registers[reg] + 1;
		val = reg_read(pcf, addr) & PCF50623_REGULATOR_ON_MASK;
	}
	return val;
}
EXPORT_SYMBOL(pcf506XX_onoff_get);

int pcf506XX_voltage_set(struct pcf50623_data *pcf,
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
		volt_bits = pcf50623_dxreg_voltage(millivolts);
		dbgl1("pcf50623_dxreg_voltage(%umV)=0x%02X\n", millivolts, volt_bits);
		break;
	case PCF506XX_REGULATOR_USBREG:
		volt_bits = pcf50623_dxreg_voltage(millivolts) | PCF50623_REGU_USB_ON_MASK;
		dbgl1("pcf50623_dxreg_voltage(%umV)=0x%02X\n", millivolts, volt_bits);
		break;
	case PCF506XX_REGULATOR_HCREG:
        /* Special case for HCREG that supports only few values */
        if ( millivolts < 2600 ) {
            millivolts = 1800;
			volt_bits = PCF50623_HCREG_1V8;
        } else
            if ( millivolts < 3000 ) {
                millivolts=2600;
				volt_bits = PCF50623_HCREG_2V6;
            } else
                if ( millivolts < 3200) {
                    millivolts = 3000;
					volt_bits = PCF50623_HCREG_3V0;
                } else {
                    millivolts = 3200;
					volt_bits = PCF50623_HCREG_3V2;
				}
		dbgl1("pcf50623_dxreg_voltage(%umV)=0x%02X\n", millivolts, volt_bits);
		break;
	default:
		return -EINVAL;
	}

	return reg_write(pcf, regnr, volt_bits);
}
EXPORT_SYMBOL(pcf506XX_voltage_set);

unsigned int pcf506XX_voltage_get(struct pcf50623_data *pcf,
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
	case PCF506XX_REGULATOR_USBREG:
		rc = pcf50623_dxreg_2voltage(volt_bits);
		break;
	case PCF506XX_REGULATOR_HCREG:
        if ( volt_bits == PCF50623_HCREG_1V8 ) {
            rc= 1800;
        } else
            if ( volt_bits == PCF50623_HCREG_2V6 ) {
                rc=2600;
            } else
                if ( volt_bits == PCF50623_HCREG_3V0 ) {
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
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
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
			case PCF50623_REGU_USB_ON_MASK: 
					return sprintf(buf, "\033[1;34m%s is \033[2;32mON\033[0m\n",attr->attr.name);
					break;
			case (PCF50623_REGU_USB_ON_MASK | PCF50623_REGU_USB_ECO_MASK): 
					return sprintf(buf, "\033[1;34m%s is \033[2;33mECO\033[0m\n",attr->attr.name);
					break;
			default : return 0;
		}
	}
	else
	{
		switch (val&PCF50623_REGULATOR_ON_MASK) {
		case 0: 
			return sprintf(buf, "\033[1;34m%s is \033[2;31mOFF\033[0m\n",attr->attr.name);
			break;
		case PCF50623_REGULATOR_ECO:
			return sprintf(buf, "\033[1;34m%s is \033[2;33mECO\033[0m\n",attr->attr.name);
			break;
		case PCF50623_REGULATOR_ON:
			return sprintf(buf, "\033[1;34m%s is \033[2;32mON\033[0m\n",attr->attr.name);
			break;
		default : return sprintf(buf, "\033[1;34m%s is \033[2;33m%x\033[0m\n",attr->attr.name, (val&PCF50623_REGULATOR_ON_MASK));
		}
	}
}

static ssize_t set_onoff(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
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
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
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
			case PCF50623_REGU_USB_ON_MASK: 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;31m\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			case (PCF50623_REGU_USB_ON_MASK | PCF50623_REGU_USB_ECO_MASK): 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;33m(but regulator is ECO)\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			default : return 0;
		}
	}
	else
	{
		switch(val&PCF50623_REGULATOR_ON_MASK){
			case 0: 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;31m(but regulator is OFF)\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			case PCF50623_REGULATOR_ECO: 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;33m(but regulator is ECO)\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			case PCF50623_REGULATOR_ON: 
					return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;31m\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id));
					break;
			default : return sprintf(buf, "\033[1;34m%-14s = \033[1;32m%umV \033[1;33m(but regulator is %x)\033[0m\n", attr->attr.name, pcf506XX_voltage_get(pcf, reg_id), (val&PCF50623_REGULATOR_ON_MASK));
		}
	}
}

static ssize_t set_vreg(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
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
	if (!(pcf50623_global->pdata->used_features & PCF506XX_FEAT_CBC))
		return;

	if (on) {
        reg_set_bit_mask(pcf50623_global, PCF50623_REG_CBCC1, PCF50623_CBCC1_CHGENA, PCF50623_CBCC1_CHGENA);
		pcf50623_global->flags |= PCF50623_F_CHG_MAIN_ENABLED;
	} else {
        reg_clear_bits(pcf50623_global, PCF50623_REG_CBCC1, PCF50623_CBCC1_CHGENA);
	    pcf50623_global->flags &= ~PCF50623_F_CHG_CV_REACHED;
	    pcf50623_global->flags &= ~PCF50623_F_CHG_BATTFULL;
		pcf50623_global->flags &= ~PCF50623_F_CHG_MAIN_ENABLED;
	}
}
EXPORT_SYMBOL(pcf506XX_main_charge_enable);

void pcf506XX_SetUsbSuspend(int val)
{
	/* check if USB charge is enable */
	if (pcf50623_global->flags & PCF50623_F_CHG_USB_ENABLED)
	{
		/* set SCUSB pin relative to USB suspend state */
		dbgl1("%s\n", val ? "USB enters in SUSPEND, disable USB charge" : "USB resume from SUSPEND, enable USB charge");
		gpio_set_value(pcf50623_global->pdata->usb_suspend_gpio,val);
	}
}


/* Enable/disable charging via USB charger */
void pcf506XX_usb_charge_enable(int on)
{
	if (!(pcf50623_global->pdata->used_features & PCF506XX_FEAT_CBC))
		return;

	if (on) {

		reg_set_bit_mask(pcf50623_global, PCF50623_REG_CBCC1, PCF50623_CBCC1_USBENA
				, PCF50623_CBCC1_USBENA);
		reg_set_bit_mask(pcf50623_global, PCF50623_REG_CBCC2, PCF50623_CBCC2_SUSPENA
				, PCF50623_CBCC2_SUSPENA);
		
		pcf50623_global->flags |= PCF50623_F_CHG_USB_ENABLED;
	} else {
        reg_clear_bits(pcf50623_global, PCF50623_REG_CBCC1, PCF50623_CBCC1_USBENA);
	    pcf50623_global->flags &= ~PCF50623_F_CHG_CV_REACHED;
	    pcf50623_global->flags &= ~PCF50623_F_CHG_BATTFULL;
		pcf50623_global->flags &= ~PCF50623_F_CHG_USB_ENABLED;
	}
}
EXPORT_SYMBOL(pcf506XX_usb_charge_enable);


static ssize_t show_chgmode(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
	char *b = buf;
	u_int8_t cbcc1 = reg_read(pcf, PCF50623_REG_CBCC1);
	u_int8_t cbcc2 = reg_read(pcf, PCF50623_REG_CBCC2);
	u_int8_t cbcc3 = reg_read(pcf, PCF50623_REG_CBCC3);
	u_int8_t cbcc4 = reg_read(pcf, PCF50623_REG_CBCC4);
	u_int8_t cbcc5 = reg_read(pcf, PCF50623_REG_CBCC5);
	u_int8_t cbcc6 = reg_read(pcf, PCF50623_REG_CBCC6);
	u_int8_t cbcs1 = reg_read(pcf, PCF50623_REG_CBCS1);
	u_int8_t cbcs2 = reg_read(pcf, PCF50623_REG_CBCS2);

    const char * const  autores[4]={"OFF","ON, Vres=Vmax-3%","ON, Vres=Vmax-6%","ON, Vres=Vmax-4.5%"};

    b += sprintf(b, "\033[1;34mBattery Charger Configuration:\033[0m\n");
/// +ACER_AUx PenhoYu, show PMU charge register
    b += sprintf(b, "CBCC1 : %02x, CBCC2 : %02x, CBCC3 : %02x\nCBCC4 : %02x, CBCC5 : %02x, CBCC6 : %02x\nCBCS1 : %02x, CBCS2 : %02x\n",
                 cbcc1, cbcc2, cbcc3, cbcc4, cbcc5, cbcc6, cbcs1, cbcs2);
/// -ACER_AUx PenhoYu, show PMU charge register
    if (cbcc1 & PCF50623_CBCC1_CHGENA)
        b += sprintf(b, "Charging via main charger    : \033[2;32menabled\033[0m\n");
    else
        b += sprintf(b, "Charging via main charger    : \033[2;31mdisabled\033[0m\n");
    if (cbcc1 & PCF50623_CBCC1_USBENA)
        b += sprintf(b, "Charging via USB charger     : \033[2;32menabled\033[0m\n");
    else
        b += sprintf(b, "Charging via USB charger     : \033[2;31mdisabled\033[0m\n");
    if (cbcc1 & PCF50623_CBCC1_AUTOCC)
        b += sprintf(b, "Fast charge                  : \033[2;32menabled\033[0m\n");
    else
        b += sprintf(b, "Fast charge                  : \033[2;31mdisabled\033[0m\n");
    if (cbcc1 & PCF50623_CBCC1_AUTOSTOP) {
        b += sprintf(b, "Auto stop charge             : \033[2;32menabled\033[0m\n");
        if (cbcc1 & PCF50623_CBCC1_AUTORES_MASK)
            b += sprintf(b, "Automatic charge resume      : \033[2;33m%s\033[0m\n",autores[((cbcc1&PCF50623_CBCC1_AUTORES_MASK)&PCF50623_CBCC1_AUTORES_MASK)>>4]);
    }
    else
        b += sprintf(b, "Auto stop charge             : \033[2;31mdisabled\033[0m\n");
    if (cbcc1 & PCF50623_CBCC1_WDTIME_5H)
        b += sprintf(b, "Charge time                  : \033[2;31mlimited to 5 hours\033[0m\n");
    else
        b += sprintf(b, "Charge time                  : \033[2;32mnot limited\033[0m\n");

    if (cbcc2 & PCF50623_CBCC2_OVPENA)
        b += sprintf(b, "Over volt. prot. Main Charger: \033[2;32menabled\033[0m\n");
    else
        b += sprintf(b, "Over volt. prot. Main Charger: \033[2;31mdisabled\033[0m\n");
    if (cbcc2 & PCF50623_CBCC2_SUSPENA)
        b += sprintf(b, "Over volt. prot. on USB input: \033[2;32menabled\033[0m\n");
    else
        b += sprintf(b, "Over volt. prot. on USB input: \033[2;31mdisabled\033[0m\n");

    b += sprintf(b, "Vmax level setting           = \033[2;33m4.%02uV\033[0m\n",2*(((cbcc2&PCF50623_CBCC2_VBATMAX_MASK)>>3)-1));

    b += sprintf(b, "Precharge cur. main charger  = \033[2;33m%umA\033[0m\n",cbcc3*1136/255);

    b += sprintf(b, "Precharge cur. USB charger   = \033[2;33m%umA\033[0m\n",cbcc4*1136/255);
    b += sprintf(b, "Trickle charge current       = \033[2;33m%umA\033[0m\n",((cbcc5&0x7F)+1)*1136/128);

    if (cbcc6 & 0x02)
        b += sprintf(b, "Charge current limited by    : \033[2;32mexternal charger\033[0m\n");
    else
        b += sprintf(b, "Charge current limited by    : \033[2;31mPMU\033[0m\n");

    b += sprintf(b, "\033[1;34m\nBattery Charger Status:\033[0m\n");

    if (cbcs1 & PCF50623_CBCS1_BATFUL)
        b += sprintf(b, "\033[32mBattery Full\033[0m\n");
    if (cbcs1 & PCF50623_CBCS1_TLIM)
        b += sprintf(b, "\033[31mThermal Limiting Active\033[0m\n");
    if (cbcs1 & PCF50623_CBCS1_WDEXP)
        b += sprintf(b, "\033[31mCBC Watchdog Timer expired\033[0m\n");
    if (cbcs1 & PCF50623_CBCS1_ILIMIT)
        b += sprintf(b, "\033[32mCharge current lower than trickle current\033[0m\n");
    else
        b += sprintf(b, "\033[31mCharge current higher than trickle current\033[0m\n");
    if (cbcs1 & PCF50623_CBCS1_VLIMIT)
        b += sprintf(b, "\033[32mVoltage higher than Vmax\033[0m\n");
    else
        b += sprintf(b, "\033[31mVoltage lower than Vmax\033[0m\n");
    if (cbcs1 & PCF50623_CBCS1_RESSTAT)
        b += sprintf(b, "\033[32mCharging has been resumed\033[0m\n");

    if (cbcs2 & PCF50623_CBCS2_USBSUSPSTAT)
        b += sprintf(b, "\033[31mUSB mode suspend\033[0m\n");
    if (cbcs2 & PCF50623_CBCS2_CHGOVP)
        b += sprintf(b, "\033[31mCharger in over voltage protection\033[0m\n");


	return b-buf;
}

static ssize_t set_chgmode(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
/* 	struct i2c_client *client = to_i2c_client(dev); */
/* 	struct pcf50623_data *pcf = i2c_get_clientdata(client); */

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
	struct pcf50623_data *pcf = i2c_get_clientdata(client);

	char *b = buf;
	int i;

    b += sprintf(b, "\033[1;34mBattery Charger State:\033[0m\n");

	/* Display messages up to Battery Full */
	for (i = 0; i <  ARRAY_SIZE(chgstate_names)-2; i++)
		if (pcf->flags & (1 << i) )
			b += sprintf(b, "\033[1;32m%s\033[0m\n", chgstate_names[i]);

	/* Special management for CC/CV mode */
	if ( (pcf->flags & PCF50623_F_CHG_MAIN_PLUGGED) || (pcf->flags & PCF50623_F_CHG_USB_PLUGGED) )
	{
		if ( pcf->flags & PCF50623_F_CHG_CV_REACHED )
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
	struct pcf50623_data *pcf = i2c_get_clientdata(client);

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
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
	unsigned long timeleft = (pcf->bbcc.endtime - jiffies)/HZ;
	int time_h, time_m, time_s;
	char *b = buf;
	u_int8_t bbcc1 = reg_read(pcf, PCF50623_REG_BBCC1);

	const char * const charge_mode[4] = {"UNKNOWN", "long ", "fast ", "long dis"};
	const char * const charge_cur[4] =  {"50uA", "100uA", "200uA", "400uA"};

	time_h = timeleft/3600;
	time_m = (timeleft-(time_h*3600))/60;
    time_s = (timeleft-(time_h*3600))%60;

	b += sprintf(b, "\033[1;34mBackup Battery Configuration:\n");
	b += sprintf(b, "Backup Battery Charge is \033[1;31m%s, ",bbcc1&PCF50623_BBCC1_ENABLE?"Enabled":"disabled");
	b += sprintf(b, "\033[1;34m(\033[1;31m%s\033[1;34m, \033[1;31m%s\033[1;34m)\n", bbcc1&PCF50623_BBCC1_3V0?"3V0":"2V5",charge_cur[(bbcc1&PCF50623_BBCC1_CURRENT_MASK)>>2]);

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
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
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
	case PCF50623_BBCC_LONG_CHARGE:
		pcf->bbcc.long_charge = timing*HZ;
		pcf50623_global->bbcc.endtime = jiffies + pcf50623_global->bbcc.discharge;
		schedule_delayed_work(&pcf50623_global->bbcc.work_timer,  pcf50623_global->bbcc.discharge);
		break;
	case PCF50623_BBCC_FAST_CHARGE:
		pcf->bbcc.fast_charge = timing*HZ;
		pcf50623_global->bbcc.endtime = jiffies + pcf50623_global->bbcc.discharge;
		schedule_delayed_work(&pcf50623_global->bbcc.work_timer,  pcf50623_global->bbcc.discharge);
		break;
	case PCF50623_BBCC_DISCHARGE:
		pcf->bbcc.discharge = timing*HZ;
		pcf50623_global->bbcc.endtime = jiffies + pcf50623_global->bbcc.fast_charge;
		schedule_delayed_work(&pcf50623_global->bbcc.work_timer,  pcf50623_global->bbcc.fast_charge);
		break;
	};

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

static struct attribute *pcf50623_pcf_sysfs_entries[32] = {
	&dev_attr_read.attr,
	&dev_attr_write.attr,
	&dev_attr_regu_d1_onoff.attr,
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

static const struct attribute_group pcf50623_attr_group = {
	.name	= NULL,			/* put in device directory */
	.attrs = pcf50623_pcf_sysfs_entries,
};

static void populate_sysfs_group(struct pcf50623_data *pcf)
{
	int i = 0;
	struct attribute **attr;

	for (attr = pcf50623_pcf_sysfs_entries; *attr; attr++)
		i++;

#if 0
	if (pcf->pdata->used_features & PCF506XX_FEAT_CBC) {
		pcf50623_pcf_sysfs_entries[i++] = &dev_attr_usb_curlim.attr;
	}

	if (pcf->pdata->used_features & PCF506XX_FEAT_CHGCUR)
		pcf50623_pcf_sysfs_entries[i++] = &dev_attr_chgcur.attr;
#endif
	if (pcf->pdata->used_features & PCF506XX_FEAT_CBC) {
		pcf50623_pcf_sysfs_entries[i++] = &dev_attr_chgstate.attr;
		pcf50623_pcf_sysfs_entries[i++] = &dev_attr_chgmode.attr;
	}
}

/***********************************************************************
 * Charge Management, cf spec VYn_ps18857
 ***********************************************************************/
static void pcf50623_void(u_int16_t x)
{
    dbgl1("WARNING !!! Function called before drvchr init !!!\n");   
};

static void pcf50623_usb_void(u_int16_t x)
{
    dbgl1("WARNING !!! Function called before usb driver init !!!\n");   
};

/* ACER Jen chang, 2009/06/17, IssueKeys:AU4.B-43, Modify sd card detecting for different compiler option { */
/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify sd card detecting to fix pmu ic bug on PCR { */
#if (defined ACER_AU2_PR1) || (defined ACER_AU2_PR2) || (defined ACER_AU4_PR1) || (defined ACER_AU4_PR2) 
static void pcf506XX_sddetect_void(u_int16_t x)
{
    dbgl1("WARNING !!! Function called before pnx_mmc driver init !!!\n");   
};

#endif
/* } ACER Jen Chang, 2009/09/02*/
/* } ACER Jen Chang, 2009/06/17*/

void pcf506XX_setAutoStop(u_int8_t on)
{
    if (on)
    {
        reg_set_bit_mask(pcf50623_global, PCF50623_REG_CBCC1, PCF50623_CBCC1_AUTOSTOP, PCF50623_CBCC1_AUTOSTOP);
/// +ACER_AUx PenhoYu, IssueKeys:AU2.FC-72
        //reg_set_bit_mask(pcf50623_global, PCF50623_REG_CBCC1, PCF50623_CBCC1_AUTORES_MASK, PCF50623_CBCC1_AUTORES_ON45);
        reg_set_bit_mask(pcf50623_global, PCF50623_REG_CBCC1, PCF50623_CBCC1_AUTORES_MASK, PCF50623_CBCC1_AUTORES_ON30);
/// -ACER_AUx PenhoYu, IssueKeys:AU2.FC-72
    } else {
        reg_clear_bits(pcf50623_global, PCF50623_REG_CBCC1, PCF50623_CBCC1_AUTOSTOP);
    }
}

void pcf506XX_setVmax(u_int8_t vmax)
{
    reg_set_bit_mask(pcf50623_global, PCF50623_REG_CBCC2, PCF50623_CBCC2_VBATMAX_MASK, vmax);
}

void pcf506XX_setMainChgCur(u_int8_t cur)
{
    reg_write(pcf50623_global, PCF50623_REG_CBCC3, cur);
}

void pcf506XX_setUsbChgCur(u_int8_t cur)
{
    reg_write(pcf50623_global, PCF50623_REG_CBCC4, cur);
}

void pcf506XX_setTrickleChgCur(u_int8_t cur)
{
    reg_write(pcf50623_global, PCF50623_REG_CBCC5, cur);
}

u_int8_t pcf506XX_getIlimit(void)
{
    u_int8_t value;

    value = reg_read(pcf50623_global,PCF50623_REG_CBCS1);
    return (value & PCF50623_CBCS1_ILIMIT);
}

u_int8_t pcf506XX_mainChgPlugged(void)
{
    return (pcf50623_global->flags & PCF50623_F_CHG_MAIN_PLUGGED);
}

u_int8_t pcf506XX_usbChgPlugged(void)
{
    return (pcf50623_global->flags & PCF50623_F_CHG_USB_PLUGGED);
}

u_int8_t pcf506XX_CVmodeReached(void)
{
    return (pcf50623_global->flags & PCF50623_F_CHG_CV_REACHED);
}

void pcf506XX_disableBattfullInt(void)
{
    reg_set_bit_mask(pcf50623_global, PCF50623_REG_INT3M, PCF50623_INT3_BATFUL, PCF50623_INT3_BATFUL);    
}

void pcf506XX_enableBattfullInt(void)
{
    reg_clear_bits(pcf50623_global, PCF50623_REG_INT3M, PCF50623_INT3_BATFUL);
}

/* void pcf50623_disableMainChg(void)
 * {
 *     reg_clear_bits(pcf50623_global, PCF50623_REG_CBCC1, PCF50623_CBCC1_CHGENA);
 *     pcf50623_global->flags &= ~PCF50623_F_CHG_CV_REACHED;
 * }
 * 
 * void pcf50623_disableUsbChg(void)
 * {
 *     reg_clear_bits(pcf50623_global, PCF50623_REG_CBCC1, PCF50623_CBCC1_USBENA);
 * }
 */

void pcf506XX_registerChgFct(itcallback chgfct)
{
    pcf50623_global->pcf50623_itcallback = chgfct;
}

void pcf506XX_unregisterChgFct(void)
{
    pcf50623_global->pcf50623_itcallback = pcf50623_void;
}

void pcf506XX_registerUsbFct(itcallback usbfct)
{
    pcf50623_global->pcf50623_usbcallback = usbfct;
    /* ACER Bright Lee, 2009/9/7, AU2.FC-178, USB Cable detection during booting { */
    if (pcf50623_global->flags & PCF50623_F_CHG_USB_PLUGGED) {
	usbfct(1);
    }
    /* } ACER Bright Lee, 2009/9/7 */
}

void pcf506XX_unregisterUsbFct(void)
{
    pcf50623_global->pcf50623_usbcallback = pcf50623_usb_void;
}

void pcf506XX_registerOnkeyFct(itcallback onkeyfct)
{
    pcf50623_onkey = onkeyfct;
}

void pcf506XX_unregisterOnkeyFct(void)
{
    pcf50623_onkey = pcf50623_void;
}

/* ACER Jen chang, 2009/06/17, IssueKeys:AU4.B-43, Modify sd card detecting debounce time { */
/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify sd card detecting to fix pmu ic bug on PCR { */
#if (defined ACER_AU2_PR1) || (defined ACER_AU2_PR2) || (defined ACER_AU4_PR1) || (defined ACER_AU4_PR2) 
void pcf506XX_registerSdDetectFct(itcallback sdfct)
{
    pcf50623_global->pcf50623_sddetectcallback = sdfct;
}

void pcf506XX_unregisterSdDetectFct(void)
{
    pcf50623_global->pcf50623_sddetectcallback = pcf50623_sddetect_void;
}

int pcf506XX_sdcardpresent(void)
{
//return 1 if sdcard is plugged

   int gpios;
   gpios= reg_read(pcf50623_global, PCF50623_REG_GPIOS);
   return  ((gpios & 0x1)?0:1);
}
EXPORT_SYMBOL(pcf506XX_registerSdDetectFct);
EXPORT_SYMBOL(pcf506XX_unregisterSdDetectFct);
EXPORT_SYMBOL(pcf506X_sdcardpresent);
#endif
/* } ACER Jen Chang, 2009/09/02*/
/* } ACER Jen Chang, 2009/06/17*/

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
static void pcf50623_bbcc_work_timer(struct work_struct *work_timer)
{
	pcf50623_global->bbcc.working = 1;

	switch (pcf50623_global->bbcc.state) {
	case PCF50623_BBCC_LONG_CHARGE:
		pcf50623_global->bbcc.endtime = jiffies + pcf50623_global->bbcc.discharge;
		schedule_delayed_work(&pcf50623_global->bbcc.work_timer,  pcf50623_global->bbcc.discharge);
        reg_clear_bits(pcf50623_global, PCF50623_REG_BBCC1, PCF50623_BBCC1_ENABLE);
		pcf50623_global->bbcc.state = PCF50623_BBCC_DISCHARGE;
		break;
	case PCF50623_BBCC_FAST_CHARGE:
		pcf50623_global->bbcc.endtime = jiffies + pcf50623_global->bbcc.discharge;
		schedule_delayed_work(&pcf50623_global->bbcc.work_timer,  pcf50623_global->bbcc.discharge);
        reg_clear_bits(pcf50623_global, PCF50623_REG_BBCC1, PCF50623_BBCC1_ENABLE);
		pcf50623_global->bbcc.state = PCF50623_BBCC_DISCHARGE;
		break;

	case PCF50623_BBCC_DISCHARGE:
		pcf50623_global->bbcc.endtime = jiffies + pcf50623_global->bbcc.fast_charge;
		schedule_delayed_work(&pcf50623_global->bbcc.work_timer,  pcf50623_global->bbcc.fast_charge);
	    reg_set_bit_mask(pcf50623_global, PCF50623_REG_BBCC1, PCF50623_BBCC1_ENABLE, PCF50623_BBCC1_ENABLE);    
		pcf50623_global->bbcc.state = PCF50623_BBCC_FAST_CHARGE;
		break;
	}

	dbgl1("New charge mode is: %u\n", pcf50623_global->bbcc.state);
	dbgl1("Current time is %lu, next expired timer is %lu\n", jiffies, pcf50623_global->bbcc.endtime);

	pcf50623_global->bbcc.working = 0;
}

/// +ACER_AUx PenhoYu, IssueKeys:AU2.B-2325
//static void pcf50623_poweroff(void)
void pcf50623_poweroff(void)
/// -ACER_AUx PenhoYu, IssueKeys:AU2.B-2325
{
	u_int8_t occmask = __reg_read(pcf50623_global, PCF50623_REG_OOCC1);
	occmask = occmask | PCF50623_OOCC1_GO_OFF | PCF50623_OOCC1_GO_HIB;
	reg_write(pcf50623_global, PCF50623_REG_OOCC1,  occmask);
}

/// +ACER_AUx PenhoYu, IssueKeys:AU2.B-2325
EXPORT_SYMBOL(pcf50623_poweroff);
/// -ACER_AUx PenhoYu, IssueKeys:AU2.B-2325


/* ACER Bright Lee, 2010/8/11, A21.B-2713, get power on cause for different driver behavior { */
static int g_poweron = 0;
int getOnCause(void) {return g_poweron;}
EXPORT_SYMBOL(getOnCause);

static int __init power_on_cause_setup(char *str)
{
	get_option(&str, &g_poweron);
	return 1;
}

__setup("androidboot.mode=", power_on_cause_setup);
/* } ACER Bright Lee, 2010/8/11 */


/* as this function is called from a botton half implemented on a work queue 
 * the launch of the work queue is always successfull */
static void pcf50623_work(struct work_struct *work)
{
	struct pcf50623_data *pcf = container_of(work, struct pcf50623_data, work);
    u_int16_t batteryInt = 0;
	u_int8_t int1, int2, int3, int4;
	u_int8_t int1m, int2m, int3m, int4m;
	char *env[] = {"TVOUT=unplugged", NULL};
	struct kobject *pkobj = &(pcf50623_pdev->dev.kobj);

	pcf->working = 1;

	/* Read interrupt MASK to filter masked interrupt when a non masked
	 * interrupt occurs */
	int1m = __reg_read(pcf, PCF50623_REG_INT1M);
	int2m = __reg_read(pcf, PCF50623_REG_INT2M);
	int3m = __reg_read(pcf, PCF50623_REG_INT3M);
	int4m = __reg_read(pcf, PCF50623_REG_INT4M);

	dbgl2("INT1M=0x%02x INT2M=0x%02x INT3M=0x%02x INT4M=0x%02x\n",
			int1m, int2m, int3m, int4m);

	int1 = __reg_read(pcf, PCF50623_REG_INT1);
	int2 = __reg_read(pcf, PCF50623_REG_INT2);
	int3 = __reg_read(pcf, PCF50623_REG_INT3);
	int4 = __reg_read(pcf, PCF50623_REG_INT4);

	dbgl2("INT1=0x%02x INT2=0x%02x INT3=0x%02x INT4=0x%02x\n",
			int1, int2, int3, int4);

	/* Prevent from masked interrupt from being handled */
	int1 &= ~int1m;
	int2 &= ~int2m;
	int3 &= ~int3m;
	int4 &= ~int4m;

#ifdef VREG
	pcf50623_user_irq_handler(pcf);
#endif 

	dbgl2("INT1=0x%02x INT2=0x%02x INT3=0x%02x INT4=0x%02x\n",int1, int2, int3, int4);

	if (int1 & PCF50623_INT1_LOWBAT) {
		/* BVM detected low battery voltage */
		dbgl1("LOWBAT\n");
        batteryInt |= PCF506XX_LOWBAT;

		dbgl2("SIGPWR(init) ");
		kill_cad_pid(SIGPWR, 0);
		/* Tell PMU we are taking care of this */
		reg_set_bit_mask(pcf, PCF50623_REG_OOCC1,
				 PCF50623_OOCC1_TOT_RST,
				 PCF50623_OOCC1_TOT_RST);
	}

	if (int1 & PCF50623_INT1_SECOND) {
		/* RTC periodic second interrupt */
		dbgl1("SECOND\n");
		if (pcf->flags & PCF50623_F_RTC_SECOND)
			rtc_update_irq(pcf->rtc, 1,
				       RTC_PF | RTC_IRQF);
		
		//Selwyn marked this because ACER don't need it
		#if 0
		if (pcf->onkey_seconds >= 0 &&
		    pcf->flags & PCF50623_F_PWR_PRESSED) {
			dbgl1("ONKEY_SECONDS(%u, OOCSTAT=0x%02x) ",
				pcf->onkey_seconds,
				reg_read(pcf, PCF50623_REG_OOCS));
			pcf->onkey_seconds++;
			if (pcf->onkey_seconds ==
			    pcf->pdata->onkey_seconds_required) {
				/* Ask init to do 'ctrlaltdel' */
				dbgl1("SIGINT(init) ");
				kill_cad_pid(SIGPWR, 0);
				/* FIXME: what if userspace doesn't shut down? */
			} else
			if (pcf->onkey_seconds >= pcf->pdata->onkey_seconds_poweroff) {
				/* Force poweroff */
				pcf50623_poweroff();
			}
		}
		#endif
    }


    if (int1 & PCF50623_INT1_MINUTE) {
		dbgl1("MINUTE\n");
	}
    
	if (int1 & PCF50623_INT1_ALARM) {
		dbgl1("ALARM\n");
		if (pcf->pdata->used_features & PCF506XX_FEAT_RTC)
			rtc_update_irq(pcf->rtc, 1,
				       RTC_AF | RTC_IRQF);
		pcf->rtc_alarm.pending = 1;
	}
	
	if (int1 & PCF50623_INT1_ONKEYF) {
		/* ONKEY falling edge (start of button press) */
		dbgl1("ONKEYF\n");
		//Selwyn marked 20090615
		input_report_key(pcf->input_dev, KEY_STOP, 1);
		input_sync(pcf->input_dev);
		//pcf50623_onkey(1);
		//~Selwyn marked
	}

	if (int1 & PCF50623_INT1_ONKEYR) {
		/* ONKEY rising edge (end of button press) */
		dbgl1("ONKEYR\n");
		pcf->onkey_seconds = -1;
		//Selwyn marked this because ACER don't need it
		#if 0
		pcf->flags &= ~PCF50623_F_PWR_PRESSED;
		/* disable SECOND interrupt in case RTC didn't
		 * request it */
		if (!(pcf->flags & PCF50623_F_RTC_SECOND))
			reg_set_bit_mask(pcf, PCF50623_REG_INT1M,
					 PCF50623_INT1_SECOND,
					 PCF50623_INT1_SECOND);
		#endif
		//~Selwyn marked
		//Selwyn marked 20090615
		input_report_key(pcf->input_dev, KEY_STOP, 0);
		input_sync(pcf->input_dev);
		//pcf50623_onkey(0);
		//~Selwyn marked
	}

	if (int1 & PCF50623_INT1_ONKEY1S) {
		dbgl1("ONKEY1S\n");
		pcf->onkey_seconds=0;
		/* Tell PMU we are taking care of this */
		reg_set_bit_mask(pcf, PCF50623_REG_OOCC1,
				 PCF50623_OOCC1_TOT_RST,
				 PCF50623_OOCC1_TOT_RST);
		//Selwyn marked this because ACER don't need it
		/* enable second interrupt to manage long key press */
		//reg_clear_bits(pcf, PCF50623_REG_INT1M, PCF50623_INT1_SECOND);
		//~Selwyn marked

/* 20100201 add by HC Lai for POWER-ON in POC {*/
              if (getOnCause() == 2) {
                  printk(KERN_ERR "getOnCause()=%d, Power on during POC\n", getOnCause());
                  kernel_restart(NULL);
              }
/* }20100201 add by HC Lai for POWER-ON in POC */
	}

	if (int1 & PCF50623_INT1_HIGHTMP) {
		dbgl1("HIGHTMP\n");
        batteryInt |= PCF506XX_HIGHTMP;
    }

    /* INT2 management */
	if (int2 & PCF50623_INT2_EXTONR) {
		dbgl1("EXTONR\n");
	}

	if (int2 & PCF50623_INT2_EXTONF) {
		dbgl1("EXTONF\n");
	}

#if defined(CONFIG_NKERNEL)
	if (int2 & PCF50623_INT2_RECLF) {
        dbgl1("RECLF: check resistance\n");
#ifdef CONFIG_NET_9P_XOSCORE
 		/* Set REC1 in continuous mode and mic bias enabled */
        reg_write(pcf, PCF50623_REG_RECC1, 0xA0);
        
        adc_aread(FID_ACC,pcf5062x_adc_accessory);
#endif
	}

	if (int2 & PCF50623_INT2_RECLR) {
        dbgl1("RECLR\n"); 
        //Selwyn modified for headset detect
        if ((pcf->accessory.headset_plugged == 1) 
#if defined (ACER_L1_CHANGED)
            && (!hs_detect_wait)
#endif
           ) { 
        //~Selwyn modified
            /* Headset plugged + RECLR => headset button released */
            if(HS_MIC)
            {
            pcf50623_global->accessory.button_pressed = 0;
            dbgl1("RECLR: Headset button released\n");
            input_report_key(pcf->input_dev, KEY_MUTE, 0);
	    input_sync(pcf->input_dev);
            }
	//Selwyn 20091001 modified 
	#if !defined (ACER_L1_CHANGED)
	else {
            /* TVout cable unplugged */
            pcf->accessory.tvout_plugged = 0;
            dbgl1("RECLR: Tvout unplugged\n");
            kobject_uevent_env(pkobj, KOBJ_CHANGE, env);
			}
	#endif
	//~Selwyn modified
        }
	}

	if (int2 & PCF50623_INT2_RECHF) {
		dbgl1("RECHF: check resistance\n");
#ifdef CONFIG_NET_9P_XOSCORE
		/* Set REC1 in continuous mode and mic bias enabled */
		reg_write(pcf, PCF50623_REG_RECC1, 0xA0);
		/* Wait for MICBIAS settle time */
		mdelay(10);
		adc_aread(FID_ACC,pcf5062x_adc_accessory);
#endif
	}
#endif /* CONFIG_NKERNEL */

	if (int2 & PCF50623_INT2_RECHR) {
        dbgl1("REC2HR\n");
        /* If current state of tvout is plugged, skip RECHR int */
        if (pcf->accessory.tvout_plugged == 1)
        {
            pcf->accessory.tvout_plugged = 0;
            dbgl1("REC2HR: Tvout unplugged\n");
            kobject_uevent_env(pkobj, KOBJ_CHANGE, env);
        } else {
            /* Headset unplugged */
            //Selwyn modified for headset detect
            dbgl1("REC2HR: Headset unplugged\n");
            #if !defined (ACER_L1_CHANGED) || defined (ACER_AU4_PR1)
            pcf->accessory.headset_plugged = 0;
            //input_event(pcf->input_dev, EV_SW, SW_HEADPHONE_INSERT, 0);
#ifdef CONFIG_SWITCH
	switch_set_state(&pcf50623_global->accessory.swheadset, pcf50623_global->accessory.headset_plugged );
#endif
            #endif
            //Selwyn modified for headset detect
        }
	}

	if (int2 & PCF50623_INT2_VMAX) {
		dbgl1("VMAX\n");
		pcf->flags |= PCF50623_F_CHG_CV_REACHED;
        batteryInt |= PCF506XX_VMAX;
	}

	if (int2 & PCF50623_INT2_CHGWD) {
		dbgl1("CHGWD\n");
        batteryInt |= PCF506XX_CHGWD;
	}


    /* INT3 management */
	if (int3 & PCF50623_INT3_CHGRES) {
		dbgl1("CHGRES\n");
        batteryInt |= PCF506XX_CHGRES;
		/* battery voltage < autoes voltage threshold */
	}

	if (int3 & PCF50623_INT3_THLIMON) {
		dbgl1("THLIMON\n");
        batteryInt |= PCF506XX_THLIMON;
		pcf->flags |= PCF50623_F_CHG_THERM_LIMIT;
	}

	if (int3 & PCF50623_INT3_THLIMOFF) {
		dbgl1("THLIMOFF\n");
        batteryInt |= PCF506XX_THLIMOFF;
		pcf->flags &= ~PCF50623_F_CHG_THERM_LIMIT;
		/* FIXME: signal this to userspace */
	}
    
	if (int3 & PCF50623_INT3_BATFUL) {
		dbgl1("BATFUL: fully charged battery \n");
        batteryInt |= PCF506XX_BATFUL;
		pcf->flags |= PCF50623_F_CHG_BATTFULL;
	}

	if (int3 & PCF50623_INT3_MCHINS) {
        dbgl1("MCHINS\n");
/// +ACER_AUx PenhoYu, For reset the USB Charge event
extern void acerchg_chgins(void);
		acerchg_chgins();
/// -ACER_AUx PenhoYu, For reset the USB Charge event
		/* charger insertion detected */
		pcf->flags |= PCF50623_F_CHG_MAIN_PLUGGED;
        batteryInt |= PCF506XX_MCHINS;
	}

	if (int3 & PCF50623_INT3_MCHRM) {
		dbgl1("MCHRM\n");
		/* charger removal detected */
        pcf->flags &= ~PCF50623_F_CHG_MAIN_PLUGGED;
        batteryInt |= PCF506XX_MCHRM;
	}

        /* ACER Bright Lee, 2009/10/22, AU2.FC-305, Single Port Charge { */
	// if (int3 & PCF50623_INT3_UCHGINS) {
	if (int3 & (PCF50623_INT3_UCHGINS | PCF50623_INT3_MCHINS)) {
	/* } ACER Bright Lee, 2009/10/22 */
		dbgl1("UCHGINS\n");
		pcf->flags |= PCF50623_F_CHG_USB_PLUGGED;
        batteryInt |= PCF506XX_UCHGINS;
	    pcf->pcf50623_usbcallback(1);
	}

	/* ACER Bright Lee, 2009/10/22, AU2.FC-305, Single Port Charge { */
	// if (int3 & PCF50623_INT3_UCHGRM) {
	if (int3 & (PCF50623_INT3_UCHGRM | PCF50623_INT3_MCHRM)) {
	/* } ACER Bright Lee, 2009/10/22 */
		dbgl1("UCHGRM\n");
		pcf->flags &= ~PCF50623_F_CHG_USB_PLUGGED;
        batteryInt |= PCF506XX_UCHGRM;
	    pcf->pcf50623_usbcallback(0);
	    /* ACER Bright Lee, 2010/6/4, A21.B-866, power off DUT if plug out USB cable when power off charging { */
	    if (getOnCause () == 2 /* UBOOT_CHARGER */) {
		    printk ("No charger detected, power off\n");
		    pcf50623_poweroff();
		    while(1);
	    }
	    /* } ACER Bright Lee, 2010/6/4 */

	}


    /* INT4 management */
	if (int4 & PCF50623_INT4_CBCOVPFLT) {
		dbgl1("CBCOVPFLT: CBC in over-voltage protection\n");
        batteryInt |= PCF506XX_CBCOVPFLT;
	}

	if (int4 & PCF50623_INT4_CBCOVPOK) {
		dbgl1("CBCOVPOK: CBC no longer in over-voltage protection\n");
        batteryInt |= PCF506XX_CBCOVPOK;
	}

	if (int4 & PCF50623_INT4_SIMUV) {
		dbgl1("SIMUV: USIMREG reported an under-voltage error\n");
	}

	if (int4 & PCF50623_INT4_USIMPRESF) {
		dbgl1("USIMPRESF: falling edge detected on USIMPRES input\n");
	}

	if (int4 & PCF50623_INT4_USIMPRESR) {
		dbgl1("USIMPRESF: rising edge detected on USIMPRES input\n");
	}

	if (int4 & PCF50623_INT4_GPIO1EV) {
/* ACER Jen chang, 2009/06/17, IssueKeys:AU4.B-43, Modify sd card detecting for different compiler option { */
/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify sd card detecting to fix pmu ic bug on PCR { */
#if (defined ACER_AU2_PR1) || (defined ACER_AU2_PR2) || (defined ACER_AU4_PR1) || (defined ACER_AU4_PR2) 
		dbgl1("GPIO1EV: rising or falling edge detected on GPIO1 input call sddetect  call function\n");
		pcf50623_global->pcf50623_sddetectcallback(pcf506XX_sdcardpresent());
#else
		dbgl1("GPIO1EV: rising or falling edge detected on GPIO1 input\n");
#endif
/* } ACER Jen Chang, 2009/09/02*/
/* } ACER Jen Chang, 2009/06/17*/
	}

	if (int4 & PCF50623_INT4_GPIO3EV) {
		dbgl1("GPIO2EV: rising or falling edge detected on GPIO3 input\n");
	}

	if (int4 & PCF50623_INT4_GPIO4EV) {
		dbgl1("GPIO4EV: rising or falling edge detected on GPIO4 input\n");
/// +ACER_AUx PenhoYu, enable PMU GPIO4 for detect OVP_FAULT of RT9718
		batteryInt |= PCF506XX_OVPFAULT;
/// -ACER_AUx PenhoYu, enable PMU GPIO4 for detect OVP_FAULT of RT9718
	}


    pcf->pcf50623_itcallback(batteryInt);
	pcf->working = 0;
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

static int pcf50623_schedule_work(struct pcf50623_data *pcf)
{
	int status;

#ifdef CONFIG_I2C_NEW_PROBE
	get_device(&pcf->client->dev);
#else
	get_device(&pcf->client.dev);
#endif
    status = queue_work(pcf->workqueue, &pcf->work);

	if (!status )
		dbgl2("pcf 50623 work item may be lost\n");
	return status;
}

static irqreturn_t pcf50623_irq(int irq, void *_pcf)
{
	struct pcf50623_data *pcf = _pcf;
    u_int8_t pmu_int;

	dbgl2("entering(irq=%u, pcf=%p): scheduling work\n", irq, _pcf);

    /* Read interrupt status to manage the pending interrupt */
	pmu_int = gpio_get_value(EXTINT_TO_GPIO(irq));

    if (pmu_int == 0)
    {
        pcf50623_schedule_work(pcf);
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
static int pcf50623_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
	u_long counter;
	u_int8_t rtc1,rtc2,rtc3,rtc4;

	mutex_lock(&pcf->lock);

	/* Get value from PCF50623 reg */
	rtc1 = (u_int8_t)__reg_read(pcf, PCF50623_REG_RTC1);
	rtc2 = (u_int8_t)__reg_read(pcf, PCF50623_REG_RTC2);
	rtc3 = (u_int8_t)__reg_read(pcf, PCF50623_REG_RTC3);
	rtc4 = (u_int8_t)__reg_read(pcf, PCF50623_REG_RTC4);

	/* Compute the counter value */
	counter = (u_long)(rtc1<<24)+(u_long)(rtc2<<16)+(u_long)(rtc3<<8)+(u_long)rtc4;

	/* Convert the counter in GMT info */
	rtc_time_to_tm(counter,tm);

	mutex_unlock(&pcf->lock);

	return 0;
}

static int pcf50623_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50623_data *pcf = i2c_get_clientdata(client);

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
	ret = __reg_write(pcf, PCF50623_REG_RTC1,rtc1);
	ret = __reg_write(pcf, PCF50623_REG_RTC2,rtc2);
	ret = __reg_write(pcf, PCF50623_REG_RTC3,rtc3);
	ret = __reg_write(pcf, PCF50623_REG_RTC4,rtc4);

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
 * pcf50623_fill_with_next_date - sets the next possible expiry date in alrm
 * @alrm: contains the sec, min and hour part of the time to be set
 *
 * This function finds the next possible expiry date and fills it into alrm.
 */
static int pcf50623_rtc_fill_with_next_date(struct device *dev, struct rtc_wkalrm *alrm)
{
    struct rtc_time next, now;

	/* read the current time */
	pcf50623_rtc_read_time(dev, &now);

	rtc_next_alarm_time(&next, &now, &alrm->time);

	alrm->time = next;
	return 0;
}

static int pcf50623_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
	u_int8_t rtc1a,rtc2a,rtc3a,rtc4a;
	u_long counter;

	mutex_lock(&pcf->lock);

	/* Get value from PCF50623 reg */
	rtc1a = (u_int8_t)__reg_read(pcf, PCF50623_REG_RTC1A);
	rtc2a = (u_int8_t)__reg_read(pcf, PCF50623_REG_RTC2A);
	rtc3a = (u_int8_t)__reg_read(pcf, PCF50623_REG_RTC3A);
	rtc4a = (u_int8_t)__reg_read(pcf, PCF50623_REG_RTC4A);

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
 * pcf50623_set_alarm - set the alarm in the RTC
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
static int pcf50623_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50623_data *pcf = i2c_get_clientdata(client);
	u_long counter;
	int ret;
	u_int8_t rtc1a,rtc2a,rtc3a,rtc4a;
	u_int8_t irqmask;

	mutex_lock(&pcf->lock);

	alrm->pending = 0;
	if (alrm->time.tm_year == -1) {
		/* called from 1) - (see doc above this function) */
		ret = pcf50623_rtc_fill_with_next_date(dev, alrm);
		
		alrm->enabled = 1; /* non-persistant */
	}

	/* disable alarm interrupt */
	irqmask = __reg_read(pcf, PCF50623_REG_INT1M);
	irqmask |= PCF50623_INT1_ALARM;
    dbgl1("INT1 irq mask = %02X\n",irqmask);
	__reg_write(pcf, PCF50623_REG_INT1M, irqmask);

	/* Convert current RTC time in secondes */
	ret = rtc_tm_to_time(&alrm->time, &counter);

	/* Split the counter to store it in the RTC registers */
	rtc1a = (u_int8_t)((counter>>24)&0xFF);
	rtc2a = (u_int8_t)((counter>>16)&0xFF);
	rtc3a = (u_int8_t)((counter>>8)&0xFF);
	rtc4a = (u_int8_t)((counter)&0xFF);

	/* Write the values in the registers */
	ret = __reg_write(pcf, PCF50623_REG_RTC1A,rtc1a);
	ret = __reg_write(pcf, PCF50623_REG_RTC2A,rtc2a);
	ret = __reg_write(pcf, PCF50623_REG_RTC3A,rtc3a);
	ret = __reg_write(pcf, PCF50623_REG_RTC4A,rtc4a);

 	if (alrm->enabled) { 
		/* (re-)enaable alarm interrupt */
		irqmask = __reg_read(pcf, PCF50623_REG_INT1M);
		irqmask &= ~PCF50623_INT1_ALARM;
        dbgl1("INT1 irq mask = %02X\n",irqmask);
		__reg_write(pcf, PCF50623_REG_INT1M, irqmask);
 	} 

	pcf->rtc_alarm.enabled = alrm->enabled;
	pcf->rtc_alarm.pending = alrm->pending;


	dbgl1("Alarm %s\n", alrm->enabled?"Enabled":"Disabled");
	dbgl1("Alarm %s\n", alrm->pending?"Pending":"Not Pending");
	mutex_unlock(&pcf->lock);

	return 0;
}

static int pcf50623_rtc_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct pcf50623_data *pcf = i2c_get_clientdata(client);

	switch (cmd) {
	case RTC_PIE_OFF:
	case RTC_UIE_OFF:
		/* disable periodic interrupt (hz tick) */
		pcf->flags &= ~PCF50623_F_RTC_SECOND;
		reg_set_bit_mask(pcf, PCF50623_REG_INT1M, PCF50623_INT1_SECOND, PCF50623_INT1_SECOND);
		return 0;
	case RTC_PIE_ON:
	case RTC_UIE_ON:
		/* ensable periodic interrupt (hz tick) */
		pcf->flags |= PCF50623_F_RTC_SECOND;
		reg_clear_bits(pcf, PCF50623_REG_INT1M, PCF50623_INT1_SECOND);
		return 0;

	case RTC_AIE_ON:
/* 		local_irq_save(flags); */
/* 		int_enabled |= RTC_AIE_MASK; */
/* 		local_irq_restore(flags); */
		reg_clear_bits(pcf, PCF50623_REG_INT1M, PCF50623_INT1_ALARM);
		break;

	case RTC_AIE_OFF:
/* 		local_irq_save(flags); */
/* 		int_enabled &= ~RTC_AIE_MASK; */
/* 		local_irq_restore(flags); */
		reg_set_bit_mask(pcf, PCF50623_REG_INT1M, PCF50623_INT1_ALARM, PCF50623_INT1_ALARM);
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

static struct rtc_class_ops pcf50623_rtc_ops = {
	.read_time	= pcf50623_rtc_read_time,
	.set_time	= pcf50623_rtc_set_time,
	.read_alarm	= pcf50623_rtc_read_alarm,
	.set_alarm	= pcf50623_rtc_set_alarm,
	.ioctl		= pcf50623_rtc_ioctl,
};


/***********************************************************************
 * GPIO pmu interface     
 ***********************************************************************/
static int pcf50623_gpios[6] =
{
	PCF50623_REG_GPIO1C1,
	PCF50623_REG_GPIO2C1,
	PCF50623_REG_GPIO3C1,
	PCF50623_REG_GPIO4C1,
	PCF50623_REG_GPIO5C1,
	PCF50623_REG_GPIO6C1,
};

static int pcf50623_check_gpio(int gpio)
{
	if (gpio<PMU_GPIO1 || gpio>PMU_GPIO4)
	{
		dbgl1("GPIO ID %u does not belong to PMU\n",gpio);
		return -EFAULT;
	}

	gpio=gpio-PMU_GPIO1;

	/* Manage only GPIOS not configured in board_pnx67xx_wavex.c */
	if ( pcf50623_global->gpios & (1<<gpio) )
	{
		dbgl1("GPIO%u is already configures in board_pnx67xx_wavex.c, cannot use it\n",gpio+1);
		return -EACCES;
	}

	dbgl1("pcf50623_check_gpio, gpio to configure is GPIO%u\n",gpio);
	return gpio;
}

int pnx_set_pmugpio_direction(int gpio, int is_input)
{
	gpio = pcf50623_check_gpio(gpio);

	if (gpio<0)
		return gpio;
	
	if ( is_input )
		reg_write(pcf50623_global, pcf50623_gpios[gpio],PCF50623_GPIO_IN);
	else
		reg_write(pcf50623_global, pcf50623_gpios[gpio],PCF50623_GPIO_HZ);

	return is_input;
}

int pnx_get_pmugpio_direction(int gpio)
{
	int status;

	gpio = pcf50623_check_gpio(gpio);

	if (gpio<0)
		return gpio;
	
	status = reg_read(pcf50623_global,pcf50623_gpios[gpio]);
	if ( (status & PCF50623_GPIO_IN)==PCF50623_GPIO_IN )
		return 1;
	else
		return 0;	
}

int pnx_write_pmugpio_pin(int gpio, int gpio_value)
{
	gpio = pcf50623_check_gpio(gpio);

	if (gpio<0)
		return gpio;

	if ( gpio_value )
		reg_write(pcf50623_global, pcf50623_gpios[gpio],PCF50623_GPIO_HZ);
	else
		reg_write(pcf50623_global, pcf50623_gpios[gpio],0);

	return 0;
}

int pnx_read_pmugpio_pin(int gpio)
{
	int status;

	gpio = pcf50623_check_gpio(gpio);

	if (gpio<0)
		return gpio;

	status = reg_read(pcf50623_global,pcf50623_gpios[gpio]);

	if ( (status & PCF50623_GPIO_IN)==PCF50623_GPIO_IN )
	{
/// +ACER_AUx PenhoYu, enable PMU GPIO4 for detect OVP_FAULT of RT9718
		status = reg_read(pcf50623_global, PCF50623_REG_GPIOS);
/// -ACER_AUx PenhoYu, enable PMU GPIO4 for detect OVP_FAULT of RT9718
		return ((status & (1<<gpio))?1:0);
	}
	else
	{
		status = reg_read(pcf50623_global, PCF50623_REG_GPIOS);
		return (status & PCF50623_GPIO_HZ);
	}

}

int pnx_set_pmugpio_mode(int gpio, int mode)
{
	gpio = pcf50623_check_gpio(gpio);

	if (gpio<0)
		return gpio;
	
	if (pnx_get_pmugpio_direction(PMU_GPIO1+gpio) == 1)
	{
		dbgl1("Cannot set Mode of GPIO%u, it's configures as INPUT\n",gpio+1);
		return -EPERM;
	}
	
	reg_write(pcf50623_global, pcf50623_gpios[gpio],mode);

	return 0;
}

EXPORT_SYMBOL(pnx_set_pmugpio_direction);
EXPORT_SYMBOL(pnx_write_pmugpio_pin);
EXPORT_SYMBOL(pnx_read_pmugpio_pin);
EXPORT_SYMBOL(pnx_get_pmugpio_direction);
EXPORT_SYMBOL(pnx_set_pmugpio_mode);

static int pmugpio_input(struct gpio_chip *chip, unsigned offset)
{
	int gpio;
	int is_input = !0;

	gpio = (int) offset + chip->base;

	return pnx_set_pmugpio_direction(gpio, is_input);
}
static int pmugpio_get(struct gpio_chip *chip, unsigned offset)
{
	int gpio;

	gpio = (int) offset + chip->base;

	return pnx_read_pmugpio_pin(gpio);
}
static int pmugpio_output(struct gpio_chip *chip, unsigned offset, int value)
{
	int gpio;
	int is_input = 0;

	gpio = (int) offset + chip->base;

	pnx_write_pmugpio_pin(gpio, value);
	return pnx_set_pmugpio_direction(gpio, is_input);
}
static void pmugpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	int gpio;

	gpio = (int) offset + chip->base;

	pnx_write_pmugpio_pin(gpio, value);

	return;
}
static int pmugpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	return -EINVAL;
}

/***********************************************************************
 * IOCTL management
 ***********************************************************************/
static int pcf50623_pmu_open(struct inode * inode, struct file * instance)
{
	dbgl1 ( KERN_INFO "pcf50623_pmu_open\n" );
  
	return 0 ;
}

static int pcf50623_pmu_close(struct inode * inode, struct file * instance)
{
	dbgl1 ( KERN_INFO "pcf50623_pmu_close\n" );
  
	return 0 ;
}


static int pcf50623_pmu_ioctl(struct inode *inode, struct file *instance,
                              unsigned int cmd, unsigned long arg)
{
    struct ioctl_write_reg swr;
    struct pmu_ioctl_gpio sgp;
	void __user *argp = (void __user *)arg;
    int reg, res;
    int value;

	dbgl1 ( KERN_INFO "pcf50623_pmu_ioctl, cmd=%u\n",cmd );

	switch (cmd) {
	//Selwyn 20090908 modified
	case PCF50623_IOCTL_HS_PLUG:
		{
			unsigned char hs_plug=0;
			hs_plug = pcf50623_global->accessory.headset_plugged;
			if(hs_plug)
			{
				#if 0
				input_event(pcf50623_global->input_dev, EV_SW, SW_HEADPHONE_INSERT, 0);			
				input_event(pcf50623_global->input_dev, EV_SW, SW_HEADPHONE_INSERT, 1);			
				#endif
				#ifdef CONFIG_SWITCH
				switch_set_state(&pcf50623_global->accessory.swheadset, 0);
				switch_set_state(&pcf50623_global->accessory.swheadset, 1);
				#endif
			}
			if (copy_to_user(argp, &hs_plug, sizeof(hs_plug)))
				return -EFAULT;
			break;
		}
	//~Selwyn modified
	case PCF506XX_IOCTL_READ_REG:
		res=copy_from_user(&reg, argp, sizeof(int));
		if (res)
			return -EFAULT;
		value = reg_read(pcf50623_global, reg);
		if (copy_to_user((void *)arg, &value, sizeof(value)))
			return -EFAULT;
		break;

	case PCF506XX_IOCTL_WRITE_REG:
		res=copy_from_user(&swr, argp, sizeof(swr));
		if (res)
			return -EFAULT;
		dbgl1("IOCTL write reg. Addr=0x%02X, Value=%u (0x%02X)\n", swr.address, swr.value, swr.value);
		return reg_write(pcf50623_global, swr.address, swr.value);
		break;

	case PCF506XX_IOCTL_SET_GPIO_DIR:
		if (copy_from_user(&sgp, argp, sizeof(sgp)))
			return -EFAULT;
		sgp.gpio+=PMU_GPIO1;
		sgp.gpio=pnx_set_pmugpio_direction(sgp.gpio, sgp.dir);
		if (copy_to_user(argp, &sgp, sizeof(sgp)))
			return -EFAULT;
		break;

	case PCF506XX_IOCTL_SET_GPIO_MODE:
		if (copy_from_user(&sgp, argp, sizeof(sgp)))
			return -EFAULT;
		sgp.gpio+=PMU_GPIO1;
		sgp.gpio=pnx_set_pmugpio_mode(sgp.gpio, sgp.state);
		if (copy_to_user(argp, &sgp, sizeof(sgp)))
			return -EFAULT;
		break;

	case PCF506XX_IOCTL_READ_GPIO:
		if (copy_from_user(&sgp, argp, sizeof(sgp)))
			return -EFAULT;

		sgp.gpio+=PMU_GPIO1;
		sgp.state=pnx_read_pmugpio_pin(sgp.gpio);
		sgp.dir=pnx_get_pmugpio_direction(sgp.gpio);
		if (copy_to_user(argp, &sgp, sizeof(sgp)))
			return -EFAULT;
		break;

	case PCF506XX_IOCTL_WRITE_GPIO:
		if (copy_from_user(&sgp, argp, sizeof(sgp)))
			return -EFAULT;
		sgp.gpio+=PMU_GPIO1;
		sgp.gpio=pnx_write_pmugpio_pin(sgp.gpio, sgp.state);
		if (copy_to_user(argp, &sgp, sizeof(sgp)))
			return -EFAULT;
		break;

	case PCF506XX_IOCTL_BOOTCAUSE:
		{
			unsigned long bootcausereport=0;
			unsigned char int1,int2,int3,int4,oocs;
			int1 = bootcause & 0xff;
			int2 = (bootcause >> 8)& 0xff;
			int3 = (bootcause >> 16) & 0xff;
			int4 = (bootcause >> 24) & 0xff;
			oocs = oocscause &0xff;
			/*bootcause contains the initial status */
			/* if neither onkey cause or alarm is set and non charger usb or main charger is plugged */
			/* system starts following several usb or charger removal*/ 
			/* boot cause bit map enum pcf50623_bootcause */
			if (int1 & 	PCF50623_INT1_ONKEYF) bootcausereport |= PCF506XX_ONKEY;
			if (int1 & 	PCF50623_INT1_ALARM)  bootcausereport |=  PCF506XX_ALARM;
#if defined (CONFIG_MACH_PNX67XX_V2_E150B_2GB) || defined (CONFIG_MACH_PNX67XX_V2_E150C_2GB) || defined (CONFIG_MACH_PNX67XX_V2_E150D_2GB) || defined (CONFIG_MACH_PNX67XX_V2_E150E_2GB)
			if (oocs &  (PCF50623_OOCS_UCHGOK|PCF50623_OOCS_MCHGOK)) {
 				bootcausereport |=  PCF506XX_USB;
 				printk(KERN_INFO "bootcausereport |=  PCF50623_USB\n");
 			}
 			if (oocs & 	PCF50623_OOCS_MCHGOK ){
 				bootcausereport |=  PCF506XX_MCH;
 				printk(KERN_INFO "bootcausereport |=  PCF50623_MCH\n");
 			}
 			switch (int3 &((PCF50623_INT3_UCHGRM|PCF50623_INT3_UCHGINS)|
 				           (PCF50623_INT3_MCHINS|PCF50623_INT3_MCHRM)))
#else
			if (oocs &  PCF50623_OOCS_UCHGOK ) bootcausereport |=  PCF506XX_USB;
			if (oocs & 	PCF50623_OOCS_MCHGOK ) bootcausereport |=  PCF506XX_MCH;
			switch (int3 &(PCF50623_INT3_UCHGRM|PCF50623_INT3_UCHGINS)) 
#endif
			{
			case ((PCF50623_INT3_UCHGRM |PCF50623_INT3_UCHGINS)|(PCF50623_INT3_MCHINS|PCF50623_INT3_MCHRM)) :
				bootcausereport |=  PCF506XX_USB_GLITCHES;
				break;
			case (PCF50623_INT3_UCHGRM |PCF50623_INT3_MCHRM):
				bootcausereport |=  PCF506XX_USB_GLITCH;
				break;
			}
			switch (int3 & (PCF50623_INT3_MCHINS|PCF50623_INT3_MCHRM)) 
			{
			case (PCF50623_INT3_MCHINS|PCF50623_INT3_MCHRM) :
				bootcausereport |=  PCF506XX_MCH_GLITCHES;
				break;
			case (PCF50623_INT3_MCHRM): 
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

static struct file_operations pcf50623_pmu_ops = {
	.owner		=	THIS_MODULE,
	.ioctl		=	pcf50623_pmu_ioctl,
	.open		= 	pcf50623_pmu_open,
	.release	=	pcf50623_pmu_close,
};

/***********************************************************************
 * Backlight device
 ***********************************************************************/

static int pcf50623_kbl_get_intensity(struct backlight_device *bd)
{
	u_int8_t intensity = reg_read(pcf50623_global, PCF50623_REG_D6REGC2)& PCF50623_REGULATOR_ON;

	return intensity;
}

static int pcf50623_kbl_set_intensity(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;
	int ret;

	if (bd->props.power != FB_BLANK_UNBLANK)
		intensity = 0;
	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		intensity = 0;

	if (intensity == 0 )
		ret = reg_clear_bits(pcf50623_global, PCF50623_REG_D6REGC2, PCF50623_REGULATOR_ON);
	else
		ret = reg_set_bit_mask(pcf50623_global, PCF50623_REG_D6REGC2, PCF50623_REGULATOR_ON, PCF50623_REGULATOR_ON);


	return ret;
}

//selwyn modified 
static int get_pmu_led(struct backlight_device *bd)
{
	u_int8_t intensity = 0;
	
	#if defined (ACER_L1_AU3)
	intensity = reg_read(pcf50623_global, PCF50623_REG_GPIO1C1);
	/* ACER BobIHLee@20100505, support AS1 project*/
	#elif defined (ACER_L1_AU4) || defined (ACER_L1_K2) || defined (ACER_L1_AS1)
	/* End BobIHLee@20100505*/
	intensity = reg_read(pcf50623_global, PCF50623_REG_GPO1C1);
	/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify key led controlling gpio from pnx to pmu on PCR { */
	#elif defined (ACER_AU2_PCR) || defined (ACER_L1_K3)
	intensity = (reg_read(pcf50623_global, PCF50623_REG_LED1C) & 0x80);
	#endif
	/* } ACER Jen Chang, 2009/09/02*/

	if(intensity == 0x02)
	{
		return 1;
	}
	else
	{
		return 0;
	}

}

static int set_pmu_led(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;

	if(intensity > 0)
	{
		#if defined (ACER_L1_AU3)
		reg_write(pcf50623_global, PCF50623_REG_GPIO1C1, 0x02);
		/* ACER BobIHLee@20100505, support AS1 project*/
		#elif defined (ACER_L1_AU4) || defined (ACER_L1_K2) || defined (ACER_L1_AS1)
		/* End BobIHLee@20100505*/
		reg_write(pcf50623_global, PCF50623_REG_GPO1C1, 0x7);
		/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify key led controlling gpio from pnx to pmu on PCR { */
		#elif defined (ACER_AU2_PCR) || defined (ACER_L1_K3)
		reg_write(pcf50623_global, PCF50623_REG_LED1C, 0xB8);//enable LED1 with continuously high
		#endif
		/* } ACER Jen Chang, 2009/09/02*/
	}
	else
	{
		#if defined (ACER_L1_AU3)
		reg_write(pcf50623_global, PCF50623_REG_GPIO1C1, 0x07);
		/* ACER BobIHLee@20100505, support AS1 project*/
		#elif defined (ACER_L1_AU4) || defined (ACER_L1_K2) || defined (ACER_L1_AS1)
		/* End BobIHLee@20100505*/
		reg_write(pcf50623_global, PCF50623_REG_GPO1C1, 0x0);
		/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify key led controlling gpio from pnx to pmu on PCR { */
		#elif defined (ACER_AU2_PCR) || defined (ACER_L1_K3)
		reg_write(pcf50623_global, PCF50623_REG_LED1C, 0x0);
		#endif
		/* } ACER Jen Chang, 2009/09/02*/
	}

	return 0;
}
//~selwyn modified

static struct backlight_ops pcf50623_kbl_ops = {
	//selwyn modified 20090615
	.get_brightness	= get_pmu_led,
	.update_status	= set_pmu_led,
	//~selwyn modified
	/*
	.get_brightness	= pcf50623_kbl_get_intensity,
	.update_status	= pcf50623_kbl_set_intensity,
	*/
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void pcf50623_kbl_suspend(struct early_suspend *pes)
{
	pcf50623_global->keypadbl->props.brightness = 0;
	pcf50623_kbl_set_intensity(pcf50623_global->keypadbl);
}

static void pcf50623_kbl_resume(struct early_suspend *pes)
{
	pcf50623_global->keypadbl->props.brightness = 1;
	pcf50623_kbl_set_intensity(pcf50623_global->keypadbl);
}

static struct early_suspend pcf50623_kbl_earlys = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 5,
	.suspend = pcf50623_kbl_suspend,
	.resume = pcf50623_kbl_resume,
};
#endif

/* ACER Jen chang, 2009/07/29, IssueKeys:AU4.B-137, Creat device for controlling charger indicated led { */
static int get_charger_led(struct backlight_device *bd)
{
	u_int8_t intensity = 0;
	
	intensity = reg_read(pcf50623_global, PCF50623_REG_GPIO2C1);

	if(intensity == 0x02)
	{
		return 1;
	}
	else
	{
		return 0;
	}

}

static int set_charger_led(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;

	if(intensity > 0)
	{
		reg_write(pcf50623_global, PCF50623_REG_GPIO2C1, 0x02);
	}
	else
	{
		reg_write(pcf50623_global, PCF50623_REG_GPIO2C1, 0x07);
	}

	return 0;
}

static struct backlight_ops pcf50623_chg_ops = {
	.get_brightness	= get_charger_led,
	.update_status	= set_charger_led,
};
/* } ACER Jen Chang, 2009/07/29 */

/* ACER Jen chang, 2009/06/24, IssueKeys:AU2.B-38, Implement vibrator device driver for HAL interface function { */
/***********************************************************************
 * Vibrator device
 ***********************************************************************/
static void set_pmu_vibrator(int status)
{
	if(status == 1)
	{
		reg_write(pcf50623_global, PCF50623_REG_GPO2C1, 0x07);
	}
	else
	{
		reg_write(pcf50623_global, PCF50623_REG_GPO2C1, 0x0);
	}
}

static void pmu_vibrator_on(struct work_struct *work)
{
	/* ACER Jen chang, 2009/09/02, IssueKeys:AU4.B-172, Add for getting dev point { */
	struct vibrator_properties *props = container_of(work, struct vibrator_properties, work_vibrator_on);
	struct vibrator_device *dev = container_of(props, struct vibrator_device, props);
	/* } ACER Jen Chang, 2009/09/02 */

	set_pmu_vibrator(1);

	/* ACER Jen chang, 2009/09/02, IssueKeys:AU4.B-172, Move hrtimer creating function from vibrator_on_off() to here for decreasing work queue delay { */
	hrtimer_start(&dev->props.vibrator_timer,
		      ktime_set(dev->props.value / 1000, (dev->props.value % 1000) * 1000000),
		      HRTIMER_MODE_REL);
	/* } ACER Jen Chang, 2009/09/02 */
}

static void pmu_vibrator_off(struct work_struct *work)
{
	/* ACER Jen chang, 2009/09/02, IssueKeys:AU4.B-172, Add for getting dev point { */
	struct vibrator_properties *props = container_of(work, struct vibrator_properties, work_vibrator_on);
	struct vibrator_device *dev = container_of(props, struct vibrator_device, props);
	/* } ACER Jen Chang, 2009/09/02 */

	set_pmu_vibrator(0);
}

static void timed_vibrator_on(struct vibrator_device *dev)
{
	schedule_work(&dev->props.work_vibrator_on);
}

static void timed_vibrator_off(struct vibrator_device *dev)
{
	schedule_work(&dev->props.work_vibrator_off);
}

static void vibrator_on_off(struct vibrator_device *dev, int value)
{
	hrtimer_cancel(&dev->props.vibrator_timer);

	if (value == 0)
	{
		timed_vibrator_off(dev);
	}
	else 
	{
		value = (value > 15000 ? 15000 : value);

	/* ACER Jen chang, 2009/09/02, IssueKeys:AU4.B-172, Record setting timer from AP { */
		dev->props.value =  value;
	/* } ACER Jen Chang, 2009/09/02 */

		timed_vibrator_on(dev);

	/* ACER Jen chang, 2009/09/02, IssueKeys:AU4.B-172, Move hrtimer creating function to pmu_vibrator_off() for decreasing work queue delay { */
	//	hrtimer_start(&dev->props.vibrator_timer,
	//		      ktime_set(value / 1000, (value % 1000) * 1000000),
	//		      HRTIMER_MODE_REL);
	/* } ACER Jen Chang, 2009/09/02 */
	}
}

static int vibrator_get_time(struct vibrator_device *dev)
{
	if (hrtimer_active(&dev->props.vibrator_timer))
	{
		ktime_t r = hrtimer_get_remaining(&dev->props.vibrator_timer);
		return r.tv.sec * 1000 + r.tv.nsec / 1000000;
	}
	else
		return 0;
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct vibrator_properties *props = container_of(timer, struct vibrator_properties, vibrator_timer);
	struct vibrator_device *dev = container_of(props, struct vibrator_device, props);

	timed_vibrator_off(dev);

	return HRTIMER_NORESTART;
}

static struct vibrator_ops pcf50623_vibrator_ops = {
	.get_time	= vibrator_get_time,
	.set_onoff	= vibrator_on_off,
};
/* } ACER Jen Chang, 2009/06/24*/

/***********************************************************************
 * Driver initialization
 ***********************************************************************/

static struct platform_device pnx6708_pm_dev = {
	.name		="pnx-pmu",
};


static void pcf50623_work_timer(struct work_struct *work_timer)
{
	u_int8_t recs;

	pcf50623_global->accessory.working = 1;
	recs   = reg_read(pcf50623_global, PCF50623_REG_RECS);
	dbgl1(KERN_INFO "RECS=0x%02x\n",recs);
	if ( recs < PCF50623_NOACC_DETECTED )
	{
#ifdef CONFIG_NET_9P_XOSCORE
		if ( adc_aread(FID_ACC,pcf5062x_adc_accessory) != 0)
		{
			/* adc not ready yet, restart the timer */
			queue_delayed_work(pcf50623_global->workqueue, &pcf50623_global->accessory.work_timer, PCF50623_ACC_INIT_TIME);
		}
#endif
	}
	pcf50623_global->accessory.working = 0;
}

static void pcf50623_init_registers(struct pcf50623_data *data)
{
//	int ret;
	struct rtc_time rtc_tm;
	u_long counter;
	u_int8_t int1, int2, int3, int4;
	u_int8_t rtc1, rtc2, rtc3, rtc4, rtc1a;
    /* ACER Jen chang, 2009/07/01, IssueKeys:AU2.B-153, Enable alarm power setting { */
    u_int8_t oocs, id_var, recs, bbcc1, oocc1;
    /* } ACER Jen Chang, 2009/07/01*/
	u_int8_t i;

   /* Read interrupt register to in order to get boot cause */
   /* readind is done prior to enable any extint that trigger a interrruption */
    mutex_lock(&data->lock);
	id_var = __reg_read(data, PCF50623_REG_ID);
	int1   = __reg_read(data, PCF50623_REG_INT1);
	int2   = __reg_read(data, PCF50623_REG_INT2);
	int3   = __reg_read(data, PCF50623_REG_INT3);
	int4   = __reg_read(data, PCF50623_REG_INT4);
	rtc1   = __reg_read(data, PCF50623_REG_RTC1);
	rtc2   = __reg_read(data, PCF50623_REG_RTC2);
	rtc3   = __reg_read(data, PCF50623_REG_RTC3);
	rtc4   = __reg_read(data, PCF50623_REG_RTC4);
	rtc1a  = __reg_read(data, PCF50623_REG_RTC1A);
    oocs   = __reg_read(data, PCF50623_REG_OOCS);
	bbcc1  = __reg_read(data, PCF50623_REG_BBCC1);
	/* ACER Jen chang, 2009/07/01, IssueKeys:AU2.B-153, Enable alarm power setting { */
	oocc1  = __reg_read(data, PCF50623_REG_OOCC1);
	/* } ACER Jen Chang, 2009/07/01*/
    mutex_unlock(&data->lock);

     //dark 20091023 modified for mmi power off issue
     // Tell PMU we are taking care of this 
     if (int1 & PCF50623_INT1_ONKEY1S) {
	 /* Tell PMU we are taking care of this */
	 reg_set_bit_mask(data, PCF50623_REG_OOCC1, PCF50623_OOCC1_TOT_RST, PCF50623_OOCC1_TOT_RST);
/* 20100201 add by HC Lai for POWER-ON in POC {*/
              if (getOnCause() == 2) {
                  printk(KERN_ERR "getOnCause()=%d, Power on during POC\n", getOnCause());
                  kernel_restart(NULL);
              }
/* }20100201 add by HC Lai for POWER-ON in POC */	 
     }
     //dark end }

	/* configure interrupt mask */
    /* Enable all interrupt but INT1_SECOND and minute */
    mutex_lock(&data->lock);
	__reg_write(data, PCF50623_REG_RECC1, 0x60);
	__reg_write(data, PCF50623_REG_RECC2, 0xB0);

	__reg_write(data, PCF50623_REG_INT1M, PCF50623_INT1_SECOND | PCF50623_INT1_MINUTE);
	__reg_write(data, PCF50623_REG_INT2M, 0x00);
	__reg_write(data, PCF50623_REG_INT3M, PCF50623_INT3_BATFUL);
        /*ACER Ed (Leon Wu, 2009/12/08), IssueKeys:AU2.FC-770
         masking GPIO3 irq at system boot to avoid that 
         PMU will wake up by GPIO3 when BT is on  {*/
	//__reg_write(data, PCF50623_REG_INT4M, 0x00);
        __reg_write(data, PCF50623_REG_INT4M, PCF50623_INT4_GPIO3EV);	// ACER_AUx PenhoYu, enable PCF50623_INT4_GPIO3EV for detect OVP_FAULT of RT9718
        /*} ACER Ed (Leon Wu, 2009/12/08)*/

/* 	__reg_write(data, PCF50623_REG_INT1M, ~PCF50623_INT1_SECOND); */
/* 	__reg_write(data, PCF50623_REG_INT2M, 0xFF); */
/* 	__reg_write(data, PCF50623_REG_INT3M, 0xFF); */
/* 	__reg_write(data, PCF50623_REG_INT4M, 0xFF); */	

//Jen alter 0604
/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify key led controlling gpio and sd card detecting gpio from pnx to pmu on PCR { */
#if defined (ACER_AU2_PR1) || defined (ACER_AU2_PR2) || defined (ACER_AU4_PR1) || defined (ACER_AU4_PR2) 
	/* GPIO1 */
	__reg_write(data, PCF50623_REG_GPIO1C1, 0x87); //GPIO1 as input pin with high impedance for SD card detecting
#elif defined (ACER_L1_AU3)
	/* GPIO1 */
	__reg_write(data, PCF50623_REG_GPIO1C1, 0x07); //GPIO1 as LED2 output pin for Navi_LED_N
#elif defined (ACER_AU2_PCR) || defined (ACER_L1_K3)
	/* GPIO1 */
	__reg_write(data, PCF50623_REG_GPIO1C1, 0x01); //GPIO1 as LED1 output pin for KEY_LED_CTRL
#endif
/* } ACER Jen Chang, 2009/09/02*/

#ifdef ACER_L1_CHANGED
	/* GPIO2 */
	/* ACER BobIHLee@20100505, support AS1 project*/
	#if (defined ACER_L1_AU4) || (defined ACER_L1_K2) || (defined ACER_L1_AS1)
	/* End BobIHLee@20100505*/
	__reg_write(data, PCF50623_REG_GPO1C1, 0x0);    //for KEYPAD LED
	#endif
	/* ACER Bright Lee, 2010/2/28, Charging LED, already setup by bootloader { */
	// __reg_write(data, PCF50623_REG_GPIO2C1, 0x07);	//for signal battery status with LED1
	/* } ACER Bright Lee, 2010/2/28 */
	//__reg_write(data, PCF50623_REG_LED1C, 0x5B);	//for signal battery status wiht 2 sec period--on:500ms
	__reg_write(data, PCF50623_REG_BVMC, 0x09);	//for signal battery status below Vth(2.8V)

	//Jen add 0701
	oocc1  |= PCF50623_OOCC1_RTC_WAK;
	__reg_write(data, PCF50623_REG_OOCC1, oocc1);	//for enabling alarm power on
	//Jen add 0701
#endif
//Jen alter 0604

	mutex_unlock(&data->lock);

    bootcause = int1 | (int2<<8) | (int3<<16) | (int4<<24);
    oocscause = oocs;
	printk(KERN_INFO "INT1=0x%02x INT2=0x%02x INT3=0x%02x INT4=0x%02x OOCS=0x%02x bootcause=0x%04lx \n",int1, int2, int3, int4, oocs,bootcause);
    /* Check current OOCS configuration to know wether a charger is plugged or not */

    if (oocs & PCF50623_OOCS_MCHGOK) {
		data->flags |= PCF50623_F_CHG_MAIN_PLUGGED;
    } else {
        data->flags &= ~PCF50623_F_CHG_MAIN_PLUGGED;
        reg_clear_bits(data, PCF50623_REG_CBCC1, PCF50623_CBCC1_CHGENA);
    }
    
    /* ACER Bright Lee, 2009/10/22, AU2.FC-305, Single Port Charge { */
    // if (oocs & PCF50623_OOCS_UCHGOK) {
    if (oocs & (PCF50623_OOCS_UCHGOK | PCF50623_OOCS_MCHGOK)) {
    /* } ACER Bright Lee, 2009/10/22 */
		data->flags |= PCF50623_F_CHG_USB_PLUGGED;
    } else {
	/* ACER Bright Lee, 2010/5/17, A21.B-287, POC with cable plugged { */
	if (getOnCause () == 2 /* UBOOT_CHARGER */) {
		printk ("No charger detected, power off\n");
		pcf50623_poweroff();
		while(1);
	}
	/* } ACER Bright Lee, 2010/5/17 */
        data->flags &= ~PCF50623_F_CHG_USB_PLUGGED;
        reg_clear_bits(data, PCF50623_REG_CBCC1, PCF50623_CBCC1_USBENA);
    }

	data->id = (u_int8_t)(id_var>>4);
	data->variant = (u_int8_t)(id_var&0x0F);

	printk(KERN_INFO "PMU50623 ID=%02u, VARIANT=%02u\n",data->id, data->variant);

        /* Add by Bob.IH.Lee@20090710, Change the OV3642 sensor power procedure */
        pnx_gpio_request(GPIO_F0);
	pnx_gpio_set_direction(GPIO_F0,GPIO_DIR_OUTPUT);
	pnx_gpio_set_mode(GPIO_F0,GPIO_MODE_MUX1);
        /* End Add by Bob.IH.Lee@20090710, Change the OV3642 sensor power procedure */

	/* Initialize Config relative to Modem */
	{
		struct pmu_reg_table *table;
		int table_size;
		switch (id_var &0xF0) 
		{
		case 0x10 : /* ES1 */
			table=data->pdata->reg_table_ES1;
			table_size=data->pdata->reg_table_size_ES1;
			break;
		case 0x20 : /* ES2 */
		case 0x40 : /* ES2 */
		case 0x50 : /* ES2 */
			table=data->pdata->reg_table_ES2;
			table_size=data->pdata->reg_table_size_ES2;
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
					printk(KERN_ERR "pcf50623 Modem Init %x ,%x failed  \n",
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
			if (data->pdata->rails[i].gpiopwn )
			{
				uint8_t addr = regulator_registers[i]+1;

				uint8_t gpio = (data->pdata->rails[i].gpiopwn & PCF50623_REGU_GPIO_MASK)>>4;
				uint8_t pwn = (data->pdata->rails[i].gpiopwn & PCF50623_REGU_PWEN_MASK);

				/* power regulator */
				pwn = (pwn & PCF50623_REGULATOR_GPIO_PWEN_MASK) | PCF50623_REGULATOR_ON;
				reg_write(data,addr,pwn);
				/* fix gpio if required */
				if (gpio)
				{
					addr=addr+1;
					reg_set_bit_mask(data,addr,PCF50623_REGULATOR_GPIO_PWEN_MASK,gpio);
					/* set gpios */
					if (gpio & 1) reg_write(data,PCF50623_REG_GPIO1C1,PCF50623_GPIO_IN );
					if (gpio & 2 ) reg_write(data,PCF50623_REG_GPIO2C1,PCF50623_GPIO_IN );
                    if (gpio & 4 ) reg_write(data,PCF50623_REG_GPIO3C1,PCF50623_GPIO_IN );
                    if (gpio & 8 ) reg_write(data,PCF50623_REG_GPIO4C1,PCF50623_GPIO_IN );

					/* Mark gpio as used */
					data->gpios |= gpio;
				}
			} 
			pcf506XX_onoff_set(data, i, data->pdata->rails[i].mode);
		}
	}

        /* Add by Bob.IH.Lee@20090710, Change the OV3642 sensor power procedure */
        pnx_gpio_write_pin(GPIO_F0,1);
        /* End Add by Bob.IH.Lee@20090710, Change the OV3642 sensor power procedure */

	/* Manage accessory detection at startup */
	recs   = reg_read(data, PCF50623_REG_RECS);
	dbgl1(KERN_INFO "RECS=0x%02x\n",recs);
	if ( recs < PCF50623_NOACC_DETECTED )
	{
#ifdef CONFIG_NET_9P_XOSCORE
		/* An accessory is detected, find which */
		if ( adc_aread(FID_ACC,pcf5062x_adc_accessory) != 0)
		{
			INIT_DELAYED_WORK(&data->accessory.work_timer, pcf50623_work_timer);
			queue_delayed_work(data->workqueue, &data->accessory.work_timer, PCF50623_ACC_INIT_TIME);
		}
#endif
	}

	/* Compute the counter value */
	counter = (u_long)(rtc1<<24)+(u_long)(rtc2<<16)+(u_long)(rtc3<<8)+(u_long)rtc4;
	dbgl1("Current counter time is %lu\n",counter);

		
	/* Start Backup Battery Management */
	INIT_DELAYED_WORK(&data->bbcc.work_timer, pcf50623_bbcc_work_timer);

	data->bbcc.fast_charge = PCF50623_BBCC_1H;
	data->bbcc.long_charge = PCF50623_BBCC_6H;
	data->bbcc.discharge   = PCF50623_BBCC_24H;

	/* PMU NoPower state detection workaround  */
	if (!(bbcc1 && PCF50623_BBCC1_SWITCH))
	{
		/* BBCC1 VSAVEEN bit equals to 0 means that the backup battery is fully discharged */
		printk("******       RTC TIMER RESET       ******\n");
		/* Set a correct date in RTC */
        rtc_tm.tm_mday = 1;
        rtc_tm.tm_mon  = 1 - 1;
        rtc_tm.tm_year = 2009 - 1900,
        rtc_tm.tm_hour = 8;
        rtc_tm.tm_min  = 0;
        rtc_tm.tm_sec  = 0;

#ifdef CONFIG_I2C_NEW_PROBE
		pcf50623_rtc_set_time(&data->client->dev, &rtc_tm);
#else
		pcf50623_rtc_set_time(&data->client.dev, &rtc_tm);
#endif
		
	}
	/* Start a 6 hours charge */
	dbgl1("Write 0x%02X in 0x%02X\n", PCF50623_BBCC1_ENABLE|PCF50623_BBCC1_50UA|PCF50623_BBCC1_3V0|PCF50623_BBCC1_SWITCH, PCF50623_REG_BBCC1);
	reg_write(data, PCF50623_REG_BBCC1, PCF50623_BBCC1_ENABLE|PCF50623_BBCC1_50UA|PCF50623_BBCC1_3V0|PCF50623_BBCC1_SWITCH);
	pcf50623_global->bbcc.endtime = jiffies + PCF50623_BBCC_6H;
	data->bbcc.state = PCF50623_BBCC_LONG_CHARGE;
	queue_delayed_work(data->workqueue, &data->bbcc.work_timer, PCF50623_BBCC_6H);

		
}

#ifdef CONFIG_SWITCH
static ssize_t pcf50623_print_switch_headset_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%s\n", "h2w");
}

static ssize_t pcf50623_print_switch_headset_state(struct switch_dev *sdev, char *buf)
{
/*	struct fsg_dev	*fsg = container_of(accessory.swheadset, struct fsg_dev, accessory.swheadset);*/
	return sprintf(buf, "%d\n", pcf50623_global->accessory.headset_plugged  );
}
#endif

//Selwyn modified for headset detect
#if defined (ACER_L1_CHANGED)
#if defined (ACER_L1_K3)
extern void hs_power_switch(int power_on);
#endif
static void hs_detect_work(struct delayed_work *work)
{
	struct pcf50623_data *hs_data = container_of(work, struct pcf50623_data, delay_work);
	int old_hs_state = pcf50623_global->accessory.headset_plugged;

	if(gpio_get_value(EXTINT_TO_GPIO(pcf50623_global->pdata->hs_irq)))
	{
		//if(pcf50623_global->accessory.headset_plugged == 1)
		{
			//#ifdef ACER_PM_DEBUG
			printk("GPIO_A17 = 1, Headset unplugged\n");
			//#endif
			//input_event(pcf50623_global->input_dev, EV_SW, SW_HEADPHONE_INSERT, 0);
			pcf50623_global->accessory.headset_plugged = 0;
			reg_write(pcf50623_global, PCF50623_REG_RECC1, 0x00); //disable hook key detect
			/* ACER BobIHLee@20100505, support AS1 project*/
			#if (defined ACER_L1_K2) || (defined ACER_L1_K3) || (defined ACER_L1_AS1)
			/* End BobIHLee@20100505*/
			pcf506XX_onoff_set(pcf50623_global,PCF506XX_REGULATOR_D6REG,PCF506XX_REGU_OFF); //HPH OFF
			#endif
			HS_MIC = 0;
			#ifdef HS_MIC_DETECT
			HS_STATE = 0;
			#endif
		}
	}
	else
	{
		//if(pcf50623_global->accessory.headset_plugged == 0)
		{
			//#ifdef ACER_PM_DEBUG
			printk("GPIO_A17 = 0, Headset plugged\n");
			//#endif
			//input_event(pcf50623_global->input_dev, EV_SW, SW_HEADPHONE_INSERT, 1);
			if(HS_FIX)
			{
				old_hs_state = 0;
				pcf50623_global->accessory.headset_plugged = HS_FIX;
			}
			else
			{
				pcf50623_global->accessory.headset_plugged = 1;
			}
			reg_write(pcf50623_global, PCF50623_REG_RECC1, 0xA0); //enable hook key detect
			/* ACER BobIHLee@20100505, support AS1 project*/
			#if (defined ACER_L1_K2) || (defined ACER_L1_K3) || (defined ACER_L1_AS1)
			/* End BobIHLee@20100505*/
   			pcf506XX_onoff_set(pcf50623_global,PCF506XX_REGULATOR_D6REG,PCF506XX_REGU_ON); //HPH ON
			#endif
			#ifdef HS_MIC_DETECT
			HS_BOOT_FIRST = 0;
			#endif
		}
	}

	if(old_hs_state != pcf50623_global->accessory.headset_plugged)
	{
#ifdef HS_MIC_DETECT
		if((!pcf50623_global->accessory.headset_plugged) || HS_FIX)
#endif
		{
			HS_FIX=0;
#ifdef CONFIG_SWITCH
		switch_set_state(&pcf50623_global->accessory.swheadset, pcf50623_global->accessory.headset_plugged);
#endif
		}
/* ACER Erace.Ma@20100209, add headset power amplifier in K2*/
#if defined (ACER_L1_K2) || defined (ACER_L1_AS1)
		//Selwyn 2010-07-12 modified for K2 Popnoise[SYScs43005]
		//pnx_gpio_write_pin(pcf50623_global->pdata->hs_amp, pcf50623_global->accessory.headset_plugged);
		//~Selwyn modified
#elif defined (ACER_L1_K3)
		hs_power_switch(pcf50623_global->accessory.headset_plugged);
#endif
/* End Erace.Ma@20100209*/
	}

	if(hs_irq != -1)
	{
		pnx_gpio_clear_irq(hs_irq);
		enable_irq(hs_irq);
	}

	hs_detect_wait = 0;
}

static int headset_detect_handler(int irq, void *dev_id)
{
	#ifdef ACER_PM_DEBUG
	printk("headset_detect_handler is called\n");
	#endif
	hs_irq = irq;

	if(!hs_detect_wait)
	{
		hs_detect_wait = 1;
		disable_irq(irq);
		/* OCL - Use PMU own queue */
		//schedule_delayed_work(&pcf50623_global->delay_work, 1.5 * HZ);
		queue_delayed_work(pcf50623_global->workqueue, &pcf50623_global->delay_work, 1 * HZ);
	}

	return IRQ_HANDLED;
}
#endif
//~Selwyn modified

#ifdef CONFIG_I2C_NEW_PROBE
static int pcf50623_probe(struct i2c_client *new_client, const struct i2c_device_id *id)
{
#else
static int pcf50623_detect(struct i2c_adapter *adapter, int address, int kind)
{
	struct i2c_client *new_client;
#endif
	struct pcf50623_data *data;
	int err = 0;
	int irq;
    int pmu_int = 0;
    u_int8_t pmu_id = 0;

	dbgl1("entering\n");
	if (!pcf50623_pdev) {
		printk(KERN_ERR "pcf50623: driver needs a platform_device!\n");
		return -EIO;
	}

	irq = platform_get_irq(pcf50623_pdev, 0);
	if (irq < 0) {
		dev_err(&pcf50623_pdev->dev, "no irq in platform resources!\n");
		return -EIO;
	}

	/* At the moment, we only support one PCF50623 in a system */
	if (pcf50623_global) {
		dev_err(&pcf50623_pdev->dev,
			"currently only one chip supported\n");
		return -EBUSY;
	}

	if (!(data = kzalloc(sizeof(*data), GFP_KERNEL)))
		return -ENOMEM;

	mutex_init(&data->lock);
	INIT_WORK(&data->work, pcf50623_work);
#ifdef VREG
	INIT_WORK(&data->work_enable_irq,pcf50623_irq_enable);
#endif
	//Selwyn modified for headset detect
#if defined (ACER_L1_CHANGED)
	INIT_DELAYED_WORK(&data->delay_work, hs_detect_work);
#endif
	//~Selwyn modified
	data->workqueue = create_workqueue("pcf50623_wq");
	data->irq = irq;
	data->working = 0;
	data->onkey_seconds = 0;

	data->pdata = pcf50623_pdev->dev.platform_data;

#ifdef CONFIG_I2C_NEW_PROBE
	i2c_set_clientdata(new_client, data);
	data->client = new_client;
#else
	new_client = &data->client;
	i2c_set_clientdata(new_client, data);
	new_client->addr = address;
	new_client->adapter = adapter;
	new_client->driver = &pcf50623_driver;
	new_client->flags = 0;
	strlcpy(new_client->name, "pnx-pmu", I2C_NAME_SIZE);
#endif
#ifdef CONFIG_SWITCH
    /* register headset switch */
	data->accessory.swheadset.name = "h2w";
	data->accessory.swheadset.print_name = pcf50623_print_switch_headset_name;
	data->accessory.swheadset.print_state = pcf50623_print_switch_headset_state;
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
	pcf50623_global = data;

	populate_sysfs_group(data);

	err = sysfs_create_group(&new_client->dev.kobj, &pcf50623_attr_group);
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

	data->input_dev->name = "PNX-PMU";
	data->input_dev->phys = "input1";
	data->input_dev->id.bustype = BUS_I2C;
	data->input_dev->dev.parent = &new_client->dev;

    /* Event EV_SW is used to forward Headset plugged detection */
	data->input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_PWR) | BIT(EV_SW);

    /* Use KEY_STOP, KEY_MUTE */
	//Selwyn marked 20090615
	set_bit(KEY_STOP, data->input_dev->keybit);
	//~Selwyn marked
	set_bit(KEY_MUTE, data->input_dev->keybit);
	set_bit(KEY_POWER2, data->input_dev->keybit);
	set_bit(KEY_BATTERY, data->input_dev->keybit);

    /* Use SW_HEADPHONE_INSERT */
	set_bit(SW_HEADPHONE_INSERT, data->input_dev->swbit);

	//selwyn modified for AU3 hall sensor
	#ifdef ACER_L1_AU3
	set_bit(SW_LID, data->input_dev->swbit);
	#endif
	//~selwyn modified

    /* /dev/input/eventX, X is automatically incremented by the kernel */
	if (input_register_device(data->input_dev))
		goto exit_sysfs;
#ifdef VREG
	data->irq_pending=0;
	data->vregdesc=vreg_alloc(data,"PMU",0,0xff,pcf5062x_reg_nwrite,pcf5062x_reg_nread);
	if  (!data->vregdesc)
		goto exit_input;
	if (pcf50623_reg_configure(data->vregdesc)) 
		goto exit_input;
	data->irqusercallback=0;
#endif 
    /* The PMU/GPIO section must be reside after memory for
	pcf50623_global has been allocated.*/
	if (pcf50623_global) {
		pcf50623_global->bank.reserved_map = 0;
		pcf50623_global->bank.chip.dev = &new_client->dev;
		spin_lock_init(&pcf50623_global->bank.lock);
		pcf50623_global->bank.chip.request = pnx_gpio_acquire;
		pcf50623_global->bank.chip.free = pnx_gpio_release;
		pcf50623_global->bank.chip.direction_input = pmugpio_input;
		pcf50623_global->bank.chip.get = pmugpio_get;
		pcf50623_global->bank.chip.direction_output = pmugpio_output;
		pcf50623_global->bank.chip.set = pmugpio_set;
		pcf50623_global->bank.chip.to_irq = pmugpio_to_irq;
		pcf50623_global->bank.chip.label = "PMU/gpio";
		pcf50623_global->bank.chip.base = PMU_GPIO1;
		pcf50623_global->bank.chip.ngpio = 6;

		gpiochip_add(&pcf50623_global->bank.chip);
	}
	else
		return -ENOMEM;

	/* configure PMU irq */
	/* register GPIO in input */
	gpio_request(EXTINT_TO_GPIO(irq), pcf50623_pdev->name);
	pnx_gpio_set_mode(EXTINT_TO_GPIO(irq), GPIO_MODE_MUX0);
	gpio_direction_input(EXTINT_TO_GPIO(irq));

	if (gpio_request(pcf50623_global->pdata->usb_suspend_gpio, pcf50623_pdev->name)!=0)
	{
		printk(KERN_ERR "Can't get USB suspend GPIO\n");
		goto exit_irq; 
	}
	gpio_direction_output(pcf50623_global->pdata->usb_suspend_gpio, 0);

    /* Read device ID */
    pmu_id = reg_read(data, PCF50623_REG_ID);

	/* Register PMU as RTC device to manage rtc */
	if (data->pdata->used_features & PCF506XX_FEAT_RTC) {
		data->rtc = rtc_device_register("pnx-pmu", &new_client->dev,
						&pcf50623_rtc_ops, THIS_MODULE);
		if (IS_ERR(data->rtc)) {
			err = PTR_ERR(data->rtc);
			goto exit_gpio;
		}
	}

	if (data->pdata->used_features & PCF506XX_FEAT_KEYPAD_BL) {
//		data->keypadbl = backlight_device_register("keyboard-backlight",
/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify sd card detecting to fix pmu ic bug on PCR { */
	#if defined (ACER_AU2_PCR) || defined (ACER_L1_K3)
		data->keypadbl = backlight_device_register("button-bl",
							    &new_client->dev,
							    data,
							    &pcf50623_kbl_ops);
	#else
		data->keypadbl = backlight_device_register("keypad-bl",
							    &new_client->dev,
							    data,
							    &pcf50623_kbl_ops);
	#endif
/* } ACER Jen Chang, 2009/09/02*/

		if (!data->keypadbl)
			goto exit_rtc;
		/* FIXME: are we sure we want default == off? */
		data->keypadbl->props.max_brightness = 1;
		data->keypadbl->props.power = FB_BLANK_UNBLANK;
		data->keypadbl->props.fb_blank = FB_BLANK_UNBLANK;
		data->keypadbl->props.brightness =0;
		backlight_update_status(data->keypadbl);
		//selwyn modified
		//pcf50623_kbl_set_intensity(data->keypadbl);
		//~selwyn
	}

//Selwyn 2010-2-25 marked because ACER don't use it
//#ifdef CONFIG_HAS_EARLYSUSPEND
//	register_early_suspend(&pcf50623_kbl_earlys);
//#endif

/* ACER Jen chang, 2009/07/29, IssueKeys:AU4.B-137, Creat device for controlling charger indicated led { */
	if (data->pdata->used_features & PCF506XX_FEAT_CHG_BL) {
		data->chargerbl = backlight_device_register("charger-bl",
							    &new_client->dev,
							    data,
							    &pcf50623_chg_ops);
		if (!data->chargerbl)
			goto exit_bl;
		/* FIXME: are we sure we want default == off? */
		data->chargerbl->props.max_brightness = 1;
		data->chargerbl->props.power = FB_BLANK_UNBLANK;
		data->chargerbl->props.fb_blank = FB_BLANK_UNBLANK;
		/* ACER Bright Lee, 2010/2/28, Power Off Charging LED { */
		if (getOnCause() == 2) {
			data->chargerbl->props.brightness = 1;
		} else {
		data->chargerbl->props.brightness = 0;
		}
		/* } ACER Bright Lee, 2010/2/28 */
		backlight_update_status(data->chargerbl);
	}
/* } ACER Jen Chang, 2009/07/29 */
	
/* ACER Jen chang, 2009/06/24, IssueKeys:AU2.B-38, Implement vibrator device driver for HAL interface function { */
	if (data->pdata->used_features & PCF506XX_FEAT_VIBRATOR) {
		data->vibrator = vibrator_device_register("pnx-vibrator",
							    &new_client->dev,
							    data,
							    &pcf50623_vibrator_ops);
		if (!data->vibrator)
			goto exit_chg_bl;
		/* FIXME: are we sure we want default == off? */
		data->vibrator->name = "vibrator control";

		INIT_WORK(&(data->vibrator->props.work_vibrator_on), pmu_vibrator_on);
		INIT_WORK(&(data->vibrator->props.work_vibrator_off), pmu_vibrator_off);

		hrtimer_init(&data->vibrator->props.vibrator_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		data->vibrator->props.vibrator_timer.function = vibrator_timer_func;
	}
/* } ACER Jen Chang, 2009/06/24*/

	/* Register PMU as dev device */
/* ACER Jen chang, 2009/06/24, IssueKeys:AU2.B-38, Implement vibrator device driver for HAL interface function { */
	major = register_chrdev(0, "pmu", &pcf50623_pmu_ops);

	if( major <0 ) {
		printk(KERN_ERR "Not able to register_chrdev: %i\n",major);
        goto exit_vibrator;
    };
/* } ACER Jen Chang, 2009/06/24*/
	printk("PMU dev device Major = %u\n",major);

    /* Set PMU register in correct initial state */
    if (pcf50623_global->pcf50623_itcallback==NULL)
        pcf50623_global->pcf50623_itcallback=pcf50623_void;

	if (pcf50623_global->pcf50623_usbcallback==NULL)
	{
	    pcf50623_global->pcf50623_usbcallback=pcf50623_usb_void;
	}
/* ACER Jen chang, 2009/06/17, IssueKeys:AU4.B-43, Modify sd card detecting for different compiler option { */
/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify sd card detecting to fix pmu ic bug on PCR { */
#if (defined ACER_AU2_PR1) || (defined ACER_AU2_PR2) || (defined ACER_AU4_PR1) || (defined ACER_AU4_PR2) 
	if (pcf50623_global->pcf50623_sddetectcallback==NULL)
	{
	    pcf50623_global->pcf50623_sddetectcallback=pcf506XX_sddetect_void;
	}
#endif
/* } ACER Jen Chang, 2009/09/02*/
/* } ACER Jen Chang, 2009/06/17*/
    
    if (pcf50623_onkey==NULL)
       pcf50623_onkey=pcf50623_void;

	pcf50623_init_registers(data);

    pnx6708_pm_dev.dev.parent = &new_client->dev;
    platform_device_register(&pnx6708_pm_dev);
    pm_power_off=pcf50623_poweroff;
    /* initialize irq  */
	pmu_int = gpio_get_value(EXTINT_TO_GPIO(irq));

    printk(KERN_INFO "PCF50623: driver ID is:%u (0x%02X)\nPCF50623: EXTINT is at %u\n",pmu_id,pmu_id,pmu_int);

    /* Configure irq for PNX */
	err = request_irq(irq, pcf50623_irq, 0, "pnx-pmu", data);
	if (err < 0)
	{
        printk(KERN_INFO "PCF50623 : request irq failed\n");
		goto exit_input;
	}
#ifdef VREG
 	set_irq_type(irq, IRQ_TYPE_LEVEL_LOW);
#endif 

	//Selwyn modified for headset detect
#if defined (ACER_L1_CHANGED)
	err = gpio_request(EXTINT_TO_GPIO(data->pdata->hs_irq),MODULE_NAME);
	pnx_gpio_set_mode_gpio(EXTINT_TO_GPIO(data->pdata->hs_irq));
	gpio_direction_input(EXTINT_TO_GPIO(data->pdata->hs_irq));
	set_irq_type(data->pdata->hs_irq, IRQ_TYPE_EDGE_BOTH);
	err |= request_irq(data->pdata->hs_irq, &headset_detect_handler, 0, "headset_detect", NULL);
	if(err < 0)
	{
		printk("PMU: request GPIO_%c%d pin as headset detect irq fail\n", 
			'A'+EXTINT_TO_GPIO(data->pdata->hs_irq)/32, EXTINT_TO_GPIO(data->pdata->hs_irq)%32);
	}
#endif
	//Selwyn modified

/* ACER Erace.Ma@20100209, add headset power amplifier in K2*/
#if (defined ACER_L1_K2) || (defined ACER_L1_AS1)
	//Selwyn 2010-07-12 modified for K2 Popnoise[SYScs43005]
	//err = pnx_gpio_request(data->pdata->hs_amp);
	//pnx_gpio_set_mode_gpio(data->pdata->hs_amp);
	//pnx_gpio_set_direction(data->pdata->hs_amp, GPIO_DIR_OUTPUT);
	//~Selwyn modified
#endif
/* End Erace.Ma@20100209*/
	return 0;

/* ACER Jen chang, 2009/06/24, IssueKeys:AU2.B-38, Implement vibrator device driver for HAL interface function { */
exit_vibrator:
	if (data->pdata->used_features & PCF506XX_FEAT_VIBRATOR)
		backlight_device_unregister(pcf50623_global->vibrator);
/* } ACER Jen Chang, 2009/06/24*/
/* ACER Jen chang, 2009/07/29, IssueKeys:AU4.B-137, Creat device for controlling charger indicated led { */
exit_chg_bl:
	if (data->pdata->used_features & PCF506XX_FEAT_CHG_BL)
		backlight_device_unregister(pcf50623_global->chargerbl);
/* } ACER Jen Chang, 2009/07/29*/
exit_bl:
	if (data->pdata->used_features & PCF506XX_FEAT_KEYPAD_BL)
		backlight_device_unregister(pcf50623_global->keypadbl);
exit_rtc:
	if (data->pdata->used_features & PCF506XX_FEAT_RTC)
		rtc_device_unregister(pcf50623_global->rtc);
exit_irq:
	free_irq(pcf50623_global->irq, pcf50623_global);
exit_gpio:
	gpio_free(pcf50623_global->pdata->usb_suspend_gpio);
exit_input:
	input_unregister_device(data->input_dev);
exit_sysfs:
/* 	pm_power_off = NULL; */
	sysfs_remove_group(&new_client->dev.kobj, &pcf50623_attr_group);
#ifndef CONFIG_I2C_NEW_PROBE
exit_detach:
	i2c_detach_client(new_client);
#endif
exit_free:
	kfree(data);
	pcf50623_global = NULL;
	return err;
}

#ifdef CONFIG_I2C_NEW_PROBE
static int __devexit pcf50623_remove(struct i2c_client *client)
#else
static int pcf50623_attach_adapter(struct i2c_adapter *adapter)
{
	dbgl1("entering, calling i2c_probe\n");
	return i2c_probe(adapter, &addr_data, &pcf50623_detect);
}

static int pcf50623_detach_client(struct i2c_client *client)
#endif
{
	struct pcf50623_data *pcf = i2c_get_clientdata(client);

	dbgl1("entering\n");

/* 	apm_get_power_status = NULL; */

	free_irq(pcf->irq, pcf);

	input_unregister_device(pcf->input_dev);

#ifdef CONFIG_SWITCH
	switch_dev_unregister(&pcf->accessory.swheadset);
#endif

//Selwyn 2010-2-25 marked because ACER don't use it
//#ifdef CONFIG_HAS_EARLYSUSPEND
//	register_early_suspend(&pcf50623_kbl_earlys);
//#endif

    unregister_chrdev(major,"pmu");

	if (pcf->pdata->used_features & PCF506XX_FEAT_RTC)
		rtc_device_unregister(pcf->rtc);

	platform_device_unregister(&pnx6708_pm_dev);

	sysfs_remove_group(&client->dev.kobj, &pcf50623_attr_group);

	pm_power_off = NULL;
	
#ifndef CONFIG_I2C_NEW_PROBE
	i2c_detach_client(client);
#endif
	kfree(pcf);

	return 0;
}

#define pcf50623_suspend NULL
#define pcf50623_resume NULL

#ifdef CONFIG_I2C_NEW_PROBE
static const struct i2c_device_id pcf50623_id[] = {
	{ "pcf50623", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pcf50623_id);

static struct i2c_driver pcf50623_driver = {
	.driver = {
		.name	= "pcf50623",
		.owner  = THIS_MODULE,
	},
	.id_table = pcf50623_id,
	.probe	  = pcf50623_probe,
	.remove	  = __devexit_p(pcf50623_remove),
	.suspend= pcf50623_suspend,
	.resume	= pcf50623_resume,
};
#else
static struct i2c_driver pcf50623_driver = {
	.driver = {
		.name	= "pnx-pmu",
		.suspend= pcf50623_suspend,
		.resume	= pcf50623_resume,
	},
	.id		= I2C_DRIVERID_PCF50623,
	.attach_adapter	= pcf50623_attach_adapter,
	.detach_client	= pcf50623_detach_client,
};
#endif

/* Add by Erace.Ma@20090922, add PMU early suspend for power consumption */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void pcf_earlysuspend(struct early_suspend *pes)
{
  int err;

  err = pcf506XX_onoff_set(pcf50623_global,PCF506XX_REGULATOR_D3REG,PCF506XX_REGU_ECO);
  /* ACER Owne chang, 2010/02/24, Modify for FM { */
  err = pcf506XX_onoff_set(pcf50623_global,PCF506XX_REGULATOR_D4REG,PCF506XX_REGU_ECO);
  /* } ACER Owen Chang, 2010/02/24*/
  /* Add by Bob.IH.Lee@20100702, IssueKeys:A21.B-1314, Don't need to control here, Change control ON/ECO to sensor driver {*/
  //err = pcf506XX_onoff_set(pcf50623_global,PCF506XX_REGULATOR_D5REG,PCF506XX_REGU_ECO);
  /* } Add by Bob.IH.Lee@20100702 */
  return;
}
static void pcf_lateresume(struct early_suspend *pes) 
{
  int err;

  err = pcf506XX_onoff_set(pcf50623_global,PCF506XX_REGULATOR_D3REG,PCF506XX_REGU_ON);
  /* ACER Owne chang, 2010/02/24, Modify for FM { */
  err = pcf506XX_onoff_set(pcf50623_global,PCF506XX_REGULATOR_D4REG,PCF506XX_REGU_ON);
  /* } ACER Owen Chang, 2010/02/24*/
  /* Add by Bob.IH.Lee@20100702, IssueKeys:A21.B-1314, Don't need to control here, Change control ON/ECO to sensor driver {*/
  //err = pcf506XX_onoff_set(pcf50623_global,PCF506XX_REGULATOR_D5REG,PCF506XX_REGU_ON);  
  /* } Add by Bob.IH.Lee@20100702 */
  return;
}

static struct early_suspend pcf50623_earlys = { 
    .level = EARLY_SUSPEND_LEVEL_MAX-1, 
    .suspend = pcf_earlysuspend,
    .resume = pcf_lateresume, 
}; 
#endif
/*End by Erace.Ma@20090922*/

/* platform driver, since i2c devices don't have platform_data */
static int __init pcf50623_plat_probe(struct platform_device *pdev)
{
	struct pcf50623_platform_data *pdata = pdev->dev.platform_data;

	if (!pdata)
		return -ENODEV;

	pcf50623_pdev = pdev;

/* Add by Erace.Ma@20090922, add PMU early suspend for power consumption */
#ifdef CONFIG_HAS_EARLYSUSPEND
  register_early_suspend(&pcf50623_earlys);
#endif
/*End by Erace.Ma@20090922*/
	return 0;
}

static int pcf50623_plat_remove(struct platform_device *pdev)
{
/* Add by Erace.Ma@20090922, add PMU early suspend for power consumption */
#ifdef CONFIG_HAS_EARLYSUSPEND
  unregister_early_suspend(&pcf50623_earlys);
#endif
/*End by Erace.Ma@20090922*/

	return 0;
}

static struct platform_driver pcf50623_plat_driver = {
	.probe	= pcf50623_plat_probe,
	.remove	= pcf50623_plat_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name 	= "pnx-pmu",
	},
};

static int __init pcf50623_init(void)
{
/* 	return register_rtc(&pcf50623_ops); */
	int rc;

	if (!(rc = platform_driver_register(&pcf50623_plat_driver)))
	{
		printk("platform_driver_register OK !!!\n");
		if (!(rc = i2c_add_driver(&pcf50623_driver)))
		{
			printk("i2c_add_driver OK !!!\n");
		}
		else
		{
			printk(KERN_ERR "i2c_add_driver failed\n");
			platform_driver_unregister(&pcf50623_plat_driver);
			return 	-ENODEV;
		}
	}
	else
	{
		printk("platform_driver_register Failed !!!\n");
	}

	return rc;
}

static void __exit pcf50623_exit(void)
{
/* 	unregister_rtc(&pcf50623_rtc_ops); */
	i2c_del_driver(&pcf50623_driver);
	platform_driver_unregister(&pcf50623_plat_driver);
}

module_init(pcf50623_init);
module_exit(pcf50623_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Olivier Clergeaud");
MODULE_DESCRIPTION("PCF50623 PMU driver");
