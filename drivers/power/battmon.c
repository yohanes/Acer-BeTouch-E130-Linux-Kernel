/*
 *  linux/drivers/power/battmon.c
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

#include <mach/gpio.h>

#include <linux/drvchg.h>
#include "drvchg.h"
#include "battmon.h"

/***********************************************************************
 * Module parameter
 ***********************************************************************/
//static unsigned int major=0;
static unsigned int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "0->No debug, 1->Light debug, 2->Complete Debug");

/***********************************************************************
 * Static data / structures
 ***********************************************************************/
struct stricon{
    uint8_t level;
	uint8_t percent;
	uint8_t charge_level;
	uint8_t discharge_level;
	uint8_t previous_level;
	uint8_t previous_state;
	levelmgt BattLevelMgt;
};

static struct stricon * icon;
static int16_t battmon_max_current;
static char buf[64];

/***********************************************************************
 * Constant data
 ***********************************************************************/
/* Range 0 data */
#define BM_TEMP_RANGE0      10
#define BM_MAIN_CUR_R0L4    80
#define BM_MAIN_CUR_R0L3   320
#define BM_MAIN_CUR_R0L2   480
#define BM_MAIN_CUR_R0L1   510
#define BM_USB_CUR_R0L4     50
#define BM_USB_CUR_R0L3    290
#define BM_USB_VOLT_R0L2  4165
#define BM_USB_VOLT_R0L1  4090

/* Range 1 data */
#define BM_TEMP_RANGE1      20
#define BM_MAIN_CUR_R1L4    70
#define BM_MAIN_CUR_R1L3   510
#define BM_MAIN_VOLT_R1L2 4090
#define BM_MAIN_VOLT_R1L1 4000
#define BM_USB_CUR_R1L4     90
#define BM_USB_VOLT_R1L3  4140
#define BM_USB_VOLT_R1L2  3990
#define BM_USB_VOLT_R1L1  3910

/* Range 2 data */
#define BM_TEMP_RANGE2      30
#define BM_MAIN_CUR_R2L4    90
#define BM_MAIN_VOLT_R2L3 4175
#define BM_MAIN_VOLT_R2L2 4015
#define BM_MAIN_VOLT_R2L1 3925
#define BM_USB_CUR_R2L4    180
#define BM_USB_VOLT_R2L3  4080
#define BM_USB_VOLT_R2L2  3940
#define BM_USB_VOLT_R2L1  3860

/* Range 3 data */
#define BM_MAIN_CUR_R3L4   130
#define BM_MAIN_VOLT_R3L3 4135
#define BM_MAIN_VOLT_R3L2 3975
#define BM_MAIN_VOLT_R3L1 3885
#define BM_USB_CUR_R3L4    230
#define BM_USB_VOLT_R3L3  4050
#define BM_USB_VOLT_R3L2  3915
#define BM_USB_VOLT_R3L1  3830

/* Range discharge */
#define BM_VOLT_DISCHARGE_L3 3903
#define BM_VOLT_DISCHARGE_L2 3773
#define BM_VOLT_DISCHARGE_L1 3712
#define BM_VOLT_DISCHARGE_L0 3460

#define BM_VOLT_MIN 3100

/* Debug Level */
#define DBG1 1
#define DBG2 2

/***********************************************************************
 * Debug function
 ***********************************************************************/
static inline void dbg (int level, const char * func,  const char *msg)
{
	if (debug>=level)
        printk(KERN_DEBUG "%-25s: %s", func, msg);
}

static void battmon_main_charge(ulong volt, int8_t temp, u_int16_t cur)
{
    /* Compute icon battery level according to temperature */
    if (temp<BM_TEMP_RANGE0) {
        switch (icon->level) {
        case 4:
            break;
        case 3:
            if ( cur<BM_MAIN_CUR_R0L4 ) {
                icon->level = 4;
				icon->charge_level = 100;
            }
			else
			{
				if(cur>BM_MAIN_CUR_R0L3) {
					sprintf(buf, "cur corrected m03 (%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = BM_MAIN_CUR_R0L3;
				}
				icon->charge_level = 80+(20*(BM_MAIN_CUR_R0L3 -cur)/(BM_MAIN_CUR_R0L3 -BM_MAIN_CUR_R0L4));
			}
            break;
        case 2:
            if ( cur<BM_MAIN_CUR_R0L3  ) {
                icon->level = 3;
				icon->charge_level = 80;
            }
			else
			{
				if(cur>BM_MAIN_CUR_R0L2) {
					sprintf(buf, "cur corrected m02 (%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = BM_MAIN_CUR_R0L2;
				}
				icon->charge_level = 50+(30*(BM_MAIN_CUR_R0L2-cur)/(BM_MAIN_CUR_R0L2-BM_MAIN_CUR_R0L3 ));
			}
            break;
        case 1:
            if ( cur<BM_MAIN_CUR_R0L2 ) {
                icon->level = 2;
				icon->charge_level = 50;
            }
			else
			{
				if(cur>BM_MAIN_CUR_R0L1) {
					sprintf(buf, "cur corrected m01 (%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = BM_MAIN_CUR_R0L1;
				}
				icon->charge_level = 25+(25*(BM_MAIN_CUR_R0L1-cur)/(BM_MAIN_CUR_R0L1-BM_MAIN_CUR_R0L2));
			}
            break;
        case 0:
            if ( cur<BM_MAIN_CUR_R0L1 ) {
                icon->level = 1;
				icon->charge_level = 25;
            }
			else
			{
				if(cur>battmon_max_current) {
					sprintf(buf, "cur corrected m00 (%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = battmon_max_current;
				}
				icon->charge_level = 25*(battmon_max_current-cur)/(battmon_max_current-BM_MAIN_CUR_R0L1);
			}
            break;
        }
    } else if ( temp<BM_TEMP_RANGE1 )
    {
        switch (icon->level) {
        case 4:
            break;
        case 3:
            if ( cur<BM_MAIN_CUR_R1L4 ) {
                icon->level = 4;
				icon->charge_level = 100;
            }
			else
			{
				if(cur>BM_MAIN_CUR_R1L3) {
					sprintf(buf, "cur corrected m13 (%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = BM_MAIN_CUR_R1L3;
				}
				icon->charge_level = 80+(20*(BM_MAIN_CUR_R1L3-cur)/(BM_MAIN_CUR_R1L3-BM_MAIN_CUR_R1L4));
			}
            break;
        case 2:
            if ( cur<BM_MAIN_CUR_R1L3 ) {
                icon->level = 3;
				icon->charge_level = 80;
            }
			else
			{
				if(cur>battmon_max_current) {
					sprintf(buf, "cur corrected m12 (%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = battmon_max_current;
				}
				icon->charge_level = 50+(30*(battmon_max_current-cur)/(battmon_max_current-BM_MAIN_CUR_R1L3));
			}
            break;
        case 1:
            if ( volt>BM_MAIN_VOLT_R1L2 ) {
                icon->level = 2;
				icon->charge_level = 50;
            }
			else
			{
				if(volt<BM_MAIN_VOLT_R1L1) {
					sprintf(buf, "volt corrected m11 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_MAIN_VOLT_R1L1;
				}
				icon->charge_level = 25+(25*(volt-BM_MAIN_VOLT_R1L1)/(BM_MAIN_VOLT_R1L2-BM_MAIN_VOLT_R1L1));
			}
            break;
        case 0:
            if ( volt>BM_MAIN_VOLT_R1L1 ) {
                icon->level = 1;
				icon->charge_level = 25;
            }
			else
			{
				if(volt<BM_VOLT_MIN) {
					sprintf(buf, "volt corrected m10 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_VOLT_MIN;
				}
				icon->charge_level = 25*(volt-BM_VOLT_MIN)/(BM_MAIN_VOLT_R1L1-BM_VOLT_MIN);
			}
            break;
        }
    } else if ( temp<BM_TEMP_RANGE2 )
    {
        switch (icon->level) {
        case 4:
            break;
        case 3:
            if ( cur<BM_MAIN_CUR_R2L4 ) {
                icon->level = 4;
				icon->charge_level = 100;
            }
			else
			{
				if(cur>battmon_max_current) {
					sprintf(buf, "cur corrected m23 (%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = battmon_max_current;
				}
				icon->charge_level = 80+(20*(battmon_max_current-cur)/(battmon_max_current-BM_MAIN_CUR_R2L4));
			}
            break;
        case 2:
            if ( volt>BM_MAIN_VOLT_R2L3 ) {
                icon->level = 3;
				icon->charge_level = 80;
            }
			else
			{
				if(volt<BM_MAIN_VOLT_R2L2) {
					sprintf(buf, "volt corrected m22 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_MAIN_VOLT_R2L2;
				}
				icon->charge_level = 50+(30*(volt-BM_MAIN_VOLT_R2L2)/(BM_MAIN_VOLT_R2L3-BM_MAIN_VOLT_R2L2));
			}
            break;
        case 1:
            if ( volt>BM_MAIN_VOLT_R2L2 ) {
                icon->level = 2;
				icon->charge_level = 50;
            }
			else
			{
				if(volt<BM_MAIN_VOLT_R2L1) {
					sprintf(buf, "volt corrected m21 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_MAIN_VOLT_R2L1;
				}
				icon->charge_level = 25+(25*(volt-BM_MAIN_VOLT_R2L1)/(BM_MAIN_VOLT_R2L2-BM_MAIN_VOLT_R2L1));
			}
            break;
        case 0:
            if ( volt>BM_MAIN_VOLT_R2L1 ) {
                icon->level = 1;
				icon->charge_level = 25;
            }
			else
			{
				if(volt<BM_VOLT_MIN) {
					sprintf(buf, "volt corrected m20 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_VOLT_MIN;
				}
				icon->charge_level = 25*(volt-BM_VOLT_MIN)/(BM_MAIN_VOLT_R2L1-BM_VOLT_MIN);
			}
            break;
        }
    } else
    {
        switch (icon->level) {
        case 4:
            break;
        case 3:
            if ( cur<BM_MAIN_CUR_R3L4 ) {
                icon->level = 4;
				icon->charge_level = 100;
            }
			else
			{
				if(cur>battmon_max_current) {
					sprintf(buf, "cur corrected m33 (%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = battmon_max_current;
				}
				icon->charge_level = 80+(20*(battmon_max_current-cur)/(battmon_max_current-BM_MAIN_CUR_R3L4));
			}
            break;
        case 2:
            if ( volt>BM_MAIN_VOLT_R3L3 ) {
                icon->level = 3;
				icon->charge_level = 80;
            }
			else
			{
				if(volt<BM_MAIN_VOLT_R3L2) {
					sprintf(buf, "volt corrected m32 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_MAIN_VOLT_R3L2;
				}
				icon->charge_level = 50+(30*(volt-BM_MAIN_VOLT_R3L2)/(BM_MAIN_VOLT_R3L3-BM_MAIN_VOLT_R3L2));
			}
            break;
        case 1:
            if ( volt>BM_MAIN_VOLT_R3L2 ) {
                icon->level = 2;
				icon->charge_level = 50;
            }
			else
			{
				if(volt<BM_MAIN_VOLT_R3L1) {
					sprintf(buf, "volt corrected m31 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_MAIN_VOLT_R3L1;
				}
				icon->charge_level = 25+(25*(volt-BM_MAIN_VOLT_R3L1)/(BM_MAIN_VOLT_R3L2-BM_MAIN_VOLT_R3L1));
			}
            break;
        case 0:
            if ( volt>BM_MAIN_VOLT_R3L1 ) {
                icon->level = 1;
				icon->charge_level = 25;
            }
			else
			{
				if(volt<BM_VOLT_MIN) {
					sprintf(buf, "volt corrected m30 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_VOLT_MIN;
				}
				icon->charge_level = 25*(volt-BM_VOLT_MIN)/(BM_MAIN_VOLT_R3L1-BM_VOLT_MIN);
			}
            break;
        }
    }

	if ( (icon->charge_level != icon->previous_level) || (debug==DBG2) )
	{
		icon->previous_level = icon->charge_level;
//		if (icon->charge_level > icon->discharge_level)
		if (icon->charge_level > icon->percent)
			icon->percent = icon->charge_level;
		icon->BattLevelMgt(icon->percent);
	    sprintf(buf, "Main Charge, volt=%lumV, temp=%d°C, current=%umA\n",volt,temp,cur);
		dbg(DBG1, __func__, buf);
		sprintf(buf, "Current Icon battery level is %u%% (level=%u), charge level=%u%%, discharge level=%u%%\n", icon->percent, icon->level, icon->charge_level, icon->discharge_level);
		dbg(DBG1, __func__, buf);
	}
}

static void battmon_usb_charge(ulong volt, int8_t temp, u_int16_t cur)
{
    /* Compute icon battery level according to temperature */
    if (temp<BM_TEMP_RANGE0) {
        switch (icon->level) {
        case 4:
            break;
        case 3:
            if ( cur<BM_USB_CUR_R0L4 ) {
                icon->level = 4;
				icon->charge_level = 100;
            }
			else
			{
				if(cur>BM_USB_CUR_R0L3) {
					sprintf(buf, "cur corrected u3(%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = BM_USB_CUR_R0L3;
				}
				icon->charge_level = 80+(20*(BM_USB_CUR_R0L3 -cur)/(BM_USB_CUR_R0L3 -BM_USB_CUR_R0L4));
			}
            break;
        case 2:
            if ( cur<BM_USB_CUR_R0L3  ) {
                icon->level = 3;
				icon->charge_level = 80;
            }
			else
			{
				if(cur>battmon_max_current) {
					sprintf(buf, "cur corrected u2(%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = battmon_max_current;
				}
				icon->charge_level = 50+(30*(battmon_max_current-cur)/(battmon_max_current-BM_USB_CUR_R0L3 ));
			}
            break;
        case 1:
            if ( volt>BM_USB_VOLT_R0L2 ) {
                icon->level = 2;
				icon->charge_level = 50;
            }
			else
			{
				if(volt<BM_USB_VOLT_R0L1) {
					sprintf(buf, "volt corrected u1(%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_USB_VOLT_R0L1;
				}
				icon->charge_level = 25+(25*(volt-BM_USB_VOLT_R0L1)/(BM_USB_VOLT_R0L2-BM_USB_VOLT_R0L1));
			}
            break;
        case 0:
            if ( volt>BM_USB_VOLT_R0L1 ) {
                icon->level = 1;
				icon->charge_level = 25;
            }
			else
			{
				if(volt<BM_VOLT_MIN) {
					sprintf(buf, "volt corrected u0(%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_VOLT_MIN;
				}
				icon->charge_level = 25*(volt-BM_VOLT_MIN)/(BM_USB_VOLT_R0L1-BM_VOLT_MIN);
			}
            break;
        }
    } else if ( temp<BM_TEMP_RANGE1 )
    {
        switch (icon->level) {
        case 4:
            break;
        case 3:
            if ( cur<BM_USB_CUR_R1L4 ) {
                icon->level = 4;
				icon->charge_level = 100;
            }
			else
			{
				if(cur>battmon_max_current) {
					sprintf(buf, "cur corrected u13(%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = battmon_max_current;
				}
				icon->charge_level = 80+(20*(battmon_max_current-cur)/(battmon_max_current-BM_USB_CUR_R1L4 ));
			}
            break;
        case 2:
            if ( volt>BM_USB_VOLT_R1L3 ) {
                icon->level = 3;
				icon->charge_level = 80;
            }
			else
			{
				if(volt<BM_USB_VOLT_R1L2) {
					sprintf(buf, "volt corrected u12 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_USB_VOLT_R1L2;
				}
				icon->charge_level = 50+(30*(volt-BM_USB_VOLT_R1L2)/(BM_USB_VOLT_R1L3-BM_USB_VOLT_R1L2));
			}
            break;
            break;
        case 1:
            if ( volt>BM_USB_VOLT_R1L2 ) {
                icon->level = 2;
				icon->charge_level = 50;
            }
			else
			{
				if(volt<BM_USB_VOLT_R1L1) {
					sprintf(buf, "volt corrected u11 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_USB_VOLT_R1L1;
				}
				icon->charge_level = 25+(25*(volt-BM_USB_VOLT_R1L1)/(BM_USB_VOLT_R1L2-BM_USB_VOLT_R1L1));
			}
            break;
        case 0:
            if ( volt>BM_USB_VOLT_R1L1 ) {
                icon->level = 1;
				icon->charge_level = 25;
            }
			else
			{
				if(volt<BM_VOLT_MIN) {
					sprintf(buf, "volt corrected u10(%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_VOLT_MIN;
				}
				icon->charge_level = 25*(volt-BM_VOLT_MIN)/(BM_USB_VOLT_R1L1-BM_VOLT_MIN);
			}
            break;
		}
    } else if ( temp<BM_TEMP_RANGE2 )
    {
        switch (icon->level) {
        case 4:
            break;
        case 3:
            if ( cur<BM_USB_CUR_R2L4 ) {
                icon->level = 4;
				icon->charge_level = 100;
            }
			else
			{
				if(cur>battmon_max_current) {
					sprintf(buf, "cur corrected u23(%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = battmon_max_current;
				}
				icon->charge_level = 80+(20*(battmon_max_current-cur)/(battmon_max_current-BM_USB_CUR_R2L4 ));
			}
            break;
        case 2:
            if ( volt>BM_USB_VOLT_R2L3 ) {
                icon->level = 3;
				icon->charge_level = 80;
            }
			else
			{
				if(volt<BM_USB_VOLT_R2L2) {
					sprintf(buf, "volt corrected u22 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_USB_VOLT_R2L2;
				}
				icon->charge_level = 50+(30*(volt-BM_USB_VOLT_R2L2)/(BM_USB_VOLT_R2L3-BM_USB_VOLT_R2L2));
			}
            break;
            break;
        case 1:
            if ( volt>BM_USB_VOLT_R2L2 ) {
                icon->level = 2;
				icon->charge_level = 50;
            }
			else
			{
				if(volt<BM_USB_VOLT_R2L1) {
					sprintf(buf, "volt corrected u21 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_USB_VOLT_R2L1;
				}
				icon->charge_level = 25+(25*(volt-BM_USB_VOLT_R2L1)/(BM_USB_VOLT_R2L2-BM_USB_VOLT_R2L1));
			}
            break;
        case 0:
            if ( volt>BM_USB_VOLT_R2L1 ) {
                icon->level = 1;
				icon->charge_level = 25;
            }
			else
			{
				if(volt<BM_VOLT_MIN) {
					sprintf(buf, "volt corrected u20 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_VOLT_MIN;
				}
				icon->charge_level = 25*(volt-BM_VOLT_MIN)/(BM_USB_VOLT_R2L1-BM_VOLT_MIN);
			}
            break;
        }
    } else
    {
        switch (icon->level) {
        case 4:
            break;
        case 3:
            if ( cur<BM_USB_CUR_R3L4 ) {
                icon->level = 4;
				icon->charge_level = 100;
            }
			else
			{
				if(cur>battmon_max_current) {
					sprintf(buf, "cur corrected u33(%u)\n",cur);
					dbg(DBG1, __func__, buf);
					cur = battmon_max_current;
				}
				icon->charge_level = 80+(20*(battmon_max_current-cur)/(battmon_max_current-BM_USB_CUR_R3L4 ));
			}
            break;
        case 2:
            if ( volt>BM_USB_VOLT_R3L3 ) {
                icon->level = 3;
				icon->charge_level = 80;
            }
			else
			{
				if(volt<BM_USB_VOLT_R3L2) {
					sprintf(buf, "volt corrected u32 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_USB_VOLT_R3L2;
				}
				icon->charge_level = 50+(30*(volt-BM_USB_VOLT_R3L2)/(BM_USB_VOLT_R3L3-BM_USB_VOLT_R3L2));
			}
            break;
            break;
        case 1:
            if ( volt>BM_USB_VOLT_R3L2 ) {
                icon->level = 2;
				icon->charge_level = 50;
            }
			else
			{
				if(volt<BM_USB_VOLT_R3L1) {
					sprintf(buf, "volt corrected u31 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_USB_VOLT_R3L1;
				}
				icon->charge_level = 25+(25*(volt-BM_USB_VOLT_R3L1)/(BM_USB_VOLT_R3L2-BM_USB_VOLT_R3L1));
			}
            break;
        case 0:
            if ( volt>BM_USB_VOLT_R3L1 ) {
                icon->level = 1;
				icon->charge_level = 25;
            }
			else
			{
				if(volt<BM_VOLT_MIN) {
					sprintf(buf, "volt corrected u30 (%lu)\n",volt);
					dbg(DBG1, __func__, buf);
					volt = BM_VOLT_MIN;
				}
				icon->charge_level = 25*(volt-BM_VOLT_MIN)/(BM_USB_VOLT_R3L1-BM_VOLT_MIN);
			}
            break;
        }
    }

	if ( (icon->charge_level != icon->previous_level) || (debug==DBG2) )
	{
		icon->previous_level = icon->charge_level;
//		if (icon->charge_level > icon->discharge_level)
		if (icon->charge_level > icon->percent)
			icon->percent = icon->charge_level;
		icon->BattLevelMgt(icon->percent);
	    sprintf(buf, "Main Charge, volt=%lumV, temp=%d°C, current=%umA\n",volt,temp,cur);
		dbg(DBG1, __func__, buf);
		sprintf(buf, "Current Icon battery level is %u%% (level=%u), charge level=%u%%, discharge level=%u%%\n", icon->percent, icon->level, icon->charge_level, icon->discharge_level);
		dbg(DBG1, __func__, buf);
	}
}

static void battmon_discharge(ulong volt)
{
	switch (icon->level) {
    case 4:
		if ( volt<BM_VOLT_DISCHARGE_L3 )
		{
            icon->level = 3;
			icon->discharge_level = 100;
		}
		else
		{
			if ( icon->discharge_level<80 )
				icon->discharge_level = 100;
		}
		break;
	case 3:
        if ( volt<BM_VOLT_DISCHARGE_L2 )
		{
            icon->level = 2;
			icon->discharge_level = 80;
		}
		else
		{
			if(volt>BM_VOLT_DISCHARGE_L3) {
				sprintf(buf, "volt corrected 3 (%lumV)\n",volt);
				dbg(DBG1, __func__, buf);
				volt = BM_VOLT_DISCHARGE_L3;
			}
			icon->discharge_level = 100-(20*(BM_VOLT_DISCHARGE_L3-volt)/(BM_VOLT_DISCHARGE_L3-BM_VOLT_DISCHARGE_L2));
		}
        break;
    case 2:
        if ( volt<BM_VOLT_DISCHARGE_L1 )
		{
            icon->level = 1;
			icon->discharge_level = 50;
		}
		else
		{
			if(volt>BM_VOLT_DISCHARGE_L2) {
				sprintf(buf, "volt corrected 2 (%lumV)\n",volt);
				dbg(DBG1, __func__, buf);
				volt = BM_VOLT_DISCHARGE_L2;
			}
			icon->discharge_level = 80-(30*(BM_VOLT_DISCHARGE_L2-volt)/(BM_VOLT_DISCHARGE_L2-BM_VOLT_DISCHARGE_L1));
		}
        break;
    case 1:
        if ( volt<BM_VOLT_DISCHARGE_L0 )
		{
            icon->level = 0;
			icon->discharge_level = 25;
		}
		else
		{
			if(volt>BM_VOLT_DISCHARGE_L1) {
				sprintf(buf, "volt corrected 1 (%lumV)\n",volt);
				dbg(DBG1, __func__, buf);
				volt = BM_VOLT_DISCHARGE_L1;
			}
			icon->discharge_level = 50-(25*(BM_VOLT_DISCHARGE_L1-volt)/(BM_VOLT_DISCHARGE_L1-BM_VOLT_DISCHARGE_L0));
		}
        break;
    case 0:
			if(volt>BM_VOLT_DISCHARGE_L0) {
				sprintf(buf, "volt corrected 0 (%lumV)\n",volt);
				dbg(DBG1, __func__, buf);
				volt = BM_VOLT_DISCHARGE_L0;
			}
			icon->discharge_level = 25-25*(BM_VOLT_DISCHARGE_L0-volt)/(BM_VOLT_DISCHARGE_L0-BM_VOLT_MIN);
        break;
    }

	if ( (icon->discharge_level != icon->previous_level) || (debug==DBG2) )
	{
		icon->previous_level = icon->discharge_level;
//		if (icon->discharge_level < icon->charge_level)
		if (icon->discharge_level < icon->percent)
			icon->percent = icon->discharge_level;
		icon->BattLevelMgt(icon->percent);
	    sprintf(buf, "Battery, volt=%lumV\n",volt);
		dbg(DBG1, __func__, buf);
		sprintf(buf, "Current Icon battery level is %u%% (level=%u), charge level=%u%%, discharge level=%u%%\n", icon->percent, icon->level, icon->charge_level, icon->discharge_level);
		dbg(DBG1, __func__, buf);
	}
}


/***********************************************************************
 * Charge Management, cf spec VYn_ps18857
 ***********************************************************************/
static void battmon_IconMgt(uint8_t charge, int8_t power_supply, ulong volt, int8_t temp, u_int16_t cur, u_int16_t max_cur)
{  
   	sprintf(buf, "%s (%s), U=%lumV, T=%uoC, I=%umA, Imax=%umA\n", charge==0?"Discharge":"Charge", power_supply==DRVCHG_MAIN_CHARGER?"main":power_supply==DRVCHG_USB_CHARGER?"USB":"Battery", volt, temp, cur, max_cur);
	dbg(DBG2, __func__, buf);

	if ( icon->previous_state != charge )
	{
        icon->previous_level = 255;
		icon->previous_state = charge;
		if (charge)
			icon->level = 0;
		else
		{
			if ( volt>BM_VOLT_DISCHARGE_L3 )
				icon->level = 4;
			else if ( volt>BM_VOLT_DISCHARGE_L2 )
				icon->level = 3;
			else if ( volt>BM_VOLT_DISCHARGE_L1 )
				icon->level = 2;
			else if ( volt>BM_VOLT_DISCHARGE_L0 )
			 	icon->level = 1;
			else
				icon->level = 0;
		}
	}

	battmon_max_current = max_cur;

	if (charge)
	{
		if (power_supply==DRVCHG_MAIN_CHARGER)
			battmon_main_charge(volt, temp, cur);
		else
			if (power_supply==DRVCHG_USB_CHARGER)
				battmon_usb_charge(volt, temp, cur);
	}
	else
	{
		battmon_discharge(volt);
	}
}

/***********************************************************************
 * Driver Interface
 ***********************************************************************/
static void battmon_void(int a) {}

void battmon_registerLevelFct(levelmgt pfct)
{
    icon->BattLevelMgt = pfct;
}

void battmon_unregisterLevelFct(void)
{
    icon->BattLevelMgt = battmon_void;
}

EXPORT_SYMBOL(battmon_registerLevelFct);
EXPORT_SYMBOL(battmon_unregisterLevelFct);

static int __init battmon_init(void)
{
	struct stricon *data;
    uint8_t charge=drvchg_get_charge_state();

	printk("battmon_init entering\n");

	if (!(data = kzalloc(sizeof(*data), GFP_KERNEL)))
		return -ENOMEM;

    icon = data;

	if ( icon->BattLevelMgt == NULL )
		icon->BattLevelMgt = battmon_void;

    drvchg_registerChgFct(battmon_IconMgt);
    
	if (charge)
	{
        icon->level = 0;
		icon->discharge_level = 0;
		icon->percent = 0;
	}
	else
	{
        icon->level = 4;
		icon->charge_level = 100;
		icon->percent = 100;
	}

	if (debug>2)
		debug=2;
	
	printk(KERN_INFO "DEBUG Level is %s\n",debug==0?"OFF":debug==1?"Light":"Complete");
	printk(KERN_INFO "Initial state: %s, level = %02u%% (%u), charge level =  %02u%%, discharge level =  %02u%%\n",charge?"Charging":"Discharging", icon->percent, icon->level, icon->charge_level, icon->discharge_level);

	return 0;
}

static void __exit battmon_exit(void)
{
	drvchg_unregisterChgFct();
	kfree(icon);
}

module_init(battmon_init);
module_exit(battmon_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Olivier Clergeaud");
MODULE_DESCRIPTION("Battery Monitoring Driver");
