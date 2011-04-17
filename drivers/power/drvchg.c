/*
 *  linux/drivers/power/drvchg.c
 *
 *  STE CHARGE Power Management Unit (PMU) driver
 *
 *  Author:     Olivier Clergeaud
 *  Copyright:  ST-Ericsson (c) 2008
 *
 *  Based on the code of rtc.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
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
#include <linux/timer.h>

#include <linux/pcf506XX.h>

#include <linux/power_supply.h>
#include <nk/adcclient.h>

#include <mach/gpio.h>
#include <mach/usb.h>

#include <linux/drvchg.h>
#include "drvchg.h"

#include <linux/wakelock.h>
/*****************************************************************************
 * Module parameter
 *****************************************************************************/
static unsigned int major=0;
static unsigned int debug;
static struct class *drvchg_class = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "0->No debug, 1->Light debug, 2->Complete Debug");

#define DEVICE_NAME "drvchg"
/*****************************************************************************
 * Constant data
 *****************************************************************************/
#define CHARGE_FAST_TIMER_TEMP        HZ/5     // 200ms
#define CHARGE_NORM_TIMER_TEMP_CHARGE HZ       // 1s
#define CHARGE_NORM_TIMER_TEMP        60*HZ    // 60s
#define CHARGE_FAST_TIMER_VOLT        HZ/5     // 200ms
#define CHARGE_NORM_TIMER_VOLT        HZ       // 1s
#define CHARGE_DISC_TIMER_VOLT        30*HZ    // 30s
#define CHARGE_MAIN_CHG_MAX_TIME      10800*HZ // 180 min
#define CHARGE_USB_CHG_MAX_TIME       14400*HZ // 240 min
#define CHARGE_INIT_TIME              2*HZ     // 2s
#define CHARGE_TEMP_MIN_TO_CHARGE     -5000    // en mili °C
#define CHARGE_TEMP_MAX_TO_CHARGE     55000    // en mili °C
#define CHARGE_TEMP_MIN_TO_RESTART     0       // en mili °C
#define CHARGE_TEMP_MAX_TO_RESTART    50000    // en mili °C
//#define CHARGE_CURRENT_MIN            0x16
#define CHARGE_CURRENT_MIN            0x07

#define CHARGE_WORK_TEMP_FLAG 0x00000001
#define CHARGE_WORK_VOLT_FLAG 0x00000002
#define CHARGE_WORK_INIT_FLAG 0x00000004
#define CHARGE_BATTFULL_FLAG  0x00000008
#define CHARGE_MAIN_CHARGER   0x00000010
#define CHARGE_USB_CHARGER    0x00000020
#define CHARGE_THERMAL_LIMIT  0x00000040
#define CHARGE_CHARGING       0x00000100
/// +ACER_AUx PenhoYu, showe charge_itcallback for debug
#define CHARGE_ACER_INITED     0x10000000
/// -ACER_AUx PenhoYu, showe charge_itcallback for debug


//#define CHARGE_VOLTAGE_MAX_TO_CHARGE 4300     // en mV
#define CHARGE_VOLTAGE_MAX_TO_CHARGE 4500     // en mV
#define CHARGE_TLIM_VOLTAGE_RESTART  4000     // en mV

#define CHARGE_FEAT_TLIM 0x00000001

#define CHARGE_MAIN_CUR_LOW  0x19
#define CHARGE_MAIN_CUR_HIGH 0xBA
#define CHARGE_USB_CUR_LOW   0x19
#define CHARGE_USB_CUR_HIGH  0x71

#if defined (CONFIG_SENSORS_PCF50616)
	#define ICCSET					833
	#define TRICKLE_CURRENT_STEP	32
	#define TRICKLE_CURRENT_INIT	0x1F
#else
	#define ICCSET					1136
	#define TRICKLE_CURRENT_STEP	128
	#define TRICKLE_CURRENT_INIT	0x7F
#endif

#define DBG1 1
#define DBG2 2

/// +ACER_AUx PenhoYu, include the acer_drvchg define
#ifdef ACER_CHG_BAT_FUN
#include "acer_drvchg.h"

// control GPIO to select the Rccset
#include <mach/gpio.h>
#define CHG_CCCTL_GPIO		GPIO_D24		// GPIOD24

static void charge_itcallback(u_int16_t battInt);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, include the acer_drvchg define


/*****************************************************************************
 * Static data / structures
 *****************************************************************************/
struct strcharge {
	struct work_struct work_timer;
	unsigned int working_timer;
    u_int32_t features;
	u_int32_t flags;
    struct timer_list mainchgtimer;
    struct timer_list usbchgtimer;
    struct timer_list scantemptimer;
    struct timer_list scanvolttimer;
    struct timer_list inittimer;
    struct {
        long average;
        u_int8_t measurenb;
        u_int8_t suspended;
    } temp;
    struct {
        ulong average;
        ulong averageComp;
        u_int8_t measurenb;
        int16_t mvoltComp;
    } volt;
	iconmgt BattIconMgt;
	void (* Online)(unsigned int);
	u_int8_t tatmode;
	struct wake_lock	charge_main_lock;
	struct wake_lock	charge_usb_lock;
};

struct strcharge * charge;
static int8_t trickleCurrent=TRICKLE_CURRENT_INIT;
//static iconmgt charge_BattIconMgt;
/// +ACER_AUx PenhoYu, include the acer_drvchg define
#ifdef ACER_VBAT_OPCOMP
static int16_t * devCompVolt;
#else	// ACER_VBAT_OPCOMP
static int16_t devCompVolt[DRVCHG_MAX];
#endif	// ACER_VBAT_OPCOMP
/// -ACER_AUx PenhoYu, include the acer_drvchg define
static int16_t usb_max_current=0;
static int16_t pmu_usb_max_current=0;
static int16_t main_max_current=ICCSET*(CHARGE_MAIN_CUR_HIGH+1)/255;
static int16_t max_current=0;
static int8_t  power_supply=0;

/*****************************************************************************
 * Debug function
 *****************************************************************************/
#define drvchg_dbg(level, fmt, args...)			\
	do {										\
	if (debug >= level)							\
		printk(KERN_DEBUG DEVICE_NAME": %-25s:" fmt,__FUNCTION__, ## args);  \
	} while (0)
/*****************************************************************************
 * function
 *****************************************************************************/
static void charge_schedule_work_timer(struct strcharge *);
static void charge_stopMainCharge(void);

/*****************************************************************************
 * Charge Management, cf spec VYn_ps18857
 *****************************************************************************/
void drvchg_addCompensation(u_int8_t type, int16_t mvolts)
{
    
    devCompVolt[type]=mvolts;
}

static void charge_battfullCbk(void)
{
    if (charge->flags & CHARGE_BATTFULL_FLAG)
        drvchg_dbg(DBG1,"End of charge detected for the user, ignore BATTFULL interrupt, PMU is in AUTO-res mode\n");
    else
    {
        drvchg_dbg(DBG1,"Battery Full detected, decrease trickle current\n");
        if ( trickleCurrent>0 )
            trickleCurrent--;
		drvchg_dbg(DBG1,"Battery current = %02umA\n",
				ICCSET*(trickleCurrent+1)/TRICKLE_CURRENT_STEP);
        pcf506XX_setTrickleChgCur(trickleCurrent);

        if (trickleCurrent<=(CHARGE_CURRENT_MIN))
        {
	        drvchg_dbg(DBG1,"I trickle falls below Imin. Enable Auto-Stop feature\n");
            pcf506XX_setTrickleChgCur(CHARGE_CURRENT_MIN);
            pcf506XX_setAutoStop(1);
            charge->flags |= CHARGE_BATTFULL_FLAG;
	        charge->BattIconMgt(1, power_supply, charge->volt.averageComp,
					(int8_t)(charge->temp.average/1000), 0, max_current);
        }
    }
}

static void charge_cccvCbk(void)
{
    /* Charger swtiched from constant current to constant voltage */
    /* Measure current value */
    int8_t cur=trickleCurrent;
    u_int8_t ilimit;

    pcf506XX_enableBattfullInt();
    
    if (charge->flags & CHARGE_BATTFULL_FLAG)
	{
		drvchg_dbg(DBG1,"End of charge detected for the user, ignore VMAX interrupt, PMU is in AUTO-res mode\n");
		drvchg_dbg(DBG1,"Stop all charge timers\n");
		del_timer(&charge->mainchgtimer);
		del_timer(&charge->usbchgtimer);
	    del_timer(&charge->scantemptimer);
/// +ACER_AUx PenhoYu, keep the battery voltage monitor
#ifndef ACER_CHG_BAT_FUN
		del_timer(&charge->scanvolttimer);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, keep the battery voltage monitor
        charge->BattIconMgt(1, power_supply, charge->volt.averageComp, (int8_t)(charge->temp.average/1000), (u_int16_t)(ICCSET*(trickleCurrent+1)/TRICKLE_CURRENT_STEP), max_current);		
	}
    else
    {
        drvchg_dbg(DBG1,"Vmax detected, decrease trickle current\n");
        do {
            pcf506XX_setTrickleChgCur((u_int8_t)cur);
            ilimit = pcf506XX_getIlimit();
            if (ilimit==0) {
				drvchg_dbg(DBG1,"Battery current = %02umA\n",ICCSET*(cur+1)/TRICKLE_CURRENT_STEP);
				trickleCurrent=cur;
                cur=0;
            }
            
            cur --;
        } while (cur>0);
    }

/// +ACER_AUx PenhoYu, IssueKeys:A43.B-1871 enable 1sec check battery id timer.
#ifdef ACER_CHG_BAT_FUN
    g_chg_flag |= CHGF_CVCHG;
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, IssueKeys:A43.B-1871 enable 1sec check battery id timer.
}

static void charge_chgRes(void)
{
	if (charge->flags & CHARGE_BATTFULL_FLAG)
		drvchg_dbg(DBG2,"\n");

}
static void charge_mainChgtimeout(unsigned long unused)
{
    drvchg_dbg(DBG1,"Main Charge timer expired. GOOD WORK !!!\n");

/// +ACER_AUx PenhoYu, 6hr AC chargeing watchdog timeout
#ifdef ACER_CHG_BAT_FUN
    printk(KERN_INFO "<CHG> AC watchdog timeout !!!\n");
    if ((!showdbglv(DBGFLAG_MONKEY)) && (!(g_chg_flag & CHGF_CVCHG))) {
    	ctrlCharge(0, CHGF_P_WDT);
    }
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, 6hr AC chargeing watchdog timeout
}

static void charge_usbChgtimeout(unsigned long unused)
{
	drvchg_dbg(DBG1,"Charge USB Charge timer expired. GOOD WORK !!!\n");
}

static void charge_scanTemptimeout(unsigned long data)
{
    struct strcharge * pcf = (struct strcharge *)data;

    pcf->flags |= CHARGE_WORK_TEMP_FLAG;

    charge_schedule_work_timer(pcf);
}

static void charge_scanVolttimeout(unsigned long data)
{
    struct strcharge * pcf = (struct strcharge *)data;

    pcf->flags |= CHARGE_WORK_VOLT_FLAG;

    charge_schedule_work_timer(pcf);
}

static void charge_inittimeout(unsigned long data)
{
    struct strcharge * pcf = (struct strcharge *)data;

    pcf->flags |= CHARGE_WORK_INIT_FLAG;

    charge_schedule_work_timer(pcf);
}

static void charge_stopMainCharge(void)
{
    drvchg_dbg(DBG1,"Stop Main charge.\n");
    pcf506XX_main_charge_enable(0);
    pcf506XX_disableBattfullInt();
    charge->temp.measurenb = 0;
    charge->volt.measurenb = 0;
	charge->flags &= ~CHARGE_BATTFULL_FLAG;
	charge->flags &= ~CHARGE_CHARGING;
//    del_timer(&charge->scanvolttimer);
    del_timer(&charge->scantemptimer);
    del_timer(&charge->mainchgtimer);
	/* unlock */
	wake_unlock(&(charge->charge_main_lock));
}

static void charge_stopUsbCharge(void)
{
    drvchg_dbg(DBG1,"Stop USB charge.\n");
    pcf506XX_usb_charge_enable(0);
    pcf506XX_disableBattfullInt();
    charge->temp.measurenb = 0;
    charge->volt.measurenb = 0;
	charge->flags &= ~CHARGE_BATTFULL_FLAG;
	charge->flags &= ~CHARGE_CHARGING;
    del_timer(&charge->usbchgtimer);
    del_timer(&charge->scantemptimer);
//    del_timer(&charge->scanvolttimer);

	/* unlock */
	wake_unlock(&(charge->charge_usb_lock));
}

static void charge_suspendCharge(void)
{
    drvchg_dbg(DBG1,"Charge suspended due to temperature or voltage out of range.\n");
    if ( charge->flags & CHARGE_MAIN_CHARGER )
        charge_stopMainCharge();
    else
        if ( charge->flags & CHARGE_USB_CHARGER )
            charge_stopUsbCharge();

    /* Reset Voltage Table */
    charge->volt.measurenb = 0;
    
    charge->scantemptimer.expires = round_jiffies(jiffies + CHARGE_NORM_TIMER_TEMP);
    charge->temp.suspended = 1;
}

static void charge_startMainCharge(void)
{
    drvchg_dbg(DBG1,"Start charge by Main charger\n");

	/*  Forbid full suspend during charge  */
	wake_lock(&(charge->charge_main_lock));

	max_current = main_max_current;
    charge->temp.measurenb = 0;
    charge->volt.measurenb = 0;

	charge->flags |= CHARGE_CHARGING;

	/* Charge current is limited by PMU, conpute trickle current to match it */
    trickleCurrent=(TRICKLE_CURRENT_STEP*CHARGE_MAIN_CUR_HIGH/255);
    pcf506XX_usb_charge_enable(0);
    pcf506XX_main_charge_enable(1);
    pcf506XX_setAutoStop(0);
    pcf506XX_setVmax(PCF506XX_VMAX_4V24);
    pcf506XX_setMainChgCur(CHARGE_MAIN_CUR_LOW);
    pcf506XX_setTrickleChgCur(trickleCurrent);
    
    charge->temp.suspended = 0;
    
    /* Start Max time charge timer */
    if ( !timer_pending(&charge->mainchgtimer) ) {
        init_timer(&charge->mainchgtimer);
        charge->mainchgtimer.function=charge_mainChgtimeout;
        charge->mainchgtimer.expires = round_jiffies(jiffies + CHARGE_MAIN_CHG_MAX_TIME);
        add_timer(&charge->mainchgtimer);
    }

    /* Start scan temperature timer */
    if ( !timer_pending(&charge->scantemptimer) ) {
        init_timer(&charge->scantemptimer);
        charge->scantemptimer.function=charge_scanTemptimeout;
        charge->scantemptimer.expires = jiffies + CHARGE_FAST_TIMER_TEMP;
        charge->scantemptimer.data= (u_long)charge;
        add_timer(&charge->scantemptimer);
    }

    /* Start scan voltage timer */
    if ( !timer_pending(&charge->scanvolttimer) ) {
        init_timer(&charge->scanvolttimer);
        charge->scanvolttimer.function=charge_scanVolttimeout;
        charge->scanvolttimer.expires = jiffies + CHARGE_FAST_TIMER_VOLT;
        charge->scanvolttimer.data= (u_long)charge;
        add_timer(&charge->scanvolttimer);
    }
	else
	{
		mod_timer(&charge->scanvolttimer, jiffies + CHARGE_FAST_TIMER_VOLT);
	}

}

static void charge_startUsbCharge(void)
{
    if ( charge->flags & CHARGE_MAIN_CHARGER ) {
        drvchg_dbg(DBG1,"Start by main charger already in progress, don't use USB\n");
        return;
    }

	drvchg_dbg(DBG1,"Start charge by USB charger\n");

	/*  Forbid full suspend during charge  */
	wake_lock(&(charge->charge_usb_lock));

	/* Get USB max current defined for USB enumeration */
	usb_max_current = get_usb_max_current();
	max_current = usb_max_current;
    charge->temp.measurenb = 0;
    charge->volt.measurenb = 0;

	charge->flags |= CHARGE_CHARGING;

	/* convert USB max current for PMU usage */
	pmu_usb_max_current = (usb_max_current * 255) / ICCSET;
	
    trickleCurrent=(TRICKLE_CURRENT_STEP*pmu_usb_max_current/255);
    pcf506XX_usb_charge_enable(1);
    pcf506XX_setAutoStop(0);
    pcf506XX_setVmax(PCF506XX_VMAX_4V24);
    pcf506XX_setUsbChgCur(CHARGE_USB_CUR_LOW);
    pcf506XX_setTrickleChgCur(trickleCurrent);
        
    /* Start Max Charge timer */
    if ( !timer_pending(&charge->usbchgtimer) ) {
        init_timer(&charge->usbchgtimer);
        charge->usbchgtimer.function=charge_usbChgtimeout;
        charge->usbchgtimer.expires = round_jiffies(jiffies + CHARGE_USB_CHG_MAX_TIME);
        add_timer(&charge->usbchgtimer);
    }
    
    /* Start scan temperature timer */
    if ( !timer_pending(&charge->scantemptimer) ) {
        init_timer(&charge->scantemptimer);
        charge->scantemptimer.function=charge_scanTemptimeout;
        charge->scantemptimer.expires = jiffies + CHARGE_FAST_TIMER_TEMP;
        charge->scantemptimer.data= (u_long)charge;
        add_timer(&charge->scantemptimer);
    }

    /* Start scan voltage timer */
    if ( !timer_pending(&charge->scanvolttimer) ) {
        init_timer(&charge->scanvolttimer);
        charge->scanvolttimer.function=charge_scanVolttimeout;
        charge->scanvolttimer.expires = jiffies + CHARGE_FAST_TIMER_VOLT;
        charge->scanvolttimer.data= (u_long)charge;
        add_timer(&charge->scanvolttimer);
    }
	else
	{
		mod_timer(&charge->scanvolttimer, jiffies + CHARGE_FAST_TIMER_VOLT);
	}
}

static void charge_computeVoltageComp(int8_t temp)
{
    if ( temp < -10 ) {
        charge->volt.mvoltComp = -34;
    } else if ( temp < -5 ) {
        charge->volt.mvoltComp = -23;
    } else if ( temp < 0 ) {
        charge->volt.mvoltComp = -15;
    } else if ( temp < 5 ) {
        charge->volt.mvoltComp = -7;
    } else if ( temp < 10 ) {
        charge->volt.mvoltComp = -4;
    } else if ( temp < 15 ) {
        charge->volt.mvoltComp = -4;
    } else if ( temp < 20 ) {
        charge->volt.mvoltComp = 0;
    } else if ( temp < 25 ) {
        charge->volt.mvoltComp = 0;
    } else if ( temp < 30 ) {
        charge->volt.mvoltComp = 4;
    } else {
        charge->volt.mvoltComp = 4;
    }
}

static void charge_adc_temperature(ulong temp)
{
    static ulong tempx[4];
    static u_int8_t tempindex=0;
	
	tempx[tempindex]=(temp/1000);
	drvchg_dbg(DBG2,"Current temperature = %02lu.%02lu°C\n",
			tempx[tempindex]/1000,tempx[tempindex]%1000);
    tempindex = (tempindex+1)%4;
    charge->temp.measurenb += 1;

    if ( charge->temp.measurenb >= 4 ) {
        charge->temp.measurenb = 4;
        charge->temp.average=(tempx[0]+tempx[1]+tempx[2]+tempx[3])/4;
    }
}

static void charge_adc_voltage(ulong volt)
{
    static ulong voltx[16];
	static u_int8_t voltindex=0;
    u_int32_t volttmp=0;
    u_int8_t i;

    /* Prevent from getting wrong measure */
    if (volt>5000000)
        return;
    
    voltx[voltindex]=(volt/1000);
	drvchg_dbg(DBG2,"Current battery voltage = %lumV\n",voltx[voltindex]);
    charge->volt.measurenb += 1;

	/* If current state is charging, average on 16 measures */
	if ( charge->flags & CHARGE_CHARGING )
	{
	    voltindex = (voltindex+1)%16;
		if ( charge->volt.measurenb >= 16 ) {
		    charge->volt.measurenb = 16;
			for ( i=0; i<16; i++ )
				volttmp += voltx[i];

	        charge->volt.average=volttmp/16;

		    /* Add temp voltage compensation */
			charge->volt.averageComp = charge->volt.average + charge->volt.mvoltComp;

	        /* Add device voltage compensation */
		    for (i=0; i<DRVCHG_MAX; i++)
			    charge->volt.averageComp += devCompVolt[i];
	    }
	} 
	else
	/* Else , average on 4 measures */
	{
	    voltindex = (voltindex+1)%4;
		if ( charge->volt.measurenb >= 4 ) {
		    charge->volt.measurenb = 4;
			for ( i=0; i<4; i++ )
				volttmp += voltx[i];

	        charge->volt.average=volttmp/4;

		    /* Add temp voltage compensation */
			charge->volt.averageComp = charge->volt.average + charge->volt.mvoltComp;

	        /* Add device voltage compensation */
		    for (i=0; i<DRVCHG_MAX; i++)
			    charge->volt.averageComp += devCompVolt[i];
	    }
	}
}

/***********************************************************************
 * Interruption Management
 ***********************************************************************/
static void charge_work_timer(struct work_struct *work_timer)
{
	struct strcharge *pcf = container_of(work_timer, struct strcharge, work_timer);
    int res;
    pcf->working_timer = 1;

    if ( pcf->flags & CHARGE_WORK_INIT_FLAG ) {
        pcf->flags &= ~CHARGE_WORK_INIT_FLAG;
/// +ACER_AUx PenhoYu, showe charge_itcallback for debug
#ifdef ACER_CHG_BAT_FUN
		// PenhoYu, IssueKeys:A21.B-288 move from charge_init
		pcf506XX_registerChgFct(charge_itcallback);
		CHG_LOCK(&g_chg_cc_lock);
        if (!(charge->flags & CHARGE_ACER_INITED)) {
        	charge->flags |= CHARGE_ACER_INITED;
        	if (pcf506XX_mainChgPlugged()) {
        		acerchg_insertCharger_work((g_chg_type == ACER_CHG_750MA) ? CC_ACCHG : CC_USBCHG);
        		g_chg_state = ACER_STA_RUN;
        		if (g_chg_type == ACER_CHG_450MA) {
					charge->flags &= ~CHARGE_MAIN_CHARGER;
					charge->flags |= CHARGE_USB_CHARGER;
					if (charge->Online != NULL)
						charge->Online(POWER_SUPPLY_TYPE_USB);
					power_supply = DRVCHG_USB_CHARGER;
        		}
        		else if (g_chg_type == ACER_CHG_750MA) {
					charge->flags &= ~CHARGE_USB_CHARGER;
					charge->flags |= CHARGE_MAIN_CHARGER;
					if (charge->Online != NULL)
						charge->Online(POWER_SUPPLY_TYPE_MAINS);
					power_supply = DRVCHG_MAIN_CHARGER;
        		}
        		else {	// (g_chg_type == ACER_CHG_UNKNOW)
					g_chg_type = ACER_CHG_450MA;
        			g_chg_state = ACER_STA_DETECT;
					mod_timer(&chg_ctrl_timer,
						jiffies + msecs_to_jiffies(CHG_INIT_CK_CHGTYPE_TIMER));
        		}
        	}
        }
        CHG_UNLOCK(&g_chg_cc_lock);
#else	// ACER_CHG_BAT_FUN
        if ( charge->flags & CHARGE_MAIN_CHARGER )
            charge_startMainCharge();
        else
            if ( charge->flags & CHARGE_USB_CHARGER )
                charge_startUsbCharge();
			else
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, showe charge_itcallback for debug
			{
			    /* Start scan voltage timer */
			    if ( !timer_pending(&charge->scanvolttimer) ) {
			        init_timer(&charge->scanvolttimer);
			        charge->scanvolttimer.function=charge_scanVolttimeout;
			        charge->scanvolttimer.expires = jiffies + CHARGE_FAST_TIMER_VOLT;
			        charge->scanvolttimer.data= (u_long)charge;
			        add_timer(&charge->scanvolttimer);
			    }
			}
		return;
    }
	
	if ( pcf->flags & CHARGE_CHARGING )
	/* Manage Charge */
	{
	    if ( pcf->flags & CHARGE_WORK_TEMP_FLAG ) {
		    drvchg_dbg(DBG2,"Scan temp timer expired. Get temperature value\n");
			res=adc_aread(FID_TEMP,charge_adc_temperature);
			if (IS_ERR_VALUE(res))
				drvchg_dbg(DBG1,"adc_aread fail err=%d\n",res);

	        if ( pcf->temp.measurenb >= 4 ) {
				drvchg_dbg(DBG2,"Average temperature = %02lu.%02lu°C\n",
						pcf->temp.average/1000,(pcf->temp.average)%1000);
				charge_computeVoltageComp(pcf->temp.average/1000);

	            if ( pcf->temp.suspended == 0 ) {
		            if ( (CHARGE_TEMP_MIN_TO_CHARGE < pcf->temp.average) &&
			             ( pcf->temp.average < CHARGE_TEMP_MAX_TO_CHARGE) ) {
					    drvchg_dbg(DBG2,"Temperature is OK to carry on normal charge, start 60s timer\n");
						mod_timer(&pcf->scantemptimer, round_jiffies(jiffies + CHARGE_NORM_TIMER_TEMP));
	                }
		            else {
					    drvchg_dbg(DBG2,"Temperature is out of range, suspend charge\n");
/// +ACER_AUx PenhoYu, disable Onboard OTP mechanism
#ifndef ACER_CHG_BAT_FUN
				        charge_suspendCharge();
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, disable Onboard OTP mechanism
					}
	            }
		        else {
			        if ( (CHARGE_TEMP_MIN_TO_RESTART < pcf->temp.average) &&
				         ( pcf->temp.average < CHARGE_TEMP_MAX_TO_RESTART) ) {
					    drvchg_dbg(DBG2,"Temperature is ok to resume the charge\n");
/// +ACER_AUx PenhoYu, disable Onboard OTP mechanism
#ifndef ACER_CHG_BAT_FUN
						if ( charge->flags & CHARGE_MAIN_CHARGER )
							charge_startMainCharge();
	                    else
		                    charge_startUsbCharge();
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, disable Onboard OTP mechanism
			            return;
				    } else {
					    drvchg_dbg(DBG2,"Temperature is still out of range\n");
						mod_timer(&pcf->scantemptimer, round_jiffies(jiffies + CHARGE_NORM_TIMER_TEMP));
	                }
		        }
			}
	        else
				mod_timer(&pcf->scantemptimer, jiffies + CHARGE_FAST_TIMER_TEMP);
       
	        pcf->flags &= ~CHARGE_WORK_TEMP_FLAG;
		}
    
	    if (pcf->flags & CHARGE_WORK_VOLT_FLAG ) {
			drvchg_dbg(DBG2,"Scan voltage timer expired. Get voltage value\n");
			res=adc_aread(FID_VBAT,charge_adc_voltage);
       		if (IS_ERR_VALUE(res))
				drvchg_dbg(DBG1,"adc_aread fail err=%d\n",res);
			
	        if ( pcf->volt.measurenb >= 16 ) {
				drvchg_dbg(DBG2,"Average battery voltage compensated= %lumV (%lumV %+dmV)\n",
						pcf->volt.averageComp,
						pcf->volt.average,pcf->volt.mvoltComp);

				if ( pcf->volt.averageComp > CHARGE_VOLTAGE_MAX_TO_CHARGE ) {
					drvchg_dbg(DBG2,"Battery voltage is upper than Max voltage to charge, suspend charge\n");
	                charge_suspendCharge();
		            return;
			    } else {
				    if ( (pcf->volt.averageComp < CHARGE_TLIM_VOLTAGE_RESTART) && (pcf->features & CHARGE_FEAT_TLIM) && pcf506XX_CVmodeReached()) {
					    drvchg_dbg(DBG2,"VBAT < TLIM voltage to restart	and CV mode already reached once, stop main charge\n");
						if ( charge->flags & CHARGE_MAIN_CHARGER )
							charge_stopMainCharge();
	                    else
		                    charge_stopUsbCharge();
			        } else {
					    drvchg_dbg(DBG2,"Manage Battery Icons\n");
					    charge->BattIconMgt(1, power_supply,
								pcf->volt.averageComp,
								(int8_t)(pcf->temp.average/1000),
								(u_int16_t)(ICCSET*(trickleCurrent+1)/TRICKLE_CURRENT_STEP),
								max_current);
	                }
		        }
				mod_timer(&pcf->scanvolttimer, round_jiffies(jiffies + CHARGE_NORM_TIMER_VOLT));
	        } else {
/// +ACER_AUx PenhoYu, showe charge_itcallback for debug
#ifndef ACER_CHG_BAT_FUN
		        if ( pcf->flags & CHARGE_MAIN_CHARGER )
			        pcf506XX_setMainChgCur(CHARGE_MAIN_CUR_HIGH);
				else
					pcf506XX_setUsbChgCur(pmu_usb_max_current);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, showe charge_itcallback for debug

				mod_timer(&pcf->scanvolttimer, jiffies + CHARGE_FAST_TIMER_VOLT);
	        }

			pcf->flags &= ~CHARGE_WORK_VOLT_FLAG;
	    }
	} 
	else
	{
		/* Manage Discharge */
		drvchg_dbg(DBG2,"discharge Scan voltage timer expired. Get voltage value\n");
		res=adc_aread(FID_VBAT,charge_adc_voltage);
       	if (IS_ERR_VALUE(res))
			drvchg_dbg(DBG1,"adc_aread fail err=%d\n",res);

        if ( pcf->volt.measurenb >= 4 ) {
			drvchg_dbg(DBG2,"Average battery voltage compensated= %lumV (%lumV %+dmV)\n",
					pcf->volt.averageComp,
					pcf->volt.average,
					pcf->volt.mvoltComp);

		    charge->BattIconMgt(0, power_supply, pcf->volt.averageComp,
					(int8_t)(pcf->temp.average/1000),
					(u_int16_t)(ICCSET*(trickleCurrent+1)/TRICKLE_CURRENT_STEP), max_current);
			mod_timer(&pcf->scanvolttimer,
					round_jiffies(jiffies + CHARGE_DISC_TIMER_VOLT));
        } 
		else {
			mod_timer(&pcf->scanvolttimer, jiffies + CHARGE_FAST_TIMER_VOLT);
        }

		pcf->flags &= ~CHARGE_WORK_VOLT_FLAG;
	}
    
    pcf->working_timer = 0;
}

static void charge_schedule_work_timer(struct strcharge *pcf)
{
	int status;

	status = schedule_work(&pcf->work_timer);
	if (!status && !pcf->working_timer)
		drvchg_dbg(DBG2,"work item may be lost\n");
}

static void charge_void(u_int8_t a, int8_t b, ulong c, int8_t d, u_int16_t e, u_int16_t f) {}

static void charge_itcallback(u_int16_t battInt)
{
/// +ACER_AUx PenhoYu, showe charge_itcallback for debug
#ifdef ACER_CHG_BAT_FUN
    if (showdbg()) {
    	printk(KERN_INFO "... ******************************************* (%x)\n", charge->flags);
    	printk(KERN_INFO "<CHG> charge_itcallback : battInt(%04x)\n", battInt);
    }
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, showe charge_itcallback for debug

	/* In TAT mode we do not manage some callback: MAIN/USB charger plug/unplug */
	if ( !charge->tatmode )
	{
/// +ACER_AUx PenhoYu, showe charge_itcallback for debug
#ifdef ACER_CHG_BAT_FUN
	    if ( battInt & PCF506XX_MCHINS ) {
	    	CHG_LOCK(&g_chg_cc_lock);
	    	g_chg_state = ACER_STA_RUN;
	    	if (g_chg_type == ACER_CHG_750MA) {
	    		acerchg_insertCharger_work(CC_ACCHG);
				charge->flags &= ~CHARGE_USB_CHARGER;
				charge->flags |= (CHARGE_ACER_INITED | CHARGE_MAIN_CHARGER);
				if (charge->Online != NULL)
					charge->Online(POWER_SUPPLY_TYPE_MAINS);
				power_supply = DRVCHG_MAIN_CHARGER;
	    	}
	    	else {	// (g_chg_type == ACER_CHG_450MA | ACER_CHG_UNKNOW)
				acerchg_insertCharger_work(CC_USBCHG);
				if (g_chg_type == ACER_CHG_450MA) {
					charge->flags &= ~CHARGE_MAIN_CHARGER;
					charge->flags |= (CHARGE_ACER_INITED | CHARGE_USB_CHARGER);
					if (charge->Online != NULL)
						charge->Online(POWER_SUPPLY_TYPE_USB);
					power_supply = DRVCHG_USB_CHARGER;
				}
				else {	// (g_chg_type == ACER_CHG_UNKNOW)
					charge->flags |= CHARGE_ACER_INITED;
					g_chg_type = ACER_CHG_450MA;
					g_chg_state = ACER_STA_DETECT;
					mod_timer(&chg_ctrl_timer,
						jiffies + msecs_to_jiffies(CHG_CK_CHGTYPE_TIMER));
				}
			}
	    	CHG_UNLOCK(&g_chg_cc_lock);
	    }

		if ( battInt & PCF506XX_MCHRM ) {
			g_chg_type = ACER_CHG_UNKNOW;
			g_chg_state = ACER_STA_RM;
			g_chg_flag = 0;

			charge->flags &= ~(CHARGE_MAIN_CHARGER | CHARGE_USB_CHARGER);
			charge->flags |= CHARGE_ACER_INITED;
			charge_stopMainCharge();
			setCCreg(CC_NOCHG);
			if (charge->Online != NULL)
				charge->Online(POWER_SUPPLY_TYPE_BATTERY);
			power_supply=DRVCHG_NONE_CHARGER;
	    }
#else	// ACER_CHG_BAT_FUN
	    if ( battInt & PCF506XX_MCHINS ) {
			drvchg_dbg(DBG1,"Main Charger plugged\n");
			charge->flags |= CHARGE_MAIN_CHARGER;
	        if ( charge->flags & CHARGE_USB_CHARGER )
		        charge_stopUsbCharge();
        
			if (charge->Online != NULL)
				charge->Online(POWER_SUPPLY_TYPE_MAINS);

			charge_startMainCharge();

			power_supply=DRVCHG_MAIN_CHARGER;
	    }

		if ( battInt & PCF506XX_MCHRM ) {
			drvchg_dbg(DBG1,"Main Charger unplugged\n");
	        charge->flags &= ~CHARGE_MAIN_CHARGER;
		    charge_stopMainCharge();
			if ( charge->flags & CHARGE_USB_CHARGER )
			{
		        charge_startUsbCharge();        
				if (charge->Online != NULL)
					charge->Online(POWER_SUPPLY_TYPE_USB);
				power_supply=DRVCHG_USB_CHARGER;
			}
			else
			{
				if (charge->Online != NULL)
					charge->Online(POWER_SUPPLY_TYPE_BATTERY);

				power_supply=DRVCHG_NONE_CHARGER;
			}
	    }
        
	    if ( battInt & PCF506XX_UCHGINS ) {
			drvchg_dbg(DBG1,"USB Charger plugged\n");
			charge->flags |= CHARGE_USB_CHARGER;
	        if ( !(charge->flags & CHARGE_MAIN_CHARGER) )
			{
				if (charge->Online != NULL)
					charge->Online(POWER_SUPPLY_TYPE_USB);
				charge_startUsbCharge();

				power_supply=DRVCHG_USB_CHARGER;
			}
	    }

		if ( battInt & PCF506XX_UCHGRM ) {
			drvchg_dbg(DBG1,"USB Charger unplugged\n");
	        charge->flags &= ~CHARGE_USB_CHARGER;
			if (!(charge->flags & CHARGE_MAIN_CHARGER))
			{
				charge_stopUsbCharge();
				if (charge->Online != NULL)
					charge->Online(POWER_SUPPLY_TYPE_BATTERY);

				power_supply=DRVCHG_NONE_CHARGER;
			}
		}
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, showe charge_itcallback for debug
	}

/// +ACER_AUx PenhoYu, if remove battery will cause to PCF50623_BATFUL, PCF50623_VMAX in CC mode.
#ifdef ACER_CHG_BAT_FUN
    if (battInt & (PCF506XX_BATFUL | PCF506XX_VMAX))
    	if (charge->Online != NULL)
    		charge->Online(POWER_SUPPLY_CK_BATID);

	// detect OVP_FAULT of RT9718 state change
	if (battInt & PCF506XX_OVPFAULT) {
		int state = pnx_read_pmugpio_pin(PMU_GPIO4);

		if (showdbg())
			printk(KERN_INFO "<CHG> detect PCF506XX_OVPFAULT : %d\n", state);

		if (state) g_chg_flag &= ~CHGF_P_OCP;
		else g_chg_flag |= CHGF_P_OCP;

		if (charge->Online != NULL)
			charge->Online(POWER_SUPPLY_BATTERY_UPDATA);
	}
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, if remove battery will cause to PCF50623_BATFUL, PCF50623_VMAX in CCmode.

    if ( battInt & PCF506XX_BATFUL )
        charge_battfullCbk();

    if ( battInt & PCF506XX_VMAX )
        charge_cccvCbk();

	if ( battInt & PCF506XX_CHGRES )
		charge_chgRes();

    if ( battInt & PCF506XX_THLIMON ) {
        if ( charge->flags & CHARGE_MAIN_CHARGER )
            pcf506XX_setMainChgCur(0);
        else if ( charge->flags & CHARGE_USB_CHARGER )
            pcf506XX_setUsbChgCur(0);
    }

    if ( battInt & PCF506XX_THLIMOFF ) {
        if ( charge->flags & CHARGE_MAIN_CHARGER )
            pcf506XX_setMainChgCur(CHARGE_MAIN_CUR_HIGH);
        else if ( charge->flags & CHARGE_USB_CHARGER )
            pcf506XX_setUsbChgCur(pmu_usb_max_current);
    }
/// +ACER_AUx PenhoYu, showe charge_itcallback for debug
#ifdef ACER_CHG_BAT_FUN
    if (showdbg())
    	printk(KERN_INFO "^^^ ******************************************* (%x), trickleCurrent(%x)\n", charge->flags, trickleCurrent);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, showe charge_itcallback for debug
}

/***********************************************************************
 * Driver Interface
 ***********************************************************************/
unsigned long drvchg_get_voltage(void)
{
	return charge->volt.averageComp;
}

unsigned long drvchg_get_temperature(void)
{
	return charge->temp.average;
}

unsigned long drvchg_get_current(void)
{
	return (u_int16_t)(ICCSET*(trickleCurrent+1)/TRICKLE_CURRENT_STEP);
}

u_int8_t drvchg_get_charge_state(void)
{
	if (charge->flags & CHARGE_MAIN_CHARGER) 
		return POWER_SUPPLY_TYPE_MAINS;
	else
		if (charge->flags & CHARGE_USB_CHARGER)
			return POWER_SUPPLY_TYPE_USB;
		else
			return POWER_SUPPLY_TYPE_BATTERY;
}

void drvchg_registerChgFct(iconmgt pfct)
{
    charge->BattIconMgt = pfct;
}

void drvchg_unregisterChgFct(void)
{
    charge->BattIconMgt = charge_void;
}

void drvchg_registerOnlineFct(void (*callback)(unsigned int) )
{
    charge->Online = callback;
}

void drvchg_unregisterOnlineFct(void)
{
    charge->Online = NULL;
}


EXPORT_SYMBOL(drvchg_get_voltage);
EXPORT_SYMBOL(drvchg_get_temperature);
EXPORT_SYMBOL(drvchg_get_current);
EXPORT_SYMBOL(drvchg_get_charge_state);
EXPORT_SYMBOL(drvchg_registerChgFct);
EXPORT_SYMBOL(drvchg_unregisterChgFct);
EXPORT_SYMBOL(drvchg_registerOnlineFct);
EXPORT_SYMBOL(drvchg_unregisterOnlineFct);
/***********************************************************************
 * IOCTL management
 ***********************************************************************/
static int drvchg_open(struct inode * inode, struct file * instance)
{
	drvchg_dbg(DBG2,"drvchg_open\n");
  	return 0;
}

static int drvchg_close(struct inode * inode, struct file * instance)
{
	drvchg_dbg(DBG2,"drvchg_close\n");
  	return 0;
}

static int drvchg_ioctl(struct inode *inode, struct file *instance,
                              unsigned int cmd, unsigned long arg)
{
	struct drvchg_ioctl_tat_charge statchg;
	void __user *argp = (void __user *)arg;
    u_int16_t value;

    switch (cmd) {
    case DRVCHG_IOCTL_READ_MVOLT:
        if (copy_to_user(argp, &charge->volt.averageComp, sizeof(ulong)))
            return -EFAULT;
        break;

    case DRVCHG_IOCTL_READ_TEMP:
        if (copy_to_user(argp, &charge->temp.average, sizeof(long)))
            return -EFAULT;
        break;
        
    case DRVCHG_IOCTL_READ_CURRENT:
        value=(u_int16_t)(ICCSET*(trickleCurrent+1)/TRICKLE_CURRENT_STEP);
        if (copy_to_user(argp, &value, sizeof(u_int16_t)))
            return -EFAULT;
        break;
       
	case DRVCHG_IOCTL_SET_CHARGE_MODE_TAT:
		charge->tatmode = 1;
		if (copy_from_user(&statchg, argp, sizeof(statchg)))
            return -EFAULT;
		if (statchg.source == DRVCHG_MAIN_CHARGER)
		{
	        charge->flags |= CHARGE_MAIN_CHARGER;
			charge_startMainCharge();
		}
		else
			if (statchg.source == DRVCHG_USB_CHARGER)
			{
		        charge->flags |= CHARGE_USB_CHARGER;
				charge_startUsbCharge();
			}
			else
				return -EFAULT;
		break;

	case DRVCHG_IOCTL_SET_CHARGE_HIGH_CURRENT_TAT:
		charge->tatmode = 1;
		if (copy_from_user(&statchg, argp, sizeof(statchg)))
            return -EFAULT;
		if (statchg.source == DRVCHG_MAIN_CHARGER)
		{
	        pcf506XX_setMainChgCur(CHARGE_MAIN_CUR_HIGH);
		}
		else
			if (statchg.source == DRVCHG_USB_CHARGER)
			{
				/* For TAT mode, as there is not supposed to have an USB cable plugged */
				/* set a fix value (not the one returned by the USB driver */
				pcf506XX_setUsbChgCur(CHARGE_USB_CUR_HIGH);
			}
			else
				return -EFAULT;
		break;

 	case DRVCHG_IOCTL_SET_CHARGE_LOW_CURRENT_TAT:
		charge->tatmode = 1;
		if (copy_from_user(&statchg, argp, sizeof(statchg)))
            return -EFAULT;
		if (statchg.source == DRVCHG_MAIN_CHARGER)
		{
	        pcf506XX_setMainChgCur(CHARGE_MAIN_CUR_LOW);
		}
		else
			if (statchg.source == DRVCHG_USB_CHARGER)
			{
				/* For TAT mode, as there is not supposed to have an USB cable plugged */
				/* set a fix value (not the one returned by the USB driver */
				pcf506XX_setUsbChgCur(CHARGE_USB_CUR_LOW);
			}
			else
				return -EFAULT;
		break;

 	case DRVCHG_IOCTL_STOP_CHARGE_IN_CHARGE_TAT:
		charge->tatmode = 1;
		if (copy_from_user(&statchg, argp, sizeof(statchg)))
            return -EFAULT;
		if (statchg.source == DRVCHG_MAIN_CHARGER)
		{
	        charge->flags &= ~CHARGE_MAIN_CHARGER;
	        charge_stopMainCharge();
		}
		else
			if (statchg.source == DRVCHG_USB_CHARGER)
			{
		        charge->flags &= ~CHARGE_USB_CHARGER;
		        charge_stopUsbCharge();
			}
			else
				return -EFAULT;
		break;

 	case DRVCHG_IOCTL_STOP_CHARGE_IN_DISCHARGE_TAT:
		charge->tatmode = 1;
		break;

     default:
        drvchg_dbg(DBG2,"IOCTL command unknown\n");
        break;
        
    }
    
    return 0;
}


static struct file_operations drvchg_ops = {
	.owner		=	THIS_MODULE,
	.ioctl		=	drvchg_ioctl,
	.open		= 	drvchg_open,
	.release	=	drvchg_close,
};


static int __init charge_init(void)
{
	struct strcharge *data;
    u_int8_t i;
	int err=0;
   
	drvchg_dbg(DBG2,"charge_init entering\n");
/// +ACER_AUx PenhoYu, request GPIO for select the Rccset
#ifdef ACER_CHG_BAT_FUN
    INIT_CHG_LOCK(&g_chg_cc_lock);
	pnx_gpio_request(CHG_CCCTL_GPIO);
	pnx_gpio_write_pin(CHG_CCCTL_GPIO, 1);
	pnx_gpio_set_direction(CHG_CCCTL_GPIO, GPIO_DIR_OUTPUT);
	pnx_gpio_set_mode(CHG_CCCTL_GPIO, GPIO_MODE_MUX1);

	// enable PMU GPIO4 for detect OVP_FAULT of RT9718
	pnx_set_pmugpio_direction(PMU_GPIO4, 1);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, request GPIO for select the Rccset

	if (!(data = kzalloc(sizeof(*data), GFP_KERNEL))) {
		err=-ENOMEM;
		goto out;
	}
		
	INIT_WORK(&data->work_timer, charge_work_timer);

    charge = data;

	/* by default we are not in TAT mode */
	charge->tatmode = 0;

/// +ACER_AUx PenhoYu, include the acer_drvchg define
#ifdef ACER_CHG_BAT_FUN
    INIT_WORK(&g_acerchg_work, acerchg_proc_work);
    INIT_WORK(&g_inschg_work, inschg_proc_work);
    INIT_WORK(&g_usbchg_work, usbchg_proc_work);
    setup_timer(&chg_ctrl_timer, acer_chg_ctrl_func, 0);
    i = bibatchg_registerdrvchg(&devCompVolt, drvchg_dispatch_func);
#endif	// ACER_CHG_BAT_FUN

    /* init wakelock for full_suspend: main and usb charge */
	wake_lock_init(&(data->charge_main_lock), WAKE_LOCK_SUSPEND,
			"main charge");
	wake_lock_init(&(data->charge_usb_lock), WAKE_LOCK_SUSPEND,
			"usb charge");


#ifndef ACER_VBAT_OPCOMP
    /* Init array devCompVolt */
    for (i=0; i<DRVCHG_MAX; i++)
        devCompVolt[i] = 0;
#endif	// ACER_VBAT_OPCOMP
/// -ACER_AUx PenhoYu, include the acer_drvchg define

/*     charge->features |= CHARGE_FEAT_TLIM; */
/*     chgfct.intCbk = charge_intCbk; */
/*     chgfct.startMainChg = charge_startMainCharge; */
/*     chgfct.startUsbChg  = charge_startUsbCharge; */
/*     chgfct.stopMainChg  = charge_stopMainCharge; */
/*     chgfct.stopUsbChg   = charge_stopUsbCharge; */
/*     chgfct.battfullCbk  = charge_battfullCbk; */
/*     chgfct.vmaxCbk      = charge_cccvCbk; */

/// +ACER_AUx PenhoYu, IssueKeys:A21.B-288 move to CHARGE_WORK_INIT_FLAG
#ifndef ACER_CHG_BAT_FUN
    pcf506XX_registerChgFct(charge_itcallback);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, IssueKeys:A21.B-288 move to CHARGE_WORK_INIT_FLAG

    pcf506XX_disableBattfullInt();
    
    if (charge->BattIconMgt==NULL)
        charge->BattIconMgt = charge_void;

/// +ACER_AUx PenhoYu, include the acer_drvchg define
#ifdef ACER_CHG_BAT_FUN
    power_supply = DRVCHG_NONE_CHARGER;
    charge->flags &= ~(CHARGE_ACER_INITED | CHARGE_USB_CHARGER | CHARGE_MAIN_CHARGER);
    if (pcf506XX_mainChgPlugged()) {
    	if (i == USBCHG_DLSHORT) {		// USBCHG_DLSHORT
    		g_chg_type = ACER_CHG_750MA;
    	}
    	else if (i != USBCHG_NONE) {	// USBCHG_DATALINK, USBCHG_RESUME, USBCHG_USBSUP
    		g_chg_type = ACER_CHG_450MA;
    	}
    	else {
    		g_chg_type = ACER_CHG_UNKNOW;
    	}
    	g_chg_state = ACER_STA_NONE;
    }
    else {
    	setCCreg(CC_NOCHG);
    }
#else	// ACER_CHG_BAT_FUN
    if ( pcf506XX_mainChgPlugged() ) {
        charge->flags |= CHARGE_MAIN_CHARGER;
    }
    else
        if ( pcf506XX_usbChgPlugged() ) {
            charge->flags |= CHARGE_USB_CHARGER;
        }
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, include the acer_drvchg define

    init_timer(&charge->inittimer);
    charge->inittimer.function=charge_inittimeout;
    charge->inittimer.expires = jiffies + CHARGE_INIT_TIME;
    charge->inittimer.data= (u_long)charge;
    add_timer(&charge->inittimer);
    
    /* Register drvchg as dev device */
	major = register_chrdev(0, DEVICE_NAME, &drvchg_ops);
    if (major < 0) {
		printk(KERN_ERR "Not able to register_chrdev: %i\n",major);
		err=major;
		goto out;
    }

	/* populate sysfs entry */
	drvchg_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(drvchg_class)) {
		printk(KERN_ERR "failed to create drvchg class\n");
		err = PTR_ERR(drvchg_class);
		goto err_chrdev;
	}	
	/* send uevents to udev, so it'll create the /dev node*/
	device_create(drvchg_class, NULL, MKDEV(major,0), NULL, "%s",DEVICE_NAME);

	if (debug>2)
		debug=2;
	
	printk(KERN_INFO "Major Number for drvchg is: %u, DEBUG Level is %s\n",major,debug==0?"OFF":debug==1?"Light":"Complete");
    return err;

err_chrdev:
	unregister_chrdev(0,DEVICE_NAME);
out:	
	return err;
}

static void __exit charge_exit(void)
{
    if ( charge->flags & CHARGE_MAIN_CHARGER )
        charge_stopMainCharge();
    else
        if ( charge->flags & CHARGE_USB_CHARGER )
            charge_stopUsbCharge();
/// +ACER_AUx PenhoYu, free the select Rccset GPIO
#ifdef ACER_CHG_BAT_FUN
    bibatchg_unregisterdrvchg();
    del_timer_sync(&chg_ctrl_timer);
    pnx_gpio_free(CHG_CCCTL_GPIO);
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, free the select Rccset GPIO
    pcf506XX_unregisterChgFct();
	del_timer(&charge->mainchgtimer);
	del_timer(&charge->usbchgtimer);
    del_timer(&charge->scantemptimer);
    del_timer(&charge->scanvolttimer);
	device_destroy(drvchg_class,MKDEV(major,0));
	class_destroy(drvchg_class);
	unregister_chrdev(major,DEVICE_NAME);
 	kfree(charge);
}

module_init(charge_init);
module_exit(charge_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Olivier Clergeaud");
MODULE_DESCRIPTION("Charge driver");

/// +ACER_AUx PenhoYu, Include the acer_drvchg function
#ifdef ACER_CHG_BAT_FUN
#include "acer_drvchg.c"
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx PenhoYu, Include the acer_drvchg function

