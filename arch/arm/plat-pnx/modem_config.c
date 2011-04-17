/*
 * linux/arch/arm/plat-pnx/modem_config.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>	/* need_resched() */

#include <linux/kernel.h>

#include <linux/types.h>
#include <linux/init.h>

#include <mach/gpio.h>
#include <asm/mach/time.h>

#include <asm/proc-fns.h>

#include <asm/memory.h>

#include <asm/io.h>
#include <linux/clk.h>
#include <linux/pm_qos_params.h>
#include <mach/nand_pnx.h>
#include <linux/mtd/nand.h>

#ifdef CONFIG_NKERNEL
#include <nk/xos_area.h>
#include <nk/xos_ctrl.h>
#else
typedef void* xos_area_handle_t;
#endif

#include "modem_config.h"

#define ACER_L1_K3

#if defined (ACER_L1_K2) || defined (ACER_L1_K3) || defined (ACER_L1_AS1)
#define NBR_MEM_COMBOS 4
#elif defined(CONFIG_MACH_PNX67XX_V2_WAVEB_2GB) || \
        defined(CONFIG_MACH_PNX67XX_V2_WAVEC_2GB) || \
        defined(CONFIG_MACH_PNX67XX_WAVED)
#define NBR_MEM_COMBOS 2
#endif

/* taken from PNX System controller manual */
#define NFI_REG_DATA      0x0000	/* direct interface to NAND Flash */
#define NFI_REG_ADDR      0x0004 /* 8 bit address register */
#define NFI_REG_CMD       0x0008 /* 8 bit command register */
#define NFI_REG_STOP      0x000C /* stop register */
#define NFI_REG_CTRL      0x0010 /* control register */
#define NFI_REG_CFG       0x0014 /* configuration register */
#define NFI_REG_STAT      0x0018 /* status register */
#define NFI_REG_INT_STAT  0x001C /* interrupt status register */
#define NFI_REG_IER       0x0020 /* interrupt enable register */
#define NFI_REG_ISR       0x0024 /* interrupt set register */
#define NFI_REG_ICR       0x0028 /* interrupt clear register */
#define NFI_REG_TAC       0x002C /* write access timings */
#define NFI_REG_TC        0x0030 /* terminal count register */
#define NFI_REG_DMA_ECC   0x0034 /* NFI ecc FIFO */
#define NFI_REG_DMA_DATA  0x0038 /* NFI data FIFO */
#define NFI_REG_PAGE_SIZE 0x003C /* NFI Nand page size register, spare area included */
#define NFI_REG_READY     0x0040 /* NFI ready register used to pause cmd/addr fifo */

/* interrupt enable register fields */
#define NFI_REG_IER_TC          1 /* tc enable flag */
#define NFI_REG_IER_RBN         0 /* ready enable flag */

/* config register fields */
#define NFI_REG_CFG_TAC         6 /* enable tac read */
#define NFI_REG_CFG_CELOW       5 /* force CE low */
#define NFI_REG_CFG_DMAECC      4 /* enable DMA ECC channel */
#define NFI_REG_CFG_ECC         3 /* enable ECC generation */
#define NFI_REG_CFG_DMABURST    2 /* enable DMA burst mode */
#define NFI_REG_CFG_DMADIR      1 /* set DMA direction */
#define NFI_REG_CFG_WIDTH       0 /* set NAND flash bus width */
#define NFI_REG_TAC_READ  0x0044 /* read access timing */

/* init sequences */
#define NFI_IER_INIT (  1 << NFI_REG_IER_TC        /* enable terminal count interrupt */ \
	                  | 1 << NFI_REG_IER_RBN       /* enable ready interrupt */          \
	                 )
#define NFI_CFG_INIT (  1 << NFI_REG_CFG_DMAECC    /* enable DMA ECC requester */        \
	                  | 1 << NFI_REG_CFG_ECC       /* enable hardware ECC generation */  \
	                  | 1 << NFI_REG_CFG_DMABURST  /* enable DMA burst */                \
	                  | 0 << NFI_REG_CFG_CELOW     /* assert CE only on NAND access */   \
	                  | 0 << NFI_REG_CFG_WIDTH     /* set bus width to 8 bit */          \
	                  | 1 << NFI_REG_CFG_TAC       /* enable TAC_READ */                 \
	                 )



extern unsigned long num_physpages;

extern struct pnx_modem_sdi_timing pnx_modem_sdi[NBR_MEM_COMBOS][4];
extern unsigned int memory_combo_index;

extern struct pnx_nand_combo_info combo_info[NBR_MEM_COMBOS];
extern struct pnx_nand_timing_data nand_timing_data[NBR_MEM_COMBOS];

extern struct pnx_modem_gpio pnx_modem_gpio_assignation;
extern struct pnx_modem_extint pnx_modem_extint_assignation;
extern struct pnx_modem_adc pnx_modem_adc_config;
extern struct pnx_modem_pwm_config pnx_modem_pwm[3];
extern unsigned int pnx_modem_gpio_reserved[];
extern unsigned int pnx_modem_gpio_reserved_nb;
#ifdef CONFIG_EBI_BUS
extern struct pnx_config_ebi_to_rtk config_ebi[3];
#endif

static struct config_modem_ctxt cfg_modem = {
	.shared = NULL,
	.base_cfg = NULL,
	.new_request = NULL,
};

static void pnx_identify_combo(void)
{
	void __iomem * regs;
	struct clk *nfi_clk;
	int i;
	int maf_id;
	int dev_id;

	nfi_clk = clk_get(0, "NFI");
	clk_enable(nfi_clk);
	pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT, "NFI", 104);

	regs = ioremap(NFI_BASE_ADDR, 0x10000);

	/* init NFI registers */
	iowrite32(0xFF, regs + NFI_REG_CMD);	
	iowrite32(NFI_CFG_INIT, regs + NFI_REG_CFG);
	iowrite32(nand_timing_data[memory_combo_index].timing_tac, 
			regs + NFI_REG_TAC);
	iowrite32(nand_timing_data[memory_combo_index].timing_tac_read, 
			regs + NFI_REG_TAC_READ);

	/* RESET */
	iowrite32(NAND_CMD_RESET, regs + NFI_REG_CMD);
	iowrite32(NAND_CMD_STATUS, regs + NFI_REG_CMD);

	/* Read ID */
	iowrite32(NAND_CMD_READID, regs + NFI_REG_CMD);
	iowrite32(0, regs + NFI_REG_ADDR);

	maf_id = ioread32(regs);
	dev_id = ioread32(regs);

	for (i = 0; combo_info[i].device_id != 0xFF; i++) {
		if (dev_id == combo_info[i].device_id &&
				maf_id == combo_info[i].manuf_id) {
			memory_combo_index = i;
			break;
		}
	}

	pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT, "NFI", PM_QOS_DEFAULT_VALUE);
	clk_disable(nfi_clk);
	clk_put(nfi_clk);
}


/* Ramdump init */
static int __init pnx_init_modem_config(void)
{
	int i;
	printk(KERN_INFO "Start Modem HW configuration\n");

	pnx_identify_combo();

	/* get area */
	cfg_modem.shared = xos_area_connect(CONFIG_AREA_NAME,
			sizeof(struct config_modem_data_shared));
	if (cfg_modem.shared) {
		cfg_modem.base_cfg =
			xos_area_ptr(cfg_modem.shared);

		cfg_modem.new_request =
			xos_ctrl_connect(CONFIG_AREA_NAME, 1);
		if (cfg_modem.new_request == NULL)
			printk(KERN_INFO "connection of pnx_init_modem_config"
				" to wrapper function failed : use xoscore"
				" 1.2\n");
		
		/* check the config_modem_data_shared global structure
		 * alignment */
		if ((cfg_modem.base_cfg->config_data_start !=
				(u32)CONFIG_DATA_START)
			|| (cfg_modem.base_cfg->config_data_end !=
				(u32)CONFIG_DATA_END)) {
			printk(KERN_ERR "Modem and Linux "
					"config_modem_data_shared global "
					"configuration structure not aligned  "
					"%x:%x ; %x:%x\n",
				cfg_modem.base_cfg->config_data_start,
				(u32)CONFIG_DATA_START,
				cfg_modem.base_cfg->config_data_end,
				(u32)CONFIG_DATA_END) ;
			panic("Modem and Linux kernel not aligned\n");
		}
		/* check the pnx_modem_sdi structure alignment */
		if ((cfg_modem.base_cfg->sdi_start != (u32)CONFIG_SDI_START)
			|| (cfg_modem.base_cfg->sdi_end !=
				(u32)CONFIG_SDI_END)) {
			printk(KERN_ERR "Modem and Linux sdi configuration "
					"structures not aligned %x:%x ; "
					"%x:%x\n",
				cfg_modem.base_cfg->sdi_start,
				(u32)CONFIG_SDI_START,
				cfg_modem.base_cfg->sdi_end,
				(u32)CONFIG_SDI_END);
			panic("Modem and Linux kernel not aligned\n");
		}
		/* check the pnx_modem_gpio structure alignment */
		if ((cfg_modem.base_cfg->pnx_modem_gpio.gpio_start
				!= (u32)CONFIG_GPIO_START)
			|| (cfg_modem.base_cfg->pnx_modem_gpio.gpio_end
				!= (u32)CONFIG_GPIO_END)) {
			printk(KERN_ERR "Modem and Linux gpio configuration st"
					"ructures not aligned %x:%x ; %x:%x\n",
				cfg_modem.base_cfg->pnx_modem_gpio.gpio_start,
				(u32)CONFIG_GPIO_START,
				cfg_modem.base_cfg->pnx_modem_gpio.gpio_end,
				(u32)CONFIG_GPIO_END);
			panic("Modem and Linux kernel not aligned\n");
		}
		/* check the pnx_modem_extint structure alignment */
		if ((cfg_modem.base_cfg->pnx_modem_extint.extint_start
				!= (u32)CONFIG_EXTINT_START)
			|| (cfg_modem.base_cfg->pnx_modem_extint.extint_end
				!= (u32)CONFIG_EXTINT_END)) {
			printk(KERN_ERR "Modem and Linux extint "
					"configuration structures "
					"not aligned  %x:%x ; %x:%x\n",
				cfg_modem.base_cfg->pnx_modem_extint.
				extint_start,
				(u32)CONFIG_EXTINT_START ,
				cfg_modem.base_cfg->pnx_modem_extint.extint_end,
				(u32)CONFIG_EXTINT_END);
			panic("Modem and Linux kernel not aligned\n");
		}
		/* check the pnx_modem_adc structure alignment */
		if ((cfg_modem.base_cfg->pnx_modem_adc.adc_start
				!= (u32)CONFIG_ADC_START)
			|| (cfg_modem.base_cfg->pnx_modem_adc.adc_end
				!= (u32)CONFIG_ADC_END)) {
			printk(KERN_ERR "Modem and Linux adc onfiguration "
					"structures not aligned  %x:%x ; "
					"%x:%x\n",
				cfg_modem.base_cfg->pnx_modem_adc.adc_start,
				(u32)CONFIG_ADC_START,
				cfg_modem.base_cfg->pnx_modem_adc.adc_end,
				(u32)CONFIG_ADC_END);
			panic("Modem and Linux kernel not aligned\n");
		}
		/* check the pnx_modem_pwm structures table alignment */
		if ((cfg_modem.base_cfg->pnx_modem_pwm[0].pwm_start
				!= (u32)CONFIG_PWM_START)
			|| (cfg_modem.base_cfg->pnx_modem_pwm[0].pwm_end
				!= (u32)CONFIG_PWM_END)
			|| (cfg_modem.base_cfg->pnx_modem_pwm[1].pwm_start
				!= (u32)CONFIG_PWM_START + 1)
			|| (cfg_modem.base_cfg->pnx_modem_pwm[1].pwm_end
				!= (u32)CONFIG_PWM_END + 1)
			|| (cfg_modem.base_cfg->pnx_modem_pwm[2].pwm_start
				!= (u32)CONFIG_PWM_START + 2)
			|| (cfg_modem.base_cfg->pnx_modem_pwm[2].pwm_end
				!= (u32)CONFIG_PWM_END + 2)) {
			printk(KERN_ERR "Modem and Linux pwm onfiguration "
					"structures not aligned\n");
			panic("Modem and Linux kernel not aligned\n");
		}
		#ifdef CONFIG_EBI_BUS
		/* check the config_ebi structures table alignment */
		if ((cfg_modem.base_cfg->config_ebi[0].ebi_to_rtk_start
				!= (u32)CONFIG_EBI_TO_RTK_START)
			|| (cfg_modem.base_cfg->config_ebi[0].ebi_to_rtk_end
				!= (u32)CONFIG_EBI_TO_RTK_END)
			|| (cfg_modem.base_cfg->config_ebi[1].ebi_to_rtk_start
				!= (u32)CONFIG_EBI_TO_RTK_START + 1)
			|| (cfg_modem.base_cfg->config_ebi[1].ebi_to_rtk_end
				!= (u32)CONFIG_EBI_TO_RTK_END + 1)
			|| (cfg_modem.base_cfg->config_ebi[2].ebi_to_rtk_start
				!= (u32)CONFIG_EBI_TO_RTK_START + 2)
			|| (cfg_modem.base_cfg->config_ebi[2].ebi_to_rtk_end
				!= (u32)CONFIG_EBI_TO_RTK_END + 2)) {
			printk(KERN_ERR "Modem and Linux config ebi "
					"configuration structures not "
					"aligned\n");
			panic("Modem and Linux kernel not aligned\n");
		}
		#endif
		/* check the general_services structure alignment */
		if ((cfg_modem.base_cfg->general_services.
				general_services_start
				!= (u32)CONFIG_GENERAL_SERVICES_START)
			|| (cfg_modem.base_cfg->general_services.
				general_services_end
				!= (u32)CONFIG_GENERAL_SERVICES_END)) {
			printk(KERN_ERR "Modem and Linux general_services "
					"configuration structures not "
					"aligned  %x:%x ; %x:%x\n",
				cfg_modem.base_cfg->general_services.
				general_services_start,
				(u32)CONFIG_GENERAL_SERVICES_START,
				cfg_modem.base_cfg->general_services.
				general_services_end,
				(u32)CONFIG_GENERAL_SERVICES_END);
			panic("Modem and Linux kernel not aligned\n");
		}

		/* transmit modem configuration on modem side */

		/* send DDR timings */
		cfg_modem.base_cfg->nb_sdi_working_point =
			sizeof(pnx_modem_sdi) / NBR_MEM_COMBOS
			/ sizeof(struct pnx_modem_sdi_timing);

		if (cfg_modem.base_cfg->nb_sdi_working_point
				<= MAX_SDI_WORKING_POINT
				&& memory_combo_index != NBR_MEM_COMBOS) {
			memcpy((void *)cfg_modem.base_cfg->pnx_modem_sdi,
					(void *)(pnx_modem_sdi + memory_combo_index),
				sizeof(struct pnx_modem_sdi_timing)
				* cfg_modem.base_cfg->nb_sdi_working_point);
		} else {
			cfg_modem.base_cfg->nb_sdi_working_point = 0;
		}

		/* copy Modem GPIO assignation */
		/* check GPIO configuration */
		
		if (((cfg_modem.base_cfg->pnx_modem_gpio.rf_clk320_needed == 0)
					&& (pnx_modem_gpio_assignation.
						rf_clk320 != -1))
				|| ((cfg_modem.base_cfg->pnx_modem_gpio.
						rf_clk320_needed == 1)
					&& (pnx_modem_gpio_assignation.
						rf_clk320 == -1))) {
			printk(KERN_ERR "Modem and Linux kernel not aligned,\n"
					"check rf_clk320 configuration\n");
			panic("Modem and Linux kernel not aligned\n");

		}
		
		if (((cfg_modem.base_cfg->pnx_modem_gpio.rf_reset_needed == 0)
					&& (pnx_modem_gpio_assignation.rf_reset
						!= -1))
				|| ((cfg_modem.base_cfg->pnx_modem_gpio.
					rf_reset_needed == 1)
					&& (pnx_modem_gpio_assignation.rf_reset
						== -1))) {
			printk(KERN_ERR "Modem and Linux kernel not aligned,\n"
					"check rf_reset configuration\n");
			panic("Modem and Linux kernel not aligned\n");
			
		}

		if (((cfg_modem.base_cfg->pnx_modem_gpio.rf_pdn_needed == 0)
					&& (pnx_modem_gpio_assignation.rf_pdn
						!= -1)) ||
				((cfg_modem.base_cfg->pnx_modem_gpio.
						rf_pdn_needed == 1)
					&& (pnx_modem_gpio_assignation.rf_pdn
						== -1))) {
			printk(KERN_ERR "Modem and Linux kernel not aligned,\n"
					"check rf_pdn configuration\n");
			panic("Modem and Linux kernel not aligned\n");
			
		}

		if (((cfg_modem.base_cfg->pnx_modem_gpio.rf_on_needed == 0)
					&& (pnx_modem_gpio_assignation.rf_on
						!= -1))
				|| ((cfg_modem.base_cfg->pnx_modem_gpio.
						rf_on_needed == 1)
					&& (pnx_modem_gpio_assignation.rf_on
						== -1))) {
			printk(KERN_ERR "Modem and Linux kernel not aligned,\n"
					"check rf_on configuration\n");
			panic("Modem and Linux kernel not aligned\n");
			
		}

		if (((cfg_modem.base_cfg->pnx_modem_gpio.rf_antdet_needed == 0)
					&& (pnx_modem_gpio_assignation.rf_antdet
						!= -1))
				|| ((cfg_modem.base_cfg->pnx_modem_gpio.
						rf_antdet_needed == 1)
					&& (pnx_modem_gpio_assignation.rf_antdet
						== -1))) {
			printk(KERN_ERR "Modem and Linux kernel not aligned,\n"
					"check rf_andet configuration\n");
			panic("Modem and Linux kernel not aligned\n");
			
		}

		if (((cfg_modem.base_cfg->pnx_modem_gpio.agps_pwm_needed == 0)
					&& (pnx_modem_gpio_assignation.agps_pwm
						!= -1))
				|| ((cfg_modem.base_cfg->pnx_modem_gpio.
						agps_pwm_needed == 1)
					&& (pnx_modem_gpio_assignation.agps_pwm
						== -1))) {
			printk(KERN_ERR "Modem and Linux kernel not aligned,\n"
					"check agps_pwm configuration\n");
			panic("Modem and Linux kernel not aligned\n");
			
		}

 		if (((cfg_modem.base_cfg->pnx_modem_gpio.rf_antdet_needed == 0)
					&& (pnx_modem_gpio_assignation.rf_antdet
						!= -1))
				|| ((cfg_modem.base_cfg->pnx_modem_gpio.
						rf_antdet_needed == 1)
					&& (pnx_modem_gpio_assignation.rf_antdet
						== -1))) {
			printk(KERN_ERR "Modem and Linux kernel not aligned,\n"
					"check rf_andet configuration\n");
			panic("Modem and Linux kernel not aligned\n");
		}

		/* preserve modem value */
		pnx_modem_gpio_assignation.rf_reset_needed =
			cfg_modem.base_cfg->pnx_modem_gpio.rf_reset_needed;
		pnx_modem_gpio_assignation.rf_clk320_needed =
			cfg_modem.base_cfg->pnx_modem_gpio.rf_clk320_needed;
		pnx_modem_gpio_assignation.rf_pdn_needed =
			cfg_modem.base_cfg->pnx_modem_gpio.rf_pdn_needed;
		pnx_modem_gpio_assignation.rf_on_needed =
			cfg_modem.base_cfg->pnx_modem_gpio.rf_on_needed;
		pnx_modem_gpio_assignation.rf_antdet_needed =
			cfg_modem.base_cfg->pnx_modem_gpio.rf_antdet_needed;
		pnx_modem_gpio_assignation.agps_pwm_needed =
			cfg_modem.base_cfg->pnx_modem_gpio.agps_pwm_needed;

		memset(pnx_modem_gpio_assignation.gpio_reserved, 0,GPIO_COUNT); 
		/* copy modem GPIO/pins reserved list */
		for(i = 0; i < pnx_modem_gpio_reserved_nb; i++){
			if (pnx_modem_gpio_reserved[i] < GPIO_COUNT)
				pnx_modem_gpio_assignation.gpio_reserved
					[pnx_modem_gpio_reserved[i]] = 1;
		}

		memcpy((void *)&(cfg_modem.base_cfg->pnx_modem_gpio),
				(void *)&pnx_modem_gpio_assignation,
				sizeof(pnx_modem_gpio_assignation));

		/* check EXT INT assignation */
		if (((cfg_modem.base_cfg->pnx_modem_extint.sim_under_volt_needed
				== 0)
			&& (cfg_modem.base_cfg->pnx_modem_extint.
				sim_under_volt_needed != -1))
		   || ((cfg_modem.base_cfg->pnx_modem_extint.
				sim_under_volt_needed == 1)
			&& (cfg_modem.base_cfg->pnx_modem_extint.
				sim_under_volt_needed == -1))) {
			printk(KERN_ERR "Modem and Linux kernel not aligned,\n"
					"check SIM EXT INT configuration\n");
			panic("Modem and Linux kernel not aligned\n");			
		}
		/* only one value --> to be change in the futur */
		cfg_modem.base_cfg->pnx_modem_extint.sim_under_volt =
			pnx_modem_extint_assignation.sim_under_volt;

		/* check ADC configuration*/
		/* chanel number must be < than 5 */
		if ((pnx_modem_adc_config.bat_voltage > 5)
				|| (pnx_modem_adc_config.bat_current > 5)
				|| (pnx_modem_adc_config.bat_fast_temp > 5)
				|| (pnx_modem_adc_config.ref_clock_temperature
					> 5)
				|| (pnx_modem_adc_config.product_temperature
					> 5)
				|| (pnx_modem_adc_config.audio_accessories > 5)
				|| (pnx_modem_adc_config.bat_type > 5)) {
			printk(KERN_ERR "Modem and Linux kernel not aligned,\n"
					"check ADC Channel configuration\n");
			panic("Modem and Linux kernel not aligned \n");				
		}

		/* configure ADC */
		memcpy((void *)&(cfg_modem.base_cfg->pnx_modem_adc),
				(void *)&pnx_modem_adc_config,
				sizeof(pnx_modem_adc_config));
	
		/* send PWM assignement */ 
		memcpy((void *)(cfg_modem.base_cfg->pnx_modem_pwm),
				(void *)pnx_modem_pwm,
				sizeof(struct pnx_modem_pwm_config)*3);

#ifdef CONFIG_EBI_BUS
		/* EBI configuration -- LMSqc17654 */
		memcpy((void *)(cfg_modem.base_cfg->config_ebi),
				(void *)config_ebi,
				sizeof(struct pnx_config_ebi_to_rtk) * NR_EBI);
#endif	

		/* send RAM size -- LMSqc10349 */
		cfg_modem.base_cfg->general_services.ram_size = num_physpages
			<< PAGE_SHIFT;


		/* raise xIT */
		xos_ctrl_raise(cfg_modem.new_request,
				CONFIG_SEND_REQUEST_EVENT_ID);
	} else {
		printk(KERN_INFO "failed to connect to xos area\n");
		panic("Modem doesn't own black box feature\n");
	}

	printk(KERN_INFO "Modem HW configuration done\n");

	return 0;
}

arch_initcall(pnx_init_modem_config);

