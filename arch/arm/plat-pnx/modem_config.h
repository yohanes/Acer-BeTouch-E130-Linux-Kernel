/*
 * linux/arch/arm/plat-pnx/modem_config.h
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *     Created:  03/02/2010 01:23:30 PM
 *      Author:  Loic Pallardy (LPA), loic.pallardy@stericsson.com
 */

#include <mach/modem_sdi.h>
#include <mach/modem_gpio.h>
#include <mach/modem_adc.h>
#include <mach/modem_pwm.h>
#ifdef CONFIG_EBI_BUS
	#include <mach/ebi.h>
#endif

#define CONFIG_AREA_NAME "CONFIG"

#define CONFIG_SEND_REQUEST_EVENT_ID 0

#define MAX_SDI_WORKING_POINT  5

#define CONFIG_DATA_START	0x53544152	/* 'STAR' */
#define CONFIG_DATA_END	0x454E4444	/* 'ENDD' */
#define CONFIG_SDI_START	0x53534430	/* 'SSD0' */
#define CONFIG_SDI_END		0x45534430	/* 'ESD0' */
#define CONFIG_GPIO_START	0x53475030	/* 'SGP0' */
#define CONFIG_GPIO_END	0x45475030	/* 'EGP0' */
#define CONFIG_EXTINT_START	0x53455830	/* 'SEX0' */
#define CONFIG_EXTINT_END	0x45455830	/* 'EEX0' */
#define CONFIG_ADC_START	0x53654430	/* 'SAD0' */
#define CONFIG_ADC_END		0x45654430	/* 'EAD0' */
#define CONFIG_PWM_START	0x53505730	/* 'SPW0' */
#define CONFIG_PWM_END		0x45505730	/* 'EPW0' */
#ifdef CONFIG_EBI_BUS
#define CONFIG_EBI_TO_RTK_START  0x53454230	/* 'SEB0' */
#define CONFIG_EBI_TO_RTK_END    0x45454230	/* 'EEB0' */
#endif
#define CONFIG_GENERAL_SERVICES_START	0x53454232	/* 'SEB2' */
#define CONFIG_GENERAL_SERVICES_END	0x53454232	/* 'EEB2' */

struct config_general_services {
	u32 general_services_start;
	u32 ram_size;
	u32 general_services_end;
};

struct config_modem_data_shared {
	__u32  config_data_start;
	__u32  nb_sdi_working_point;
	u32 sdi_start;
	struct pnx_modem_sdi_timing pnx_modem_sdi[MAX_SDI_WORKING_POINT];
	u32 sdi_end;
	struct pnx_modem_gpio pnx_modem_gpio;
	struct pnx_modem_extint pnx_modem_extint;
	struct pnx_modem_adc pnx_modem_adc;
	struct pnx_modem_pwm_config pnx_modem_pwm[3];
#ifdef CONFIG_EBI_BUS
    struct pnx_config_ebi_to_rtk config_ebi[3];
#endif
	struct config_general_services general_services;
	__u32  config_data_end;
};

struct config_modem_ctxt {
	xos_area_handle_t shared;
	struct config_modem_data_shared *base_cfg;
	xos_ctrl_handle_t new_request;
};

