/*
 * STE PCF50623 Power Managemnt Unit (PMU) driver
 * Copyright (C) 2010 ST-Ericsson
 * Author: Olivier Clergeaud <olivier.clergeaud@stericsson.com>
 *
 */

#ifndef _PCF50623_H
#define _PCF50623_H

enum pfc50623_regs {
	PCF50623_REG_ID		= 0x00,
	PCF50623_REG_INT1	= 0x01,	/* Interrupt Status */
	PCF50623_REG_INT2	= 0x02,	/* Interrupt Status */
	PCF50623_REG_INT3	= 0x03,	/* Interrupt Status */
	PCF50623_REG_INT4	= 0x04,	/* Interrupt Status */
	PCF50623_REG_INT1M	= 0x05,	/* Interrupt Mask */
	PCF50623_REG_INT2M	= 0x06,	/* Interrupt Mask */
	PCF50623_REG_INT3M	= 0x07,	/* Interrupt Mask */
	PCF50623_REG_INT4M	= 0x08,	/* Interrupt Mask */
	/* = 0x09, Reserved */
	/* = 0x0A, Reserved */
    /* = 0x0B, Reserved */
	/* = 0x0C, Reserved */
	/* = 0x0D, Reserved */
	/* = 0x0E, Reserved */
	/* = 0x0F Reserved */
	/* = 0x10, Reserved */
	/* = 0x11, Reserved */
	/* = 0x12, Reserved */
	/* = 0x13, Reserved */
	PCF50623_REG_OOCC1	= 0x14,
	PCF50623_REG_OOCC2	= 0x15,
	PCF50623_REG_OOCPH	= 0x16,
	PCF50623_REG_OOCS	= 0x17,
	PCF50623_REG_BVMC	= 0x18,
	PCF50623_REG_RECC1	= 0x19,
	PCF50623_REG_RECC2	= 0x1a,
	PCF50623_REG_RECS	= 0x1b,
	PCF50623_REG_RTC1	= 0x1c,
	PCF50623_REG_RTC2	= 0x1d,
	PCF50623_REG_RTC3	= 0x1e,
	PCF50623_REG_RTC4	= 0x1f,
	PCF50623_REG_RTC1A	= 0x20,
	PCF50623_REG_RTC2A	= 0x21,
	PCF50623_REG_RTC3A	= 0x22,
	PCF50623_REG_RTC4A	= 0x23,
	PCF50623_REG_CBCC1	= 0x24,
	PCF50623_REG_CBCC2	= 0x25,
	PCF50623_REG_CBCC3	= 0x26,
	PCF50623_REG_CBCC4	= 0x27,
	PCF50623_REG_CBCC5	= 0x28,
	PCF50623_REG_CBCC6	= 0x29,
	PCF50623_REG_CBCS1	= 0x2a,
	PCF50623_REG_CBCS2	= 0x2b,
	PCF50623_REG_BBCC1	= 0x2c,
	PCF50623_REG_PWM1S	= 0x2d,
	PCF50623_REG_PWM1D	= 0x2e,
	PCF50623_REG_PWM2S	= 0x2f,
	PCF50623_REG_PWM2D	= 0x30,
	PCF50623_REG_LED1C	= 0x31,
	PCF50623_REG_LED2C	= 0x32,
	PCF50623_REG_LEDCC	= 0x33,
	/* = 0x34, Reserved */
	/* = 0x35, Reserved */
    /* = 0x36, Reserved */
	/* = 0x37, Reserved */
	/* = 0x38, Reserved */
	/* = 0x39, Reserved */
	/* = 0x3A, Reserved */
	/* = 0x3B, Reserved */
	/* = 0x3C, Reserved */
	/* = 0x3D, Reserved */
	/* = 0x3E, Reserved */
	/* = 0x3F, Reserved */
	PCF50623_REG_GPIO1C1= 0x40,
	/* = 0x41, Reserved */
	/* = 0x42, Reserved */
	PCF50623_REG_GPIO2C1= 0x43,
	/* = 0x44, Reserved */
	/* = 0x45, Reserved */
	PCF50623_REG_GPIO3C1= 0x46,
	/* = 0x47, Reserved */
	/* = 0x48, Reserved */
	PCF50623_REG_GPIO4C1= 0x49,
	/* = 0x4a, Reserved */
	/* = 0x4b, Reserved */
	PCF50623_REG_GPIO5C1= 0x4c,
	/* = 0x4d, Reserved */
	/* = 0x4e, Reserved */
	PCF50623_REG_GPIO6C1= 0x4f,
	/* = 0x50, Reserved */
	/* = 0x51, Reserved */
	PCF50623_REG_GPO1C1	= 0x52,
	/* = 0x53, Reserved */
	/* = 0x54, Reserved */
	PCF50623_REG_GPO2C1	= 0x55,
	/* = 0x56, Reserved */
	/* = 0x57, Reserved */
	PCF50623_REG_GPO3C1	= 0x58,
	/* = 0x59, Reserved */
	/* = 0x5a, Reserved */
	/* = 0x5b, Reserved */
	/* = 0x5c, Reserved */
	/* = 0x5d, Reserved */
	PCF50623_REG_D1REGC1= 0x5e,
	PCF50623_REG_D1REGC2= 0x5f,
	PCF50623_REG_D1REGC3= 0x60,
	PCF50623_REG_D2REGC1= 0x61,
	PCF50623_REG_D2REGC2= 0x62,
	PCF50623_REG_D2REGC3= 0x63,
	PCF50623_REG_D3REGC1= 0x64,
	PCF50623_REG_D3REGC2= 0x65,
	PCF50623_REG_D3REGC3= 0x66,
	PCF50623_REG_D4REGC1= 0x67,
	PCF50623_REG_D4REGC2= 0x68,
	PCF50623_REG_D4REGC3= 0x69,
	PCF50623_REG_D5REGC1= 0x6a,
	PCF50623_REG_D5REGC2= 0x6b,
	PCF50623_REG_D5REGC3= 0x6c,
	PCF50623_REG_D6REGC1= 0x6d,
	PCF50623_REG_D6REGC2= 0x6e,
	PCF50623_REG_D6REGC3= 0x6f,
	PCF50623_REG_D7REGC1= 0x70,
	PCF50623_REG_D7REGC2= 0x71,
	PCF50623_REG_D7REGC3= 0x72,
	/* = 0x73, Reserved */
	/* = 0x74, Reserved */
	/* = 0x75, Reserved */
	PCF50623_REG_RF1REGC1	= 0x76,
	PCF50623_REG_RF1REGC2	= 0x77,
	PCF50623_REG_RF1REGC3	= 0x78,
	PCF50623_REG_RF2REGC1	= 0x79,
	PCF50623_REG_RF2REGC2	= 0x7a,
	PCF50623_REG_RF2REGC3	= 0x7b,
	PCF50623_REG_RF3REGC1	= 0x7c,
	PCF50623_REG_RF3REGC2	= 0x7d,
	PCF50623_REG_RF3REGC3	= 0x7e,
	PCF50623_REG_RF4REGC1	= 0x7f,
	PCF50623_REG_RF4REGC2	= 0x80,
	PCF50623_REG_RF4REGC3	= 0x81,
	PCF50623_REG_IOREGC1	= 0x82,
	PCF50623_REG_IOREGC2	= 0x83,
	PCF50623_REG_IOREGC3	= 0x84,
	PCF50623_REG_USBREGC1	= 0x85,
	PCF50623_REG_USBREGC2	= 0x86,
	PCF50623_REG_USBREGC3	= 0x87,
	PCF50623_REG_USIMREGC1	= 0x88,
	/* = 0x89, Reserved */
	/* = 0x8a, Reserved */
	/* = 0x8b, Reserved */
	/* = 0x8c, Reserved */
	/* = 0x8d, Reserved */
	PCF50623_REG_HCREGC1	= 0x8E,
	PCF50623_REG_HCREGC2	= 0x8F,
	PCF50623_REG_HCREGC3	= 0x90,
	PCF50623_REG_DCD1C1		= 0x91,
	PCF50623_REG_DCD1C2		= 0x92,
	PCF50623_REG_DCD1C3		= 0x93,
	/* = 0x94, Reserved */
	PCF50623_REG_DCD1DVM1	= 0x95,
	PCF50623_REG_DCD1DVM2	= 0x96,
	PCF50623_REG_DCD1DVM3	= 0x97,
	PCF50623_REG_DCD1DVM4	= 0x98,
	PCF50623_REG_DCD2C1		= 0x99,
	PCF50623_REG_DCD2C2		= 0x9a,
	PCF50623_REG_DCD2C3		= 0x9b,
	PCF50623_REG_DCD2C4		= 0x9c,
	/* = 0x9D, Reserved */
	/* = 0x9E, Reserved */
	/* = 0x9F, Reserved */
	/* = 0xA0, Reserved */
	PCF50623_REG_DCD3C1		= 0xA1,
	PCF50623_REG_DCD3C2		= 0xA2,
	PCF50623_REG_DCD3C3		= 0xA3,
	PCF50623_REG_DCD3C4		= 0xA4,
	/* = 0xA5, Reserved */
	/* = 0xA6, Reserved */
	/* = 0xA7, Reserved */
	/* = 0xA8, Reserved */
	/* = 0xA9, Reserved */
	/* = 0xAA, Reserved */
	/* = 0xAB, Reserved */
	/* = 0xAC, Reserved */
	/* = 0xAD, Reserved */
	/* = 0xAE, Reserved */
	/* = 0xAF, Reserved */
	PCF50623_REG_GPIOS	= 0xb0,
	/* = 0xB1, Reserved */
	/* = 0xB2, Reserved */
    /* = 0xB3, Reserved */
	/* = 0xB4, Reserved */
	/* = 0xB5, Reserved */
	/* = 0xB6, Reserved */
	PCF50623_REG_USIMDETC	= 0xb7,
	__NUM_PCF50623_REGS
};

enum pcf50623_reg_int1 {
	PCF50623_INT1_LOWBAT	= 0x01,	/* BVM detected low battery voltage */
	PCF50623_INT1_SECOND	= 0x02,	/* RTC periodic second interrupt */
	PCF50623_INT1_MINUTE	= 0x04,	/* RTC periodic minute interrupt */
	PCF50623_INT1_ALARM		= 0x08,	/* RTC alarm time reached */
	PCF50623_INT1_ONKEYR	= 0x10, /* ONKEY rising edge */
	PCF50623_INT1_ONKEYF	= 0x20, /* ONKEY falling edge */
	PCF50623_INT1_ONKEY1S	= 0x40, /* ONKEY pressed at least 1s */
	PCF50623_INT1_HIGHTMP	= 0x80,	/* THS detected high temperature condition */
};

enum pcf50623_reg_int2 {
	PCF50623_INT2_EXTONR	= 0x01, /* rising edge detected on EXTON_N input */
	PCF50623_INT2_EXTONF	= 0x02, /* falling edge detected on EXTON_N input */
	PCF50623_INT2_RECLF		= 0x04, /* Voltage on REC2_N < Vth low */
	PCF50623_INT2_RECLR		= 0x08, /* Voltage on REC2_N > Vth low */
	PCF50623_INT2_RECHF		= 0x10, /* Voltage on REC2_N < Vth high */
	PCF50623_INT2_RECHR		= 0x20, /* Voltage on REC2_N > Vth high */
	PCF50623_INT2_VMAX		= 0x40, /* Charger switwhes from constant current to constant voltage */
	PCF50623_INT2_CHGWD		= 0x80, /* Charger watchdog timer will expire in 10s */
};

enum pcf50623_reg_int3 {
	PCF50623_INT3_CHGRES	= 0x01, /* battery voltage < autores voltage threshold */
	PCF50623_INT3_THLIMON	= 0x02, /* charger temperature limiting activated */
	PCF50623_INT3_THLIMOFF	= 0x04, /* charger temperature limiting deactivated */
	PCF50623_INT3_BATFUL	= 0x08, /* fully charged battery */
	PCF50623_INT3_MCHINS	= 0x10, /* USB charger connected */
	PCF50623_INT3_MCHRM		= 0x20, /* USB charger removed */
	PCF50623_INT3_UCHGINS	= 0x40, /* USB charger connected */
	PCF50623_INT3_UCHGRM	= 0x80, /* USB charger removed */
};

enum pcf50623_reg_int4 {
	PCF50623_INT4_CBCOVPFLT	= 0x01, /* CBC in over-voltage protection */
	PCF50623_INT4_CBCOVPOK	= 0x02, /* CBC no longer in over-voltage protection */
	PCF50623_INT4_SIMUV		= 0x04, /* USIMREG reported an under-voltage error */
	PCF50623_INT4_USIMPRESF	= 0x08, /* falling edge detected on USIMPRES input */
	PCF50623_INT4_USIMPRESR	= 0x10, /* rising edge detected on USIMPRES input */
	PCF50623_INT4_GPIO1EV	= 0x20, /* rising or falling edge detected on GPIO1 input */
	PCF50623_INT4_GPIO3EV	= 0x40, /* rising or falling edge detected on GPIO3 input */
	PCF50623_INT4_GPIO4EV	= 0x80, /* rising or falling edge detected on GPIO4 input */
};

enum pcf50623_reg_oocc1 {
	PCF50623_OOCC1_GO_OFF		= 0x01, /* initiate Active-to-Off state transition */
	PCF50623_OOCC1_GO_HIB		= 0x02, /* initiate Active-to-Hibernate state transition */
	PCF50623_OOCC1_WDT_RST		= 0x04, /* OOC watchdog timer control */
	PCF50623_OOCC1_TOT_RST		= 0x08, /* timeout timer control */
	PCF50623_OOCC1_TSI_WAK		= 0x10, /* touchscreen wake-up condition */
	PCF50623_OOCC1_RTC_WAK		= 0x20, /* RTC alarm wake-up condition */
	PCF50623_OOCC1_RESERVED6	= 0x40, /* reserved */
	PCF50623_OOCC1_LOWBATHIBEN	= 0x80, /* when a low-battery condition is detected in Hibernate state */
};

enum pcf50623_reg_oocc2 {
	PCF50623_OOCC2_NODEBOUNCE	= 0x00, /* No Debounce */
	PCF50623_OOCC2_14MS_DEB		= 0x01, /* Debounce=14ms */
	PCF50623_OOCC2_62MS_DEB		= 0x02, /* Debounce=62ms */
	PCF50623_OOCC2_500MS_DEB	= 0x03, /* Debounce=500ms */
	PCF50623_OOCC2_1S_DEB		= 0x04, /* Debounce=1s */
	PCF50623_OOCC2_2S_DEB		= 0x05, /* Debounce=2s */
};

#define PCF50623_OOCC2_DEBOUNCE_MASK = 0x07

enum pcf50623_reg_oocs {
	PCF50623_OOCS_ONKEY		= 0x01, /* ONKEY_N pin status */
	PCF50623_OOCS_EXTON		= 0x02, /* REC1_N status */
	PCF50623_OOCS_BATOK		= 0x04, /* main battery status */
	PCF50623_OOCS_RESERVED3	= 0x08, /* reserved */
	PCF50623_OOCS_MCHGOK	= 0x10, /* main charger connected? */
	PCF50623_OOCS_UCHGOK	= 0x20, /* USB charger connected? */
	PCF50623_OOCS_TEMPOK	= 0x40, /* junction temperature status */
	PCF50623_OOCS_RESERVED7	= 0x80, /* reserved */
};

enum pcf50623_reg_hcreg{
	PCF50623_HCREG_1V8	= 0x00, /*  */
	PCF50623_HCREG_2V6	= 0x04, /*  */
	PCF50623_HCREG_3V0	= 0x08, /*  */
	PCF50623_HCREG_3V2	= 0x0C, /*  */
};

enum pcf50623_reg_oocwake {
	PCF50623_OOCWAKE_ONKEY		= 0x01,
	PCF50623_OOCWAKE_EXTON1		= 0x02,
	PCF50623_OOCWAKE_EXTON2		= 0x04,
	PCF50623_OOCWAKE_EXTON3		= 0x08,
	PCF50623_OOCWAKE_RTC		= 0x10,
	/* reserved */
	PCF50623_OOCWAKE_USB		= 0x40,
	PCF50623_OOCWAKE_ADP		= 0x80,
};

enum pcf50623_reg_cbcc1 {
    PCF50623_CBCC1_CHGENA		= 0x01, /* VCHG Enable */
    PCF50623_CBCC1_USBENA		= 0x03, /* UCHG Enable, VCHG must be enable as well */
	PCF50623_CBCC1_AUTOCC		= 0x04,	/* Automatic CC mode */
	PCF50623_CBCC1_AUTOSTOP		= 0x08,
	PCF50623_CBCC1_AUTORES_OFF	= 0x00, /* automatic resume OFF */
	PCF50623_CBCC1_AUTORES_ON30	= 0x10, /* automatic resume ON, Vmax-3.0% */
	PCF50623_CBCC1_AUTORES_ON60	= 0x20, /* automatic resume ON, Vmax-6.0% */
	PCF50623_CBCC1_AUTORES_ON45	= 0x30, /* automatic resume ON, Vmax-4.5% */
	PCF50623_CBCC1_WDRST		= 0x40,	/* CBC watchdog Reset */
	PCF50623_CBCC1_WDTIME_5H	= 0x80, /* Maximum charge time */
};
#define PCF50623_CBCC1_AUTORES_MASK	  0x30

enum pcf50623_reg_cbcc2 {
	PCF50623_CBCC2_OVPENA		= 0x02, /* CBC over-voltage protection */
	PCF50623_CBCC2_SUSPENA		= 0x04, /* USB suspend mode */
	PCF50623_CBCC2_VBATMAX_4V00	= 0x08,
	PCF50623_CBCC2_VBATMAX_4V02 = 0x10,
	PCF50623_CBCC2_VBATMAX_4V04 = 0x18,
	PCF50623_CBCC2_VBATMAX_4V06 = 0x20,
	PCF50623_CBCC2_VBATMAX_4V08 = 0x28,
	PCF50623_CBCC2_VBATMAX_4V10 = 0x30,
	PCF50623_CBCC2_VBATMAX_4V12 = 0x38,
	PCF50623_CBCC2_VBATMAX_4V14 = 0x40,
	PCF50623_CBCC2_VBATMAX_4V16 = 0x48,
	PCF50623_CBCC2_VBATMAX_4V18 = 0x50,
	PCF50623_CBCC2_VBATMAX_4V20 = 0x58,
	PCF50623_CBCC2_VBATMAX_4V22 = 0x60,
	PCF50623_CBCC2_VBATMAX_4V24 = 0x68,
	PCF50623_CBCC2_VBATMAX_4V26 = 0x70,
	PCF50623_CBCC2_VBATMAX_4V28 = 0x78,
	PCF50623_CBCC2_VBATMAX_4V30 = 0x80,
};
#define PCF50623_CBCC2_VBATMAX_MASK	  0xF8

enum pcf50623_reg_cbcc6 {
    PCF50623_CBCC6_CVMOD = 0x02,
};

enum pcf50623_reg_cbcs1 {
	PCF50623_CBCS1_BATFUL		= 0x01,
	PCF50623_CBCS1_TLIM			= 0x02,
	PCF50623_CBCS1_WDEXP		= 0x04,
	PCF50623_CBCS1_ILIMIT		= 0x08,
	PCF50623_CBCS1_VLIMIT		= 0x10,
	PCF50623_CBCS1_RESSTAT		= 0x80,
};

enum pcf50623_reg_cbcs2 {
	PCF50623_CBCS2_USBSUSPSTAT	= 0x04,
	PCF50623_CBCS2_CHGOVP		= 0x08,
};

enum pcf5023_reg_bbcc1 {
	PCF50623_BBCC1_ENABLE   = 0x01,
	PCF50623_BBCC1_RESISTOR = 0x02,
	PCF50623_BBCC1_50UA     = 0x00,
	PCF50623_BBCC1_100UA    = 0x04,
	PCF50623_BBCC1_200UA    = 0x08,
	PCF50623_BBCC1_400UA    = 0x0C,
	PCF50623_BBCC1_2V5      = 0x00,
	PCF50623_BBCC1_3V0      = 0x10,
	PCF50623_BBCC1_HIBERN   = 0x20,
	PCF50623_BBCC1_SWITCH   = 0x40,
};
#define PCF50623_BBCC1_CURRENT_MASK 0x0C
#define PCF50623_BBCC1_VOLTAGE_MASK 0x10

enum pcf50623_reg_adcc1 {
	PCF50623_ADCC1_ADCSTART		= 0x01,
	PCF50623_ADCC1_RES_10BIT	= 0x00,
	PCF50623_ADCC1_RES_8BIT		= 0x02,
	PCF50623_ADCC1_AVERAGE_NO	= 0x00,
	PCF50623_ADCC1_AVERAGE_4	= 0x04,
	PCF50623_ADCC1_AVERAGE_8	= 0x08,
	PCF50623_ADCC1_AVERAGE_16	= 0x0c,

	PCF50623_ADCC1_MUX_BATSNS_RES	= 0x00,
	PCF50623_ADCC1_MUX_BATSNS_SUBTR	= 0x10,
	PCF50623_ADCC1_MUX_ADCIN2_RES	= 0x20,
	PCF50623_ADCC1_MUX_ADCIN2_SUBTR	= 0x30,
	PCF50623_ADCC1_MUX_ADCIN1		= 0x70,
};
#define PCF50623_ADCC1_AVERAGE_MASK	0x0c
#define	PCF50623_ADCC1_ADCMUX_MASK	0xf0

enum pcf50623_reg_adcs3 {
	PCF50623_ADCS3_ADCRDY		= 0x80,
};

#define PCF50623_ADCS3_ADCDAT1L_MASK	0x03
#define PCF50623_ADCS3_ADCDAT2L_MASK	0x0c
#define PCF50623_ADCS3_ADCDAT2L_SHIFT	2
#define PCF50623_ASCS3_REF_MASK		0x70

enum pcf50623_regulator_pwen {
PCF50623_REGULATOR_PWEN1 =0x1,
PCF50623_REGULATOR_PWEN2 =0x2,
PCF50623_REGULATOR_PWEN3 =0x4,
PCF50623_REGULATOR_PWEN4 =0x8
};
enum pcf50623_regulator_gpio {
PCF50623_REGULATOR_GPIO1 =0x1,
PCF50623_REGULATOR_GPIO2 =0x2,
PCF50623_REGULATOR_GPIO3 =0x4,
PCF50623_REGULATOR_GPIO4 =0x8
};
#define PCF50623_REGULATOR_GPIO_PWEN_MASK	0xf

enum pcf50623_regulator_enable {
	PCF50623_REGULATOR_OFF  = 0x00,
	PCF50623_REGULATOR_ECO  = 0x20,
    PCF50623_REGULATOR_ECO_OFF = 0x40,
    PCF50623_REGULATOR_OFF_ON = 0x60,
	PCF50623_REGULATOR_ECO_ON = 0x80,
	PCF50623_REGULATOR_ON_OFF = 0xa0,
	PCF50623_REGULATOR_ON_ECO = 0xc0,
	PCF50623_REGULATOR_ON	= 0xE0,
	PCF50623_REGU_USB_ON		= 0x01,
	PCF50623_REGU_USB_ECO		= 0x02,
};


#define PCF50623_REGULATOR_ON_MASK	0xE0
#define PCF50623_REGU_USB_ON_MASK	0x01
#define PCF50623_REGU_USB_ECO_MASK	0x02

enum pcf50623_regulator_phase {
	PCF50623_REGULATOR_ACTPH1	= 0x00,
	PCF50623_REGULATOR_ACTPH2	= 0x10,
	PCF50623_REGULATOR_ACTPH3	= 0x20,
	PCF50623_REGULATOR_ACTPH4	= 0x30,
};
#define PCF50623_REGULATOR_ACTPH_MASK	0x30

enum pcf50623_reg_gpocfg {
	PCF50623_GPOCFG_GPOSEL_0	= 0x00,
	PCF50623_GPOCFG_GPOSEL_LED_NFET	= 0x01,
	PCF50623_GPOCFG_GPOSEL_SYSxOK	= 0x02,
	PCF50623_GPOCFG_GPOSEL_CLK32K	= 0x03,
	PCF50623_GPOCFG_GPOSEL_ADAPUSB	= 0x04,
	PCF50623_GPOCFG_GPOSEL_USBxOK	= 0x05,
	PCF50623_GPOCFG_GPOSEL_ACTPH4	= 0x06,
	PCF50623_GPOCFG_GPOSEL_1	= 0x07,
	PCF50623_GPOCFG_GPOSEL_INVERSE	= 0x08,
};
#define PCF50623_GPOCFG_GPOSEL_MASK	0x07

enum pcf50623_gpio_out {
	PCF50623_GPIO_HZ = 0x7
};
enum pcf50623_gpio_pol {
	PCF50623_GPIO_IN = 0x80
};
/* macro to compute setting according to mv */
#define PCF50623_RFREG_mV(V) (((V-600)/100)*4)
#define PCF50623_DREG_mV(V) (((V-600)/100)*4)
#define PCF50623_DCD_mV(V) ((V-600)/25)
#define PCF50623_DCD_mA(A) ((A/60)&0xf)
#define PCF50623_DCD_PWM_ONLY 0x10
/* phase field is present with gpio reg config */ 
/* phase field is written with the same default value according to pmu variant */
enum pfc50623_reg_phase {
	PCF50623_PHASE_1 = 0,
	PCF50623_PHASE_2 = 0x40,
	PCF50623_PHASE_3 = 0x80,
	PCF50623_PHASE_4 = 0xc0
};
#define PCF50623_PHASE_MASK 0xc0

/* this is to be provided by the board implementation */
extern const u_int8_t pcf50623_initial_regs[__NUM_PCF50623_REGS];


#endif /* _PCF50606_H */
