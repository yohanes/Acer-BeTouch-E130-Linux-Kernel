/*
 *  linux/arch/arm/plat-pnx/include/mach/usb.h
 *
 *  ST-Ericsson specific platform specific data for usb device
 *
 *  Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef ASMARM_ARCH_PNX_USB_H
#define ASMARM_ARCH_PNX_USB_H

#include <linux/pcf506XX.h>

typedef enum{
	SUSPEND_OFF, 
	SUSPEND_ON,
}suspend_state;

typedef enum{
	USB_OFF,
	USB_ON,
	/* ACER Bright Lee, 2010/4/22, usb detect when power off charing { */
	USB_SHUTDOWN,
	/* } ACER Bright Lee, 2010/4/22 */
}usb_state;

typedef u_int8_t (* usb_cable_plugged_t)(void);
typedef void (* usb_register_pmu_callback_t)(itcallback);
typedef void (* usb_unregister_pmu_callback_t)(void);
typedef void (* usb_suspend_onoff_t)(int onoff);

struct usb_platform_data {
	usb_register_pmu_callback_t   usb_register_pmu_callback;
	usb_unregister_pmu_callback_t usb_unregister_pmu_callback;
	usb_cable_plugged_t           usb_cable_plugged;
	itcallback                    usb_vdd_onoff;
	usb_suspend_onoff_t			  usb_suspend_onoff;
	int                           gpio_host_usbpwr:16;
	int                           gpio_mux_host_usbpwr:16;
};

static inline void usb_set_vdd_onoff( u_int16_t on)
{
	/* ACER Bright Lee, 2010/4/22, usb detect when power off charing { */
	if (on == USB_OFF)		
		pcf506XX_onoff_set(pcf506XX_global,PCF506XX_REGULATOR_USBREG,PCF506XX_REGU_ECO);
	else if (on == USB_SHUTDOWN)
		pcf506XX_onoff_set(pcf506XX_global,PCF506XX_REGULATOR_USBREG,PCF506XX_REGU_OFF);
	/* } ACER Bright Lee, 2010/4/22 */
	else
		pcf506XX_onoff_set(pcf506XX_global,PCF506XX_REGULATOR_USBREG,PCF506XX_REGU_ON);
}



static inline int get_usb_max_current(void)
{
	return CONFIG_USB_GADGET_VBUS_DRAW;
}

#endif

