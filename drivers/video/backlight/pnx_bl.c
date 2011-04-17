/*
 *  linux/drivers/video/backlight/pnx_bl.c
 *
 *  Pnx PWM backlight driver
 *
 *  Author:     Olivier Clergeaud
 *  Copyright (C) 2010 ST-Ericsson
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/clock.h>
#include <linux/pnx_bl.h>
/* ACER BobIHLee@20100505, support AS1 project*/

#define ACER_L1_K3
#define ACER_L1_CHANGED


#if (defined ACER_L1_AU4) || (defined ACER_L1_K2) || defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
#include <linux/delay.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif


/***********************************************************************
 * Static data / structures
 ***********************************************************************/
/* ACER BobIHLee@20100505, support AS1 project*/
#if (defined ACER_L1_AU4) || (defined ACER_L1_K2) || defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
#define HW_LEVEL_MAX           0x0010
#elif (defined CONFIG_MACH_PNX67XX_V2_E150A_2GB) || (defined ACER_L1_AU2) || (defined ACER_L1_AU3) || (defined ACER_L1_K3)
#define HW_LEVEL_MAX           0x0289
#else
#define HW_LEVEL_MAX           0xFFFF
#endif
#define HW_LEVEL_MIN           0x0000

/* static struct platform_device *pnx_bl_pdev; */
static struct pnx_bl_platform_data *pnx_bl_pdata;
static struct backlight_device *pnx_backlight_device;

/* ACER Bright Lee, 2009/8/12, AU2.B-474, remove the sleep control for backlight { */
#ifndef ACER_L1_CHANGED
#ifdef CONFIG_HAS_EARLYSUSPEND
static int intensity_saved;
#endif
#endif
/* } ACER Bright Lee, 2009/8/12 */

/* ACER BobIHLee@20100505, support AS1 project*/
#if (defined ACER_L1_AU4) || (defined ACER_L1_K2) || defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
static int BackLight_Level=0;
#else
static struct clk *pwm1Clk;
#endif


/***********************************************************************
 * Backlight device
 ***********************************************************************/
static int pnx_bl_get_intensity(struct backlight_device *bd)
{
/* ACER BobIHLee@20100505, support AS1 project*/
#if (defined ACER_L1_AU4) || (defined ACER_L1_K2) || defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
/* 	int intensity = __raw_readl(PWM1_TMR_REG); */
/*	int intensity = readl(pnx_bl_pdata->pwm_tmr);*/
	return BackLight_Level;
#else
	int intensity=readl(pnx_bl_pdata->pwm_tmr);

	if ( (gpio_get_value(pnx_bl_pdata->gpio)==1) && (intensity==0) )
		intensity = bd->props.max_brightness;

	return intensity;
#endif
}

static int pnx_bl_set_intensity(struct backlight_device *bd)
{
/* ACER BobIHLee@20100505, support AS1 project*/
#if (defined ACER_L1_AU4) || (defined ACER_L1_K2) || defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
	int intensity = bd->props.brightness;
	int Jump_level;
	int i;
	
	if (bd->props.power != FB_BLANK_UNBLANK)
		intensity = 0;
	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		intensity = 0;

	if ( intensity == BackLight_Level ) {
		return 0;
	}

	if ( BackLight_Level == 0 ) {
		pnx_gpio_write_pin(pnx_bl_pdata->gpio, 1);
		udelay(30);
		Jump_level = 16 - intensity ;
		for ( i = 0 ; i < Jump_level ; i++) {
			pnx_gpio_write_pin(pnx_bl_pdata->gpio, 0);
			udelay(1);
			pnx_gpio_write_pin(pnx_bl_pdata->gpio, 1);
			udelay(1);
		}
		BackLight_Level = intensity;
	} else {
		if ( intensity == 0 ) {
			pnx_gpio_write_pin(pnx_bl_pdata->gpio, 0); /* backlight OFF */
			mdelay(10);
			BackLight_Level = 0 ;
			return 0;
		} else if ( BackLight_Level > intensity ) {
			Jump_level = BackLight_Level - intensity ;
			for ( i = 0 ; i < Jump_level ; i++) {
				pnx_gpio_write_pin(pnx_bl_pdata->gpio, 0);
				udelay(1);
				pnx_gpio_write_pin(pnx_bl_pdata->gpio, 1);
				udelay(1);
			}
			BackLight_Level = intensity;
		}else {
			Jump_level = 16 + BackLight_Level - intensity ;
			for ( i = 0 ; i < Jump_level ; i++) {
				pnx_gpio_write_pin(pnx_bl_pdata->gpio, 0);
				udelay(1);
				pnx_gpio_write_pin(pnx_bl_pdata->gpio, 1);
				udelay(1);
			}
			BackLight_Level = intensity;
		}
	}

	return 0;
#else
	int intensity = bd->props.brightness;
	int ret=0;

	if (bd->props.power != FB_BLANK_UNBLANK)
		intensity = 0;
	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		intensity = 0;

	if ( intensity < bd->props.max_brightness )
	{
		pnx_gpio_set_mode(pnx_bl_pdata->gpio, GPIO_MODE_MUX1);

		if ((intensity !=0) && (clk_get_usecount(pwm1Clk) == 0))
		    clk_enable(pwm1Clk);

		ret = writel(intensity, pnx_bl_pdata->pwm_tmr);

		if (intensity==0)
			clk_disable(pwm1Clk);
	} 
	else
	{
		/* Write value even if not used for pnx_bl_get_intensity usage */
		ret = writel(intensity, pnx_bl_pdata->pwm_tmr);
		clk_disable(pwm1Clk);
		pnx_gpio_set_mode(pnx_bl_pdata->gpio, GPIO_MODE_MUX0);
		gpio_set_value(pnx_bl_pdata->gpio, 1);
	}
	return ret;
#endif
}

static struct backlight_ops pnx_bl_ops = {
	.get_brightness	= pnx_bl_get_intensity,
	.update_status	= pnx_bl_set_intensity,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void pnx_bl_suspend(struct early_suspend *pes)
{
/* ACER Bright Lee, 2009/8/12, AU2.B-474, remove the sleep control for backlight { */
#ifndef ACER_L1_CHANGED
	intensity_saved = pnx_bl_get_intensity(pnx_backlight_device);
	pnx_backlight_device->props.brightness = 0;
	pnx_bl_set_intensity(pnx_backlight_device);
	pnx_backlight_device->props.brightness = intensity_saved;
#endif
/* } ACER Bright Lee, 2009/8/12 */
}

static void pnx_bl_resume(struct early_suspend *pes)
{
/* ACER Bright Lee, 2009/8/12, AU2.B-474, remove the sleep control for backlight { */
#ifndef ACER_L1_CHANGED
//	pnx_backlight_device->props.brightness = intensity_saved;
	pnx_bl_set_intensity(pnx_backlight_device);
#endif
/* } ACER Bright Lee, 2009/8/12 */
}

static struct early_suspend pnx_bl_earlys = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 10,
	.suspend = pnx_bl_suspend,
	.resume = pnx_bl_resume,
};
#endif

static int pnx_bl_probe(struct platform_device *pdev)
{
	/* ACER Bright Lee, 2010/7/21, silent boot don't need turn on backlight during booting { */
	extern int getOnCause(void);
	/* } ACER Bright Lee, 2010/7/21 */
/* ACER BobIHLee@20100505, support AS1 project*/
#if (defined ACER_L1_AU4) || (defined ACER_L1_K2)|| defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
	//struct backlight_device *pnx_backlight_device;
	//int ret;
	pnx_bl_pdata = pdev->dev.platform_data;

	if (!pnx_bl_pdata)
		return -ENODEV;
	
	pnx_backlight_device = backlight_device_register("pnx-bl",
							     &pdev->dev, NULL,
							     &pnx_bl_ops);
	if (IS_ERR(pnx_backlight_device))
		return PTR_ERR(pnx_backlight_device);

	platform_set_drvdata(pdev, pnx_backlight_device);


	//Config GPIO
	pnx_gpio_request(pnx_bl_pdata->gpio);	
	pnx_gpio_set_direction(pnx_bl_pdata->gpio, GPIO_DIR_OUTPUT);
	pnx_gpio_set_mode(pnx_bl_pdata->gpio, pnx_bl_pdata->pin_mux);

        //Selwyn 2009/1/20 modified for Acer power off charging
        #if 0
	//make sure the initial state is off
	pnx_gpio_write_pin(pnx_bl_pdata->gpio, 0); /* default: backlight OFF */	
	mdelay(3);
	//turn on backlight at max level first
	pnx_gpio_write_pin(pnx_bl_pdata->gpio, 1);
	udelay(1);
	#endif
	#ifdef DISABLE_CHARGING_ANIMATION
	if (getOnCause() == 2)
	{
		BackLight_Level = HW_LEVEL_MIN;
	}
	else
	{
		BackLight_Level = 4;
	}
	#else
	{
		BackLight_Level = HW_LEVEL_MAX;
	}
	#endif
	/* ACER Bright Lee, 2010/7/21, silent boot don't need turn on backlight during booting { */
	#if defined(ENABLE_SILENT_ALL) || defined(ENABLE_SILENT_BASIC)
	if (getOnCause() == 4)
	{
		BackLight_Level = HW_LEVEL_MIN;
	}
	#endif
	/* } ACER Bright Lee, 2010/7/21 */
        //~Selwyn 2009/1/20 modified
	pnx_backlight_device->props.power = FB_BLANK_UNBLANK;
	pnx_backlight_device->props.brightness = HW_LEVEL_MAX;
	pnx_backlight_device->props.max_brightness = HW_LEVEL_MAX - HW_LEVEL_MIN;
	//BackLight_Level = HW_LEVEL_MAX;

    return 0;
#else
/*     struct pnx_bl_platform_data *pdata = pdev->dev.platform_data; */
	int ret;

/* 	pnx_bl_pdev = pdev; */
    pnx_bl_pdata = pdev->dev.platform_data;

	if (!pnx_bl_pdata)
		return -ENODEV;
    
    
    pwm1Clk = clk_get(0, pnx_bl_pdata->pwm_clk);
    clk_enable(pwm1Clk);
	ret = writel(HW_LEVEL_MAX, pnx_bl_pdata->pwm_pf);
    

//	pnx_backlight_device = backlight_device_register("lcd-backlight",
	pnx_backlight_device = backlight_device_register("pnx-bl",
							     &pdev->dev, NULL,
							     &pnx_bl_ops);
	if (IS_ERR(pnx_backlight_device))
		return PTR_ERR(pnx_backlight_device);

	platform_set_drvdata(pdev, pnx_backlight_device);

	gpio_request(pnx_bl_pdata->gpio, pdev->name);
	gpio_direction_output(pnx_bl_pdata->gpio, 1 /* TBD LPA*/);
	pnx_gpio_set_mode(pnx_bl_pdata->gpio, GPIO_MODE_MUX1);
    
	pnx_backlight_device->props.power = FB_BLANK_UNBLANK;
	pnx_backlight_device->props.brightness = HW_LEVEL_MAX;

	/* ACER Bright Lee, 2010/7/21, silent boot don't need turn on backlight during booting { */
	#if defined(ENABLE_SILENT_ALL) || defined(ENABLE_SILENT_BASIC)
	if (getOnCause() == 4)
	{
		pnx_backlight_device->props.brightness = HW_LEVEL_MIN;
	}
	else
	{
		pnx_backlight_device->props.brightness = HW_LEVEL_MAX;
	}
	#endif
	/* } ACER Bright Lee, 2010/7/21 */
	pnx_backlight_device->props.max_brightness = HW_LEVEL_MAX - HW_LEVEL_MIN;
    clk_disable(pwm1Clk);
	pnx_bl_set_intensity(pnx_backlight_device);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&pnx_bl_earlys);
#endif

    return 0;
#endif
}

static int pnx_bl_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&pnx_bl_earlys);
#endif
	gpio_free(pnx_bl_pdata->gpio);
	backlight_device_unregister(bd);

	return 0;
}
//added for AU3.B-322 2009.10.23 wesley start
void pnx_bl_shutdown(struct platform_device *pdev)
{
/* ACER BobIHLee@20100505, support AS1 project*/
#if (defined ACER_L1_AU4) || (defined ACER_L1_K2)|| defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
	pnx_gpio_write_pin(pnx_bl_pdata->gpio, 0); /* backlight OFF */
	mdelay(3);
#else
	pnx_gpio_set_mode(pnx_bl_pdata->gpio, GPIO_MODE_MUX1);
	writel(0, pnx_bl_pdata->pwm_tmr);
	clk_disable(pwm1Clk);	
#endif
}
//added for AU3.B-322 2009.10.23 wesley end

static struct platform_driver pnx_bl_driver = {
	.probe = pnx_bl_probe,
	.remove = pnx_bl_remove,
	.shutdown = pnx_bl_shutdown,
	.driver = {
		   .name = "pnx-bl",
		   },
};

static struct platform_device *pnx_bl_device;

static int __init pnx_bl_init(void)
{
	int ret = platform_driver_register(&pnx_bl_driver);

	if (!ret) {
		pnx_bl_device = platform_device_alloc("pnx-bl", -1);
		if (!pnx_bl_device)
			return -ENOMEM;

/* 		ret = platform_device_add(pnx_bl_device); */

/* 		if (ret) { */
/* 			platform_device_put(pnx_bl_device); */
/* 			platform_driver_unregister(&pnx_bl_driver); */
/* 		} */
	}

	return ret;
}

static void __exit pnx_bl_exit(void)
{
	platform_device_unregister(pnx_bl_device);
	platform_driver_unregister(&pnx_bl_driver);
}

module_init(pnx_bl_init);
module_exit(pnx_bl_exit);

MODULE_AUTHOR("Olivier Clergeaud <olivier.clergeaud@stnwireless.com>");
MODULE_DESCRIPTION("Pnx Backlight Driver");
MODULE_LICENSE("GPL");
