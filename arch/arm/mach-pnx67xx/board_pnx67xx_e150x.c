/*
 *	linux/arch/arm/mach-pnx67xx/board_pnx67xx_e150x.c
 *
 * Description: ACER PNX E150x Board specific code
 * created by:	P. Langlais
 * date created: 04 jun 2008
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * HISTORY *************************************************************
 * 2008-07-08 O. Clergeaud
 * Update MMC ressources
 **********************************************************************/

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>

#include <mach/hardware.h>
#include <asm/setup.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <mach/pcf50623.h>
#include <linux/pcf506XX.h>

#include <linux/gns7560.h>
#ifdef CONFIG_SPI_HALL_MOUSE
#include <linux/spi_hallmouse.h>
#endif
#include <asm/mach/flash.h>
/*ACER Ed & nanoradio@20091202 Validate SPI configuration */
#if defined(CONFIG_SPI_HALL_MOUSE) && defined(CONFIG_SPI_NANORADIO_WIFI)
#error "Hall mouse and nanoradio WiFi are mutually exclusive"
#include <linux/spi_hallmouse.h>
#endif
/* END nanoradio@20091202 */

#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <mach/irqs.h>         /* PNX specific IRQ defs */
#include <mach/timer.h>        /* PNX specific timer defs */
#include <mach/gpio.h>
#include <mach/keypad.h>
#include <mach/nand_pnx.h>
#include <mach/dma.h>
#include <mach/mci.h>
#include <mach/spi.h>

#define ACER_L1_K3
#define ACER_L1_CHANGED
#define ACER_L1_KX

#if defined (ACER_L1_K2) || defined (ACER_L1_K3) || defined (ACER_L1_AS1)
#define NBR_MEM_COMBOS 4
#elif defined(CONFIG_MACH_PNX67XX_V2_WAVEB_2GB) || \
        defined(CONFIG_MACH_PNX67XX_V2_WAVEC_2GB) || \
        defined(CONFIG_MACH_PNX67XX_WAVED)
#define NBR_MEM_COMBOS 2
#endif


#include <linux/pnx_bl.h>

#include <mach/modem_sdi.h>
#include <mach/modem_gpio.h>
#include <mach/modem_adc.h>
#include <mach/modem_pwm.h>
#include <mach/scon.h>
#include <mach/extint.h>
#include <mach/io.h>

#include "devices.h"

#ifdef CONFIG_EBI_BUS
	#include <mach/ebi.h>		/* CR LMSqc17654 */

	/* HYPERION EBI virtual addresses */
	#define EBI_VIRT_ADDR					0xF0200000
	#define ADDR_EBI_CS0					0xC0000000
	#define ADDR_EBI_CS1					0xC0080000
	#define ADDR_EBI_CS2					0xC0100000
	#define EBI_SIZE					0x7FFFF
#endif

/**
 * Modem configuration
 * The goal of the following structures is to configure HW settings
 * like SDI timings or GPIO assignation to modem software
 * This will avoid to re-compil a modem SW at each HW change
 */
#ifdef CONFIG_MODEM_BLACK_BOX

/* We use the dummy combo settings until it's been identified */
unsigned int memory_combo_index = NBR_MEM_COMBOS - 1;

struct pnx_modem_sdi_timing pnx_modem_sdi[NBR_MEM_COMBOS][4] = {
/* ACER Erace.Ma@20100128, support samsung 4G3G MCP*/
	/* CONFIG_MEMCOMBO_K524F2HACMB050 */
 	[0] = {
 	/* Set right SDI values to support DDR at 200 Mhz for HFVS*/
		{
 		.timingTrcd = 0x00020002,
 		.timingTrc  = 0x0000000A,
  	.timingTwtr = 0x00000001,
 		.timingTwr  = 0x00000003,
 		.timingTrp  = 0x00020002,
 		.timingTras = 0x00000007,
  	.timingTrrd = 0x00070001,
 		.timingTrfc = 0x00000017,
 		.timingTmrd = 0x00000001,
 		.timingLtcy = 0x00010006,
 		.refresh    = 0x0002061B,
 		.timingTrtw = 0x00000007,
 		.timingTxsr = 0x001A0017,
 		.timingTcke = 0x00000000,
 	},

	/* Set right SDI values to support DDR at 156 Mhz for HFVS*/
 	{
 		.timingTrcd = 0x00020002,
 		.timingTrc  = 0x00000008,
  	.timingTwtr = 0x00000001,
 		.timingTwr  = 0x00000002,
 		.timingTrp  = 0x00020002,
 		.timingTras = 0x00000006,
#ifdef ACER_OPTIMIZE_MEMCOMBO_K524F2HACMB050
 		.timingTrrd = 0x00070001,
#else
  	.timingTrrd = 0x00060001,
#endif
 		.timingTrfc = 0x00000012,
 		.timingTmrd = 0x00000001,
 		.timingLtcy = 0x00010006,
 		.refresh    = 0x000204C3,
 		.timingTrtw = 0x00000007,
 		.timingTxsr = 0x00150012,
 		.timingTcke = 0x00000000,
 	},

 	/* Set right SDI values to support DDR at 104 Mhz for HFVS*/
 	{
 		.timingTrcd = 0x00010001,
 		.timingTrc  = 0x00000005,
    .timingTwtr = 0x00000001,
 		.timingTwr  = 0x00000002,
 		.timingTrp  = 0x00010001,
 		.timingTras = 0x00000004, /* uboot in 0x4*/
#ifdef ACER_OPTIMIZE_MEMCOMBO_K524F2HACMB050
 		.timingTrrd = 0x00070001,
#else
    .timingTrrd = 0x00040001,
#endif
 		.timingTrfc = 0x0000000C,
 		.timingTmrd = 0x00000001,
 		.timingLtcy = 0x00010006,
 		.refresh    = 0x0002032D,
 		.timingTrtw = 0x00000007,
 		.timingTxsr = 0x000E000C,
 		.timingTcke = 0x00000000,
 	},

 	/* Set right SDI values to support DDR at 52 Mhz for HFVS*/
 	{
 		.timingTrcd = 0x00000000,
 		.timingTrc  = 0x00000002,
    .timingTwtr = 0x00000001,
 		.timingTwr  = 0x00000001,
 		.timingTrp  = 0x00000000,
 		.timingTras = 0x00000002,
#ifdef ACER_OPTIMIZE_MEMCOMBO_K524F2HACMB050
 		.timingTrrd = 0x00030000,
#else
    .timingTrrd = 0x00020000,
#endif
 		.timingTrfc = 0x00000006,
 		.timingTmrd = 0x00000001,
 		.timingLtcy = 0x00010006,
 		.refresh    = 0x00020197,
 		.timingTrtw = 0x00000007,
 		.timingTxsr = 0x00070006,
 		.timingTcke = 0x00000000,
 	}
	},

	/*AUx Micron MCP*/
	/* CONFIG_MEMCOMBO_MT29C2G24MAJJAJC */
	{
	/* Set right SDI values to support DDR at 200 Mhz for HFVS*/
		{
		.timingTrcd = 0x30003,
		.timingTrc  = 0xC,
		.timingTwtr = 0,
		.timingTwr  = 4,
		.timingTrp  = 0x30003,
		.timingTras = 8,
		.timingTrrd = 0xB0002,
		.timingTrfc = 0x19,
		.timingTmrd = 1,
		.timingLtcy = 0x10006,
		.refresh    = 0x20619,
		.timingTrtw = 8,
		.timingTxsr = 0x1F001F,
		.timingTcke = 0,
	},

	/* Set right SDI values to support DDR at 156 Mhz for HFVS */
	{
		.timingTrcd = 0x20002,
		.timingTrc  = 9,
		.timingTwtr = 0,
		.timingTwr  = 3,
		.timingTrp  = 0x20002,
		.timingTras = 6,
		.timingTrrd = 0x70001,
		.timingTrfc = 0x14,
		.timingTmrd = 1,
		.timingLtcy = 0x10006,
		.refresh    = 0x2050F,
		.timingTrtw = 8,
		.timingTxsr = 0x190019,
		.timingTcke = 0,
	},

	/* Set right SDI values to support DDR at 104 Mhz for HFVS */
	{
		.timingTrcd = 0x20002,
		.timingTrc  = 7,
		.timingTwtr = 0,
		.timingTwr  = 2,
		.timingTrp  = 0x20002,
		.timingTras = 5,
		.timingTrrd = 0x70001,
		.timingTrfc = 0x10,
		.timingTmrd = 1,
		.timingLtcy = 0x10006,
		.refresh    = 0x203F7,
		.timingTrtw = 7,
		.timingTxsr = 0x140014,
		.timingTcke = 0,
	},

	/* Set right SDI values to support DDR at 52 Mhz for HFVS */
	{
		.timingTrcd = 0x10001,
		.timingTrc  = 3,
		.timingTwtr = 0,
		.timingTwr  = 1,
		.timingTrp  = 0x10001,
		.timingTras = 2,
		.timingTrrd = 0x30000,
		.timingTrfc = 0x8,
		.timingTmrd = 1,
		.timingLtcy = 0x10006,
		.refresh    = 0x201FC,
		.timingTrtw = 7,
		.timingTxsr = 0xA000A,
		.timingTcke = 0,
		}
	},

	/*AUx Micron MCP: CONFIG_MEMCOMBO_MT29C2G48MAJAMAKC */
	{
		/* Set right SDI values to support DDR at 200 Mhz for HFVS */
		{
			.timingTrcd = 0x30003,
			.timingTrc  = 0xC,
			.timingTwtr = 1,
			.timingTwr  = 4,
			.timingTrp  = 0x30003,
			.timingTras = 8,
			.timingTrrd = 0xB0002,
			.timingTrfc = 0x19,
			.timingTmrd = 1,
			.timingLtcy = 0x10006,
			.refresh    = 0x20619,
			.timingTrtw = 8,
			.timingTxsr = 0x1F001F,
			.timingTcke = 0,
		},

		/* Set right SDI values to support DDR at 156 Mhz for HFVS */
		{
			.timingTrcd = 0x20002,
			.timingTrc  = 9,
			.timingTwtr = 1,
			.timingTwr  = 3,
			.timingTrp  = 0x20002,
			.timingTras = 6,
			.timingTrrd = 0x70001,
			.timingTrfc = 0x14,
			.timingTmrd = 1,
			.timingLtcy = 0x10006,
			.refresh    = 0x2050F,
			.timingTrtw = 8,
			.timingTxsr = 0x190019,
			.timingTcke = 0,
		},

		/* Set right SDI values to support DDR at 104 Mhz for HFVS */
		{
			.timingTrcd = 0x20002,
			.timingTrc  = 7,
			.timingTwtr = 1,
			.timingTwr  = 2,
			.timingTrp  = 0x20002,
			.timingTras = 5,
			.timingTrrd = 0x70001,
			.timingTrfc = 0x10,
			.timingTmrd = 1,
			.timingLtcy = 0x10006,
			.refresh    = 0x203F7,
			.timingTrtw = 7,
			.timingTxsr = 0x140014,
			.timingTcke = 0,
		},

		/* Set right SDI values to support DDR at 52 Mhz for HFVS */
		{
			.timingTrcd = 0x10001,
			.timingTrc  = 3,
			.timingTwtr = 1,
			.timingTwr  = 1,
			.timingTrp  = 0x10001,
			.timingTras = 2,
			.timingTrrd = 0x30000,
			.timingTrfc = 0x8,
			.timingTmrd = 1,
			.timingLtcy = 0x10006,
			.refresh    = 0x201FC,
			.timingTrtw = 7,
			.timingTxsr = 0xA000A,
			.timingTcke = 0,
		}
	},

	/* HYNIX 4G/2G  */
	{
		/* Set right SDI values to support DDR at 200 Mhz for HFVS */
		{
			.timingTrcd = 0x00030003,
			.timingTrc  = 0x0000000a,
			.timingTwtr = 0x00000001,
			.timingTwr  = 0x00000003,
			.timingTrp  = 0x00020002,
			.timingTras = 0x00000007,
			.timingTrrd = 0x00070001,
			.timingTrfc = 0x00000011,
			.timingTmrd = 0x00000001,
			.timingLtcy = 0x00010006,
			.refresh    = 0x00020618,
			.timingTrtw = 0x00000007,
			.timingTxsr = 0x001f001b,
			.timingTcke = 0x00000000,
		},

		/* Set right SDI values to support DDR at 156 Mhz for HFVS */
		{
			.timingTrcd = 0x00030003,
			.timingTrc  = 0x00000008,
			.timingTwtr = 0x00000001,
			.timingTwr  = 0x00000003,
			.timingTrp  = 0x00020002,
			.timingTras = 0x00000006,
			.timingTrrd = 0x00060001,
			.timingTrfc = 0x0000000e,
			.timingTmrd = 0x00000001,
			.timingLtcy = 0x00010006,
			.refresh    = 0x000204c1,
			.timingTrtw = 0x00000007,
			.timingTxsr = 0x00180015,
			.timingTcke = 0x00000000,
		},

		/* Set right SDI values to support DDR at 104 Mhz for HFVS */
		{
			.timingTrcd = 0x00020002,
			.timingTrc  = 0x00000005,
			.timingTwtr = 0x00000001,
			.timingTwr  = 0x00000002,
			.timingTrp  = 0x00010001,
			.timingTras = 0x00000004,
			.timingTrrd = 0x00040001,
			.timingTrfc = 0x00000009,
			.timingTmrd = 0x00000001,
			.timingLtcy = 0x00010006,
			.refresh    = 0x0002032b,
			.timingTrtw = 0x00000007,
			.timingTxsr = 0x0010000e,
			.timingTcke = 0x00000000,
		},

		/* Set right SDI values to support DDR at 52 Mhz for HFVS */
		{
			.timingTrcd = 0x00010001,
		.timingTrc  = 0x00000002,
		.timingTwtr = 0x00000001,
		.timingTwr  = 0x00000001,
		.timingTrp  = 0x00000000,
		.timingTras = 0x00000002,
		.timingTrrd = 0x00020000,
		.timingTrfc = 0x00000004,
		.timingTmrd = 0x00000001,
		.timingLtcy = 0x00010006,
		.refresh    = 0x00020196,
		.timingTrtw = 0x00000007,
		.timingTxsr = 0x00080007,
		.timingTcke = 0x00000000,
		}
	}
};

/**
 *  the following structure associated GPIO and modem functionalities
 */

struct pnx_modem_gpio pnx_modem_gpio_assignation = {
	.dvm1 = GPIO_A8,
	.dvm2 = GPIO_A9,	/* warning if WAVED must be initialised */
				/* in usb_suspend field of PMU50616 */
#ifdef CONFIG_DCXO_AFC
	.rf_pdn = GPIO_A22,	/* warning PIN number must be defined as PDN in
				   MUX configuration */
#else
	.rf_pdn = -1,
#endif
	.rf_on = GPIO_A21,
	.rf_clk320 = GPIO_B14,
	.rf_reset = GPIO_A23,
	/*.rf_antdet = GPIO_A15,*/
	.rf_antdet = -1,
	.agps_pwm = -1, /* .agps_pwm = -1, */
};

/**
 *  the following structure associated EXTINT and modem functionalities
 */

/* ACER Bright Lee, 2009/10/14, DO NOT modify this configuration, reserved for RTK Modem { */
struct pnx_modem_extint pnx_modem_extint_assignation = {
	.sim_under_volt = 4,
};
/* } ACER Bright Lee, 2009/10/14 */

struct pnx_extint_config pnx_extint_init_config[] = {
	/* ACER Bright Lee, 2009/10/14, DO NOT modify this configuration, reserved for RTK Modem { */
	/* extint 4 configuration for modem (SIM under voltage feature )*/
	{
		(void __iomem*) EXTINT_CFG4_REG,
		0 |
		EXTINT_SEL_EXTINT |
		EXTINT_POL_NEGATIVE |
		EXTINT_DEBOUNCE_0 |
		EXTINT_MODE_DUAL_EDGE
	},
	/* } ACER Bright Lee, 2009/10/14 */
};

unsigned int pnx_modem_extint_nb = ARRAY_SIZE(pnx_extint_init_config);


/**
 * the next table lists all the GPIO used by the modem side
 * (either in GPIO mode or in feature mode)
 * These will be reserved during GPIO driver initialization
 * to avoid conflict with Linux usage.
 */

unsigned int pnx_modem_gpio_reserved[] = {
	GPIO_A4, /* SIM OFF */

	//Selwyn 2010-07-12 modified for K2 Popnoise[SYScs43005]
	#if defined (ACER_L1_K2) || defined (ACER_L1_AS1)
	GPIO_A6,
	#endif
	//~Selwyn modified

	GPIO_A8, /* DVM1 */
	GPIO_A9, /* DVM2 */
	GPIO_A15, /* RF */
	GPIO_A21, /* RF */
	GPIO_A22, /* RF */
	GPIO_A23, /* RF */
	GPIO_B0, /* DD */
	GPIO_B1, /* DU */
	GPIO_B2, /* FSC */
	GPIO_B3, /* DCL */
	GPIO_B8, /* RFEN0 */
	GPIO_B12, /* RFSIG6 */
	GPIO_B13, /* RFSIG7 */
	GPIO_B14,
	GPIO_B15, /* RFDATA */
	GPIO_C24, /* RF3GSPIEN0 */
	GPIO_C25, /* RF3GSPIDATA */
	GPIO_C26, /* RF3GSPICLK */
	GPIO_C27, /* RFSM_OUT0 */
	GPIO_C28, /* RFSM_OUT1 */
	GPIO_C29, /* RFSM_OUT2 */
	GPIO_D16, /* RF3GGPO9 */
	GPIO_D17, /* RF3GGPO8 */
/* ACER BobIHLee@20100505, support AS1 project*/
#if !defined (ACER_L1_K2) && !defined (ACER_L1_K3)&& !defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
	GPIO_D21, /* Audio IIS */	/* Not used on EVB WaveX */
#endif
/* ACER BobIHLee@20100505, support AS1 project*/
#if !defined (ACER_L1_K2) && !defined (ACER_L1_K3)&& !defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
	GPIO_D22, /* Audio IIS */	/* Not used on EVB WaveX */
	GPIO_D23, /* Audio IIS */	/* Not used on EVB WaveX */
#endif
	GPIO_D25, /* RFSIG3 */
	GPIO_D26, /* RF3GGPO6 */
	GPIO_D27, /* RF3GGPO7 */
	GPIO_D28, /* RF3GGPO5 */
#if !defined (ACER_L1_K3)
	GPIO_D30, /* RF PDN */		/* Not used on EVB WaveX */
#endif
	GPIO_D31, /* RFCLK */
	GPIO_E30 /* RFSM_OUT3 */
};

unsigned int pnx_modem_gpio_reserved_nb = ARRAY_SIZE(pnx_modem_gpio_reserved);


struct pnx_modem_adc pnx_modem_adc_config = {
	.bat_voltage = 0,
	.bat_current = 1,
	.bat_fast_temp = 1,
	.ref_clock_temperature = 2,
	.product_temperature = 2,
	.audio_accessories = 4,
	.bat_type = 3,
};


#ifdef CONFIG_EBI_BUS
/**
 *      EBI Initialisation Values for Synchronous Mode
 */

const struct pnx_ebi_config pnx_ebi_init_tab[NR_EBI] =
{
	/* actually no values provided from HSI;
	 * reset values are kept for all channels */
	{
		/* Main cfg reg.                                    mainCfg */
		{
			(void __iomem*) EBI_DEV0_MAINCFG_REG,
			( 0
			  | EBI_MSIZE_16
			  | EBI_MODE_8080
			  | EBI_BE_OP_CS
			  | EBI_EN_POL_HIGH
			  | EBI_ADV_CFG_4_CYCLES)
		},
		/* Read timing cfg. reg.                            readCfg */
		{
			(void __iomem*) EBI_DEV0_READCFG_REG,
			( 0
			 | (0x02UL << EBI_RC_SHIFT)
			 | (0x0UL << EBI_RH_SHIFT)
			 | (0x1UL << EBI_RS_SHIFT)
			 | (0x1UL << EBI_RT_SHIFT)
			 | (0x1UL << EBI_RRT_SHIFT))
		},
		/* Write timing cfg. reg.                          writeCfg */
		{
			(void __iomem*) EBI_DEV0_WRITECFG_REG,
			( 0
			  | (0x01UL << EBI_WC_SHIFT)
			  | (0x1UL << EBI_WH_SHIFT)
			  | (0x0UL << EBI_WS_SHIFT)
			  | (0x1UL << EBI_WT_SHIFT)
			  | (0x1UL << EBI_WWT_SHIFT))
		},
		/* Burst/page cfg. reg.                            burstCfg */
		{
			(void __iomem*) EBI_DEV0_BURSTCFG_REG,
			( 0
			  | EBI_SIZE_4_DATA
			  | EBI_PRC_PAGE_READ_ACCESS_TIME)
		}
	},
	/* EBI-1 : ChipSelect[1] -------------------------------------------*/
	{
		/* Main cfg reg.                                               mainCfg */
		{
			(void __iomem*) EBI_DEV1_MAINCFG_REG,
			( 0
			  | EBI_MSIZE_16
			  | EBI_MODE_8080
			  | EBI_BE_OP_CS
			  | EBI_EN_POL_HIGH
			  | EBI_ADV_CFG_4_CYCLES)
		},
		/* Read timing cfg. reg.                            readCfg */
		{
			(void __iomem*) EBI_DEV1_READCFG_REG,
			( 0
			 | EBI_RC_READ_ACCESS_TIME
			 | EBI_RH_READ_HOLD_TIME
			 | EBI_RS_READ_SETUP_TIME
			 | EBI_RT_READ_TURNAROUND_TIME
			 | EBI_RRT_READ_TO_READ_TURNAROUND_TIME)
		},
		/* Write timing cfg. reg.                           writeCfg */
		{
			(void __iomem*) EBI_DEV1_WRITECFG_REG,
			( 0
			 | EBI_WC_WRITE_ACCESS_TIME
			 | EBI_WH_WRITE_HOLD_TIME
			 | EBI_WS_WRITE_SETUP_TIME
			 | EBI_WT_WRITE_TURNAROUND_TIME
			 | EBI_WWT_WRITE_TO_WRITE_TURNAROUND_TIME)
		},
		/* Burst/page cfg. reg.                             burstCfg */
		{
			(void __iomem*) EBI_DEV1_BURSTCFG_REG,
			( 0
			  | EBI_SIZE_4_DATA
			  | EBI_PRC_PAGE_READ_ACCESS_TIME)
		}

	},
	/* EBI-2 : ChipSelect[2] --------------------------------------------*/
	{
		/* Main cfg reg.                                               mainCfg */
		{
			(void __iomem*) EBI_DEV2_MAINCFG_REG,
			( 0
			  | EBI_MSIZE_16
			  | EBI_MODE_8080
			  | EBI_BE_OP_CS
			  | EBI_EN_POL_HIGH
			  | EBI_ADV_CFG_4_CYCLES)
		},
		/* Read timing cfg. reg.                            readCfg */
		{
			(void __iomem*) EBI_DEV2_READCFG_REG,
			( 0
			 | EBI_RC_READ_ACCESS_TIME
			 | EBI_RH_READ_HOLD_TIME
			 | EBI_RS_READ_SETUP_TIME
			 | EBI_RT_READ_TURNAROUND_TIME
			 | EBI_RRT_READ_TO_READ_TURNAROUND_TIME)
		},
		/* Write timing cfg. reg.                           writeCfg */
		{
			(void __iomem*) EBI_DEV2_WRITECFG_REG,
			( 0
			 | EBI_WC_WRITE_ACCESS_TIME
			 | EBI_WH_WRITE_HOLD_TIME
			 | EBI_WS_WRITE_SETUP_TIME
			 | EBI_WT_WRITE_TURNAROUND_TIME
			 | EBI_WWT_WRITE_TO_WRITE_TURNAROUND_TIME)
		},
		/* Burst/page cfg. reg.                             burstCfg */
		{
			(void __iomem*) EBI_DEV2_BURSTCFG_REG,
			( 0
			  | EBI_SIZE_4_DATA
			  | EBI_PRC_PAGE_READ_ACCESS_TIME)
		}
	}
};

const struct pnx_config_ebi_to_rtk config_ebi[NR_EBI]=
{
	[0] = {
		.ebi_cs = 0,
		.ebi_phys_addr = ADDR_EBI_CS0,
		.ebi_virt_addr = EBI_VIRT_ADDR,
		.ebi_size = EBI_SIZE,
	},
	{
		.ebi_cs = 1,
		.ebi_phys_addr = ADDR_EBI_CS1,
		.ebi_virt_addr = EBI_VIRT_ADDR,
		.ebi_size = EBI_SIZE,
	},
	{
		.ebi_cs = 2,
		.ebi_phys_addr = ADDR_EBI_CS2,
		.ebi_virt_addr = EBI_VIRT_ADDR,
		.ebi_size = EBI_SIZE,
	}

};
#endif

#endif

/********************************************************************
 * RESTRICTION : Each PWM are independant. PWM mixing is forbiden
 ********************************************************************/

struct pnx_modem_pwm_config pnx_modem_pwm[3] = {
	/* Set PWM1 config -> used for backlight by linux */
	[0] = {
		.pwm_name = PWM1,
		.gpio_number = GPIO_A6,
		.pwm_accessible = PWM_NO_ACCESSIBLE,
	},
	/* Set PWM2 config */
	{
		.pwm_name = PWM2,
		.gpio_number = GPIO_A3,
		.pwm_accessible = PWM_NO_ACCESSIBLE,
	},
	/* Set PWM3 config */
	{
		.pwm_name = PWM3,
		.gpio_number = GPIO_D18, /* GPIO_D18 or GPIO_F2 for copy */
		.pwm_accessible = PWM_NO_ACCESSIBLE,
	},
};

/*******************************************************************************
 * Keyboard device
 *******************************************************************************/
/* Keypad mapping for PNX67xx e150x board */
static int pnx_e150x_keymap[] = {
//selwyn modified
#if defined (ACER_L1_AS1)
	KEY(1, 0, KEY_AGAIN),		/* SELECT */	/* Col 1, Row 0 */
	KEY(2, 0, KEY_SEND),		/* SEND */	/* Col 2, Row 1 */
	KEY(1, 2, KEY_VOLUMEUP),	/* VOL_UP */	/* Col 1, Row 2 */
	KEY(2, 2, KEY_VOLUMEDOWN),	/* VOL_DOWN */	/* Col 2, Row 2 */
/* ACER BobIHLee@20100505, support AS1 project*/
#elif defined (ACER_L1_AU4) || defined (ACER_L1_K2)
/* End BobIHLee@20100505*/
	KEY(0, 0, KEY_LEFT),		/* LEFT */ 	/* Col 0, Row 0 */
	KEY(0, 1, KEY_UP),		/* UP */	/* Col 0, Row 1 */

	KEY(1, 0, KEY_AGAIN),		/* SELECT */	/* Col 1, Row 0 */
	KEY(1, 1, KEY_DOWN),		/* DOWN */	/* Col 1, Row 1 */
	KEY(1, 2, KEY_VOLUMEUP),	/* VOL_UP */	/* Col 1, Row 2 */

	KEY(2, 0, KEY_RIGHT),		/* RIGHT */	/* Col 2, Row 0 */
	KEY(2, 1, KEY_SEND),		/* SEND */	/* Col 2, Row 1 */
	KEY(2, 2, KEY_VOLUMEDOWN),	/* VOL_DOWN */	/* Col 2, Row 2 */
#elif defined (ACER_L1_AU2) || defined (ACER_L1_K3)
	KEY(0, 0, KEY_F24),		/* * */ 	/* Col 0, Row 0 */
	KEY(0, 1, KEY_F21),		/* 0 + */	/* Col 0, Row 1 */
	KEY(0, 2, KEY_RIGHTCTRL),	/* # */		/* Col 0, Row 2 */
	KEY(0, 3, KEY_LEFTALT),		/* FN */	/* Col 0, Row 3 */
	KEY(0, 4, KEY_MENU),		/* MENU */	/* Col 0, Row 4 */
	KEY(0, 5, KEY_LEFT),		/* LEFT */	/* Col 0, Row 5 */
	KEY(0, 6, KEY_RIGHT),		/* RIGHT */	/* Col 0, Row 6 */

	KEY(1, 0, KEY_Z),		/* 7 Z */	/* Col 1, Row 0 */
	KEY(1, 1, KEY_X),		/* 8 X */	/* Col 1, Row 1 */
	KEY(1, 2, KEY_C),		/* 9 C */	/* Col 1, Row 2 */
	KEY(1, 3, KEY_VOLUMEUP),
	KEY(1, 4, KEY_F23),		/* DRAWER */	/* Col 1, Row 4 */
	KEY(1, 5, KEY_UP),		/* UP */	/* Col 1, Row 5 */
	KEY(1, 6, KEY_DOWN),		/* DOWN */	/* Col 1, Row 6 */

	KEY(2, 0, KEY_S),		/* 4 S */	/* Col 2, Row 0 */
	KEY(2, 1, KEY_SEND),		/* SEND */	/* Col 2, Row 1 */
	KEY(2, 2, KEY_F),		/* 6 F */	/* Col 2, Row 2 */
	KEY(2, 3, KEY_VOLUMEDOWN),
	KEY(2, 4, KEY_AGAIN),		/* SELECT */	/* Col 2, Row 4 */
	KEY(2, 5, KEY_HOME),		/* HOME */	/* Col 2, Row 5 */
	KEY(2, 6, KEY_BACK),		/* BACK_UP */	/* Col 2, Row 6 */

	KEY(3, 0, KEY_W),		/* 1 W */	/* Col 3, Row 0 */
	KEY(3, 1, KEY_E),		/* 2 E */	/* Col 3, Row 1 */
	KEY(3, 2, KEY_R),		/* 3 R */	/* Col 3, Row 2 */
	KEY(3, 3, KEY_BACKSPACE),	/* Back */	/* Col 3, Row 3 */
	KEY(3, 4, KEY_SPACE),		/* Space */	/* Col 3, Row 4 */
	KEY(3, 5, KEY_ENTER),		/* Enter */	/* Col 3, Row 5 */
	KEY(3, 6, KEY_D),		/* 5 D */	/* Col 3, Row 6 */

	KEY(4, 0, KEY_Q),		/* Q */		/* Col 4, Row 0 */
	KEY(4, 1, KEY_T),		/* T */		/* Col 4, Row 1 */
	KEY(4, 2, KEY_Y),		/* Y */		/* Col 4, Row 2 */
	KEY(4, 3, KEY_U),		/* U */		/* Col 4, Row 3 */
	KEY(4, 4, KEY_I),		/* I */		/* Col 4, Row 4 */
	KEY(4, 5, KEY_O),		/* O */		/* Col 4, Row 5 */
	KEY(4, 6, KEY_F22),		/* SYM */	/* Col 4, Row 6 */

	KEY(5, 0, KEY_A),		/* A */		/* Col 5, Row 0 */
	KEY(5, 1, KEY_G),		/* G */		/* Col 5, Row 1 */
	KEY(5, 2, KEY_H),		/* H */		/* Col 5, Row 2 */
	KEY(5, 3, KEY_J),		/* J */		/* Col 5, Row 3 */
	KEY(5, 4, KEY_K),		/* K */		/* Col 5, Row 4 */
	KEY(5, 5, KEY_L),		/* L */		/* Col 5, Row 5 */
	KEY(5, 6, KEY_DOT),		/* Browser */	/* Col 5, Row 6 */

	KEY(6, 0, KEY_P),		/* P */		/* Col 6, Row 0 */
	KEY(6, 1, KEY_V),		/* V */		/* Col 6, Row 1 */
	KEY(6, 2, KEY_B),		/* B */		/* Col 6, Row 2 */
	KEY(6, 3, KEY_N),		/* N */		/* Col 6, Row 3 */
	KEY(6, 4, KEY_M),		/* M */		/* Col 6, Row 4 */
	KEY(6, 5, KEY_COMMA),		/* . */		/* Col 6, Row 5 */
	KEY(6, 6, KEY_LEFTSHIFT),	/* Caps */	/* Col 6, Row 6 */
#else
	KEY(0, 0, KEY_LEFTSHIFT),	/* * */ 	/* Col 0, Row 0 */
	KEY(0, 1, KEY_F22),		/* 0 + */	/* Col 0, Row 1 */
	KEY(0, 2, KEY_F21),		/* # */		/* Col 0, Row 2 */
	KEY(0, 3, KEY_LEFTALT),		/* FN */	/* Col 0, Row 3 */
	KEY(0, 5, KEY_MENU),		/* MENU */	/* Col 0, Row 5 */
	KEY(0, 6, KEY_BACK),		/* BACK */	/* Col 0, Row 6 */

	KEY(1, 0, KEY_Z),		/* 7 Z */	/* Col 1, Row 0 */
	KEY(1, 1, KEY_X),		/* 8 X */	/* Col 1, Row 1 */
	KEY(1, 2, KEY_C),		/* 9 C */	/* Col 1, Row 2 */
	KEY(1, 3, KEY_VOLUMEUP),
	KEY(1, 5, KEY_SEND),		/* SEND */	/* Col 1, Row 5 */
	KEY(1, 6, KEY_SELECT),		/* SELECT */	/* Col 1, Row 6 */

	KEY(2, 0, KEY_S),		/* 4 S */	/* Col 2, Row 0 */
	KEY(2, 2, KEY_F),		/* 6 F */	/* Col 2, Row 2 */
	KEY(2, 3, KEY_VOLUMEDOWN),
	KEY(2, 5, KEY_F23),		/* AP */	/* Col 2, Row 5 */
	KEY(2, 6, KEY_HOME),		/* HOME */	/* Col 2, Row 6 */

	KEY(3, 0, KEY_W),		/* 1 W */	/* Col 3, Row 0 */
	KEY(3, 1, KEY_E),		/* 2 E */	/* Col 3, Row 1 */
	KEY(3, 2, KEY_R),		/* 3 R */	/* Col 3, Row 2 */
	KEY(3, 3, KEY_BACKSPACE),	/* Back */	/* Col 3, Row 3 */
	KEY(3, 4, KEY_SPACE),		/* Space */	/* Col 3, Row 4 */
	KEY(3, 5, KEY_ENTER),		/* Enter */	/* Col 3, Row 5 */
	KEY(3, 6, KEY_D),		/* 5 D */	/* Col 3, Row 6 */

	KEY(4, 0, KEY_Q),		/* Q */		/* Col 4, Row 0 */
	KEY(4, 1, KEY_T),		/* T */		/* Col 4, Row 1 */
	KEY(4, 2, KEY_Y),		/* Y */		/* Col 4, Row 2 */
	KEY(4, 3, KEY_U),		/* U */		/* Col 4, Row 3 */
	KEY(4, 4, KEY_I),		/* I */		/* Col 4, Row 4 */
	KEY(4, 5, KEY_O),		/* O */		/* Col 4, Row 5 */
	KEY(4, 6, KEY_DOT),		/* SYM */	/* Col 4, Row 6 */

	KEY(5, 0, KEY_A),		/* A */		/* Col 5, Row 0 */
	KEY(5, 1, KEY_G),		/* G */		/* Col 5, Row 1 */
#ifdef CONFIG_SPI_HALL_MOUSE
	KEY(5, 2, KEY_H),		/* H */		/* Col 5, Row 2 */
#else
	KEY(5, 2, KEY_LEFT),		/* H */		/* Col 5, Row 2 */
#endif
#ifdef CONFIG_SPI_HALL_MOUSE
	KEY(5, 3, KEY_J),		/* J */		/* Col 5, Row 3 */
#else
        KEY(5, 3, KEY_UP),		/* J */		/* Col 5, Row 3 */
#endif
#ifdef CONFIG_SPI_HALL_MOUSE
	KEY(5, 4, KEY_K),		/* K */		/* Col 5, Row 4 */
#else
	KEY(5, 4, KEY_RIGHT),		/* K */		/* Col 5, Row 4 */
#endif
	KEY(5, 5, KEY_L),		/* L */		/* Col 5, Row 5 */
	KEY(5, 6, KEY_F24),		/* Browser */	/* Col 5, Row 6 */

	KEY(6, 0, KEY_P),		/* P */		/* Col 6, Row 0 */
	KEY(6, 1, KEY_V),		/* V */		/* Col 6, Row 1 */
	KEY(6, 2, KEY_B),		/* B */		/* Col 6, Row 2 */
#ifdef CONFIG_SPI_HALL_MOUSE
	KEY(6, 3, KEY_N),		/* N */		/* Col 6, Row 3 */
#else
	KEY(6, 3, KEY_DOWN),		/* N */		/* Col 6, Row 3 */
#endif
	KEY(6, 4, KEY_M),		/* M */		/* Col 6, Row 4 */
	KEY(6, 5, KEY_COMMA),		/* . */		/* Col 6, Row 5 */
	KEY(6, 6, KEY_RIGHTCTRL),	/* Caps */	/* Col 6, Row 6 */
#endif
//~selwyn modified
	0 /* end Keypad mapping */
};

static struct resource pnx_e150x_kp_resources[] = {
	[0] = {
		.start = KBS_BASE_ADDR,   /* Physical address */
		.end   = KBS_BASE_ADDR + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_KBS,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct pnx_kbd_platform_data pnx_e150x_keypad_data = {
//selwyn modified
/*ACER, IdaChiang, 20100628, modify K2 row and col value { */
#if defined (ACER_L1_AU4) || defined (ACER_L1_AS1) || defined (ACER_L1_K2)
/* } ACER, IdaChiang, 20100628, modify K2 row and col value */
	.rows		= 3,
	.cols		= 3,
#else
	.rows		= 7,
	.cols		= 7,
#endif
/* ACER Jen chang, 2010/04/11, IssueKeys:AU21.B-1, Modify GPIO setting for K2/K3 PR2 { */
//~selwyn modified
#if defined (ACER_AU2_PR1) || defined (ACER_AU2_PR2) || defined (ACER_L1_AU3)
	.led1_gpio = GPIO_A3,
	.led1_pin_mux = GPIO_MODE_MUX0,
#elif (defined ACER_L1_AU2)
	.led1_gpio = GPIO_D21,
	.led1_pin_mux = GPIO_MODE_MUX1,
	.led2_gpio = GPIO_D30,
	.led2_pin_mux = GPIO_MODE_MUX1,
	.led3_gpio = GPIO_F8,
	.led3_pin_mux = GPIO_MODE_MUX1,
	.led4_gpio = GPIO_F5,
	.led4_pin_mux = GPIO_MODE_MUX1,
#elif (defined ACER_K3_PR1)
	.led1_gpio = GPIO_D21,
	.led1_pin_mux = GPIO_MODE_MUX1,
	.led2_gpio = GPIO_D30,
	.led2_pin_mux = GPIO_MODE_MUX1,
	.led3_gpio = GPIO_F8,
	.led3_pin_mux = GPIO_MODE_MUX1,
#endif
/* } ACER Jen Chang, 2010/04/11*/
	.keymap		= pnx_e150x_keymap,
	.keymapsize	= ARRAY_SIZE(pnx_e150x_keymap),
	.dbounce_delay	= 10,  /* 10 ms */
};

static struct platform_device pnx_e150x_kp_device = {
	.name		= "pnx-keypad",
	.id		= -1,
	.dev		= {
		.platform_data = &pnx_e150x_keypad_data,
	},
	.num_resources	= ARRAY_SIZE(pnx_e150x_kp_resources),
	.resource	= pnx_e150x_kp_resources,
};

#if defined(CONFIG_KEYBOARD_PNX_JOGBALL)
static struct pnx_jogball_platform_data pnx_e150x_jogball_data = {
	.irq_left	= IRQ_EXTINT(14),	// GPIO_A20 for Left
	.irq_down	= IRQ_EXTINT(7),	// GPIO_A13 for Down
	.irq_right	= IRQ_EXTINT(12),	// GPIO_D19 for Right
	.irq_up		= IRQ_EXTINT(22),	// GPIO_D18 for Up
	.irq_sel	= IRQ_EXTINT(6),	// GPIO_A12 for Select
};

static struct platform_device pnx_e150x_jogball_device = {
	.name		= "pnx-jogball",
	.id		= -1,
	.dev		= {
		.platform_data = &pnx_e150x_jogball_data,
	},
//	.num_resources	= ARRAY_SIZE(pnx_e150x_jogball_resources),
//	.resource	= pnx_e150x_jogball_resources,
};
#endif

/****************************************************************************
 * Nand device NFI (partition mapping)
 *
 * Used the 2Gb nand flash.
 * To 'simplify' the mapping, the Nand is divided into 4 areas:
 *
 *       0x10000000 -+----------------------+
 *                   | BBM reservoir area   |     6MB-3*128k = 45 Nand blocks
 *       0x0FA60000 -+----------------------+
 *                   | System Binaries area |    12MB+3*128k   (Modem and Linux)
 *       0x0EE00000 -+----------------------+
 *                   | File Systems area    |   237MB = 256-(6+12+1)
 *       0x00100000 -+----------------------+
 *                   | Boot Stages area     |     1MB
 *       0x00000000 -+----------------------+
 *
 ****************************************************************************/
#if !defined(CONFIG_ANDROID)
#include "mtdparts.h"
#else
#include "mtdparts_android.h"
#endif

struct pnx_nand_combo_info combo_info[] = {
	{ 0xAC, 0xEC }, /* CONFIG_MEMCOMBO_K524F2HACMB050 */
	{ 0xAA, 0x2C }, /* Micron NAND 256MiB 1,8V 8-bit */
	{ 0xA1, 0x20 }, /* ST */
	{ 0xAC, 0xAD }, /* Hynix 4G/2G */
	{ 0xFF, 0xFF }  /* DUMMY COMBO for initialization */
};

struct pnx_nand_timing_data nand_timing_data[] = {
	/* CONFIG_MEMCOMBO_K524F2HACMB050 */
	[0] = {
	.timing_tac = 0xA208,
		.timing_tac_read = 0x0442
	},
	/* CONFIG_MEMCOMBO_MT29C2G24MAJJAJC */
	{
		.timing_tac      = 0xA110,
		.timing_tac_read = 0x0201
	},
	/* CONFIG_MEMCOMBO_MT29C2G48MAJAMAKC */
	{
		.timing_tac      = 0xA210,
		.timing_tac_read = 0x0211
	},
	/* HYNIX 4Gb/2Gb */
	{
		.timing_tac      = 0xA218,
		.timing_tac_read = 0x0422
	},
	/* DUMMY COMBO; use the highest values for identifying the NAND */
	{
		.timing_tac      = 0xA218,
		.timing_tac_read = 0x0442
	}
};

static struct pnx_nand_platform_data nand_data = {
	.timing			= nand_timing_data,
	.combo_index	= &memory_combo_index,
	.combo_info		= combo_info,
	.parts = nand_partitions,
	.nr_parts = ARRAY_SIZE(nand_partitions),
	.usedma = 1,	/* 1 dma and ecc_hw enabled*/
	.dma_data_ch = -1,
	.dma_ecc_ch =  -1,
};

static struct resource nand_resource[] = {
	[0] = {
		.start = NFI_BASE_ADDR, /* physical address (ioremap) */
		.end   = NFI_BASE_ADDR + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_NFI,
		.flags = IORESOURCE_IRQ,
	},
};

static u64 nfi_dmamask = MAX_DMA_ADDRESS;
static struct platform_device nand_device = {
	.name		= "pnx-nand",
	.id		= 0,
	.dev		= {
		.platform_data	= &nand_data,
		.dma_mask = &nfi_dmamask,
		.coherent_dma_mask = MAX_DMA_ADDRESS,
	},
	.num_resources = ARRAY_SIZE(nand_resource),
	.resource = nand_resource,
};
/* End nand device */


/****************************************************************************
 * MMC device
 ****************************************************************************/
static struct resource mci_pnx_resources[] = {
	{
		.start		= FCI_BASE_ADDR, /* Physical address */
		.end		= FCI_BASE_ADDR + SZ_4K - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= IRQ_FCI,
		.flags		= IORESOURCE_IRQ,
	},
/* ACER Jen chang, 2009/09/29, IssueKeys:AU4.FC-77, Modify sd card detecting pin to fit AU4 PCR { */
/* ACER BobIHLee@20100505, support AS1 project*/
#if (defined ACER_L1_AU3) || (defined ACER_AU4_PCR) || (defined ACER_L1_K2) || (defined ACER_L1_AS1)
/* End BobIHLee@20100505*/
	{ // detection by wp
		.start		= IRQ_EXTINT(22),/* gpioD18 for detection */
		.flags		= IORESOURCE_IRQ,
	},
#elif (defined ACER_AU2_PCR) || (defined ACER_L1_K3)
	{
		.start		= IRQ_EXTINT(3),/* gpioA3 for detection */
		.flags		= IORESOURCE_IRQ,
	},
#endif
/* } ACER Jen Chang, 2009/09/29*/
	{
		.start		= -1, /* dma channel set by driver dma */
		.flags		= IORESOURCE_DMA,
	},

};

/* mmc data ressource
 * If detect_invert is set (1), then the value from
 * gpio is inverted, which makes 1 mean card inserted.
 */
static struct pnx_mci_pdata mci_data = {
	.gpio_fcicmd		= GPIO_B9,
	.gpio_mux_fcicmd	= GPIO_MODE_MUX0,
	.gpio_fciclk		= GPIO_B10,
	.gpio_mux_fciclk	= GPIO_MODE_MUX0,
	.gpio_data0		= GPIO_B11,
	.gpio_mux_data0		= GPIO_MODE_MUX0,
	.gpio_data1		= GPIO_A19,
	.gpio_mux_data1		= GPIO_MODE_MUX1,
	.gpio_data2		= GPIO_A18,
	.gpio_mux_data2		= GPIO_MODE_MUX1,
	.gpio_data3		= GPIO_A16,
	.gpio_mux_data3		= GPIO_MODE_MUX1,

	.detect_invert		= 0,

	.ocr_avail		= MMC_VDD_165_195 | MMC_VDD_32_33,
	.set_power		= mci_set_power,
};

static u64 mci_dmamask = MAX_DMA_ADDRESS;

static struct platform_device mci_device = {
	.name		= "pnxmci",
	.id			= 1,
	.dev = {
		.platform_data	= &mci_data,
		.dma_mask = &mci_dmamask,
		.coherent_dma_mask=MAX_DMA_ADDRESS,
	},
	.num_resources = ARRAY_SIZE(mci_pnx_resources),
	.resource = mci_pnx_resources,
};


/* ACER Bright Lee, 2009/11/20, AU2.FC-741, Improve Hall sensor coding architecture { */
#ifdef CONFIG_HALL_SENSOR
static struct resource hallsensor_resource[] = {
        {
                .start = IRQ_EXTINT(12),
                .flags = IORESOURCE_IRQ,
        },
};


static struct platform_device hallsensor_dev = {
        .name   = "PNX_HallSensor",
        .id     = 0,

        .num_resources = ARRAY_SIZE(hallsensor_resource),
        .resource = hallsensor_resource,
};
#endif
/* } ACER Bright Lee, 2009/11/20 */

/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Add touch panel driver device to improve coding architecture { */
#ifdef CONFIG_TOUCHSCREEN_TSC2007
static struct resource tsc2007_resource[] = {
        {
                .start = IRQ_EXTINT(21),
                .flags = IORESOURCE_IRQ,
        },
};

static struct platform_device pnx_tsc2007_device = {
	.name		= "pnx-tsc2007",
	.id			= 0,

    .num_resources = ARRAY_SIZE(tsc2007_resource),
    .resource = tsc2007_resource,
};
#endif
/* } ACER Jen Chang, 2009/12/23*/

/* ACER Jen chang, 2010/05/05, IssueKeys:A41.B-738, Add capacity type touch panel driver device { */
#ifdef CONFIG_TOUCHSCREEN_BU21018MWV
static struct resource bu21018mwv_resource[] = {
        {
                .start = IRQ_EXTINT(21),
                .flags = IORESOURCE_IRQ,
        },
};

static struct platform_device pnx_bu21018mwv_device = {
	.name		= "pnx-bu21018mwv",
	.id			= 0,

    .num_resources = ARRAY_SIZE(bu21018mwv_resource),
    .resource = bu21018mwv_resource,
};
#endif
/* } ACER Jen Chang, 2010/05/05*/

/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Add io-expender driver device to improve coding architecture { */
#ifdef CONFIG_GPIO_SX1502
static struct resource sx1502_resource[] = {
        {
                .start = IRQ_EXTINT(6),
                .flags = IORESOURCE_IRQ,
        },
};

static struct platform_device pnx_sx1502_device = {
	.name		= "pnx-sx1502",
	.id			= 0,

    .num_resources = ARRAY_SIZE(sx1502_resource),
    .resource = sx1502_resource,
};
#endif
/* } ACER Jen Chang, 2010/03/25*/

/*ACER Erace.Ma@20100506, cm3623 light sensor */
#ifdef CONFIG_SENSORS_CM3623
static struct resource cm3623_resource[] = {
  {
    .start = IRQ_EXTINT(12),
    .flags = IORESOURCE_IRQ,
  },
};

static struct platform_device pnx_cm3623_device = {
  .name = "pnx-cm3623",
  .id = 0,
  .num_resources = ARRAY_SIZE(cm3623_resource),
  .resource = cm3623_resource,
};
#endif
/* End Erace.Ma@20100506*/

#if defined(CONFIG_SPI_PNX67XX) || defined(CONFIG_SPI_PNX67XX_MODULE)
/*******************************************************************************
 * SPI devices
*******************************************************************************/
static struct resource spi1_resources[] = {
	[0] = {
		.start = SPI1_BASE_ADDR,  /* Physical address */
		.end   = SPI1_BASE_ADDR + 0x0FFF,
		.flags = IORESOURCE_MEM,
	 },
	[1] = {
		.start = IRQ_SPI1,
		.end   = IRQ_SPI1,
		.flags = IORESOURCE_IRQ,
	 },
/*Add by Erace.Ma@2090809, support hall mouse in AU3*/
#ifdef CONFIG_SPI_HALL_MOUSE
	[2] = {
		.start		= IRQ_EXTINT(14),/* gpioA20 for interrupt */
		.flags		= IORESOURCE_IRQ,
	},
#endif
/*End by Erace.Ma@20090615*/
};

static u64 spi1_dmamask =MAX_DMA_ADDRESS;

static struct pnx_spi_pdata spi1_data = {
	.gpio_sclk                  = GPIO_A27,
	.gpio_mux_sclk              = GPIO_MODE_MUX1,
	.gpio_sdatin                = GPIO_A31,
	.gpio_mux_sdatin            = GPIO_MODE_MUX2,
	.gpio_sdatio                = GPIO_A28,
	.gpio_mux_sdatio            = GPIO_MODE_MUX1,
};


static struct platform_device spi1_device = {
	.name = "pnx67xx_spi1",
	.id	  = 1,
	.dev = {
		.dma_mask          = &spi1_dmamask,
		.coherent_dma_mask = MAX_DMA_ADDRESS,
		.platform_data	= &spi1_data,
	},

	.resource       = spi1_resources,
	.num_resources  = ARRAY_SIZE(spi1_resources),
};

static struct pnx_spi_pdata spi2_data = {
	.gpio_sclk                  = GPIO_E23,
	.gpio_mux_sclk              = GPIO_MODE_MUX0,
	.gpio_sdatin                = GPIO_E22,
	.gpio_mux_sdatin            = GPIO_MODE_MUX0,
	.gpio_sdatio                = GPIO_E21,
	.gpio_mux_sdatio            = GPIO_MODE_MUX0,
};

static struct resource spi2_resources[] = {
	[0] = {
		.start = SPI2_BASE_ADDR,  /* Physical address */
		.end   = SPI2_BASE_ADDR + 0x0FFF,
		.flags = IORESOURCE_MEM,
	 },
	[1] = {
		.start = IRQ_SPI2,
		.end   = IRQ_SPI2,
		.flags = IORESOURCE_IRQ,
	 }
};

static u64 spi2_dmamask = MAX_DMA_ADDRESS;

static struct platform_device spi2_device = {
	.name = "pnx67xx_spi2",
	.id	  = 2,
	.dev = {
		.dma_mask          = &spi2_dmamask,
		.coherent_dma_mask = MAX_DMA_ADDRESS,
		.platform_data	= &spi2_data,
	},

	.resource       = spi2_resources,
	.num_resources  = ARRAY_SIZE(spi2_resources),
};


#ifdef CONFIG_SPI_HALL_MOUSE
static struct pnx_hall_mouse_pdata hall_mouse_data = {
	.hm_en		= GPIO_B4,		/* device enable */
//	.hm_cs		= GPIO_A13,		/* chip select */
	.hm_sck_pin	= GPIO_A27,		/* SPI1 Clock */
	.hm_sck_mux	= GPIO_MODE_MUX1,
	.hm_si_pin	= GPIO_A31,		/* SDATIN1 */
	.hm_si_mux	= GPIO_MODE_MUX2,
	.hm_so_pin	= GPIO_A28,		/* SDATIO1 */
	.hm_so_mux	= GPIO_MODE_MUX1,
	.hm_irq		= IRQ_EXTINT(14),	/* interrupt: GPIO_A20 */
};
#endif

/* GPS device */
#if defined(CONFIG_SPI_GNS7560) || defined(CONFIG_GPS_HW)
/*  board specific resource for gns7560 */
static struct gps_pdata gps_data = {
/* ACER BobIHLee@20100505, support AS1 project*/
#if defined(ACER_L1_K2) || defined(ACER_L1_K3) || defined(ACER_L1_AS1)
/* End BobIHLee@20100505*/
	.gpo_rst	= GPIO_F9,
#else
	.gpo_rst	= GPIO_A25,
#endif
	.gpo_pwr	= GPIO_A30
};
#endif

static struct spi_board_info pnx67xx_spi_info[] __initdata = {
	[0] = {
/*Add by Erace.Ma@2090809, support hall mouse in AU3*/
#ifdef CONFIG_SPI_HALL_MOUSE
		.modalias       = "spi_hallmouse",
		.platform_data	= &hall_mouse_data,
		.bus_num        = 1,
		.chip_select    = GPIO_A13,
		.max_speed_hz   = 10000,
		.mode           = SPI_MODE_1 | SPI_LSB_FIRST,
/*ACER Ed 20100322 & nanoradio for wifi*/
#elif defined(CONFIG_SPI_NANORADIO_WIFI)
		.modalias       = "spi_nano",
		.bus_num        = 1,
		.chip_select    = GPIO_A20,
		.max_speed_hz   = 17500000,
		.mode           = SPI_MODE_3,
		.bits_per_word  = 8
/*~ACER Ed 20100322 & nanoradio for wifi*/
#else
		/* The following configuration is used to test SPI1
		 * using the WLAN chip connected on it */
		.modalias       = "spi_pnx67xx_test",
		.bus_num        = 1,
		.max_speed_hz   = 26000000,
		.irq            = IRQ_EXTINT(9),/* gpioA15 */
		.mode           = SPI_MODE_3,
		.bits_per_word  = 8
#endif /*CONFIG_SPI_HALL_MOUSE*/
/*End by Erace.Ma@20090615*/
	},
	[1] = {
		/* The following configuration is used to test SPI2
		 * using the GPS chip connected on it */
#ifdef CONFIG_SPI_GNS7560
		.modalias      = "gns7560",
		.platform_data = &gps_data,
#else
		.modalias      = "spidev",
#endif
		.bus_num       = 2,
		.max_speed_hz  = 500000 ,
		.chip_select   = GPIO_A5,
		.mode          = SPI_MODE_0,
		.bits_per_word = 8
	}
};
#endif
#ifdef CONFIG_GPS_HW
static struct platform_device gps_hw_device = {
	.name		= "gps_hw",
	.dev = {
		.platform_data	= &gps_data,
	},
};
#endif

/*****************************************************************************
 * PMU device
 *****************************************************************************/
#if defined (CONFIG_MACH_PNX67XX_WAVED)


static unsigned char  rf1c_Modem[] = {
/*2.8V sensitive on PWREN1*/
/*rf1c*/	PCF50616_RFREG_mV(2800) | PCF50616_REGULATOR_OFF_OFF_ON_ON,
/*2.8V sensitive on PWREN1*/
/*rf2c*/	PCF50616_RFREG_mV(2800) | PCF50616_REGULATOR_OFF_OFF_ON_ON
};

static unsigned char dcd2_Modem[]= {
/*dcd2c1 */     PCF50616_DCD2C1_DCD2ENA,
/*dcd2c2 */     PCF50616_DCD_mV(1800),
/*dcd2vs1*/     PCF50616_DCD_mV(1800),
/*dcd2vs2*/     PCF50616_DCD_mV(1800),
/*dcd2vs3*/     PCF50616_DCD_mV(1800),
/*dcd2vs4*/     PCF50616_DCD_mV(1800)
};

static unsigned char  oocc_Modem[]={
/*oocc*/    PCF50616_OOCC_MICB_EN | PCF50616_OOCC_REC_EN
};

static unsigned char  lcc_Modem[]={
/*lcc*/    PCF50616_LCC_mV(1200) | PCF50616_REGULATOR_ECO_ON_ON_ECO
};

static unsigned char  hcc_Modem[]={
/*hcc*/    PCF50616_HCC_HCREGVOUT_2V6 | PCF50616_REGULATOR_ECO_ECO_ON_ON
};

static unsigned char  usbc_Modem[]={
/*usbc*/    PCF50616_USBC_USBSWCTL_CHGUSB | PCF50616_USBC_USBSWENA \
	| PCF50616_USBC_USBREGCTL_CHGUSB | PCF50616_USBC_USBREGENA
};



static struct pmu_reg_table pcf50616_Modem[] = {
	{
		.addr = PCF50616_REG_RF1C,
		.value = rf1c_Modem,
		.size = sizeof(rf1c_Modem)
	},
	{
		.addr = PCF50616_REG_DCD2C1,
		.value = dcd2_Modem,
		.size = sizeof(dcd2_Modem)
	},
	{
		.addr = PCF50616_REG_OOCC,
		.value = oocc_Modem,
		.size = sizeof(oocc_Modem)
	},
	{
		.addr = PCF50616_REG_LCC,
		.value = lcc_Modem,
		.size = sizeof(lcc_Modem)
	},
	{
		.addr = PCF50616_REG_HCC,
		.value = hcc_Modem,
		.size = sizeof(hcc_Modem)
	},
	{
		.addr = PCF50616_REG_USBC,
		.value = usbc_Modem,
		.size = sizeof(usbc_Modem)
	},


};


static struct resource pmu_pnx_resources[] = {
	{
		.start		= IRQ_EXTINT(8),/* gpioA14 for detection */
		.flags		= IORESOURCE_IRQ,
	},
};

static struct pcf50616_platform_data pmu_data = {
	.used_features = PCF506XX_FEAT_RTC
		| PCF506XX_FEAT_KEYPAD_BL | PCF506XX_FEAT_CBC,
	.onkey_seconds_required = 3,

	.rails[PCF506XX_REGULATOR_D1REG].used= 1,
	.rails[PCF506XX_REGULATOR_D1REG].voltage.init = 1200,
	.rails[PCF506XX_REGULATOR_D1REG].voltage.max  = 3300,
	.rails[PCF506XX_REGULATOR_D1REG].mode = PCF506XX_REGU_OFF,

	.rails[PCF506XX_REGULATOR_D2REG].used= 1,
	.rails[PCF506XX_REGULATOR_D2REG].voltage.init = 1200,
	.rails[PCF506XX_REGULATOR_D2REG].voltage.max  = 3300,
	.rails[PCF506XX_REGULATOR_D2REG].mode = PCF506XX_REGU_OFF,
	/* on WaveD regu is used for BLUETOOTH */
	.rails[PCF506XX_REGULATOR_D3REG].used= 1,
	.rails[PCF506XX_REGULATOR_D3REG].voltage.init = 1200,
	.rails[PCF506XX_REGULATOR_D3REG].voltage.max = 3300,
	.rails[PCF506XX_REGULATOR_D3REG].mode = PCF506XX_REGU_OFF,
	/* ***********************************/
	.rails[PCF506XX_REGULATOR_D4REG].used= 1,
	.rails[PCF506XX_REGULATOR_D4REG].voltage.init = 1200,
	.rails[PCF506XX_REGULATOR_D4REG].voltage.max = 3300,
	.rails[PCF506XX_REGULATOR_D4REG].mode = PCF506XX_REGU_OFF,

	.rails[PCF506XX_REGULATOR_D5REG].used= 1,
	.rails[PCF506XX_REGULATOR_D5REG].voltage.init = 1200,
	.rails[PCF506XX_REGULATOR_D5REG].voltage.max  = 3300,
	.rails[PCF506XX_REGULATOR_D5REG].mode = PCF506XX_REGU_OFF,

	.rails[PCF506XX_REGULATOR_D6REG].used= 1,
	.rails[PCF506XX_REGULATOR_D6REG].voltage.init = 3300,
	.rails[PCF506XX_REGULATOR_D6REG].voltage.max = 3300,
	.rails[PCF506XX_REGULATOR_D6REG].mode = PCF506XX_REGU_OFF,

	.rails[PCF506XX_REGULATOR_D7REG].used= 1,
	.rails[PCF506XX_REGULATOR_D7REG].voltage.init = 1250,
	.rails[PCF506XX_REGULATOR_D7REG].voltage.max = 3350,
	.rails[PCF506XX_REGULATOR_D7REG].mode = PCF506XX_REGU_OFF,

	.rails[PCF506XX_REGULATOR_IOREG].used= 1,
	.rails[PCF506XX_REGULATOR_IOREG].voltage.init = 2600,
	.rails[PCF506XX_REGULATOR_IOREG].voltage.max = 2600,
	.rails[PCF506XX_REGULATOR_IOREG].mode = PCF506XX_REGU_ECO_ECO_ON_ON,

	.rails[PCF506XX_REGULATOR_HCREG].used = 1,
	.rails[PCF506XX_REGULATOR_HCREG].voltage.init = 3000,
	.rails[PCF506XX_REGULATOR_HCREG].voltage.max = 3200,
	.rails[PCF506XX_REGULATOR_HCREG].mode = PCF506XX_REGU_ON,
	.reg_table=pcf50616_Modem,
	.reg_table_size=ARRAY_SIZE(pcf50616_Modem),
	.reduce_wp=pnx67xx_reduce_wp,
	.usb_suspend_gpio=GPIO_A9,
};

#ifdef CONFIG_I2C_NEW_PROBE
static struct i2c_board_info __initdata pmu_i2c_device[] = {
	{
		I2C_BOARD_INFO("pcf50616", 0x70),
	}
};
#endif



#else /* else CONFIG_MACH_PNX67XX_WAVED */


static unsigned char  rf1regc1_Modem[] = {
/*rf1regc1*/    PCF50623_RFREG_mV(2800),   /*2.8V*/
/*rf1regc2*/    PCF50623_REGULATOR_ON |PCF50623_REGULATOR_PWEN1 ,
/*rf1regc3*/    PCF50623_PHASE_3 | PCF50623_REGULATOR_GPIO3, /* re-write default value */
/*rf2regc1*/    PCF50623_RFREG_mV(2800),
/*rf2regc2*/    PCF50623_REGULATOR_ON |PCF50623_REGULATOR_PWEN1,
/*rf2regc3*/    PCF50623_PHASE_3 | PCF50623_REGULATOR_GPIO3, /* re-write default value */
/*rf3regc1*/    PCF50623_RFREG_mV(2800),
/*rf3regc2*/    PCF50623_REGULATOR_ON |PCF50623_REGULATOR_PWEN3,
/*rf3regc3*/    PCF50623_PHASE_3, /* re-write default value */
/*rf4regc1*/    PCF50623_RFREG_mV(1500),/*0b00100100*/   /*1.5V */
/*rf4regc2*/    0x00,
/*rf4regc3*/    PCF50623_PHASE_3 /* re-write default value */
};

static unsigned char rf1regc2_Modem[]= {
/*rf1regc2*/    PCF50623_REGULATOR_ON_OFF |PCF50623_REGULATOR_PWEN1
};
static unsigned char rf2regc2_Modem[]= {
/*rf2regc2*/    PCF50623_REGULATOR_ON_OFF |PCF50623_REGULATOR_PWEN1
};

static unsigned char rf3regc2_Modem[]= {
/*rf3regc2*/    PCF50623_REGULATOR_ON_OFF |PCF50623_REGULATOR_PWEN3
};

static unsigned char dcd2_Modem_ES1[]= {
/*dcd2c1*/     PCF50623_DCD_mV(1800),
/*dcd2c2*/     PCF50623_REGULATOR_ON | PCF50623_REGULATOR_PWEN1,
/*dcd2c3*/     PCF50623_PHASE_2,
/*dcd2c4*/     PCF50623_DCD_mA(600)|PCF50623_DCD_PWM_ONLY
};
static unsigned char dcd2_Modem[]= {
/*dcd2c1*/     PCF50623_DCD_mV(1800),
/*dcd2c2*/     PCF50623_REGULATOR_ON | PCF50623_REGULATOR_PWEN1,
/*dcd2c3*/     PCF50623_PHASE_2,
/*dcd2c4*/     PCF50623_DCD_mA(600)
};


static unsigned char dcd3_Modem[]= {
/*dcd3c1*/     PCF50623_DCD_mV(1500),
/*dcd3c2*/     PCF50623_REGULATOR_ON | PCF50623_REGULATOR_PWEN3,
/*dcd3c3*/     PCF50623_PHASE_2,
/*dcd3c4*/     PCF50623_DCD_mA(600)|PCF50623_DCD_PWM_ONLY
};

static unsigned char dcd3c2_Modem[]= {
/*dcd3c2*/     PCF50623_REGULATOR_ON_OFF | PCF50623_REGULATOR_PWEN3 };

#if defined (CONFIG_MACH_PNX67XX_V2_WAVEB_2GB)

static unsigned char d1regc1_Modem[]= {
/*d2regc1 */	PCF50623_DREG_mV(1200),
/*d2regc2*/	PCF50623_REGULATOR_ON |PCF50623_REGULATOR_PWEN2,
/*d2regc3*/	PCF50623_PHASE_2
};

static unsigned char d4regc1_Modem[]= {
/*d4regc1 */	PCF50623_DREG_mV(2600),
/*d4regc2*/	PCF50623_REGULATOR_ON |PCF50623_REGULATOR_PWEN2,
/*d4regc3*/	PCF50623_PHASE_2
};
static unsigned char  d1regc2_Modem[]={
/*d1regc2*/    PCF50623_REGULATOR_ON_ECO | PCF50623_REGULATOR_PWEN2
};

static unsigned char  d4regc2_Modem[]={
/*d4regc2*/    PCF50623_REGULATOR_ON_ECO | PCF50623_REGULATOR_PWEN2
};
#endif

static unsigned char d2regc1_Modem[]= {
/*d2regc1 */	PCF50623_DREG_mV(1200),
/*d2regc2*/	PCF50623_REGULATOR_ON |PCF50623_REGULATOR_PWEN1,
/*d2regc3*/	PCF50623_PHASE_2
};

static unsigned char  d2regc2_Modem[]={
/*d2regc2*/    PCF50623_REGULATOR_ON_ECO | PCF50623_REGULATOR_PWEN1
};

static unsigned char  gpio2c1_Modem[]={
/*gpio2c1*/    PCF50623_GPIO_HZ
};

static unsigned char  oocc1_Modem[]={
/*oocc1*/    PCF50623_OOCC1_RTC_WAK | PCF50623_OOCC1_TOT_RST
};

static struct pmu_reg_table pcf50623_ES1_Modem[] = {
#if defined (CONFIG_MACH_PNX67XX_V2_WAVEB_2GB)
	{
		.addr = PCF50623_REG_D1REGC1,
		.value = d1regc1_Modem,
		.size = sizeof(d1regc1_Modem)
	},
	{
		.addr = PCF50623_REG_D4REGC1,
		.value = d4regc1_Modem,
		.size = sizeof(d4regc1_Modem)
	},
#endif
	{
		.addr = PCF50623_REG_D2REGC1,
		.value = d2regc1_Modem,
		.size = sizeof(d2regc1_Modem)
	},
	{
		.addr = PCF50623_REG_RF1REGC1,
		.value = rf1regc1_Modem,
		.size = sizeof(rf1regc1_Modem)
	},
	{
		.addr = PCF50623_REG_DCD2C1,
		.value = dcd2_Modem_ES1,
		.size = sizeof(dcd2_Modem_ES1)
	},
	{
		.addr = PCF50623_REG_DCD3C1,
		.value = dcd3_Modem,
		.size = sizeof(dcd3_Modem)
	},
#if defined (CONFIG_MACH_PNX67XX_V2_WAVEB_2GB)
	{
		.addr = PCF50623_REG_D1REGC2,
		.value = d1regc2_Modem,
		.size = sizeof(d1regc2_Modem)
	},
	{
		.addr = PCF50623_REG_D4REGC2,
		.value = d4regc2_Modem,
		.size = sizeof(d4regc2_Modem)
	},
#endif
	{
		.addr = PCF50623_REG_D2REGC2,
		.value = d2regc2_Modem,
		.size = sizeof( d2regc2_Modem)
	},

	{
		.addr = PCF50623_REG_RF1REGC2,
		.value = rf1regc2_Modem,
		.size = sizeof( rf1regc2_Modem)
	},
	{
		.addr = PCF50623_REG_RF2REGC2,
		.value = rf2regc2_Modem,
		.size = sizeof( rf2regc2_Modem)
	},
	{
		.addr = PCF50623_REG_RF3REGC2,
		.value = rf3regc2_Modem,
		.size = sizeof( rf3regc2_Modem)
	},
	{
		.addr = PCF50623_REG_DCD3C2,
		.value = dcd3c2_Modem,
		.size = sizeof( dcd3c2_Modem)
	}
};


static struct pmu_reg_table pcf50623_ES2_Modem[] = {
#if defined (CONFIG_MACH_PNX67XX_V2_WAVEB_2GB)
	{
		.addr = PCF50623_REG_D1REGC1,
		.value = d1regc1_Modem,
		.size = sizeof(d1regc1_Modem)
	},
	{
		.addr = PCF50623_REG_D4REGC1,
		.value = d4regc1_Modem,
		.size = sizeof(d4regc1_Modem)
	},
#endif
	{
		.addr = PCF50623_REG_D2REGC1,
		.value = d2regc1_Modem,
		.size = sizeof(d2regc1_Modem)
	},
	{
		.addr = PCF50623_REG_RF1REGC1,
		.value = rf1regc1_Modem,
		.size = sizeof(rf1regc1_Modem)
	},
	{
		.addr = PCF50623_REG_DCD2C1,
		.value = dcd2_Modem,
		.size = sizeof(dcd2_Modem)
	},
	{
		.addr = PCF50623_REG_DCD3C1,
		.value = dcd3_Modem,
		.size = sizeof(dcd3_Modem)
	},
#if defined (CONFIG_MACH_PNX67XX_V2_WAVEB_2GB)
	{
		.addr = PCF50623_REG_D1REGC2,
		.value = d1regc2_Modem,
		.size = sizeof(d1regc2_Modem)
	},
	{
		.addr = PCF50623_REG_D4REGC2,
		.value = d4regc2_Modem,
		.size = sizeof(d4regc2_Modem)
	},
#endif
    {
		.addr = PCF50623_REG_D2REGC2,
		.value = d2regc2_Modem,
		.size = sizeof( d2regc2_Modem)
	},

	{
		.addr = PCF50623_REG_RF1REGC2,
		.value = rf1regc2_Modem,
		.size = sizeof( rf1regc2_Modem)
	},
	{
		.addr = PCF50623_REG_RF2REGC2,
		.value = rf2regc2_Modem,
		.size = sizeof( rf2regc2_Modem)
	},
	{
		.addr = PCF50623_REG_RF3REGC2,
		.value = rf3regc2_Modem,
		.size = sizeof( rf3regc2_Modem)
	},
	{
		.addr = PCF50623_REG_DCD3C2,
		.value = dcd3c2_Modem,
		.size = sizeof( dcd3c2_Modem)
	},
	/* ACER Bright Lee, 2010/2/28, Charging LED, already setup by bootloader { */
	#ifndef ACER_L1_CHANGED
	{
		.addr = PCF50623_REG_GPIO2C1,
		.value = gpio2c1_Modem,
		.size = sizeof( gpio2c1_Modem)
	},
	#endif
	/* } ACER Bright Lee, 2010/2/28 */
	{
		.addr = PCF50623_REG_OOCC1,
		.value = oocc1_Modem,
		.size = sizeof( oocc1_Modem)
	}
};


static struct resource pmu_pnx_resources[] = {
	{
		.start		= IRQ_EXTINT(8),/* gpioA14 for detection */
		.flags		= IORESOURCE_IRQ,
	},
};

static struct pcf50623_platform_data pmu_data = {
  /* ACER Jen chang, 2009/07/29, IssueKeys:AU2.B-38, AU4.B-137, Modify for register charger backlight { */
  //.used_features = PCF506XX_FEAT_RTC|PCF506XX_FEAT_KEYPAD_BL|PCF506XX_FEAT_CBC,
  .used_features = PCF506XX_FEAT_RTC|PCF506XX_FEAT_KEYPAD_BL|PCF506XX_FEAT_CHG_BL|PCF506XX_FEAT_CBC|PCF506XX_FEAT_VIBRATOR,
  /* } ACER Jen Chang, 2009/07/29 */
	.onkey_seconds_required = 3,
	.onkey_seconds_poweroff = 6,
#if defined (ACER_L1_CHANGED)
  .hs_irq = IRQ_EXTINT(11),/* gpioA17 for headset detection */
#endif
/* ACER Erace.Ma@20100209, add headset power amplifier in K2*/
/* ACER BobIHLee@20100505, support AS1 project*/
#if (defined ACER_L1_K2) || (defined ACER_L1_AS1)
/* End BobIHLee@20100505*/
  .hs_amp = GPIO_A6,
#endif
/* End Erace.Ma@20100209*/
  .rails[PCF506XX_REGULATOR_D1REG].used= 1,
  .rails[PCF506XX_REGULATOR_D1REG].voltage.init = 1800,
  .rails[PCF506XX_REGULATOR_D1REG].voltage.max  = 3300,
  .rails[PCF506XX_REGULATOR_D1REG].mode = PCF506XX_REGU_OFF,
  .rails[PCF506XX_REGULATOR_D3REG].used= 1,
  .rails[PCF506XX_REGULATOR_D3REG].voltage.init = 2800, /* LCD Analog */
  .rails[PCF506XX_REGULATOR_D3REG].voltage.max = 2800,  /* LCD Analog */
  .rails[PCF506XX_REGULATOR_D3REG].mode = PCF506XX_REGU_ON,
  .rails[PCF506XX_REGULATOR_D3REG].gpiopwn = PCF50623_REGU_GPIO3,
  .rails[PCF506XX_REGULATOR_D4REG].used= 1,
/* ACER BobIHLee@20100505, support AS1 project*/
#if defined (ACER_L1_K2) || defined (ACER_L1_K3) || defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
  .rails[PCF506XX_REGULATOR_D4REG].voltage.init = 2600,
#else
	.rails[PCF506XX_REGULATOR_D4REG].voltage.init = 1800,
#endif
  .rails[PCF506XX_REGULATOR_D4REG].voltage.max = 3300,
  /* ACER Owen chang, 2010/02/24, Modify for FM { */
  .rails[PCF506XX_REGULATOR_D4REG].mode = PCF506XX_REGU_ON,
  /* } ACER Owen chang, 2010/02/24 */

  .rails[PCF506XX_REGULATOR_D5REG].used= 1,
  .rails[PCF506XX_REGULATOR_D5REG].voltage.init = 1800,
  .rails[PCF506XX_REGULATOR_D5REG].voltage.max  = 3300,
  /* Add by Bob.IH.Lee@20090901, Added the power on function */
  .rails[PCF506XX_REGULATOR_D5REG].mode = PCF506XX_REGU_ECO,
  /* End Add by Bob.IH.Lee@20090901, Added the power on function */

  .rails[PCF506XX_REGULATOR_D6REG].used= 1,
	/* ACER Owen chang, 2010/03/10, Modify for Audio AMP { */
/* ACER BobIHLee@20100505, support AS1 project*/
#if defined (ACER_L1_K2) || defined (ACER_L1_K3) || defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
  .rails[PCF506XX_REGULATOR_D6REG].voltage.init = 2800,
#else
	.rails[PCF506XX_REGULATOR_D6REG].voltage.init = 3300,
#endif
	/* } ACER Owen chang, 2010/03/10 */
  .rails[PCF506XX_REGULATOR_D6REG].voltage.max = 3300,
/* ACER BobIHLee@20100505, support AS1 project*/
#if defined (ACER_L1_K2) || defined (ACER_L1_K3) || defined (ACER_L1_AS1)
/* End BobIHLee@20100505*/
	.rails[PCF506XX_REGULATOR_D6REG].mode = PCF506XX_REGU_OFF,
#else
  .rails[PCF506XX_REGULATOR_D6REG].mode = PCF506XX_REGU_ON,
#endif

  .rails[PCF506XX_REGULATOR_D7REG].used= 1,
#if defined (ACER_L1_KX)
  .rails[PCF506XX_REGULATOR_D7REG].voltage.init = 1800, /* VDD_IR_LED */
#else
  .rails[PCF506XX_REGULATOR_D7REG].voltage.init = 2900,
#endif
  .rails[PCF506XX_REGULATOR_D7REG].voltage.max = 3300,
#if defined (ACER_L1_KX)
  .rails[PCF506XX_REGULATOR_D7REG].mode = PCF506XX_REGU_ECO,
#else
  .rails[PCF506XX_REGULATOR_D7REG].mode = PCF506XX_REGU_OFF,
#endif

#if defined (CONFIG_MACH_PNX67XX_V2_WAVEB_2GB)
	.rails[PCF506XX_REGULATOR_IOREG].voltage.init = 1800,
	.rails[PCF506XX_REGULATOR_IOREG].voltage.max = 1800,
	.rails[PCF506XX_REGULATOR_IOREG].mode = PCF506XX_REGU_ECO,
#else /* WaveC & others */
	.rails[PCF506XX_REGULATOR_IOREG].voltage.init = 2600,
	.rails[PCF506XX_REGULATOR_IOREG].voltage.max = 2600,
	.rails[PCF506XX_REGULATOR_IOREG].mode = PCF506XX_REGU_ON_ECO,
	.rails[PCF506XX_REGULATOR_IOREG].gpiopwn =PCF50623_REGU_PWEN1,
#endif

	.rails[PCF506XX_REGULATOR_USBREG].used = 1,
	.rails[PCF506XX_REGULATOR_USBREG].voltage.init= 3300,
	.rails[PCF506XX_REGULATOR_USBREG].voltage.max= 3300,
	.rails[PCF506XX_REGULATOR_USBREG].mode = PCF506XX_REGU_ECO,

	.rails[PCF506XX_REGULATOR_HCREG].used = 1,
	.rails[PCF506XX_REGULATOR_HCREG].voltage.init = 3000,
	.rails[PCF506XX_REGULATOR_HCREG].voltage.max = 3200,
	.rails[PCF506XX_REGULATOR_HCREG].mode = PCF506XX_REGU_ON,
	.reg_table_ES1=pcf50623_ES1_Modem,
	.reg_table_size_ES1=ARRAY_SIZE(pcf50623_ES1_Modem),
	.reg_table_ES2=pcf50623_ES2_Modem,
	.reg_table_size_ES2=ARRAY_SIZE(pcf50623_ES2_Modem),
	.usb_suspend_gpio=GPIO_D29,
};

#ifdef CONFIG_I2C_NEW_PROBE
static struct i2c_board_info __initdata pmu_i2c_device[] = {
	{
		I2C_BOARD_INFO("pcf50623", 0x70),
	}
};

/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, Add i2c_2 device struct to meet new i2c architecture { */
static struct i2c_board_info __initdata pnx_i2c2_device[] = {
#ifdef CONFIG_TOUCHSCREEN_TSC2007
	{ I2C_BOARD_INFO("tsc2007", 0x48) },
#endif
/* ACER Bob IH LEE, 2010/03/26, IssueKeys:A21.B-1, Add i2c_2 device struct to meet new i2c architecture { */
#ifdef CONFIG_RADIO_TEA599X
	{ I2C_BOARD_INFO("tea599x", 0x11) },
#endif
/* } ACER Bob IH LEE, 2010/03/26 */
#ifdef CONFIG_GPIO_SX1502
	{ I2C_BOARD_INFO("sx1502", 0x20) },
#endif
/* ACER Jen chang, 2010/05/05, IssueKeys:A41.B-738, Add capacity type touch panel driver device { */
#ifdef CONFIG_TOUCHSCREEN_BU21018MWV
	{ I2C_BOARD_INFO("bu21018mwv", 0x5C) },
#endif
/* } ACER Jen chang, 2010/05/05 */
/* ACER Bob IH LEE, 2010/05/06, new i2c architecture { */
#ifdef CONFIG_INPUT_ECOMPASS
	{
		I2C_BOARD_INFO("MS3C", 0x2e),
	},
#endif
#ifdef CONFIG_SENSORS_LIS331
	{
		I2C_BOARD_INFO("lis331_gsensor", 0x19),
	},
#endif
/* } ACER Bob IH LEE, 2010/05/06 */
/* ACER Erace.Ma@2010/05/06, cm3623 light sensor { */
#ifdef CONFIG_SENSORS_CM3623
	{
		I2C_BOARD_INFO("cm3623", 0x0C),
	},
#endif
/* } ACER Erace.Ma@2010/05/06 */
};
/* } ACER Jen chang, 2010/03/24 */

#endif
#endif /* endif CONFIG_MACH_PNX67XX_WAVED */

static struct platform_device pmu_device = {
	.name		= "pnx-pmu",
	.id			= 2,
	.dev = {
		.platform_data	= &pmu_data,
	},
	.num_resources = ARRAY_SIZE(pmu_pnx_resources),
	.resource = pmu_pnx_resources,
};

#if defined(CONFIG_ANDROID_BATTERY_INTERFACE) || defined(CONFIG_ANDROID_BATTERY_INTERFACE_MODULE)
static struct platform_device pnx_batt_device = {
	.name		= "pnx-battery",
};
#endif


#if defined(CONFIG_BACKLIGHT_PNX) || defined(CONFIG_BACKLIGHT_PNX_MODULE)
/*****************************************************************************
 * PWM backlight device
 *****************************************************************************/
static struct pnx_bl_platform_data pnx_bl_data = {
/* ACER Jen chang, 2010/04/11, IssueKeys:AU21.B-1, Modify GPIO setting for K2/K3 PR2 { */
#if (defined ACER_K2_PR1)
  .gpio    = GPIO_C10,
  .pin_mux = GPIO_MODE_MUX1,
/* ACER BobIHLee@20100505, support AS1 project*/
#elif (defined ACER_K2_PR2)||(defined ACER_AS1_PR1)
/* End BobIHLee@20100505*/
  .gpio    = GPIO_A29,
  .pin_mux = GPIO_MODE_MUX0,
/* } ACER Jen Chang, 2010/04/11*/
#else
	.gpio    = GPIO_A6,
	.pin_mux = GPIO_MODE_MUX0,
#endif
	.pwm_pf  = PWM1_PF_REG,
	.pwm_tmr = PWM1_TMR_REG,
	.pwm_clk = "PWM1",
};

static struct platform_device pnx_bl_device = {
	.name		= "pnx-bl",
	.id			= 0,
	.dev = {
		.platform_data	= &pnx_bl_data,
	},
};
#endif

/* List of board specific devices */
static struct platform_device *devices[] __initdata = {
#if defined(CONFIG_SPI_PNX67XX) || defined(CONFIG_SPI_PNX67XX_MODULE)
/*Add by Erace.Ma@2090809, support hall mouse in AU3*/
/*ACER Ed 20100322 & nanoradio for wifi*/
#if defined(CONFIG_SPI_HALL_MOUSE) || defined(CONFIG_SPI_NANORADIO_WIFI)
	&spi1_device,
#endif /*CONFIG_SPI_HALL_MOUSE*/
/*End by Erace.Ma@20090809*/
	&spi2_device,
#endif
	&pmu_device,
#if defined(CONFIG_ANDROID_BATTERY_INTERFACE) || defined(CONFIG_ANDROID_BATTERY_INTERFACE_MODULE)
	&pnx_batt_device,
#endif
#if defined(CONFIG_GPS_HW)
	&gps_hw_device,
#endif
#if defined(CONFIG_BACKLIGHT_PNX) || defined(CONFIG_BACKLIGHT_PNX_MODULE)
	&pnx_bl_device,
#endif
	&pnx_e150x_kp_device,
	&nand_device,
	&mci_device,
/* ACER Bright Lee, 2009/11/20, AU2.FC-741, Improve hall sensor coding architecture { */
#ifdef CONFIG_HALL_SENSOR
        &hallsensor_dev,
#endif
/* } ACER Bright Lee, 2009/11/20 */

/* ACER Jen chang, 2009/12/23, IssueKeys:A43.B-235, Add touch panel driver device to improve coding architecture { */
#ifdef CONFIG_TOUCHSCREEN_TSC2007
	&pnx_tsc2007_device,
#endif
/* } ACER Jen chang, 2009/12/23 */
/* ACER Jen chang, 2010/05/05, IssueKeys:A41.B-738, Add capacity type touch panel driver device { */
#ifdef CONFIG_TOUCHSCREEN_BU21018MWV
	&pnx_bu21018mwv_device,
#endif
/* } ACER Jen chang, 2010/05/05 */

/* ACER Jen chang, 2010/03/25, IssueKeys:A21.B-1, Add io-expender driver device to improve coding architecture { */
#ifdef CONFIG_GPIO_SX1502
	&pnx_sx1502_device,
#endif
/* } ACER Jen Chang, 2010/03/25*/

/* ACER Erace.Ma@2010/05/05, CM3623 light sensor */
#ifdef CONFIG_SENSORS_CM3623
	&pnx_cm3623_device,
#endif
/* } ACER Erace.Ma@2010/05/05 */

};

void __init pnx67xx_init(void)
{
	/* ACER Bright Lee, 2010/5/5, A41.B-739, dynamic partition layout { */
	int num = sizeof(nand_partitions)/sizeof(struct mtd_partition);
	int sorted[num];
	int i;
	/* } ACER Bright Lee, 2010/5/5 */

	pnx_rtke_timer_init();
	pnx_gpio_init();

	/* ACER Bright Lee, 2010/5/5, A41.B-739, dynamic partition layout { */
	for (i = 0; i < num; i ++) {
		if (nand_partitions[i].index <= 0 || nand_partitions[i].index > num)
			panic("nand partitions index out of range!!\n");
		sorted[nand_partitions[i].index-1] = i;
	}

	nand_partitions[sorted[0]].offset = 0;
	for (i = 1; i < num; i ++) {
		int offset = nand_partitions[sorted[i-1]].offset + nand_partitions[sorted[i-1]].size;
		if (nand_partitions[sorted[i]].offset == 0) {
			nand_partitions[sorted[i]].offset = offset;
		} else {
			if (nand_partitions[sorted[i]].offset != offset)
				panic ("Error on index %d offset: 0x%08X my offset: 0x%08X\n", sorted[i], nand_partitions[sorted[i]].offset, offset);
		}
	}
	/* } ACER Bright Lee,2010/5/5 */

	/* Add specific board devices */
#ifdef CONFIG_I2C_NEW_PROBE
	i2c_register_board_info(0, pmu_i2c_device, ARRAY_SIZE(pmu_i2c_device));
/* ACER Jen chang, 2010/03/24, IssueKeys:A21.B-1, register i2c_2 device to meet new i2c architecture { */
	i2c_register_board_info(1, pnx_i2c2_device, ARRAY_SIZE(pnx_i2c2_device));
/* } ACER Jen chang, 2010/03/24 */
#endif

	/* Generic devices that must be registered 1st */
	pnx67xx_devices_init_pre();

	/* Board related devices */
	platform_add_devices(devices, ARRAY_SIZE(devices));

	/* Generic devices that must be registered last */
	pnx67xx_devices_init_post();

#if defined(CONFIG_SPI_PNX67XX) || defined(CONFIG_SPI_PNX67XX_MODULE)
	spi_register_board_info(pnx67xx_spi_info, ARRAY_SIZE(pnx67xx_spi_info));
#endif

}

MACHINE_START(U6715, CONFIG_MACHINE_NAME)
	/* Maintainer: ST-Ericsson Le Mans */
	.phys_io	= 0xC0000000,
	.io_pg_offst	= ((0xC0000000) >> 18) & 0xfffc,
#if defined(CONFIG_MACH_PNX_REALLOC)
	.boot_params	= 0xC03FFC00,
#else
	.boot_params	= 0x203FFC00,
#endif
	.map_io		= pnx67xx_map_io,
	.init_irq	= pnx_init_irq,
	.init_machine	= pnx67xx_init,
	.timer		= &pnx_timer,
MACHINE_END

