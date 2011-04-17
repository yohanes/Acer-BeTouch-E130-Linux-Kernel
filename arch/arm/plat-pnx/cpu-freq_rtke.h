/*
 * linux/arch/arm/plat-pnx/cpu-freq_rtke.h
 * 
 * Copyright (C) 2010 ST-Ericsson
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 *     Created:  03/02/2010 01:23:30 PM
 *      Author:  Vincent Guittot (VGE), vincent.guittot@stericsson.com
 */

#ifndef  CPU_FREQ_RTKE_INC
#define  CPU_FREQ_RTKE_INC

/*** Working point List Management ***/

#define WRAPPER_CPUFREQ_NB_MAX_WP_FREQ 4 /* nb of working points*/

#define WPLIST_AREA_NAME "WPLIST_FRAME"

struct working_points_list{
		unsigned int	wrapper_nr_wp;		
        unsigned long   P_SC[WRAPPER_CPUFREQ_NB_MAX_WP_FREQ];
        unsigned long   P_DSP_audio[WRAPPER_CPUFREQ_NB_MAX_WP_FREQ];
        unsigned long   P_DSP_tele[WRAPPER_CPUFREQ_NB_MAX_WP_FREQ];
        unsigned long   P_AHB[WRAPPER_CPUFREQ_NB_MAX_WP_FREQ];
        unsigned long   P_HCLK2[WRAPPER_CPUFREQ_NB_MAX_WP_FREQ];
        unsigned long   P_SDM[WRAPPER_CPUFREQ_NB_MAX_WP_FREQ];
        unsigned long   P_3G[WRAPPER_CPUFREQ_NB_MAX_WP_FREQ];
        unsigned long   P_FC[WRAPPER_CPUFREQ_NB_MAX_WP_FREQ];
        unsigned long   P_CAM[WRAPPER_CPUFREQ_NB_MAX_WP_FREQ];
        unsigned long   P_PCLK2[WRAPPER_CPUFREQ_NB_MAX_WP_FREQ];
};


#define CPUFREQ_INIT_WP_REQUEST_EVENT_NAME "WP_REQUEST"

#define WP_LIST_REQUEST_EVENT_ID 0
#define WP_LIST_AVAILABLE_EVENT_ID 1 

/*** Current Frequencies Management ***/

# define CURRENTFREQ_AREA_NAME "CFREQ_FRAME"

struct current_frequencies {
	volatile unsigned long SC_clk;
	volatile unsigned long HCLK1;
	volatile unsigned long SDM_clk;
	volatile unsigned long HCLK2;
	volatile unsigned long PCLK2;
	volatile unsigned long FC_clk;
	volatile unsigned long CAM_clk;
};

#define CPUFREQ_RECEIVED_CURRENT_FREQ_EVENT_NAME "CURRENT_FREQ_UPDATE_RECEIVED" 

#define CPUFREQ_RECEIVE_FREQUENCY_CHANGE_EVENT_ID 0

/*** Linux Frequencies Request Management ***/

# define SENDFREQ_AREA_NAME "SFREQ_FRAME"

struct linux_frequencies {
	volatile unsigned long SC_clk_min;
	volatile unsigned long SC_clk_max;
	volatile unsigned long HCLK1;
	volatile unsigned long SDM_clk;
	volatile unsigned long HCLK2;
	volatile unsigned long PCLK2;
	volatile unsigned long FC_clk;
	volatile unsigned long CAM_clk;
};
#define CPUFREQ_SEND_REQUEST_EVENT_NAME "SEND_REQUEST" 

#define CPUFREQ_SEND_REQUEST_EVENT_ID 0

/********/
# define LIBREMA_VAR_AREA_NAME "REMA_FRAME"
struct librema_intf_var {
	volatile signed long librema_PID_state;
	volatile signed long suspend_state;
#ifdef CONFIG_SENSORS_PCF50616
	volatile signed long librema_reduce_WP;
#endif
};

#ifndef CONFIG_SENSORS_PCF50616
	#define CPUFREQ_ENABLE_LIBREMA_PID_EVENT_NAME "ENABLE_PID" 
	#define CPUFREQ_ENABLE_LIBREMA_PID_EVENT_ID 0
#else
	#define CPUFREQ_LIBREMA_CHANGE_EVENT_NAME "REMA_CHANGE" 
#endif

#define CPUFREQ_LIBREMA_ENABLE_PID_EVENT_ID 0
#define CPUFREQ_LIBREMA_REDUCE_WP_EVENT_ID 1




/*** Xos control generic parameters ***/

#define ONE_EVENT 1
#define TWO_EVENTS 2

#define CLEAR_EVENT_BEFORE_REGISTRATION 1 /* used in xos_ctrl_register */


#endif   /* ----- #ifndef CPUFREQ_RTKE_INC  ----- */
