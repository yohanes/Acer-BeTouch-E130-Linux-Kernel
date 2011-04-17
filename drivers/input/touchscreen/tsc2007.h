/**
 * tsc2007.h. Header file for the driver for Texas Instruments TSC2007 touchscreen
 * controller on I2C bus on AT91SAM9261 board.
 *
 * Author: Joseph Robert (joseph_robert@mindtree.com)
 * Date:   12-06-2008
 * */
/* ACER Jen chang, 2009/06/15, IssueKeys:AU4.F-2, Touch panel driver porting { */
#ifndef _H_TSC2007
#define _H_TSC2007

#include <mach/gpio.h>

#ifndef BOOL
#define BOOL    int
#endif

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

#define DRIVER_NAME "TSC2007 touch_screen"

#ifndef PNX67XX
#define PNX67XX
#endif

//#define VALIDATE_ALL_COMMANDS
#define SETUP_MAV_RIRQ
#define REPORT_ACTUAL_PRESSURE

/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Modify for easy determining { */
#ifdef PNX67XX
#define IS_PEN_UP(pin) gpio_get_value(pin)
#else
#error "No PEN_UP_STATE found.Define a PEN_UP_STATE macro"
#endif
/* } ACER Jen Chang, 2009/12/23*/

/* debug macros */
#define dbg_tsc(format, arg...)\
	printk(KERN_DEBUG format, ## arg)

#define alert_tsc(format, arg...)\
	printk(KERN_ALERT format, ## arg)

/* command byte */

#define TSC2007_CMD(cmd,pdn,m) (((cmd) << 4) | ((pdn) << 2) | ((m) << 1))

/* set up command */
#ifdef SETUP_MAV_RIRQ
#define TSC2007_SETUP_CMD(mav,rirq) ((0xb0)  | ((mav) << 1) | ((rirq) ))

/* filter control */
#define USE_MAV 	0
#define BYPASS_MAV 	1

/* Rirq value */
#define RIRQ_50KOHMS	0
#define RIRQ_90KOHMS	1

#define USE_MAV_RIRQ_50KOHMS TSC2007_SETUP_CMD(USE_MAV,RIRQ_50KOHMS)
#define USE_MAV_RIRQ_90KOHMS TSC2007_SETUP_CMD(USE_MAV,RIRQ_90KOHMS)
#define BYPASS_MAV_RIRQ_50KOHMS TSC2007_SETUP_CMD(BYPASS_MAV,RIRQ_50KOHMS)
#define BYPASS_MAV_RIRQ_90KOHMS TSC2007_SETUP_CMD(BYPASS_MAV,RIRQ_90KOHMS)

/*
 *  NOTE::The tsc2007 driver has a default set up setting of
 *  USE_MAV_RIRQ_50KOHMS.
 *  CAUTION: The Rirq value of 90Kohms is a weak pullup and may lead
 *  to inconsistent results.
 */
#define CURRENT_SETUP_SETTING USE_MAV_RIRQ_50KOHMS
#endif


/* Use M_12BIT for 12 bit resolution and M_8BIT for 8 bit resolution */
/* ACER Jen chang, 2009/06/22, IssueKeys:AU4.FC-5, Modify touch panel resolution { */
#define BIT_MODE_12
/* } ACER Jen Chang, 2009/06/22*/
#ifndef BIT_MODE_12
#define BIT_MODE_8
#endif

/* ACER Jen chang, 2010/02/06, IssueKeys:A43.B-813, Modify default calibration data { */
/* ACER Jen chang, 2009/12/25, IssueKeys:AU4.B-2253, Using shift instead of dividing { */
#ifdef BIT_MODE_12
#define ADC_MAX 4096
#define ADC_MAX_LEVEL 12 //ADC MAX is 4096, use shift instead of dividing
#define VALUE_MASK 0XFFF
#else
#define ADC_MAX 256
#define ADC_MAX_LEVEL 8 //ADC MAX is 256, use shift instead of dividing
#define VALUE_MASK 0XFF
#endif
/* } ACER Jen Chang, 2009/12/25*/

#define ADC_PRESSURE_MAX 150000
#define FIXED_PRESSURE_VAL 7500
/* ACER Jen chang, 2009/12/30, IssueKeys:AU4.B-2253, Modify X, Y trance impedance providing by touch panel vendor { */
#define RX_PLATE_VAL 383 //Measure resistance of X+ and X- by vendor
#define RY_PLATE_VAL 392 //Measure resistance of Y+ and Y- by vendor
/* } ACER Jen Chang, 2009/12/30*/
#define PRESSURE_FALSE 0
#define TOUCH_FALSE 0
#define TOUCH_TRUE 1

#define ACER_L1_K3

/* ACER Jen chang, 2009/09/23, IssueKeys:AU4.FC-62, Decrease schedule work queue delay time to get more sampling point { */
#define START_TIMER_DELAY 2	//5
#define RESTART_TIMER_DELAY 2	//5
/* } ACER Jen Chang, 2009/09/23*/

/* ACER Jen chang, 2009/12/30, IssueKeys:AU4.B-2253, Modify pressure threshold providing by touch panel vendor to filter abnormal points { */
#define PRESURE_Hi_THRESHOLD 1000
#define PRESURE_Lo_THRESHOLD 50
/* } ACER Jen Chang, 2009/12/30*/
#ifdef ACER_L1_K1
#define Raw_Pressure_Hi_Threshold 4000
#define Raw_Pressure_Lo_Threshold 100
#else
#define Raw_Pressure_Hi_Threshold 4100
#define Raw_Pressure_Lo_Threshold 100
#endif

#define BYTES_TO_TX 1
#ifdef BIT_MODE_12
#define BYTES_TO_RX 2
#else
#define BYTES_TO_RX 1
#endif

#define ENABLE_DEGLITCH 1
#define DISABLE_DEGLITCH 0

/* ACER Jen chang, 2010/04/14, IssueKeys:A21.B-276, Modify touch screen default calibration data to mapping 0~4096 pixel for different model { */
#if (defined ACER_L1_AU4)
/*Define the screen size if not defined*/
#ifndef CONFIG_INPUT_MOUSEDEV_SCREEN_X
#define CONFIG_INPUT_MOUSEDEV_SCREEN_X 240
#endif

#ifndef CONFIG_INPUT_MOUSEDEV_SCREEN_Y
#define CONFIG_INPUT_MOUSEDEV_SCREEN_Y 340
#endif

#ifndef CONFIG_INPUT_SCREEN_OFFSET_X
#define CONFIG_INPUT_SCREEN_OFFSET_X	355
#endif
#ifndef CONFIG_INPUT_SCREEN_OFFSET_Y
#define CONFIG_INPUT_SCREEN_OFFSET_Y	335
#endif

#ifndef CONFIG_INPUT_SCREEN_SCALE_X
#define CONFIG_INPUT_SCREEN_SCALE_X		3245
#endif
#ifndef CONFIG_INPUT_SCREEN_SCALE_Y
#define CONFIG_INPUT_SCREEN_SCALE_Y		3555
#endif

#elif (defined ACER_L1_K2)
/*Define the screen size if not defined*/
#ifndef CONFIG_INPUT_MOUSEDEV_SCREEN_X
#define CONFIG_INPUT_MOUSEDEV_SCREEN_X	240
#endif

#ifndef CONFIG_INPUT_MOUSEDEV_SCREEN_Y
#define CONFIG_INPUT_MOUSEDEV_SCREEN_Y	340
#endif

#ifndef CONFIG_INPUT_SCREEN_OFFSET_X
#define CONFIG_INPUT_SCREEN_OFFSET_X	200
#endif
#ifndef CONFIG_INPUT_SCREEN_OFFSET_Y
#define CONFIG_INPUT_SCREEN_OFFSET_Y	100
#endif

#ifndef CONFIG_INPUT_SCREEN_SCALE_X
#define CONFIG_INPUT_SCREEN_SCALE_X		3855
#endif
#ifndef CONFIG_INPUT_SCREEN_SCALE_Y
#define CONFIG_INPUT_SCREEN_SCALE_Y		3920
#endif

#elif (defined ACER_L1_K3)
/*Define the screen size if not defined*/
#ifndef CONFIG_INPUT_MOUSEDEV_SCREEN_X
#define CONFIG_INPUT_MOUSEDEV_SCREEN_X	320
#endif

#ifndef CONFIG_INPUT_MOUSEDEV_SCREEN_Y
#define CONFIG_INPUT_MOUSEDEV_SCREEN_Y	240
#endif

#ifndef CONFIG_INPUT_SCREEN_OFFSET_X
#define CONFIG_INPUT_SCREEN_OFFSET_X	128
#endif
#ifndef CONFIG_INPUT_SCREEN_OFFSET_Y
#define CONFIG_INPUT_SCREEN_OFFSET_Y	260
#endif

#ifndef CONFIG_INPUT_SCREEN_SCALE_X
#define CONFIG_INPUT_SCREEN_SCALE_X		3925
#endif
#ifndef CONFIG_INPUT_SCREEN_SCALE_Y
#define CONFIG_INPUT_SCREEN_SCALE_Y		3850
#endif
#endif
/* } ACER Jen Chang, 2010/04/14*/

/* converter functions */
enum converter_function
{
	MEAS_TEMP0 = 0,
	MEAS_AUX = 2,
	MEAS_TEMP1 = 4,
	ACTIVATE_X_DRIVERS =8,
	ACTIVATE_Y_DRIVERS = 9,
	ACTIVATE_YnX_DRIVERS = 10,
	MEAS_XPOS = 12,
	MEAS_YPOS = 13,
	MEAS_Z1 = 14,
	MEAS_Z2 = 15
};

enum power_down_mode
{
	PD_POWERDOWN_ENABLEPENIRQ = 0, /* ADC off & penirq */
	PD_ADCON_DISABLEPENIRQ = 1, /* ADC on & no penirq */
};

/* resolution modes */

enum resolution_mode
{
	M_12BIT = 0,
	M_8BIT = 1
};
#endif
/* } ACER Jen Chang, 2009/06/15*/
