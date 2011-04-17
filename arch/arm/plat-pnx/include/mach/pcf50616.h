/*
 * Filename:     pcf50616.h
 *
 * Description:  Power Management Unit (PMU) 50616 driver
 *
 * Created:      20.11.2009 11:14:40
 * Author:       Patrice Chotard (PCH), patrice.chotard@stericsson.com
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * changelog:
 *
 */
 
#ifndef _PCF50616_H
#define _PCF50616_H

 enum pfc50616_regs {
	PCF50616_REG_ID		  = 0x00,
	/*					  = 0x01, Reserved */
	PCF50616_REG_INT1	  = 0x02,	/* Interrupt Status */
	PCF50616_REG_INT2	  = 0x03,	/* Interrupt Status */
	PCF50616_REG_INT3	  = 0x04,	/* Interrupt Status */
	PCF50616_REG_INT4	  = 0x05,	/* Interrupt Status */
	/*					  = 0x06, Reserved */
	PCF50616_REG_INT1M	  = 0x07,	/* Interrupt Mask */
	PCF50616_REG_INT2M	  = 0x08,	/* Interrupt Mask */
	PCF50616_REG_INT3M	  = 0x09,	/* Interrupt Mask */
	PCF50616_REG_INT4M	  = 0x0A,	/* Interrupt Mask */
	/*					  = 0x0B, Reserved */
	PCF50616_REG_OOCC	  = 0x0C,
	PCF50616_REG_OOCS1	  = 0x0D,
	PCF50616_REG_OOCS2	  = 0x0E,
	PCF50616_REG_DEBC	  = 0x0F,
	PCF50616_REG_RTC1	  = 0x10,
	PCF50616_REG_RTC2	  = 0x11,
	PCF50616_REG_RTC3	  = 0x12,
	PCF50616_REG_RTC4	  = 0x13,
	PCF50616_REG_RTC1A	  = 0x14,
	PCF50616_REG_RTC2A	  = 0x15,
	PCF50616_REG_RTC3A	  = 0x16,
	PCF50616_REG_RTC4A	  = 0x17,
	PCF50616_REG_RECC1	  = 0x18,
	PCF50616_REG_PSSC1	  = 0x19,
	PCF50616_REG_PSSC2	  = 0x1A,
	PCF50616_REG_D1C	  = 0x1B,
	PCF50616_REG_D2C	  = 0x1C,
	PCF50616_REG_D3C	  = 0x1D,
	PCF50616_REG_D4C	  = 0x1E,
	PCF50616_REG_D5C	  = 0x1F,
	PCF50616_REG_D6C	  = 0x20,
	PCF50616_REG_D7C	  = 0x21,
	PCF50616_REG_USBC	  = 0x22,
	PCF50616_REG_HCC	  = 0x23,
	PCF50616_REG_IOC	  = 0x24,
	PCF50616_REG_LPC	  = 0x25,
	/*					  = 0x26, Reserved */
	PCF50616_REG_LCC	  = 0x27,
	PCF50616_REG_RF1C	  = 0x28,
	PCF50616_REG_RF2C	  = 0x29,
	PCF50616_REG_RFGAIN	  = 0x2A,
	PCF50616_REG_LDOPDCTL = 0x2B,
	PCF50616_REG_LDOSWCTL = 0x2C,
	PCF50616_REG_DCD1C1	  = 0x2D,
	PCF50616_REG_DCD1C2	  = 0x2E,
	PCF50616_REG_DCD1DVS1 = 0x2F,
	PCF50616_REG_DCD1DVS2 = 0x30,
	PCF50616_REG_DCD1DVS3 = 0x31,
	PCF50616_REG_DCD1DVS4 = 0x32,
	/*					  = 0x33, Reserved */
	PCF50616_REG_DCD2C1	  = 0x34,
	PCF50616_REG_DCD2C2	  = 0x35,
	PCF50616_REG_DCD2DVS1 = 0x36,
	PCF50616_REG_DCD2DVS2 = 0x37,
	PCF50616_REG_DCD2DVS3 = 0x38,
	PCF50616_REG_DCD2DVS4 = 0x39,
	/*					  = 0x3A, Reserved */
	PCF50616_REG_BVMC     = 0x3B,
	PCF50616_REG_SIMREGC1 = 0x3C,
	PCF50616_REG_SIMREGS1 = 0x3D,
	PCF50616_REG_CBCC1    = 0x3E,
	PCF50616_REG_CBCC2    = 0x3F,
	PCF50616_REG_CBCC3    = 0x40,
	PCF50616_REG_CBCC4    = 0x41,
	PCF50616_REG_CBCC5    = 0x42,
	PCF50616_REG_CBCC6    = 0x43,
	PCF50616_REG_CBCS1    = 0x44,
	PCF50616_REG_BBCC     = 0x45,
	PCF50616_REG_RECC2    = 0x46,
	__NUM_PCF50616_REGS
};

enum pcf50616_reg_int1 {
	PCF50616_INT1_LOWBAT	= 0x01,	/* BVM detected low battery voltage */
	PCF50616_INT1_SECOND	= 0x02,	/* RTC periodic second interrupt */
	PCF50616_INT1_MINUTE	= 0x04,	/* RTC periodic minute interrupt */
	PCF50616_INT1_ALARM		= 0x08,	/* RTC alarm time reached */
	PCF50616_INT1_ONKEYR	= 0x10, /* ONKEY rising edge */
	PCF50616_INT1_ONKEYF	= 0x20, /* ONKEY falling edge */
	PCF50616_INT1_ONKEY1S	= 0x40, /* ONKEY pressed at least 1s */
	PCF50616_INT1_HIGHTMP	= 0x80,	/* THS detected high temperature condition */
};

enum pcf50616_reg_int2 {
	PCF50616_INT2_EXTONR	= 0x01, /* rising edge detected on EXTON_N input */
	PCF50616_INT2_EXTONF	= 0x02, /* falling edge detected on EXTON_N input */
	PCF50616_INT2_RECLF		= 0x04, /* Voltage on REC2_N < Vth low */
	PCF50616_INT2_RECLR		= 0x08, /* Voltage on REC2_N > Vth low */
	PCF50616_INT2_RECHF		= 0x10, /* Voltage on REC2_N < Vth high */
	PCF50616_INT2_RECHR		= 0x20, /* Voltage on REC2_N > Vth high */
	PCF50616_INT2_VMAX		= 0x40, /* Charger switwhes from constant current to 
									   constant voltage */
	PCF50616_INT2_CHGWD		= 0x80, /* Charger watchdog timer will expire in 10s */
};

enum pcf50616_reg_int3 {
	PCF50616_INT3_SIMUV     = 0x01, /* SIMREG reported a SIMVCC under-voltage error; 
									   SIM card deactivated */
	PCF50616_INT3_INSERT    = 0x02, /* SIM card has been inserted */
	PCF50616_INT3_EXTRACT   = 0x04, /* SIM card has been extract */
	PCF50616_INT3_OVPCHGON  = 0x08, /* Charging is continued, after OVP condition has 
									   been resolved */
	PCF50616_INT3_OVPCHGOFF = 0x10, /* Charging is stopped due OVP condition */
	PCF50616_INT3_SIMRDY    = 0x20, /* SIMVCC ready; Vsimvcc > Vth(simuv) */
	PCF50616_INT3_MCHGINS   = 0x40, /* charger has been connected (to VCHG pin)*/
	PCF50616_INT3_MCHGRM    = 0x80, /* charger has been removed (from VCHG pin)*/
};

enum pcf50616_reg_int4 {
	PCF50616_INT4_CHGRES	= 0x01, /* battery voltage < autores voltage threshold */
	PCF50616_INT4_THLIMON	= 0x02, /* charger temperature limiting activated */
	PCF50616_INT4_THLIMOFF	= 0x04, /* charger temperature limiting deactivated */
	PCF50616_INT4_BATFUL	= 0x08, /* fully charged battery */
	/*                      = 0x10, Reserved */
	/*                      = 0x20, Reserved */
	PCF50616_INT4_UCHGINS	= 0x40, /* USB charger connected */
	PCF50616_INT4_UCHGRM	= 0x80, /* USB charger removed */
};

enum pcf50616_reg_oocc {
	PCF50616_OOCC_GO_STDBY		= 0x01, /* initiate Active-to-Standby state 
										   transition */
	PCF50616_OOCC_TOT_RST		= 0x02, /* timeout timer reset */
	PCF50616_OOCC_MICB_EN		= 0x04, /* MBGEN enable */
	PCF50616_OOCC_WDT_RST		= 0x08, /* watchdog timer reset*/	
	PCF50616_OOCC_RTC_WAK		= 0x10, /* RTC alarm wake-up condition */
	PCF50616_OOCC_REC_EN		= 0x20, /* REC accessory detection enable */
	PCF50616_OOCC_REC_MOD	    = 0x40, /* REC mode */
	PCF50616_OOCC_BATRM_EN   	= 0x80, /* automatic restart control*/
};

enum pcf50616_reg_oocs1 {
	PCF50616_OOCS1_ONKEY		= 0x01, /* ONKEY_N pin status */
	PCF50616_OOCS1_EXTON		= 0x02, /* EXTON status */
	PCF50616_OOCS1_RECL			= 0x04, /* REC status ins relation Vth(l) */
	PCF50616_OOCS1_BATOK		= 0x08, /* main battery status */
	PCF50616_OOCS1_BACKOK		= 0x10, /* backup battery status */
	PCF50616_OOCS1_MCHGOK		= 0x20, /* main charger present? */
	PCF50616_OOCS1_TEMPOK		= 0x40, /* junction temperature status */
	PCF50616_OOCS1_RECH			= 0x80, /* REC status in relation Vth(h) */
};

enum pcf50616_reg_oocs2 {
	PCF50616_OOCS2_WDTEXP		= 0x01, /* watchdog timer status */
	PCF50616_OOCS2_BATRMSTAT	= 0x02, /* return to the active state due to battery 
										   removal */
	/*							= 0x04, Reserved */
	PCF50616_OOCS2_UCHG_OK		= 0x08, /* main battery status */
	/*							= 0x10, Reserved */
	/*							= 0x20, Reserved */
	/*							= 0x40, Reserved */
	/*							= 0x80, Reserved */
};

#define PCF50616_DEBC_DEBOUNCE_MASK = 0x07

enum pcf50616_reg_debc{
	PCF50616_DEBC_NODEBOUNCE	= 0x00, /* No Debounce */
	PCF50616_DEBC_14MS_DEB		= 0x01, /* Debounce=14ms */
	PCF50616_DEBC_62MS_DEB		= 0x02, /* Debounce=62ms */
	PCF50616_DEBC_500MS_DEB		= 0x03, /* Debounce=500ms */
	PCF50616_DEBC_1S_DEB		= 0x04, /* Debounce=1s */
	PCF50616_DEBC_2S_DEB		= 0x05, /* Debounce=2s */
	/*							= 0x08, Reserved */
	/*							= 0x10, Reserved */
	/*							= 0x20, Reserved */
	/*							= 0x40, Reserved */
	/*							= 0x80, Reserved */
};

enum pcf50616_reg_hcreg{
	PCF50616_HCC_HCREGVOUT_1V8	= 0x11, /* 1.8V */
	PCF50616_HCC_HCREGVOUT_2V6	= 0x13, /* 2.6V */
	PCF50616_HCC_HCREGVOUT_3V0	= 0x15, /* 3.0V */
	PCF50616_HCC_HCREGVOUT_3V2	= 0x17, /* 3.2V */
};

enum pcf50616_reg_cbcc1 {
    PCF50616_CBCC1_CHGENA		= 0x01, /* VCHG Enable */
    PCF50616_CBCC1_USBENA		= 0x03, /* UCHG Enable, VCHG must be enable as well */
	PCF50616_CBCC1_AUTOCC		= 0x04,	/* Automatic CC mode */
	PCF50616_CBCC1_AUTOSTOP		= 0x08,
	PCF50616_CBCC1_AUTORES_OFF	= 0x00, /* automatic resume OFF */
	PCF50616_CBCC1_AUTORES_ON30	= 0x10, /* automatic resume ON, Vmax-3.0% */
	PCF50616_CBCC1_AUTORES_ON60	= 0x20, /* automatic resume ON, Vmax-6.0% */
	PCF50616_CBCC1_AUTORES_ON45	= 0x30, /* automatic resume ON, Vmax-4.5% */
	PCF50616_CBCC1_WDRST		= 0x40,	/* CBC watchdog Reset */
	PCF50616_CBCC1_WDTIME_5H	= 0x80, /* Maximum charge time */
};
#define PCF50616_CBCC1_AUTORES_MASK	  0x30

enum pcf50616_reg_cbcc2 {
	PCF50616_CBCC2_REVERSE		= 0x01, /* Activate revere mode  */
	PCF50616_CBCC2_VBATCOND_3	= 0x02, /* 3.0V for Li-ion/Li-pol batteries */
	PCF50616_CBCC2_VBATCOND_33	= 0x04, /* 3.3V for NiMH  batteries */
	PCF50616_CBCC2_VMAX_400		= 0x08, /* Wmax 4.00 V */
	PCF50616_CBCC2_VMAX_402		= 0x10, /* Wmax 4.02 V */
	PCF50616_CBCC2_VMAX_404		= 0x18, /* Wmax 4.04 V */
	PCF50616_CBCC2_VMAX_406		= 0x20, /* Wmax 4.06 V */
	PCF50616_CBCC2_VMAX_408		= 0x28, /* Wmax 4.08 V */
	PCF50616_CBCC2_VMAX_410		= 0x30, /* Wmax 4.10 V */
	PCF50616_CBCC2_VMAX_412		= 0x38, /* Wmax 4.12 V */
	PCF50616_CBCC2_VMAX_414		= 0x40, /* Wmax 4.14 V */
	PCF50616_CBCC2_VMAX_416		= 0x48, /* Wmax 4.16 V */
	PCF50616_CBCC2_VMAX_418		= 0x50, /* Wmax 4.18 V */
	PCF50616_CBCC2_VMAX_420		= 0x58, /* Wmax 4.20 V */
	PCF50616_CBCC2_VMAX_422		= 0x60, /* Wmax 4.22 V */
	PCF50616_CBCC2_VMAX_424		= 0x68, /* Wmax 4.24 V */
	PCF50616_CBCC2_VMAX_426		= 0x70, /* Wmax 4.26 V */
	PCF50616_CBCC2_VMAX_428		= 0x78, /* Wmax 4.28 V */
	PCF50616_CBCC2_VMAX_430		= 0x80, /* Wmax 4.30 V */
	PCF50616_CBCC2_VMAX_465		= 0x88, /* Wmax 4.65 V (NiMH only) */
};
#define PCF50616_CBCC2_VBATMAX_MASK	  0xF8

enum pcf50616_reg_cbcc5 {
	PCF50616_CBCC5_CBCMOD_DIRECT_CONTROL		= 0x40, /* direct control by PWREN4 */
	PCF50616_CBCC5_CBCMOD_NO_DIRECT_CONTROL		= 0xE0, /* No direct control CBC is enabled */
};

#define PCF50616_CBCC5_TRICKLE_MASK	0x1F
#define PCF50616_CBCC5_CBCMOD_MASK	0xE0

enum pcf50616_reg_cbcc6 {
    PCF50616_CBCC6_OVPENA		= 0x01, /* Control for over-voltage protection (OVP) */
	PCF50616_CBCC6_CVMOD		= 0x02, /* CV-only mode selection (VCHG charge path) */
	/*                          = 0x04, Reserved */
	/*                          = 0x08, Reserved */
	/*                          = 0x10, Reserved */
	/*                          = 0x20, Reserved */
	/*                          = 0x40, Reserved */
	/*                          = 0x80, Reserved */
};

enum pcf50616_reg_cbcs1 {
	PCF50616_CBCS1_BATFUL		= 0x01, /* battery full */
	PCF50616_CBCS1_TLMT			= 0x02, /* Thermal limiting active */
	PCF50616_CBCS1_WDEXP		= 0x04, /* CBC watchdog timer status */
	PCF50616_CBCS1_ILMT			= 0x08, /* charge current status */
	PCF50616_CBCS1_VLMT			= 0x10, /* battery voltage status */
	PCF50616_CBCS1_CHGOVP		= 0x20, /* over voltage protection status */
	PCF50616_CBCS1_RESSTAT		= 0x40, /* automatic resume status */
	/*							= 0x80, Reserved */ 
};

enum pcf50616_reg_bbcc {
	PCF50616_BBCC_BBCE		= 0x01, /* enable backup battery charger */
	PCF50616_BBCC_BBCR		= 0x02, /* bypass output resistor */
	PCF50616_BBCC_BBCC_50	= 0x00, /* select backup battery charge current 50uA */
	PCF50616_BBCC_BBCC_100	= 0x04, /* select backup battery charge current 50uA */
	PCF50616_BBCC_BBCC_200	= 0x08, /* select backup battery charge current 50uA */
	PCF50616_BBCC_BBCC_400	= 0x0C, /* select backup battery charge current 50uA */
	PCF50616_BBCC_BBCV		= 0x10, /* select limiting voltage for backup battery 
									   charger*/
	PCF50616_BBCC_BBCL		= 0x08, /* linear regulator mode */
	PCF50616_BBCC_BBCSTB	= 0x0C, /* BBC activity in standby state */
	/*                      = 0x08 Reserved */
};

#define PCF50616_BBCC_CURRENT_MASK 0x0C
#define PCF50616_BBCC_VOLTAGE_MASK 0x10

enum pcf50616_regulator_pwen {
PCF50616_REGULATOR_PWEN1 =0x1,
PCF50616_REGULATOR_PWEN2 =0x2,
PCF50616_REGULATOR_PWEN3 =0x4,
PCF50616_REGULATOR_PWEN4 =0x8,
};

#define PCF50616_REGULATOR_PWEN_MASK	0x04

enum pcf50616_regulator_enable {
	PCF50616_REGULATOR_OFF				= 0x00,
	PCF50616_REGULATOR_ECO_ON_ON_ON		= 0x20,
    PCF50616_REGULATOR_ECO				= 0x40,
    PCF50616_REGULATOR_OFF_ON_ON_OFF	= 0x60,
	PCF50616_REGULATOR_OFF_OFF_ON_ON	= 0x80,
	PCF50616_REGULATOR_ECO_ECO_ON_ON	= 0xA0,
	PCF50616_REGULATOR_ECO_ON_ON_ECO	= 0xC0,
	PCF50616_REGULATOR_ON				= 0xE0,
	PCF50616_REGU_USB_ON				= 0x40,
	PCF50616_REGU_USB_ECO				= 0x80,
};

#define PCF50616_REGULATOR_ON_MASK	0xE0
#define PCF50616_REGU_USB_ON_MASK	0x40
#define PCF50616_REGU_USB_ECO_MASK	0x80

enum pcf50616_reg_pssc1 {
	PCF50616_PSSC1_D1PH2	= 0x01,
	PCF50616_PSSC1_D2PH2	= 0x02,
	PCF50616_PSSC1_D3PH2	= 0x04,
	PCF50616_PSSC1_D4PH2	= 0x08,
	PCF50616_PSSC1_IOPH2	= 0x10,
	PCF50616_PSSC1_LPPH2	= 0x20,
	PCF50616_PSSC1_RF1PH2	= 0x40,
	PCF50616_PSSC1_RF2PH2	= 0x80,
};

enum pcf50616_reg_pssc2 {
	PCF50616_PSSC2_LCPH2	= 0x01,
	PCF50616_PSSC2_HCPH2	= 0x02,
	PCF50616_PSSC2_D5PH2	= 0x04,
	PCF50616_PSSC2_D6PH2	= 0x08,
	PCF50616_PSSC2_D7PH2	= 0x10,
	PCF50616_PSSC2_DCD1PH2	= 0x20,
	PCF50616_PSSC2_DCD2PH2	= 0x40,
	/*							= 0x80 Reserved */
};

enum pcf50616_reg_usbc {
	PCF50616_USBC_USBSWCTL_NOCHGUSB	= 0x00, /* Switch is ON when USB regulator is enabled , no automatic control by charger or USB detection*/
   	PCF50616_USBC_USBSWCTL_CHG		= 0x01, /* Switch is ON when USB regulator is enabled and charger connected */
   	PCF50616_USBC_USBSWCTL_USB		= 0x02, /* Switch is ON when USB regulator is enabled and USB connected */
   	PCF50616_USBC_USBSWCTL_CHGUSB	= 0x03, /* Switch is ON when USB regulator is enabled and aither charger or USB connected */
   	PCF50616_USBC_USBSWENA			= 0x04, /* USB switch enabled, op mode depends on usbswctl control bits */
   	PCF50616_USBC_USBSWPD			= 0x08, /* pull down is connected when USB switch is OFF*/
   	PCF50616_USBC_USBREGCTL_NOCHGUSB= 0x00, /* USB regulator is enabled, no automatic control by charger or USB detection */
   	PCF50616_USBC_USBREGCTL_CHG		= 0x10, /* USB regulator is enabled when charger is connected */
   	PCF50616_USBC_USBREGCTL_USB		= 0x20, /* USB regulator is enabled when USB is connected */
   	PCF50616_USBC_USBREGCTL_CHGUSB	= 0x30, /* USB regulator is enabled either charger is connectedt or USB is connected */
   	PCF50616_USBC_USBREGENA			= 0x40, /* USB regulator is ON */
   	PCF50616_USBC_USBREGECO			= 0x80, /* USB regulator is ECO */
};

enum pcf50616_reg_dcd1c1 {
	PCF50616_DCD1C1_DCD1PWM		= 0x01, /* Configuration in ON mode */ 
	PCF50616_DCD1C1_DCD1DVSENA	= 0x02, /* DVS control */
	PCF50616_DCD1C1_DCD1ENA		= 0x04, /* enable for DCD1 converter */
	/*							= 0x08 Reserved */
	/*							= 0x10 Reserved */
	/*							= 0x20 Reserved */
	/*							= 0x40 Reserved */
	/*							= 0x80 Reserved */
};

enum pcf50616_reg_dcd2c1 {
	PCF50616_DCD2C1_DCD2PWM		= 0x01, /* Configuration in ON mode */ 
	PCF50616_DCD2C1_DCD2DVSENA	= 0x02, /* DVS control */
	PCF50616_DCD2C1_DCD2ENA		= 0x04, /* enable for DCD2 converter */
	/*							= 0x08 Reserved */
	/*							= 0x10 Reserved */
	/*							= 0x20 Reserved */
	/*							= 0x40 Reserved */
	/*							= 0x80 Reserved */
};


/* macro to compute setting according to mv */
#define PCF50616_RFREG_mV(V) ((V-900)/100)
#define PCF50616_DREG_mV(V) ((V-900)/100)
#define PCF50616_DCD_mV(V) ((V-600)/25)
#define PCF50616_LCC_mV(V) ((V < 1400) ? ((V-600)/50) : ((V+200)/100))
#define PCF50616_DCDxCy_ECO_MODE_SELECTION	0x80


/* this is to be provided by the board implementation */
extern const u_int8_t pcf50616_initial_regs[__NUM_PCF50616_REGS];


#endif /* _PCF50606_H */

